/* ToolbarPalette Command for Directory Opus
**DOpus Toolbar Palette!**
ToolbarPalette © 2024-2025 by Christian Arellano García 
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
*/
var script_name = 'ToolbarPalette';
var script_version = '3.0.0';
var FSU, str_tools, dlg, toolbars_map, fayt_map, regexps, dopus_data_dir, g_last_query,
	saved_toolbars, saved_lister_toolbars, excluded_toolbars, user_excluded;

// Called by Directory Opus to initialize the script
function OnInit(initData) {
	initData.name = script_name;
	initData.version = script_version;
	initData.copyright = '(c) Christian Arellano García';
	initData.url = 'https://resource.dopus.com/t/toolbarpalette-a-command-palette-option-for-opus/53883';
	initData.desc = 'DOpus Toolbar Palette!';
	initData.default_enable = true;
	initData.min_version = '13.17.6';
	initData.config = DOpus.Create().OrderedMap();
	initData.config_desc = DOpus.Create().OrderedMap();
	initData.config_groups = DOpus.Create().OrderedMap();
	initData.config_group_order = DOpus.NewVector('General', 'UI');
	AddConfig('excluded toolbars', DOpus.NewVector(), DOpus.strings.Get('config_excluded_toolbars'), 'General');
	AddConfig('excluded words', DOpus.NewVector(), DOpus.strings.Get('config_excluded_words'), 'General');
	AddConfig('include all toolbars', false, DOpus.strings.Get('config_include_all'), 'General');
	AddConfig('include floating toolbars', true, DOpus.strings.Get('config_include_docks'), 'General');
	AddConfig('include lister hotkeys', true, DOpus.strings.Get('config_include_hotkeys'), 'General');
	AddConfig('include system hotkeys', true, DOpus.strings.Get('config_include_system_hotkeys'), 'General');
	AddConfig('include context menu', true, DOpus.strings.Get('config_include_context_menu'), 'General');
	AddConfig('log level', DOpus.NewVector(2, 'debug', 'standard', 'warning', 'off'), DOpus.strings.Get('config_log_level'), 'General');
	AddConfig('allow multiple instances per Lister', false, DOpus.strings.Get('config_allow_multiple'), 'UI');
	AddConfig('load/save UI position', true, DOpus.strings.Get('config_loadsave_position'), 'UI');
	AddConfig('nested separator', '>', DOpus.strings.Get('config_nested_separator'), 'UI');
	AddConfig('Label column max width', 350, DOpus.strings.Get('config_max_width_label'), 'UI');
	AddConfig('FunctionType column max width', 0, DOpus.strings.Get('config_max_width_functype'), 'UI');
	AddConfig('Hotkey column max width', 0, DOpus.strings.Get('config_max_width_hotkey'), 'UI');
	AddConfig('Description column max width', 350, DOpus.strings.Get('config_max_width_desc'), 'UI');
	AddConfig('Instructions column max width', 0, DOpus.strings.Get('config_max_width_instr'), 'UI');
	if (!initData.startup) initData.vars.Delete('~(search_in_flags|menu|version|config_*)');
	return;

	function AddConfig(name, value, desc, group) {
		initData.config[name] = value;
		initData.config_desc(name) = desc;
		initData.config_groups(name) = group;
	}

}
// Called to add commands to Opus
function OnAddCommands(addCmdData) {
	var cmd = addCmdData.AddCommand();
	cmd.name = script_name;
	cmd.method = 'OnToolbarPalette';
	cmd.desc = 'DOpus Toolbar Palette!';
	cmd.label = script_name;
	cmd.template = 'SEARCHFILTER/R,FORCERELOAD/S,REGEX/S,CASE/S,WHOLEWORDS/S,DIACRITICS/S,FORCEEXEC/S,WILDCARDS/S,ADVSEARCH/S,EDITMENU/S,NOFILTER/S';
	cmd.hide = false;
	cmd.icon = 'showtoolbar';

	var fayt = cmd.fayt;
	fayt.enable = true;
	fayt.key = ')';
	fayt.backcolor = '#ffad5b';
	fayt.textcolor = '#000000';
	fayt.label = script_name;
	fayt.realtime = 250;
	fayt.wantempty = true;
	fayt.flags = DOpus.Create().Map();
	fayt.flags[(1 << 0)] = DOpus.strings.Get('flag_case_sensitive'); //Case sensitive
	fayt.flags[(1 << 1)] = DOpus.strings.Get('flag_whole_words'); //Whole words
	fayt.flags[(1 << 2)] = DOpus.strings.Get('label_regex_enabled'); //Regular expressions
	fayt.flags[(1 << 3)] = DOpus.strings.Get('flag_ignore_diacritics'); //Ignore diacritics
	fayt.flags[(1 << 4)] = DOpus.strings.get('flag_wildcards'); //Wildcards support
	fayt.flags[(1 << 5)] = DOpus.strings.Get('flag_include_description'); //include description
	fayt.flags[(1 << 6)] = DOpus.strings.Get('flag_force_execution'); //force execution
	fayt.flags[(1 << 7)] = DOpus.strings.Get('flag_no_filter'); //no filter
	fayt.flags[(1 << 8)] = DOpus.strings.Get('flag_no_pass_items'); //no pass selected items
}

// Implement the ToolbarPalette command
function OnToolbarPalette(scriptCmdData) {
	if (!scriptCmdData.fayt) //Command Mode
		OnCmdToolbarPalette(scriptCmdData);
	else if (scriptCmdData.fayt === script_name)
		OnFAYToolbarPalette(scriptCmdData);
}
// Implement the ToolbarPalette as a FAYT script
function OnFAYToolbarPalette(ScriptFAYTCommandData) {
	var tab = ScriptFAYTCommandData.tab;
	var lister = tab.lister;
	if (!FSU) {
		FSU = DOpus.FSUtil();
		str_tools = DOpus.Create().StringTools();
		var strings = DOpus.Create().Map(
			'global_hotkeys_name', str_tools.LanguageStr(29323),
			'lister_hotkeys_name', str_tools.LanguageStr(29324),
			'menu_AllFilesystemObjects', str_tools.LanguageStr(4031) + ' (' + str_tools.LanguageStr(7209) + ')'
		);
		var include_all = Script.config['include all toolbars'];
		var context_menu_arr = DOpus.NewVector('AllFilesystemObjects');
		if (!excluded_toolbars) excluded_toolbars = DOpus.Create().StringSetI(Script.config['excluded toolbars']);
		if (!saved_toolbars) saved_toolbars = Script.vars.Exists('saved_toolbars') ?
			Script.vars.Get('saved_toolbars') : DOpus.Create().OrderedMap();
		if (!saved_lister_toolbars) saved_lister_toolbars = lister.vars.Exists('saved_lister_toolbars') ?
			lister.vars.Get('saved_lister_toolbars') : DOpus.Create().UnorderedSet();
		var refresh_needed = DiffToolbars(include_all ? DOpus.toolbars : lister.toolbars, strings, include_all);
		if (refresh_needed && (include_all || !saved_lister_toolbars.empty)) {
			Log(2, 'Toolbars change detected. Needs a rebuild...');
			toolbars_map = null;
		}

	}
	if (!toolbars_map) {
		tab.Notify('status', script_name + ' : Building list...');
		var nofilter = ScriptFAYTCommandData.flags & (1 << 7);
		if (!user_excluded) SetUserExcludedVector();
		toolbars_map = toolbars_map === undefined && lister.vars.Exists('toolbars_map') ? lister.vars.Get('toolbars_map') :
			GetToolbarsMap(Script.config['include all toolbars'] ? DOpus.toolbars : lister.toolbars, lister, include_all, nofilter, strings, context_menu_arr);

		lister.vars.Set('toolbars_map', toolbars_map);
		lister.vars.Set('last_no_filtered', nofilter);
		Log(1, 'Saving...saved_toolbars=' + saved_toolbars.count);
		Script.vars.Set('saved_toolbars', saved_toolbars);
		lister.vars.Set('saved_lister_toolbars', saved_lister_toolbars);
	}
	if (toolbars_map.empty) {
		Log(4, 'No items found!');
		tab.Notify('status', script_name + ' : No items found!', '', 1000);
		return;
	}
	tab.Notify('status', '', 100);
	var query = ScriptFAYTCommandData.cmdline.trim();

	if (!fayt_map) fayt_map = {
		'suggest': DOpus.Create().Map(),
		'run': DOpus.Create().Map()
	};
	if (g_last_query !== query) {
		g_last_query = query;
		if (!fayt_map.run.Exists(query)) BuildFAYTMaps(query); //query has changed and does not exists in current map
	}
	if (ScriptFAYTCommandData.suggest && !fayt_map.suggest.empty) {
		Log(1, 'Asking for suggestions : ' + fayt_map.suggest.count);
		tab.UpdateFAYTSuggestions(fayt_map.suggest);
	}

	if (!query || ScriptFAYTCommandData.key !== 'return') return;
	RunInstrfromFAYT(query, DOpus.GetQualifiers());
	tab.CloseFAYT();
	return;

	function BuildFAYTMaps(value) {
		Log(1, 'BuildFAYTMaps : value "' + value + '"');
		fayt_map.run.Clear();
		fayt_map.suggest.Clear();
		var flags = ScriptFAYTCommandData.flags;

		var c_value = value;;
		if (value) {
			var use_regex = flags & (1 << 2) || flags & (1 << 1);
			var use_wildcards = flags & (1 << 4) && !use_regex;
			var str_flags = (use_regex ? 'rn' : '') + (flags & (1 << 0) ? 'c' : '') + (flags & (1 << 3) ? 'i' : '') + 'p';
			var wild = FSU.NewWild();
			if (!(flags & (1 << 4)) && !(flags & (1 << 2)))
				c_value = wild.EscapeString(c_value, flags & (1 << 1) ? 'r' : '');
			if (flags & (1 << 1)) c_value = '\\b' + c_value + '\\b';

			if (!wild.parse(c_value, str_flags)) {
				Log(3, '=> Error parsing "' + value + '" with ' + str_flags + ' flags');
				c_value = '';
			}
		}
		var use_desc = flags & (1 << 5);
		var row, label, desc, item_index, e = new Enumerator(toolbars_map);
		for (; !e.atEnd(); e.moveNext()) {
			item_index = e.item(); //toolbarname_pos
			label = toolbars_map(item_index)('label');
			desc = toolbars_map(item_index)('desc');
			if (!c_value || wild.match(label) || (use_desc && wild.match(desc))) {
				fayt_map.run.Set(value, item_index);
				if (value && value !== label) label = value + ' : ​' + label;
				while (fayt_map.suggest.Exists(label)) label += '​';
				fayt_map.run.Set(label, item_index);
				fayt_map.suggest.Set(label, '(' + toolbars_map(item_index)('toolbar') + ') ' + desc);
			}
		}
		Log(1, 'Total=' + fayt_map.run.count + ';exist=' + fayt_map.run.Exists(value));
		e = null;
		return true;
	}

	function RunInstrfromFAYT(query, qualifiers) {
		Log(1, 'Attempting to run "' + query + '"...');
		var sw;
		while (true) {
			if (fayt_map.run.Exists(query) && toolbars_map.Exists(fayt_map.run(query))) break;
			if (sw) {
				Log(4, 'Unable to locate "' + query + '"!');
				tab.Notify('status', script_name + ' : Unable to locate "' + query + '"!', 1000);
				return;
			}
			Log(4, 'Re-running the search...');
			query = query.replace(/^(?:(.*?): \u200B)?(.*?)\u200B*$/, '$1');
			BuildFAYTMaps(query);
			sw = true;
		}

		if (!regexps) SetRegeXP();
		try {
			var data = toolbars_map(fayt_map.run(query));
			var succ, keydown, key_state, key_match, line;
			var qual_array = qualifiers.split(',');
			var cmd = DOpus.Create().Command();
			if (tab) {
				Log(2, '=> Items selected : ' + tab.stats.selitems);
				cmd.SetSourceTab(tab);
				if (!(ScriptFAYTCommandData.flags & (1 << 8)) && tab.stats.selitems > 0) cmd.SetFiles(tab.selected);
				if (lister.desttab) cmd.SetDestTab(lister.desttab);
			}
			if (data('functype') != 'normal') cmd.SetType(data('functype'));

			main_loop: for (var i = 0; i < data('instructions').length; i++) {
				line = data('instructions')[i];
				//exclude this command
				if (regexps['command_regex'].test(line)) {
					Log(3, ' ==> This command can not be used in this context!');
					break;
				}

				if (!(ScriptFAYTCommandData.flags & (1 << 6)) && regexps['forbidden_modifiers'].test(line)) { //no force exec
					Log(3, '=> Run skipped because at the moment, the command may not be run properly in this context : ' + line);
					break main_loop;
				}
				if (regexps['skipped_modifiers'].test(line)) {
					Log(1, '=> ' + line + ' skipped because modifier is unnecesary in this context');
					continue;
				}
				if (data('functype') !== 'script' && regexps['comment_line'].test(line)) {
					Log(1, '=> ' + line + ' skipped because is a comment');
					continue;
				}

				Log(1, '=> Adding ' + data('instructions')[i]);
				cmd.AddLine(line);

			}
			if (!cmd.linecount) return;
			cmd.SetQualifiers(qualifiers);
			succ = cmd.RunAsync();

			Log(1, '=> : Instructions :');
			for (var e = new Enumerator(cmd); !e.atEnd(); e.moveNext()) {
				Log(1, '=>=> ' + e.item());
			}

		}
		catch (err) {
			Log(4, '=> Error running command for "' + data('label') + '" : ' + err.description);
		}
	}
}

// Implement the ToolbarPalette as a command script
function OnCmdToolbarPalette(scriptCmdData) {
	DOpus.ClearOutput();
	Log(2, '=== COMMAND BEGIN ===');
	Log(1, 'cmdline              : ' + scriptCmdData.cmdline);

	var tab = scriptCmdData.func.sourcetab;
	if (!tab) {
		Log(3, dlgHelper(DOpus.strings.Get('msg_notab')));
		return;
	}
	if (scriptCmdData.func.args.got_arg.EDITMENU) {
		EditMenu(tab);
		return;
	}
	var lister = tab.lister;

	if (!Script.config['allow multiple instances per Lister'] && Script.vars.Exists(lister + '_ToolbarPalette:active')) {
		Log(3, 'A command is currently in use for this lister!');
		// return;
	}

	if (!FSU) FSU = DOpus.FSUtil();
	if (!str_tools) str_tools = DOpus.Create().StringTools();
	var strings = DOpus.Create().Map(
		'adv_search', DOpus.strings.get('label_adv_search'),
		'label_msg_DO_pattern', str_tools.LanguageStr(9338),
		'label_regex', DOpus.strings.get('label_regex_enabled'),
		'label_case', str_tools.LanguageStr(9341),
		'label_ww', str_tools.LanguageStr(9340),
		'label_ignore_diacritics', str_tools.LanguageStr(29499),
		'label_diac_enabled', DOpus.strings.get('label_diac_enabled'),
		'label_use_DO_wildcards', DOpus.strings.get('label_use_DO_wildcards'),
		'label_msg_dlg', DOpus.strings.get('label_msg_dlg'),
		'label_filtering', DOpus.strings.get('label_filtering'),
		'msg_copy_instructions', DOpus.strings.get('msg_copy_instructions'),
		'msg_new_version', DOpus.strings.get('msg_new_version'),
		'label_dlg_remember', DOpus.strings.get('label_dlg_remember'),
		'label_dlg_question', DOpus.strings.get('label_dlg_question'),
		'label_sug_filter', DOpus.strings.get('label_sug_filter'),
		'label_sug_filter_adv', DOpus.strings.get('label_sug_filter_adv'),
		'global_hotkeys_name', str_tools.LanguageStr(29323),
		'lister_hotkeys_name', str_tools.LanguageStr(29324),
		'menu_config', [DOpus.strings.Get('config_no_exit_after_run'), DOpus.strings.Get('config_esc_toclose')],
		'menu_AllFilesystemObjects', str_tools.LanguageStr(4031) + ' (' + str_tools.LanguageStr(7209) + ')'
	);

	var config_opt = [Script.vars.Exists('config_no_exit_after_run') ? Script.vars.Get('config_no_exit_after_run') : 0,
		Script.vars.Exists('config_esc_toclose') ? Script.vars.Get('config_esc_toclose') : 0];
	if (!user_excluded) SetUserExcludedVector();

	if (!regexps) SetRegeXP();
	dopus_data_dir = DOpus.Aliases('dopusdata').path;
	var force_exec = scriptCmdData.func.args.got_arg.forceexec;
	var ask_to_run = !force_exec;
	var adv_search = scriptCmdData.func.args.got_arg.advsearch;
	var arg_force_reload = scriptCmdData.func.args.got_arg.forcereload;
	var nofilter = scriptCmdData.func.args.got_arg.nofilter;
	if (!lister.vars.Exists('last_no_filtered') || lister.vars.Get('last_no_filtered') != nofilter) arg_force_reload = true; //force reload when nofilter is different from the last time
	var include_all = Script.config['include all toolbars'];
	excluded_toolbars = DOpus.Create().StringSetI(Script.config['excluded toolbars']);
	saved_toolbars = Script.vars.Exists('saved_toolbars') && !arg_force_reload ? Script.vars.Get('saved_toolbars') : DOpus.Create().OrderedMap();
	saved_lister_toolbars = lister.vars.Exists('saved_lister_toolbars') ? lister.vars.Get('saved_lister_toolbars') : DOpus.Create().UnorderedSet();
	var refresh_needed = DiffToolbars(include_all ? DOpus.toolbars : lister.toolbars, strings, include_all);
	if (refresh_needed && (include_all || !saved_lister_toolbars.empty)) Log(2, 'Toolbars change detected. Needs a rebuild...');
	var wild_obj = FSU.NewWild();
	var regex, search_mode;
	var match_map = {};
	var search_mode_arr = [];
	Log(1, 'NO FILTER            : ' + nofilter);
	Log(1, 'ADV_SEARCH           : ' + adv_search);
	Log(1, 'FORCE RELOAD         : ' + arg_force_reload + ' (' + scriptCmdData.func.args.got_arg.forcereload + ')');
	Log(1, 'FORCE EXEC           : ' + force_exec);
	Log(1, 'INCLUDE ALL TOOLBARS : ' + include_all);
	Log(1, 'EXCLUDED TOOLBARS    : ' + excluded_toolbars.count);
	BuildMatchMap();
	// ======= VARS FOR CONTROLS ==========================================
	dlg = {};
	dlg.main = tab.Dlg();
	dlg.main.template = 'main';
	dlg.main.title = script_name + ' v' + script_version;
	// dlg.main.disable_window = tab;
	dlg.main.icon = DOpus.LoadImage(FSU.Resolve('/home\\dopusrt.exe') + ',2');
	dlg.main.Create();
	dlg.query_edit = dlg.main.Control('query_edit');
	dlg.clear_btn = dlg.main.Control('clear_btn');
	dlg.listview = dlg.main.Control('listview');
	dlg.total_title = dlg.main.Control('total_title');
	dlg.adv_btn = dlg.main.Control('adv_btn');
	dlg.search_btn = dlg.main.Control('search_btn');
	dlg.regex_btn = dlg.main.Control('regex_btn');
	dlg.wild_btn = dlg.main.Control('wild_btn');
	dlg.case_btn = dlg.main.Control('case_btn');
	dlg.diac_btn = dlg.main.Control('diac_btn');
	dlg.ww_btn = dlg.main.Control('ww_btn');
	dlg.wait_label = dlg.main.Control('wait_label');
	dlg.search_mode = dlg.main.Control('search_mode');
	dlg.progress_bar = dlg.main.Control('progress_bar');
	dlg.progress_track = dlg.main.Control('progress_track');
	dlg.progress_msg = dlg.main.Control('progress_msg');

	dlg.progress_track.bg = '#%vs_progress_background';
	dlg.progress_bar.fg = '#%jobsbar_text';
	dlg.progress_bar.style = 'b';
	dlg.progress_bar.bg = '#%vs_progress_bar_normal';
	dlg.progress_bar.cy = dlg.progress_track.cy - 4;
	dlg.progress_bar.x = dlg.progress_track.x + 2;
	dlg.progress_bar.y = dlg.progress_track.y + 2;
	dlg.progress_step_width = dlg.progress_total_steps = 0;

	dlg.wait_label.label = DOpus.strings.get('msg_wait_results');
	dlg.main.Control('refresh_btn').label = '<a id="refresh" text="' + str_tools.LanguageStr(3073).replace('&', '') + ' (F5)"><%oned:4></a>';
	dlg.main.Control('edit_btn').label = '<a id="edit" text="' + DOpus.strings.Get('label_edit_menu') + '"><%ddbi:105></a>';
	var search_menu = DOpus.Create().Vector(str_tools.LanguageStr(1791), '&' + str_tools.LanguageStr(24), '&' + str_tools.LanguageStr(852), '&' + str_tools.LanguageStr(151), '&' + str_tools.LanguageStr(853));
	// ======= SET HOTKEYS ==========================================
	dlg.main.AddHotkey('refresh_key', 'F5');
	dlg.main.AddHotkey('focus_query', 'f3');
	dlg.main.AddHotkey('wild_btn', 'Alt+W');
	dlg.main.AddHotkey('regex_btn', 'Alt+G');
	dlg.main.AddHotkey('case_btn', 'Alt+C');
	dlg.main.AddHotkey('diac_btn', 'Alt+D');
	dlg.main.AddHotkey('ww_btn', 'Alt+H');
	dlg.main.AddHotkey('up', 'up');
	dlg.main.AddHotkey('down', 'down');
	if (config_opt[1]) dlg.main.AddHotkey('esc', 'Escape');

	// ======= SET SEARCH FLAGS ==========================================
	var search_flags = DOpus.Create().OrderedMap();
	search_flags('regex_btn') = scriptCmdData.func.args.got_arg.regex;
	search_flags('wild_btn') = scriptCmdData.func.args.got_arg.wildcards;
	search_flags('case_btn') = scriptCmdData.func.args.got_arg['case'];
	search_flags('diac_btn') = scriptCmdData.func.args.got_arg.diacritics;
	search_flags('ww_btn') = scriptCmdData.func.args.got_arg.wholewords;
	if (search_flags('regex_btn') || search_flags('ww_btn')) search_flags('wild_btn') = false;

	//======= SET UI ARRANGEMENTS ==========================================

	dlg.listview.EnableGroupView(true);

	var cols_list = dlg.listview.columns;

	cols_list.GetColumnAt(0).name = search_menu(0).replace('&', '');
	cols_list.GetColumnAt(1).name = str_tools.LanguageStr(12);
	cols_list.GetColumnAt(2).name = str_tools.LanguageStr(853);
	cols_list.GetColumnAt(3).name = search_menu(1).replace('&', '');
	cols_list.GetColumnAt(4).name = search_menu(3).replace('&', '');

	var font_id = dlg.main.CreateFont('Segoe UI', 0, 'b');
	dlg.regex_btn.SetFont(font_id);
	dlg.case_btn.SetFont(font_id);
	dlg.diac_btn.SetFont(font_id);
	dlg.ww_btn.SetFont(font_id);
	dlg.regex_btn.autosize();
	dlg.wild_btn.SetFont(font_id);
	dlg.case_btn.autosize();
	dlg.diac_btn.autosize();
	dlg.ww_btn.autosize();
	dlg.search_btn.autosize();

	var search_in_flags = (Script.Vars.Exists('search_in_flags')) ? Script.Vars.Get('search_in_flags') : 1; //default to just label

	if (scriptCmdData.func.args.got_arg.searchfilter) {
		dlg.query_edit.value = scriptCmdData.func.args.searchfilter;
		dlg.query_edit.SelectRange(dlg.query_edit.value.length, -1);
	}
	dlg.is_busy = false;
	dlg.map_count = 0;
	var groups_map = DOpus.Create().OrderedMap();
	var context_menu_arr = DOpus.NewVector('AllFilesystemObjects');
	dlg.watch_files_ids = DOpus.Create().UnorderedSet();
	dlg.list_time = dlg.list_count = dlg_list_timer = dlg_list_total = 0;
	dlg.max_count = 100;

	var label_var, desc_var, str_instr_var, toolbar_var, toolbars_enum;
	ChangeSearchModifiers();
	dlg.main.AddCustomMsg('ToolbarPalette:update', true);
	dlg.main.AddCustomMsg('ToolbarPalette:toolbars_change', true);
	dlg.main.AddCustomMsg('ToolbarPalette:menu_changed', true);
	dlg.main.AddCustomMsg('ToolbarPalette:config_no_exit_after_run', true);
	dlg.main.AddCustomMsg('ToolbarPalette:config_esc_toclose', true);
	if (Script.config['load/save UI position']) dlg.main.LoadPosition('ToolbarPalette:position');
	var msg, sel_index, data, menu_map, menu_vector;

	updateQueryCueText(search_in_flags);

	var max_cols_size = [Script.config['Label column max width'], Script.config['FunctionType column max width'], Script.config['Hotkey column max width'], Script.config['Description column max width'], Script.config['Instructions column max width']];
	for (var i = max_cols_size.length - 1; i >= 0; i--) {
		if (max_cols_size[i] < 0) max_cols_size[i] = 0;
	}
	dlg.main.FlushMsg();
	LoadMenu();
	dlg.main.Show();
	updateList(dlg.query_edit.value, true, refresh_needed, true, true);
	Script.vars.Set(lister + '_ToolbarPalette:active', true);
	Script.vars.Set('saved_toolbars', saved_toolbars);
	lister.vars.Set('saved_lister_toolbars', saved_lister_toolbars);
	lister.vars.Set('toolbars_map', toolbars_map);
	lister.vars.Set('last_no_filtered', nofilter);
	dlg.main.SetTimer(1000, 'check_updates_timer', true);
	main: while (true) {
		msg = dlg.main.GetMsg();
		if (!msg.result) break;
		// Log(1, 'event:"' + msg.event + '"; name="' + msg.name + '"');
		switch (msg.event) {
			case 'focus':
				if (msg.control === 'listview') {
					if (msg.focus) {
						dlg.main.AddHotkey('enter', 'enter');
						dlg.main.AddHotkey('copy', 'ctrl+c');
					}
					else {
						dlg.main.DelHotkey('enter');
						dlg.main.DelHotkey('copy');
					}
				}
				if (msg.control === 'query_edit' && adv_search)
					dlg.main.AddHotkey('enter_key', 'enter');
				else dlg.main.DelHotkey('enter_key');
				break;
			case 'hotkey':
			case 'dblclk':
			case 'click':
				if (msg.name === 'focus_query') dlg.query_edit.focus = true;
				else if (msg.name === 'esc') break main;
				else if (msg.name === 'config_btn') {
					ShowConfigMenu();
				}
				else if (msg.name === 'search_mode' && msg.value === 'update') {
					DOpus.Create().Command().RunCommand('https://resource.dopus.com/t/toolbarpalette-a-command-palette-option-for-opus/53883#release');
					break main;
				}

				else if (msg.name === 'refresh_key' || msg.name === 'refresh_btn') {
					Log(1, 'dlg.is_busy=' + dlg.is_busy);
					dlg.main.FlushMsg(); //just in case
					updateList(dlg.query_edit.value, true, true, false, true);
				}
				else if (msg.name === 'up' || msg.name == 'down') {
					if (!dlg.listview.focus) dlg.listview.focus = true;
					sel_index = dlg.listview.value.index;
					if (msg.name === 'up') dlg.listview.value = sel_index == 0 ? dlg.listview.count - 1 : sel_index - 1;
					else dlg.listview.value = sel_index == dlg.listview.count - 1 ? 0 : sel_index + 1;
				}
				else if (dlg.listview.focus && (msg.name === 'enter' || msg.control === 'listview')) {
					if (RunInstr(dlg.listview.value.data, msg.qualifiers)) break main;
				}
				else if (dlg.listview.focus && msg.name === 'copy') CopyInstr(dlg.listview.value.data);

				else if (msg.control === 'clear_btn' && dlg.query_edit.value !== '') {
					dlg.query_edit.value = '';
					dlg.query_edit.focus = true;
				}

				else if (adv_search && dlg.query_edit.focus && msg.name === 'enter_key')
					updateListAdv(dlg.query_edit.value, false, false, false, true);
				else if (msg.control === 'search_btn') {
					var dlgMenu = DOpus.Dlg();
					dlgMenu.title = script_name + ' v' + script_version;
					dlgMenu.template = 'search_in_flags';
					dlgMenu.window = dlg.main;
					dlgMenu.disable_window = dlg.main;
					dlgMenu.Create();
					dlgMenu.Control('static1').label = strings('label_msg_dlg');
					for (var i = 0; i < search_menu.length; i++) {
						with(dlgMenu.Control('check' + i)) {
							label = search_menu(i);
							value = search_in_flags & (1 << i);
						}
					}
					dlgMenu.RunDlg();

					if (!dlgMenu.result) continue;
					setSearchFlags(dlgMenu);
					updateList(dlg.query_edit.value, false, false, false, true);

				}
				else if (msg.control === 'adv_btn') {
					adv_search = !adv_search;
					dlg.search_btn.enabled = !adv_search;
					dlg.search_btn.bg = dlg.search_btn.fg = adv_search ? '#%vs_button_background_pressed' : '#%vs_button_background';
					updateQueryCueText(search_in_flags);
					updateSearchModeLabel();
				}
				else if (msg.control === 'edit_btn') EditMenu(dlg.main, true);
				else if (search_flags.Exists(msg.name)) {
					ChangeSearchModifiers(msg.name);
					if (dlg.query_edit.value) {
						dlg.main.SetTimer(10, 'update_list_timer', true);
					}
				}
				break;
			case 'custom':
				Log(1, 'Custom msg received  : ' + msg.name);
				if (msg.name === 'ToolbarPalette:toolbars_change') { //update required via ListerChange
					if (!include_all && msg.object.Get('lister') == String(lister)) {
						lister.Update();
						if (DiffToolbars(lister.toolbars, strings, include_all)) {
							Log(2, 'A update is required : A toolbar has changed in this lister');
							dlg.main.FlushMsg();
							updateList(dlg.query_edit.value, true, true, false, true);
						}
					}
				}
				else if (msg.name === 'ToolbarPalette:update') { //update required via ListerChange
					Log(2, 'A update is required : A config has been changed');
					dlg.main.FlushMsg();
					updateList(dlg.query_edit.value, true, true, false, true);
				}
				else if (msg.name === 'ToolbarPalette:menu_changed') { //update required via ListerChange
					Log(1, 'Reloading command menu...');
					LoadMenu();
				}
				else if (msg.name === 'ToolbarPalette:config_no_exit_after_run') { //update required via ListerChange
					config_opt[0] = msg.data; //0 or 1
					Log(1, 'Setting config no exit after run = ' + config_opt[0]);
				}
				else if (msg.name === 'ToolbarPalette:config_esc_toclose') { //update required via ListerChange
					config_opt[1] = msg.data; //0 or 1
					Log(1, 'Setting config esc to close = ' + config_opt[1]);
					if (config_opt[1]) dlg.main.AddHotkey('esc', 'Escape');
					else dlg.main.DelHotkey('esc');

				}
				break;
			case 'rclick':
				if (menu_vector.size > 0 && dlg.listview.focus) {
					var dlgMenu = DOpus.Dlg();
					dlgMenu.choices = menu_vector;
					dlgMenu.menu = 0;
					var menuReturn = dlgMenu.Show();
					if (!menuReturn) continue;
					RunFromMenu(dlg.listview.value.data, menu_vector(menuReturn - 1));
				}
				break;
			case 'dirchange':
				Log(2, 'A update is required : "' + msg.control + '" has been changed');
				saved_toolbars.erase(msg.control);
				dlg.main.SetTimer(50, 'trigger_map_update_timer', true);
				break;
			case 'editchange':
				if (msg.control === 'query_edit' && !adv_search) {
					dlg.main.SetTimer(dlg.query_edit.value !== '' ? 250 : 10, 'update_list_timer', true);
				}
				break;
			case 'timer':
				if (msg.control === 'update_list_timer') {
					if (dlg.is_busy) dlg.main.FlushMsg();
					dlg.is_busy = false;
					updateList(dlg.query_edit.value, false, false, false, true);
				}
				else if (msg.control === 'trigger_map_update_timer') {
					if (dlg.is_busy) dlg.main.FlushMsg();
					dlg.is_busy = false;
					updateList(dlg.query_edit.value, true, true, false, true);
				}
				else if (dlg_list_timer && msg.control === dlg_list_timer) { //must the lazy update list timer
					updateList(dlg.query_edit.value);
				}
				else if (msg.control === 'check_updates_timer') {
					dlg.http = CheckForUpdates('https://resource.dopus.com/raw/53883/1');
				}
				break;
			case 'http':
				if (!dlg.http) continue;
				if (msg.value === 'error') {
					Log(3, 'Unable to connect to the endpoint to check for updates!');
					dlg.http = null;
					continue;
				}
				if (msg.value !== 'data') continue;
				try {
					var res = dlg.http.ReadResponse().match(/<a name="release">([^<]+)<\/a>/i);
					if (res) {
						var new_version = ParseVersion(res[1]);
						Log(1, 'new_version=' + new_version + '; current version=' + script_version);
						if (new_version > ParseVersion(script_version)) {
							Log(1, 'new version detected:' + res[1]);
							dlg.update_exist = true;
							dlg.search_mode.label = strings('msg_new_version').replace('%1', '<a id="update">' + res[1] + '</a>');
						}
					}
					else Log(3, 'Unable to find the pattern required to detect the latest release!');
					dlg.http.shutdown();
				}
				catch (err) {};
				dlg.http = null;
				break;
		}
	}
	if (Script.config['load/save UI position']) dlg.main.SavePosition('ToolbarPalette:position');
	Script.vars.Delete(lister + '_ToolbarPalette:active');
	Script.vars.Set('config_esc_toclose', config_opt[1]);
	Script.vars('config_esc_toclose').persist = true;
	Script.vars.Set('config_no_exit_after_run', config_opt[0]);
	Script.vars('config_no_exit_after_run').persist = true;
	search_flags = null;
	cols_list = null;
	dlg = null;
	groups_map = null;
	max_cols_size = null;
	strings = null;
	tab = null;
	lister = null;
	Log(2, '=== COMMAND FINISHED ===');
	return;

	function CheckForUpdates(endpoint) {
		try {
			var http = dlg.main.NewHTTPReq();
			http.SendRequest(endpoint);
			Log(1, 'Checking for updates from "' + endpoint + '"...');
		}
		catch (err) {
			Log(3, 'Error sending http request : ' + err.description);
			return null;
		}
		return http;
	}

	function ShowConfigMenu() {
		var menuDlg = DOpus.Dlg();
		menuDlg.choices = strings('menu_config');
		menuDlg.menu = config_opt;
		var res = menuDlg.Show();
		menuDlg = null;
		if (!res) return;
		DOpus.SendCustomMsg(res === 2 ? 'ToolbarPalette:config_esc_toclose' : 'ToolbarPalette:config_no_exit_after_run', 2 - config_opt[res - 1]);
		return;
	}

	function RunFromMenu(item_index, menu_entry) {
		if (!toolbars_map.Exists(item_index)) return false;
		Log(2, 'Running menu "' + menu_entry + '"');
		if (!menu_map.Exists(menu_entry)) {
			Log(3, ' ==> Not existent!!!');
			return;
		}
		var cmdline, success;
		try {
			cmdline = menu_map(menu_entry)('cmd');
			cmdline = cmdline.replace(/\$(t|toolbar)\$/gi, toolbars_map(item_index)('toolbar'))
				.replace(/\$(l|label)\$/gi, toolbars_map(item_index)('label'))
				.replace(/\$(d|desc)\$/gi, toolbars_map(item_index)('desc'))
				.replace(/\$(h|hotkey)\$/gi, toolbars_map(item_index)('hotkey'))
				.replace(/\$toolbar_file\$/gi, saved_toolbars(toolbars_map(item_index)('toolbar'))('file'))
				.replace(/\$toolbar_pos\$/gi, saved_toolbars(toolbars_map(item_index)('toolbar'))('pos'))
				.replace(/\$toolbar_line\$/gi, saved_toolbars(toolbars_map(item_index)('toolbar'))('line'))
				.replace(/\$toolbar_group\$/gi, saved_toolbars(toolbars_map(item_index)('toolbar'))('group'))
				.replace(/\$(i|instr)\$/gi, toolbars_map(item_index)('str_instr'));
			if (!cmdline) return false;
			Log(1, ' ==> cmdline : ' + cmdline);
			var cmd = DOpus.Create().Command();
			try {
				lister.Update();
				DOpus.Delay(2); //needed for tab to work 
				tab = lister.activetab;
				if (tab) cmd.SetSourceTab(tab);
				if (lister.desttab) cmd.SetDestTab(lister.desttab);
			}
			catch (err) {
				Log(3, ' ==> Unable to set tabs for command...Continuing as is');
			}
			success = cmd.RunCommand(cmdline);
			cmd = null;
		}
		catch (err) {
			Log(3, ' ==> ' + err.description);
		};
		return success;
	}

	function RunInstr(sel_index, qualifiers) {
		Log(1, 'RUNNING ::: sel_index=' + sel_index + '; qualifiers=' + qualifiers);
		if (!toolbars_map.Exists(sel_index)) return;
		var cmd = DOpus.Create().Command();
		try {
			var succ, keydown, key_state, key_match, line;
			var qual_array = qualifiers.split(',');
			data = toolbars_map(sel_index);
			Log(2, '=> ' + msg.value + ' : functype = ' + data('functype'));
			lister.Update(); //update selected items
			tab = lister.activetab;
			if (tab) {
				Log(2, '=> Items selected : ' + tab.stats.selitems);
				cmd.SetSourceTab(tab);
				if (tab.stats.selitems > 0) cmd.SetFiles(tab.selected);
				if (lister.desttab) cmd.SetDestTab(lister.desttab);
			}
			if (data('functype') !== 'normal') cmd.SetType(data('functype'));

			main_loop: for (var i = 0; i < data('instructions').length; i++) {
				line = data('instructions')[i];
				//exclude this command
				if (regexps['command_regex'].test(line)) {
					Log(3, ' ==> This command can not be used in this context!');
					break;
				}
				if (nofilter) {
					//exclude for forbidden words
					for (var k = 0; k < user_excluded.length; k++) {
						if (user_excluded(k).test(line)) {
							Log(3, ' ==> Run skipped because instr line = "' + line + '"');
							cmd.Clear();
							break main_loop;
						}
					}
				}
				if (regexps['forbidden_modifiers'].test(line)) {
					if (ask_to_run) {
						var msgDlg = DOpus.Dlg();
						msgDlg.title = script_name + ' v' + script_version;
						msgDlg.template = 'msg_ask_run';
						msgDlg.window = dlg.main;
						msgDlg.disable_window = dlg.main;
						msgDlg.Create();
						with(msgDlg.Control('label_question')) {
							label = strings('label_dlg_question').replace('%s', '<nowrap><kbd>' + line + '</kbd></nowrap>');
							AutoSize(true);
						}
						msgDlg.Control('remember_chk').label = strings('label_dlg_remember');
						msgDlg.Control('yes_btn').label = str_tools.LanguageStr(5639);
						msgDlg.Control('no_btn').label = str_tools.LanguageStr(5640);
						msgDlg.AutoSize();
						msgDlg.RunDlg();

						if (msgDlg.result) force_exec = true;
						if (msgDlg.Control('remember_chk').value) ask_to_run = false;

					}
					if (!force_exec) {
						Log(3, '=> Run skipped because at the moment, the command may not be run properly in this context : ' + line);
						cmd.Clear();
						break main_loop;
					}
				}
				if (regexps['skipped_modifiers'].test(line)) {
					Log(1, '=> ' + line + ' skipped because modifier is unnecesary in this context');
					continue;
				}
				if (data('functype') !== 'script' && regexps['comment_line'].test(line)) {
					Log(1, '=> ' + line + ' skipped because is a comment');
					continue;
				}

				Log(1, '=> Adding ' + data('instructions')[i]);
				cmd.AddLine(line);

			}
			if (!cmd.linecount) {
				Log(2, '=> : Instructions : none!');
				return;
			}
			cmd.SetQualifiers(qualifiers);
			succ = cmd.RunAsync();

			Log(1, '=> : Instructions :');
			for (var e = new Enumerator(cmd); !e.atEnd(); e.moveNext()) {
				Log(1, '=>=> ' + e.item());
			}

			if (succ && !config_opt[0]) return true;

		}
		catch (err) {
			Log(4, '=> Error running command for "' + toolbars_map(sel_index)('label') + '" : ' + err.description);
		}
		cmd = null;
		return false;
	}

	function CopyInstr(sel_index) {
		if (toolbars_map.Exists(sel_index)) {
			try {
				data = toolbars_map(sel_index);
				Log(2, 'Copying instructions for "' + data('label') + '" to clipboard');
				var output = '';
				for (var i = 0; i < data('instructions').length; i++)
					output += data('instructions')[i] + '\n';
				if (output) {
					DOpus.SetClip(output.slice(0, -1));
					DOpus.Notify(script_name + ' v' + script_version, strings('msg_copy_instructions').replace('%s', data('label')), 'n');
				}
			}
			catch (err) {
				Log(4, 'Error copy instructions for "' + toolbars_map(sel_index)('label') + '"');
			}
		}
	}

	function globUpdateList(reset_update, reload, force_update, is_first_time) {
		if (reset_update) {
			Log(1, 'Resetting list...');
			dlg.list_count = 0; //reset list because filter content has changed
			dlg.is_busy = false;
			dlg.total_title.label = strings('label_filtering');
		}

		if (!is_first_time && (force_update || arg_force_reload)) lister.Update();
		dlg.listview.redraw = false;
		if (dlg.list_count === 0) {
			Log(1, '=> Clearing list...');
			dlg.listview.RemoveItem(-1);
			dlg.is_busy = true;
			dlg.list_time = new Date();
		}

		if (reload) {
			dlg.wait_label.visible = true;
			if (!force_update && !arg_force_reload && lister.vars.Exists('toolbars_map')) {
				toolbars_map = lister.vars.Get('toolbars_map');
				if (is_first_time) {
					for (var i = 0; i < saved_lister_toolbars.length; i++) {
						if (saved_lister_toolbars(i) == strings('lister_hotkeys_name')) AddToWatch(dopus_data_dir + '\\ConfigFiles\\lister_hotkeys.oxc', saved_lister_toolbars(i));
						else if (saved_lister_toolbars(i) == strings('global_hotkeys_name')) AddToWatch(dopus_data_dir + '\\ConfigFiles\\global_hotkeys.oxc', saved_lister_toolbars(i));
						else if (saved_lister_toolbars(i) == strings('menu_AllFilesystemObjects')) {
							for (var j = context_menu_arr.length - 1; j >= 0; j--) {
								AddToWatch(dopus_data_dir + '\\FileTypes\\' + context_menu_arr[j] + '.oxr', context_menu_arr[j]);
							}
						}
						else AddToWatch(dopus_data_dir + '\\Buttons\\' + saved_lister_toolbars(i) + '.dop', saved_lister_toolbars(i));
					}
				}
			}
			else toolbars_map = GetToolbarsMap(include_all ? DOpus.toolbars : lister.toolbars, lister, include_all, nofilter, strings, context_menu_arr);
			toolbars_enum = new Enumerator(toolbars_map);
			dlg_list_total = toolbars_map.count;
			dlg.list_count = 0;
			Log(2, 'Total buttons        : ' + dlg_list_total);
			dlg.wait_label.visible = false;
			if (is_first_time) {
				for (var i = 0; i < saved_lister_toolbars.length; i++) {
					groups_map(saved_lister_toolbars(i)) = dlg.listview.AddGroup(saved_lister_toolbars(i), i);
					Log(1, 'Added group ' + saved_lister_toolbars(i));
				}
			}
			Log(1, '=> Values ready in : ' + (new Date() - dlg.list_time) + 'ms');
		}
		else if (dlg.list_count === 0) {
			Log(1, '=> Going to first item...');
			toolbars_enum.moveFirst();
			Log(1, 'Total buttons        : ' + dlg_list_total);
		}
	}

	function PostUpdateList(curr_count) {
		if (!dlg.list_count && curr_count) {
			ResizeCols();
			dlg.listview.value = 0;
		}
		dlg.list_count += curr_count;
		if (dlg.list_count < dlg_list_total) {
			Log(1, 'Setting a timer : ' + dlg.list_count + '<' + dlg_list_total);
			dlg_list_timer = dlg.main.SetTimer(10, '', true);
		}
		else {
			Log(1, 'Finished updateList : ' + dlg.list_count + '=' + dlg_list_total);
			dlg.total_title.label = dlg.listview.count + ' / ' + dlg_list_total + ' (' + groups_map.size + ' groups)';
			dlg.query_edit.focus = true;
			dlg.list_count = dlg_list_timer = 0;
			ResizeCols();
			dlg.is_busy = false;
			Log(1, 'List ready in : ' + (new Date() - dlg.list_time) + 'ms');
		}

		dlg.listview.redraw = true;
	}

	function updateList(value, reload, force_update, is_first_time, reset_update) {
		try {
			if (adv_search) {
				updateListAdv(value, reload, force_update, is_first_time, reset_update);
				return;
			}
			globUpdateList(reset_update, reload, force_update, is_first_time);

			if (toolbars_map.empty) return;
			Log(1, 'Filtering by "' + value + '"... count=' + dlg.list_count);
			if (value) {
				var local_value = value;
				var use_DO_wildcards_local = search_flags('wild_btn') && !search_flags('regex_btn') && !search_flags('ww_btn');
				Log(1, '=> use DO wilcards   : ' + use_DO_wildcards_local);

				if (!search_flags('wild_btn') && !search_flags('regex_btn')) local_value = local_value.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
				if (!search_flags('diac_btn')) local_value = str_tools.RemoveDiacritics(local_value);
				if (search_flags('ww_btn')) local_value = '\\b' + local_value + '\\b';
				var sm_local;
				if (use_DO_wildcards_local) {
					sm_local = 'wild_' + search_mode;
					wild_obj.parse(local_value, 'p' + (search_flags('case_btn') ? 'c' : ''));
				}
				else {
					sm_local = 'regex_' + search_mode;
					try {
						regex = new RegExp(local_value, search_flags('case_btn') ? "" : "i");
					}
					catch (err) {
						regex = new RegExp(local_value.replace(/[.*+?^${}()|[\]\\]/g, '\\$&'), search_flags('case_btn') ? "" : "i");
					}
				}

				Log(1, '=> search mode       : ' + sm_local);
			}

			var row, label, desc, instr, item_index, toolbar, helper = use_DO_wildcards_local ? wild_obj : regex;

			for (var curr_count = 0; !toolbars_enum.atEnd(); toolbars_enum.moveNext(), curr_count++) {
				if (curr_count === dlg.max_count) break;
				item_index = toolbars_enum.item(); //toolbarname_pos
				if (!groups_map.Exists(toolbars_map(item_index)('toolbar'))) continue;
				if (!toolbars_map(item_index).Exists('label_d')) toolbars_map(item_index)('label_d') = str_tools.RemoveDiacritics(toolbars_map(item_index)('label'));
				if (!toolbars_map(item_index).Exists('desc_d')) toolbars_map(item_index)('desc_d') = str_tools.RemoveDiacritics(toolbars_map(item_index)('desc'));
				if (!toolbars_map(item_index).Exists('toolbar_d')) toolbars_map(item_index)('toolbar_d') = str_tools.RemoveDiacritics(toolbars_map(item_index)('toolbar'));
				if (!toolbars_map(item_index).Exists('str_instr_d')) toolbars_map(item_index)('str_instr_d') = str_tools.RemoveDiacritics(toolbars_map(item_index)('str_instr'));
				label = toolbars_map(item_index)(label_var);
				desc = toolbars_map(item_index)(desc_var);
				str_instr = toolbars_map(item_index)(str_instr_var);
				toolbar = toolbars_map(item_index)(toolbar_var);
				if (local_value && !match_map[sm_local](helper, label, desc, str_instr, toolbar, toolbars_map(item_index)('hotkey'))) continue;
				row = dlg.listview.getItemAt(dlg.listview.AddItem(toolbars_map(item_index)('label'), item_index, groups_map(toolbars_map(item_index)('toolbar'))));
				if (!row) continue;
				row.subitems(0) = toolbars_map(item_index)('functype');
				row.subitems(1) = toolbars_map(item_index)('hotkey_s');
				row.subitems(2) = toolbars_map(item_index)('desc');
				row.subitems(3) = toolbars_map(item_index)('str_instr');
			}
			PostUpdateList(curr_count);
		}
		catch (err) {
			Log(3, 'Error when trying to update the listview : ' + err.description);
		}
		return;
	}

	function updateListAdv(value, reload, force_update, is_first_time, reset_update) {
		try {

			globUpdateList(reset_update, reload, force_update, is_first_time);
			if (toolbars_map.empty) return;
			if (value) var compiledSearch = compileSearch(value, search_flags);
			var row, item_index, item;
			for (var curr_count = 0; !toolbars_enum.atEnd(); toolbars_enum.moveNext(), curr_count++) {
				if (curr_count === dlg.max_count) break;
				item_index = toolbars_enum.item(); //toolbarname_pos
				if (!toolbars_map(item_index).Exists('label_d')) toolbars_map(item_index)('label_d') = str_tools.RemoveDiacritics(toolbars_map(item_index)('label'));
				if (!toolbars_map(item_index).Exists('desc_d')) toolbars_map(item_index)('desc_d') = str_tools.RemoveDiacritics(toolbars_map(item_index)('desc'));
				if (!toolbars_map(item_index).Exists('toolbar_d')) toolbars_map(item_index)('toolbar_d') = str_tools.RemoveDiacritics(toolbars_map(item_index)('toolbar'));
				if (!toolbars_map(item_index).Exists('str_instr_d')) toolbars_map(item_index)('str_instr_d') = str_tools.RemoveDiacritics(toolbars_map(item_index)('str_instr'));
				if (!groups_map.Exists(toolbars_map(item_index)('toolbar'))) continue;
				item = {
					'label': toolbars_map(item_index)(label_var),
					'desc': toolbars_map(item_index)(desc_var),
					'toolbar': toolbars_map(item_index)(toolbar_var),
					'hotkey': toolbars_map(item_index)('hotkey'),
					'str_instr': toolbars_map(item_index)(str_instr_var)
				}
				if (compiledSearch && !compiledSearch(item)) continue;
				row = dlg.listview.getItemAt(dlg.listview.AddItem(toolbars_map(item_index)('label'), item_index, groups_map(toolbars_map(item_index)('toolbar'))));
				if (!row) continue;
				row.subitems(0) = toolbars_map(item_index)('functype');
				row.subitems(1) = toolbars_map(item_index)('hotkey_s');
				row.subitems(2) = toolbars_map(item_index)('desc');
				row.subitems(3) = toolbars_map(item_index)('str_instr');
			}
			PostUpdateList(curr_count);
		}
		catch (err) {
			Log(3, ' => Error filtering list : ' + err.description);
		}
		dlg.listview.redraw = true;
		return;
	}

	function ResizeCols() {
		var col;
		cols_list.autosize();
		for (var i = cols_list.count - 1; i >= 0; i--) {
			if (!max_cols_size[i]) continue;
			col = cols_list.GetColumnAt(i);
			if (col.width > max_cols_size[i]) col.width = max_cols_size[i];
		}
	}

	function ChangeSearchModifiers(control) {
		if (control) {
			search_flags(control) = !search_flags(control);
			if (control == 'wild_btn') {
				search_flags('regex_btn') = false;
				search_flags('ww_btn') = false;
			}
			else if (control == 'regex_btn' || control == 'ww_btn') search_flags('wild_btn') = false;
		}
		dlg.wild_btn.label = '<a id="link" text="' + strings('label_msg_DO_pattern') + ' (Alt+W)">' + (search_flags('wild_btn') ? '<#!HOTLIGHT>' : '<#!BTNTEXT>') + '✱</#></a>';
		dlg.case_btn.label = '<a id="link" text="' + strings('label_case') + ' (Alt+C)">' + (search_flags('case_btn') ? '<#!HOTLIGHT>' : '<#!BTNTEXT>') + 'Aa</#></a>';
		dlg.regex_btn.label = '<a id="link" text="' + strings('label_regex') + ' (Alt+G)">' + (search_flags('regex_btn') ? '<#!HOTLIGHT>' : '<#!BTNTEXT>') + '•✱</#></a>';
		dlg.diac_btn.label = '<a id="link" text="' + strings('label_diac_enabled') + ' (Alt+D)">' + (search_flags('diac_btn') ? '<#!HOTLIGHT>' : '<#!BTNTEXT>') + 'ůü</#></a>';
		dlg.ww_btn.label = '<a id="link" text="' + strings('label_ww') + ' (Alt+H)">' + (search_flags('ww_btn') ? '<#!HOTLIGHT>' : '<#!BTNTEXT>') + 'ww</#></a>';
		updateSearchModeLabel();
		label_var = 'label' + (!search_flags('diac_btn') ? '_d' : '');
		desc_var = 'desc' + (!search_flags('diac_btn') ? '_d' : '');
		str_instr_var = 'str_instr' + (!search_flags('diac_btn') ? '_d' : '');
		toolbar_var = 'toolbar' + (!search_flags('diac_btn') ? '_d' : '');
		return;
	}

	function updateSearchModeLabel() {
		var l = adv_search ? strings('adv_search') : '';
		if (search_flags('wild_btn') && !search_flags('regex_btn') && !search_flags('ww_btn')) l = l ? (l + ', ' + strings('label_msg_DO_pattern').toLowerCase()) : strings('label_msg_DO_pattern');;
		if (search_flags('regex_btn')) l = l ? (l + ', ' + strings('label_regex').toLowerCase()) : strings('label_regex');
		if (search_flags('ww_btn')) l = l ? (l + ', ' + strings('label_ww').toLowerCase()) : strings('label_ww');
		if (search_flags('case_btn')) l = l ? (l + ', ' + strings('label_case').toLowerCase()) : strings('label_case');
		if (!search_flags('diac_btn')) l = l ? (l + ', ' + strings('label_ignore_diacritics').toLowerCase()) : strings('label_ignore_diacritics');
		if (!dlg.update_exist) dlg.search_mode.label = l;
		Log(1, '=> search_in_flags   : ' + search_in_flags);
		search_mode = search_mode_arr[search_in_flags - 1];
	}

	function setSearchFlags(dlg) {
		var s = 0;
		for (var i = 0; i < search_menu.count; i++)
			if (dlg.Control('check' + i).value) s += 1 << i;
		if (s == 0) s = 1; //always have at least one flag active
		search_in_flags = s;
		Log(1, 'Set search_in_flags  : ' + search_in_flags);
		Script.Vars.Set('search_in_flags', s);
		Script.Vars('search_in_flags').persist = true;
		updateQueryCueText(s);
		updateSearchModeLabel();
	}

	function updateQueryCueText(search_in_flags) {
		Log(1, 'Updating query cue text');
		if (!adv_search) {
			var sug = strings('label_sug_filter');
			for (var i = 0; i < search_menu.count; i++) {
				if (search_in_flags & (1 << i)) sug += ' ' + search_menu(i) + ',';
			}
		}
		dlg.query_edit.cuetext = adv_search ? strings('label_sug_filter_adv') : sug.slice(0, -1).replace(/&/g, '');
		dlg.search_btn.label = '<%ddbi:25><a id="search_opt" text="' + dlg.query_edit.cuetext + '"><%ddbi:6></a>';
	}

	function BuildMatchMap() {
		var keys = ["label", "desc", "toolbar", "instr", "hotkey"];

		// Generar todas las combinaciones posibles
		function generateCombinations(array) {
			var result = [];
			var n = array.length;
			for (var i = 1; i < (1 << n); i++) {
				var combination = [];
				for (var j = 0; j < n; j++) {
					if (i & (1 << j)) {
						combination.push(array[j]);
					}
				}
				result.push(combination);
			}
			return result;
		}

		var combinations = generateCombinations(keys);

		for (var i = 0; i < combinations.length; i++) {
			var combination = combinations[i];
			search_mode_arr[i] = combination.join("_");

			var wild_code = "";
			var regex_code = "";

			for (var j = 0; j < combination.length; j++) {
				if (wild_code !== "") {
					wild_code += " || ";
				}
				wild_code += "wild_obj.Match(" + combination[j] + ")";
				if (regex_code !== "") {
					regex_code += " || ";
				}
				regex_code += "regex.test(" + combination[j] + ")";
			}
			if (wild_code === "") {
				wild_code = "false";
			}
			if (regex_code === "") {
				regex_code = "false";
			}

			match_map["wild_" + search_mode_arr[i]] = new Function("wild_obj",
				"label", "desc", "instr", "toolbar", "hotkey",
				"return " + wild_code + ";"
			);
			match_map["regex_" + search_mode_arr[i]] = new Function("regex",
				"label", "desc", "instr", "toolbar", "hotkey",
				"return " + regex_code + ";"
			);
		}
	}

	function LoadMenu() {
		menu_map = Script.vars.Exists('menu') ? Script.vars.Get('menu') : DOpus.Create().OrderedMap();
		menu_vector = DOpus.NewVector();
		if (menu_map.empty) {
			menu_map.Set(DOpus.strings.get('label_menu_open'), DOpus.Create().Map('type', 'cmd', 'cmd', 'Go "$toolbar_file$" NEWTAB=findexisting'));
			menu_map.Set(DOpus.strings.get('label_menu_show'), DOpus.Create().Map('type', 'cmd', 'cmd', '=return "$toolbar_group$"!="hotkeys" && "$toolbar_group$"!="menu" ? "TOOLBAR ""$t$"" LOCAL" : ""; '));
			Script.vars.Set('menu', menu_map);
			Script.vars('menu').persist = true;
		}
		var e = new Enumerator(menu_map);
		var item;
		for (; !e.atEnd(); e.moveNext()) {
			// Log(1, e.item());
			item = menu_map(e.item())('type') == 'separator' ? '-' : e.item();
			menu_vector.push_back(item);
		}
		Log(1, 'Loaded menu entries : ' + menu_vector.size);
		e = null;
		item = null;
	}

}

function EditMenu(parent, disable) {
	var dlg = {};
	dlg.main = DOpus.Dlg();
	dlg.main.template = 'config_main';
	dlg.main.singleton = true;
	dlg.main.title = script_name + ' - Edit Menu';
	dlg.main.window = parent;
	if (disable) dlg.main.disable_window = parent;
	if (!dlg.main.Create()) {
		Log(3, 'A config window is already opened!');
		return;
	}
	dlg.main.icon = DOpus.LoadImage(DOpus.Aliases('home').path + '\\dopusrt.exe,5');
	dlg.list = dlg.main.Control('list');
	dlg.add_btn = dlg.main.Control('add_btn');
	dlg.del_btn = dlg.main.Control('del_btn');
	dlg.up_btn = dlg.main.Control('up_btn');
	dlg.down_btn = dlg.main.Control('down_btn');
	dlg.save_btn = dlg.main.Control('save_btn');
	dlg.cancel_btn = dlg.main.Control('cancel_btn');
	dlg.command = dlg.main.Control('command');
	dlg.label = dlg.main.Control('label');
	var menu = Script.vars.Exists('menu') ? Script.vars.Get('menu') : DOpus.Create().OrderedMap();
	var menu_list = DOpus.NewVector();
	var enum_menu = new Enumerator(menu);
	for (; !enum_menu.atEnd(); enum_menu.moveNext())
		menu_list.push_back(enum_menu.item());
	enum_menu = null;
	Log(1, 'Menu entries : ' + menu.size);
	var str_tools = DOpus.Create().StringTools();
	var add_menu = DOpus.NewVector(str_tools.LanguageStr(4025), str_tools.LanguageStr(28643));
	var msg, curr_item, has_change;
	var curr_pos = 0;
	UpdateMenuList(curr_pos);
	dlg.label.label = DOpus.strings.get('msg_menulabeldlg') + ': <a id="$label$">$l$|$label$</a>, <a id="$desc$">$d$|$desc$</a>, <a id="$toolbar$">$t$|$toolbar$</a>, <a id="$hotkey$">$h$|$hotkey$</a>, ' +
		'<a id="$instr$">$i$|$instr$</a>, <a id="$toolbar_file$">$toolbar_file$</a>, <a id="$toolbar_pos$">$toolbar_pos$</a>, <a id="$toolbar_line$">$toolbar_line$</a>, <a id="$toolbar_group$">$toolbar_group$</a>';
	dlg.add_btn.label = '<a id="add_btn" text="' + str_tools.LanguageStr(5063).replace('&', '') + '"><%ddbi:140></a>';
	dlg.del_btn.label = '<a id="del_btn" text="' + str_tools.LanguageStr(10050).replace('&', '') + '"><%ddbi:74></a>';
	dlg.down_btn.label = '<a id="down_btn" text="' + str_tools.LanguageStr(29704).replace('&', '') + '"><%ddbi:23></a>';
	dlg.up_btn.label = '<a id="up_btn" text="' + str_tools.LanguageStr(29705).replace('&', '') + '"><%ddbi:22></a>';
	dlg.main.FlushMsg();
	dlg.main.Show();
	while (true) {
		msg = dlg.main.GetMsg();
		if (!msg.result) break;
		switch (msg.event) {
			case 'selchange':
				if (msg.control === 'list') {
					if (dlg.list.value) {
						curr_pos = dlg.list.value.index;
						curr_item = menu_list(curr_pos);
						dlg.del_btn.enabled = true;
					}
					else {
						curr_pos = -1;
						curr_item = null;
						dlg.del_btn.enabled = false;
					}
					Log(1, 'New value:' + curr_item + ';' + curr_pos);
					LoadRightPanel();
				}
				break;
			case 'editchange':
				if (msg.control === 'command')
					dlg.save_btn.enabled = msg.value.trim() != '';
				break;
			case 'click':
				switch (msg.control) {
					case 'add_btn':
						var dlgMenu = DOpus.Dlg();
						dlgMenu.choices = add_menu;
						dlgMenu.menu = 0;
						switch (dlgMenu.Show()) {
							case 1:
								new_value = DOpus.Dlg.GetString(DOpus.strings.Get('msg_newmenu'), '', '', '', script_name + ' - Add new Menu Entry', dlg.main);
								if (new_value && '-' != new_value) {
									curr_pos = dlg.list.GetItemAt(dlg.list.AddItem(new_value));
									curr_item = new_value;
									menu_list.push_back(curr_item);
									dlg.list.value = curr_pos;
									LoadRightPanel();
									has_change = true;
								}
								break;
							case 2:
								curr_item = new Date().getTime();
								menu_list.push_back(curr_item);
								SaveEntry(curr_item, '', 'separator');
								UpdateMenuList(curr_pos);
								has_change = true;
								break;
						}
						break;
					case 'del_btn':
						if (curr_item != null && (!menu.Exists(curr_item) || menu(curr_item)('type') === 'separator' || dlgHelper(DOpus.strings.Get('msg_delmenuentry').replace('%1', curr_item), 1, dlg.main))) {
							menu_list.erase(curr_pos);
							if (curr_pos > 0) curr_pos--;
							UpdateMenuList(curr_pos);
							has_change = true;
						}
						break;
					case 'save_btn':
						if (curr_item) {
							SaveEntry(curr_item, dlg.command.value.trim(), 'cmd');
							has_change = true;
						}
						break;
					case 'up_btn':
						if (curr_pos > 0) {
							menu_list.exchange(curr_pos - 1, curr_pos);
							curr_pos--;
							UpdateMenuList(curr_pos);
							has_change = true;
						}
						break;
					case 'down_btn':
						if (curr_pos != -1 && curr_pos < menu_list.length - 1) {
							menu_list.exchange(curr_pos, curr_pos + 1);
							curr_pos++;
							UpdateMenuList(curr_pos);
							has_change = true;
						}
						break;
					case 'cancel_btn':
						LoadRightPanel();
						break;
					case 'label':
						DOpus.SetClip(msg.value);
						break;
				}
				break;
		}
	}
	if (has_change) {
		Log(2, 'Saving menu to disk...');
		var new_menu = DOpus.Create().OrderedMap();
		for (i = 0; i < menu_list.length; i++) {
			if (!menu.Exists(menu_list(i))) continue;
			new_menu.Set(menu_list(i), menu(menu_list(i)));
		}
		Script.vars.Set('menu', new_menu);
		Script.vars('menu').persist = true;
	}
	menu_list = null;
	dlg = null;
	str_tools = null;
	DOpus.SendCustomMsg(script_name + ':menu_changed');
	Log(1, 'Closing CONFIG...');

	function UpdateMenuList(select_pos) {
		Log(1, 'Updating list...:' + select_pos);
		dlg.list.RemoveItem(-1);
		dlg.list.redraw = false;
		var item;
		for (i = 0; i < menu_list.length; i++) {
			Log(1, menu_list(i));
			if (!menu.Exists(menu_list(i))) continue;
			item = menu(menu_list(i))('type') == 'separator' ? '-----------' : menu_list(i);
			dlg.list.AddItem(item);
		}
		dlg.list.redraw = true;
		dlg.main.FlushMsg();
		Log(1, 'select_pos:' + select_pos);
		if (!menu_list.empty) {
			dlg.list.value = select_pos != undefined && select_pos < dlg.list.count ? select_pos : dlg.list.count - 1;
			curr_pos = dlg.list.value.index;
			curr_item = menu_list(curr_pos);
		}
		else curr_pos = -1;
		LoadRightPanel();
	}

	function LoadRightPanel() {
		var hide = curr_pos == -1 || (menu.Exists(curr_item) && menu(curr_item)('type') == 'separator');
		dlg.command.visible = dlg.save_btn.visible = dlg.cancel_btn.visible = !hide;
		dlg.del_btn.enabled = curr_item ? true : false;
		Log(1, (hide ? 'Unloading fields for ' : 'Loading fields for ') + curr_item);
		if (hide) return;
		dlg.command.value = menu.Exists(curr_item) ? menu(curr_item)('cmd') : '';
		dlg.save_btn.enabled = dlg.command.value ? true : false;
		return;
	}

	function SaveEntry(item, cmd, type) {
		Log(1, 'Saving field "' + item + '"');
		menu.Set(item, DOpus.Create().Map('type', type, 'cmd', cmd));
	}
}

// read an XML toolbar file and remove problematic characters and return valid XML as string
// @return string
function SanitizeXML(toolbar_file) {
	var item = FSU.GetItem(toolbar_file);
	var xmlString;
	var file = item.Open();
	if (file.error === 0) {
		xmlString = str_tools.Decode(file.Read(), 'utf-8');
		file.Close();
	}
	if (xmlString) xmlString = xmlString.replace(/<(\w+)([^>]*?)(\s+\d+\w*=\")([^>]*?)>/g,
		function(match, p1, p2, p3, p4) {
			return '<' + p1 + p2 + p3.replace(/\d+/g, "") + '=' + p4 + '>';
		});
	file = null;
	item = null;
	return xmlString;
}

//Checks changes for saved toolbars since last call
// @return boolean : True if change was detected, False otherwise
function DiffToolbars( /*object.Toolbars*/ curr_toolbars, /*object.Map*/ strings, /*boolean*/ include_all) {
	Log(2, 'Checking if a rebuild is needed : saved_lister_toolbars=' + saved_lister_toolbars.count + '; saved_toolbars=' + saved_toolbars.count);
	var counter = 0;
	if (Script.config['include lister hotkeys']) {
		if (!saved_lister_toolbars.Exists(strings('lister_hotkeys_name'))) {
			Log(1, '=> lister hotkeys are not included in cache');
			return true;
		}
		counter++;
	}
	if (Script.config['include system hotkeys']) {
		if (!saved_lister_toolbars.Exists(strings('global_hotkeys_name'))) {
			Log(1, '=> system hotkeys are not included in cache');
			return true;
		}
		counter++;
	}
	if (Script.config['include context menu']) {
		if (!saved_lister_toolbars.Exists(strings('menu_AllFilesystemObjects'))) {
			Log(1, '=> context menu are not included in cache');
			return true;
		}
		counter++;
	}
	if (excluded_toolbars.empty && counter + curr_toolbars.count != saved_lister_toolbars.count) {
		Log(1, '=> No excluded:saved lister toolbars are different than current ones : ' + (counter + curr_toolbars.count) + '!=' + saved_lister_toolbars.count);
		return true;
	}
	var toolbars_e = new Enumerator(curr_toolbars);
	var toolbar_file, toolbar_name;
	var toolbars_dir = DOpus.Aliases('dopusdata').path + '\\Buttons\\';
	for (; !toolbars_e.atEnd(); toolbars_e.moveNext()) {
		try {
			toolbar_name = toolbars_e.item().def_value;
			Log(1, '=> Checking "' + toolbar_name + '"...');
			if (excluded_toolbars.Exists(toolbar_name)) continue;
			toolbar_file = FSU.GetItem(toolbars_dir + toolbar_name + '.dop');
			if (!include_all && !saved_lister_toolbars.Exists(toolbar_name)) {
				Log(1, '=>' + toolbar_name + ' was not present before in this lister...');
				return true;
			}
			//toolbar is different size than last time,so it must be edited
			if (!saved_toolbars.Exists(toolbar_name)) {
				Log(1, '=>' + toolbar_name + ' not available...');
				return true;
			}
			if (saved_toolbars(toolbar_name)('size') != toolbar_file.size) {
				Log(1, '=>' + toolbar_name + ' size changes...');
				return true;
			}
		}
		catch (err) {
			Log(3, '=> Error comparing toolbar ' + toolbar_name + ' : ' + err.description);
			return true;
		}
		counter++;
	}
	if (counter !== saved_lister_toolbars.count) {
		Log(1, '=> saved lister toolbars are different than current ones : ' + counter + '!=' + saved_lister_toolbars.count);
		return true;
	}
	Log(2, '=> ...Looks OK!');
	return false;
}

/**
 * Get a toolbars map containing all buttons with all the needed values
 *
 * @class      GetToolbarsMap (name)
 * @param      {object.Toolbars}	toolbars	Toolbars object either from DOpus or Lister object
 * @param      {object.Lister}	lister	Current Lister
 * @param      {boolean}    include_all	Include even non active toolbars or not
 * @param      {boolean}    nofilter	Wheter to apply filters  to buttons or not
 * @param      {object.Map}    strings           used mostly for multi language support
 * @param      {object.Vector}    context_menu_arr  List of currently context menus to process
 * @return     {object.Map} return a Map of all indexed buttons/hotkeys/menus
 */
function GetToolbarsMap(toolbars, lister, include_all, nofilter, strings, context_menu_arr) {
	var ini = new Date();
	Log(2, 'Building command database...');
	if (!dopus_data_dir) dopus_data_dir = DOpus.Aliases('dopusdata').path;
	var main_map = DOpus.Create().OrderedMap();
	var sep = Script.config['nested separator'];
	var sep_length = sep.length * -1;
	var map_count = dlg ? dlg.map_count : 0;
	var keys_str = {};
	keys_str[str_tools.LanguageStr(2374)] = /\bwin\b/gi;
	keys_str[str_tools.LanguageStr(2375)] = /\bctrl\b/gi;
	keys_str[str_tools.LanguageStr(2376)] = /\bshift\b/gi;
	keys_str[str_tools.LanguageStr(2377)] = /\balt\b/gi;
	keys_str[str_tools.LanguageStr(2378)] = /\bnum\b/gi;
	keys_str[str_tools.LanguageStr(2008)] = /\bbackspace\b/gi;
	keys_str[str_tools.LanguageStr(2037)] = /\binsert\b/gi;
	keys_str[str_tools.LanguageStr(2041)] = /\benter\b/gi;
	keys_str[str_tools.LanguageStr(2007)] = /\bleft\b/gi;
	keys_str[str_tools.LanguageStr(2006)] = /\bright\b/gi;
	keys_str[str_tools.LanguageStr(2032)] = /\bhome\b/gi;
	keys_str[str_tools.LanguageStr(2033)] = /\bend\b/gi;
	keys_str[str_tools.LanguageStr(2031)] = /\bspace\b/gi;
	keys_str[str_tools.LanguageStr(2038)] = /\bdelete\b/gi;
	keys_str[str_tools.LanguageStr(2034)] = /\bpageup\b/gi;
	keys_str[str_tools.LanguageStr(2035)] = /\bpagedown\b/gi;
	keys_str[str_tools.LanguageStr(2040)] = /\bescape\b/gi;
	keys_str[str_tools.LanguageStr(2039)] = /\btab\b/gi;
	keys_str[str_tools.LanguageStr(2029)] = /\bup\b/gi;
	keys_str[str_tools.LanguageStr(2030)] = /\bdown\b/gi;
	keys_str[str_tools.LanguageStr(5940)] = /app\+0x10/gi; //
	keys_str[str_tools.LanguageStr(5941)] = /app\+0x11/gi; //
	keys_str[str_tools.LanguageStr(5942)] = /app\+0x12/gi; //: Ejecutar Aplicación 2
	keys_str[str_tools.LanguageStr(5943)] = /app\+0x13/gi; //: Atenuar Bajos
	keys_str[str_tools.LanguageStr(5944)] = /app\+0x14/gi; //: Enfatizar Bajos
	keys_str[str_tools.LanguageStr(5945)] = /app\+0x15/gi; //: Aumentar Bajos
	keys_str[str_tools.LanguageStr(5946)] = /app\+0x16/gi; //: Atenuar Agudos
	keys_str[str_tools.LanguageStr(5947)] = /app\+0x17/gi; //: Aumentar Agudos
	keys_str[str_tools.LanguageStr(5948)] = /app\+0x18/gi; //: Silenciar Volumen de Micrófono
	keys_str[str_tools.LanguageStr(5949)] = /app\+0x19/gi; //: Bajar Volumen de Micrófono
	keys_str[str_tools.LanguageStr(5950)] = /app\+0x1a/gi; //: Subir Volumen de Micrófono
	keys_str[str_tools.LanguageStr(5951)] = /app\+0x1b/gi; //: Ayuda
	keys_str[str_tools.LanguageStr(5952)] = /app\+0x1c/gi; //: Buscar
	keys_str[str_tools.LanguageStr(5953)] = /app\+0x1d/gi; //: Nuevo
	keys_str[str_tools.LanguageStr(5954)] = /app\+0x1e/gi; //: Abrir
	keys_str[str_tools.LanguageStr(5955)] = /app\+0x1f/gi; //: Cerrar
	keys_str[str_tools.LanguageStr(5956)] = /app\+0x20/gi; //: Guardar
	keys_str[str_tools.LanguageStr(5957)] = /app\+0x21/gi; //: Imprimir
	keys_str[str_tools.LanguageStr(5958)] = /app\+0x22/gi; //: Deshacer
	keys_str[str_tools.LanguageStr(5959)] = /app\+0x23/gi; //: Rehacer
	keys_str[str_tools.LanguageStr(5960)] = /app\+0x24/gi; //: Copiar
	keys_str[str_tools.LanguageStr(5961)] = /app\+0x25/gi; //: Cortar
	keys_str[str_tools.LanguageStr(5962)] = /app\+0x26/gi; //: Pegar
	keys_str[str_tools.LanguageStr(5963)] = /app\+0x27/gi; //: Contestar por Correo Electrónico
	keys_str[str_tools.LanguageStr(5964)] = /app\+0x28/gi; //: Redireccionar Correo Electrónico
	keys_str[str_tools.LanguageStr(5965)] = /app\+0x29/gi; //: Enviar Correo Electrónico
	keys_str[str_tools.LanguageStr(5966)] = /app\+0x2a/gi; //: Validar Ortografía
	keys_str[str_tools.LanguageStr(5967)] = /app\+0x2b/gi; //: Cambiar Dictado
	keys_str[str_tools.LanguageStr(5968)] = /app\+0x2c/gi; //: Cambiar Micrófono
	keys_str[str_tools.LanguageStr(5969)] = /app\+0x2d/gi; //: Lista de Corrección
	keys_str[str_tools.LanguageStr(5970)] = /app\+0x2e/gi; //: Reproducir Media
	keys_str[str_tools.LanguageStr(5971)] = /app\+0x2f/gi; //: Pausar Media
	keys_str[str_tools.LanguageStr(5972)] = /app\+0x30/gi; //: Grabar Media
	keys_str[str_tools.LanguageStr(5973)] = /app\+0x31/gi; //: Avance Rápido Media
	keys_str[str_tools.LanguageStr(5974)] = /app\+0x32/gi; //: Media Rebobinar
	keys_str[str_tools.LanguageStr(5975)] = /app\+0x33/gi; //: Media Subir Canal
	keys_str[str_tools.LanguageStr(5976)] = /app\+0x34/gi; //: Media Bajar Canal
	keys_str[str_tools.LanguageStr(5925)] = /app\+0x1/gi; //
	keys_str[str_tools.LanguageStr(5926)] = /app\+0x2/gi; //: Adelante
	keys_str[str_tools.LanguageStr(5927)] = /app\+0x3/gi; //: Actualizar
	keys_str[str_tools.LanguageStr(5928)] = /app\+0x4/gi; //: Detener
	keys_str[str_tools.LanguageStr(5930)] = /app\+0x6/gi; //: Favoritos
	keys_str[str_tools.LanguageStr(5931)] = /app\+0x7/gi; //: Tecla Navegador Casa
	keys_str[str_tools.LanguageStr(5932)] = /app\+0x8/gi; //: Tecla Silenciar Volumen
	keys_str[str_tools.LanguageStr(5933)] = /app\+0x9/gi; //: Tecla Bajar Volumen
	keys_str[str_tools.LanguageStr(5934)] = /app\+0xa/gi; //: Tecla Subir Volumen
	keys_str[str_tools.LanguageStr(5935)] = /app\+0xb/gi; //: Siguiente Pista
	keys_str[str_tools.LanguageStr(5936)] = /app\+0xc/gi; //: Pista Anterior
	keys_str[str_tools.LanguageStr(5937)] = /app\+0xd/gi; //: Tecla Detener
	keys_str[str_tools.LanguageStr(5938)] = /app\+0xe/gi; //: Reproducir  Pausa
	keys_str[str_tools.LanguageStr(5939)] = /app\+0xf/gi; //: Tecla Correo
	var label_cleaner_regex = '';
	for (var k in keys_str) label_cleaner_regex += '\\b' + k + '\\b|';
	label_cleaner_regex = new RegExp('\\\\t(?:(?:' + label_cleaner_regex.slice(0, -1).replace(/[.*+?^${}()[\]\\]/g, '\\$&') + ').*|(?:.{1,3}))$', 'i');
	var code_regex = /^(\d+),(\d+)(;.*)?/;
	var arr_modifiers = ['', 'shift', 'ctrl', 'ctrl+shift', 'alt', 'shift+alt', '', 'ctrl+shift+alt', 'win', 'win+shift', 'win+ctrl', 'win+ctrl+shift', 'win+alt', 'win+shift+alt', '', 'win+ctrl+shift+alt'];
	var keyscode_map;
	switch (DOpus.language) {
		case 'esm':
		case 'espanol':
			keyscode_map = {
				219: "'",
				221: "¡",
				186: "`",
				187: "+",
				222: "´",
				191: "ç",
				188: ",",
				190: ".",
				189: "-",
				192: "ñ",
				220: "º"
			};
			break;
		case 'english':
		case 'en-gb':
			keyscode_map = {
				189: "-",
				187: "=",
				219: "[",
				221: "]",
				222: "'",
				220: "\\",
				188: ",",
				190: ".",
				191: "/",
				186: ";",
				192: "`"
			};
			break;
		case 'deutsch':
			keyscode_map = {
				219: "ß",
				221: "´",
				186: "ü",
				187: "+",
				222: "ä",
				191: "#",
				188: "-",
				190: ".",
				189: "/",
				192: "ö",
				220: "^"
			};
			break;
		default:
			keyscode_map = {};
	}
	if (dlg) {
		//cancel watchers
		dlg.main.CancelWatchDir('*');
		dlg.watch_files_ids.Clear();
		saved_lister_toolbars.Clear();

		//progressbar
		if (!dlg.progress_step_width) {
			dlg.progress_total_steps = toolbars.count;
			if (Script.config['include lister hotkeys']) dlg.progress_total_steps++;
			if (Script.config['include system hotkeys']) dlg.progress_total_steps++;
			if (!include_all && Script.config['include floating toolbars']) dlg.progress_total_steps += DOpus.toolbars('docks').count;
			dlg.progress_step_width = ~~((dlg.progress_track.cx - 4) / dlg.progress_total_steps);
			dlg.progress_bar.cx = dlg.progress_step_width;
		}
		dlg.progress_bar.visible = true;
		dlg.progress_track.visible = true;
		dlg.progress_msg.visible = true;
		dlg.search_mode.visible = false;
		dlg.total_title.visible = false;
		var progress_pos = 0;
	}
	var item_count = 1;
	main_map.merge(GetToolbars(toolbars, include_all ? 0 : 1));
	if (!include_all && Script.config['include floating toolbars'])
		main_map.merge(GetToolbars(DOpus.toolbars('docks'), 2));
	if (Script.config['include system hotkeys']) main_map.merge(GetHotkeys('global'));
	if (Script.config['include lister hotkeys']) main_map.merge(GetHotkeys('lister'));
	if (Script.config['include context menu']) main_map.merge(GetContextMenus(context_menu_arr));

	Log(1, '=> Done in ' + (new Date() - ini) + 'ms');
	if (dlg) {
		dlg.progress_bar.visible = false;
		dlg.progress_track.visible = false;
		dlg.progress_msg.visible = false;
		dlg.search_mode.visible = true;
		dlg.total_title.visible = true;
		dlg.map_count = map_count;
	}
	keys_str = null;
	keyscode_map = null;
	return main_map;

	function GetToolbars(toolbars, from_lister) {
		var toolbars_enum = new Enumerator(toolbars);
		var map = DOpus.Create().OrderedMap();
		var toolbar_file, toolbar_map, toolbar_name, in_cache, pos, line, group;

		for (; !toolbars_enum.atEnd(); toolbars_enum.moveNext()) {
			try {
				toolbar_name = toolbars_enum.item().def_value;
				if (dlg) {
					dlg.progress_bar.cx = progress_pos;
					dlg.progress_msg.label = toolbar_name + ' (' + item_count + '/' + dlg.progress_total_steps + ')';
				}
				if (excluded_toolbars.Exists(toolbar_name)) continue;
				toolbar_file = FSU.GetItem(dopus_data_dir + '\\Buttons\\' + toolbar_name + '.dop');
				in_cache = saved_toolbars.Exists(toolbar_name) && saved_toolbars(toolbar_name)('size') == toolbar_file.size.def_value;
				toolbar_map = in_cache ? saved_toolbars(toolbar_name)('map') : readToolbar(toolbar_file.def_value, toolbar_name);
				if (dlg) progress_pos += dlg.progress_step_width;
				if (toolbar_map.empty) continue;
				Log(1, 'Adding ' + toolbar_map.count + ' items');
				map.merge(toolbar_map);
				saved_lister_toolbars.insert(toolbar_name);
				if (!in_cache) {
					if (from_lister === 0) {
						for (var h = new Enumerator(lister.toolbars); !h.atEnd(); h.moveNext()) {
							if (h.item().def_value == toolbar_name) {
								pos = h.item().pos;
								line = h.item().line;
								group = h.item().group;
								break;
							}
						}
						h = null;
					}
					else if (from_lister === 1) {
						pos = toolbars_enum.item().pos;
						line = toolbars_enum.item().line;
						group = toolbars_enum.item().group;
					}
					else { //floating toolbars
						pos = -1;
						line = -1;
						group = 'docks';
					}
					if (pos === undefined) pos = -1;
					if (line === undefined) line = -1;
					if (group === undefined) group = 'none';
					saved_toolbars.Set(toolbar_name, DOpus.Create().Map('map', toolbar_map, 'size', toolbar_file.size.def_value, 'file', toolbar_file.def_value,
						'group', group, 'line', line, 'pos', pos));
				}
				else Log(1, 'Reading ' + toolbar_name + ' from cache...');
				if (dlg) AddToWatch(toolbar_file, toolbar_name);
			}
			catch (err) {
				Log(3, '=> Error processing toolbar ' + toolbar_name + ' : ' + err.description);
			}
			item_count++;
		}
		toolbars_enum = null;
		return map;
	}

	function GetHotkeys(type) {
		try {
			var hotkeys_file = FSU.GetItem(dopus_data_dir + '\\ConfigFiles\\' + type + '_hotkeys.oxc');
			var name = strings(type + '_hotkeys_name');
			Log(1, '=> ' + hotkeys_file + '::' + name);
			if (dlg) {
				dlg.progress_bar.cx = progress_pos;
				dlg.progress_msg.label = name + ' (' + item_count + '/' + dlg.progress_total_steps + ')';
			}
			var in_cache = saved_toolbars.Exists(name) && saved_toolbars(name)('size') == hotkeys_file.size.def_value;
			var hotkeys_map = in_cache ? saved_toolbars(name)('map') : readToolbar(hotkeys_file.def_value, name, true);
			if (!in_cache) saved_toolbars.Set(name, DOpus.Create().Map('map', hotkeys_map, 'size', hotkeys_file.size.def_value, 'file', hotkeys_file.def_value,
				'group', 'hotkeys', 'line', -1, 'pos', -1));
			else Log(1, '=> Reading ' + name + ' from cache...');
			saved_lister_toolbars.insert(name);
			if (dlg) {
				AddToWatch(hotkeys_file, name);
				progress_pos += dlg.progress_step_width;
			}
			item_count++;
		}
		catch (err) {
			Log(3, '=> Error processing ' + type + 'hotkeys : ' + err.description);
		}
		Log(1, 'Adding ' + hotkeys_map.count + ' items');
		return hotkeys_map;
	}

	function readToolbar(toolbar_file, toolbar_name, is_hotkeys) {
		Log(2, 'Parsing "' + toolbar_file + '"...');
		var toolbar_map = DOpus.Create().OrderedMap();
		try {
			if (!FSU.Exists(toolbar_file)) return toolbar_map;

			var xmlDoc = new ActiveXObject('Msxml2.DOMDocument');
			if (!xmlDoc) return toolbar_map;
			var buttons, button, str_instr, childs, functionType, instructions, label, tip, hotkey, key_match, inst, index, pe, modifier_end;

			xmlDoc.load(toolbar_file);
			pe = xmlDoc.parseError;
			if (pe.errorCode === 0) {
				if (is_hotkeys)
					processButtons(false, xmlDoc.selectNodes('//hotkeys/key[@defkey or @hotkey]'), '');
				else
					processButtons(true, xmlDoc.selectNodes('//toolbar/buttons/button'), '');
			}
			else {
				Log(1, ' => Unable to parse ' + toolbar_file + ' normally. Trying to sanitize content...');
				var xmlString = SanitizeXML(toolbar_file);
				if (!xmlString) return toolbar_map;
				xmlDoc.loadXML(xmlString);
				pe = xmlDoc.parseError;
				if (pe.errorCode === 0) {
					if (is_hotkeys)
						processButtons(false, xmlDoc.selectNodes('//hotkeys/key[@defkey or @hotkey]'), '');
					else
						processButtons(true, xmlDoc.selectNodes('//toolbar/buttons/button'), '');
				}
				else Log(3, ' => Error parsing xml file!: ' + pe.line + '_' + pe.linepos);
			}
			xmlDoc = null;
		}
		catch (err) {
			Log(3, ' => Error while trying to get "' + toolbar_name + '" data : ' + err.description);
		};
		return toolbar_map;

		function processButtons(recursive, buttons, parents_label) {
			if (!buttons) return;
			Log(1, ' => Processing ' + buttons.length + ' buttons ; parents_label=' + parents_label);
			var added = '';
			for (var i = 0; i < buttons.length; i++) {
				button = buttons[i];
				label = button.selectSingleNode("label");

				label = label ? label.text.replace(label_cleaner_regex, '').replace(/&+/g, function(match) {
					return (match.length > 1) ? match.substring(1) : '';
				}) : '';
				if (button.getAttribute("field_type")) {
					Log(1, ' ==> Discarding "' + label + '" because is a field!');
					continue;
				}
				str_instr = '';
				hotkey = button.getAttribute("hotkey") || '';
				if (!hotkey) {
					var hotkey_sep = button.selectSingleNode("hotkeys/chord/key") ? ';' : '\n';
					var hotkeyNode = button.selectNodes("hotkeys/key|hotkeys/chord/key");
					if (hotkeyNode) {
						for (var j = 0; j < hotkeyNode.length; j++)
							hotkey += hotkeyNode[j].text + hotkey_sep;
					}
					hotkey = hotkey.slice(0, -1);
				}
				if (hotkey) {
					hotkey = hotkey.toUpperCase();
					key_match = hotkey.match(code_regex);
					if (key_match) {
						hotkey = ConvertKeyCode(key_match);
						// Log(1, label + ':' + key_match[1] + ',' + key_match[2] + '=>' + hotkey);
					}
					for (var kkk in keys_str) hotkey = hotkey.replace(keys_str[kkk], kkk);
				}
				functionType = button.selectSingleNode("function/@type");
				functionType = functionType ? functionType.nodeValue : null;
				if (button.getAttribute("type") == 'menu' && functionType)
					Log(4, toolbar_name + ':' + label + ' is malformed!');
				tip = button.selectSingleNode("tip");
				instructions = button.selectNodes("function/instruction");
				//no instructions means discard
				if (instructions && functionType) {
					var not_excluded = true;
					var instructionsVector = DOpus.NewVector();
					loop: for (var j = 0; j < instructions.length; j++) {
						inst = instructions[j].text.trim();
						if (!inst) continue;
						if (!nofilter) {
							//exclude for forbidden words
							for (var k = 0; k < user_excluded.length; k++) {
								if (user_excluded(k).test(inst)) {
									not_excluded = false;
									Log(1, ' ==> Excluding "' + label + '" because instr line = "' + inst + '"');
									break loop;
								}
							}
						}
						str_instr += '; ' + inst;
						// if (inst[0] == '@' && banned_modifiers_regex.test(inst)) continue;
						instructionsVector.push_back(inst);
					}

					if (not_excluded && !instructionsVector.empty) {
						added += '"' + label + '",';
						toolbar_map.set(map_count, DOpus.Create().OrderedMap(
							'label', (parents_label && parents_label.slice(sep_length) != sep ? (parents_label + sep) : parents_label) + label,
							'desc', tip ? tip.text : '',
							'hotkey', hotkey,
							'hotkey_s', hotkey.replace(/\n/g, ' | '),
							'functype', functionType == 'batch' ? 'msdos' : functionType,
							'instructions', instructionsVector,
							'str_instr', str_instr.slice(2),
							'toolbar', toolbar_name
						));
						map_count++;
					}
				}
				else Log(1, ' ==> Discarding "' + label + '" because no instructions/functype');
				if (recursive) {
					childs = button.selectNodes('button');
					if (childs && childs.length > 0) {
						// Log(1, ' ==> "' + label + '" has childs');
						if (parents_label && parents_label.slice(sep_length) != sep) parents_label += sep;
						processButtons(true, childs, parents_label + label);
					}
				}
			}
			if (added) Log(1, ' ==> Added ' + added.slice(0, -1));
		}

		function ConvertKeyCode(keycodes) {
			var r = arr_modifiers[keycodes[2]] || '';
			return r + (r ? '+' : '') + (keyscode_map[keycodes[1]] || '???') + keycodes[3];
		}
	}

	function GetContextMenus(arr_names) {
		for (var i = 0; i < arr_names.length; i++) {
			try {
				var context_file = FSU.GetItem(dopus_data_dir + '\\FileTypes\\' + arr_names[i] + '.oxr');
				var name = strings('menu_' + arr_names[i]);
				if (dlg) {
					dlg.progress_bar.cx = progress_pos;
					dlg.progress_msg.label = name + ' (' + item_count + '/' + dlg.progress_total_steps + ')';
				}
				var in_cache = saved_toolbars.Exists(name) && saved_toolbars(name)('size') == context_file.size.def_value;
				var context_map = in_cache ? saved_toolbars(name)('map') : readContextMenu(context_file.def_value, name);
				if (!in_cache) saved_toolbars.Set(name, DOpus.Create().Map('map', context_map, 'size', context_file.size.def_value, 'file', context_file.def_value,
					'group', 'menu', 'line', -1, 'pos', -1));
				else Log(1, 'Reading ' + name + ' from cache...');
				saved_lister_toolbars.insert(name);
				if (dlg) {
					AddToWatch(context_file, name);
					progress_pos += dlg.progress_step_width;
				}
				item_count++;
			}
			catch (err) {
				Log(3, '=> Error processing ' + name + ' menu : ' + err.description);
			}
		}
		Log(1, 'Adding ' + context_map.count + ' items');
		return context_map;

		function readContextMenu(c_file, name) {
			Log(2, 'Parsing file "' + c_file + '"...');
			var c_map = DOpus.Create().OrderedMap();
			try {
				if (!FSU.Exists(c_file)) return c_map;
				var xmlDoc = new ActiveXObject('Msxml2.DOMDocument');
				if (!xmlDoc) return c_map;
				var str_instr, child, index, label, com, isStartSubMenu, isEndSubMenu, pos, instructions;
				xmlDoc.load(c_file);
				pe = xmlDoc.parseError;
				if (pe.errorCode == 0) processMenu(xmlDoc.selectNodes('//key[@name="opus"]/key'));
				else {
					Log(1, ' => Unable to parse ' + c_file + ' normally. Trying to sanitize content...');
					var xmlString = SanitizeXML(c_file);
					if (xmlString) {
						xmlDoc.loadXML(xmlString);
						pe = xmlDoc.parseError;
						if (pe.errorCode == 0) processMenu(xmlDoc.selectNodes('//key[@name="opus"]/key'));
						else Log(3, ' => Error parsing xml file!: ' + pe.line + '_' + pe.linepos);
					}
				}
				xmlDoc = null;
			}
			catch (err) {
				Log(3, ' => Error while trying to get "' + c_file + '" menu : ' + err.description);
			};
			return c_map;

			function processMenu(node) {
				if (!node) return;
				Log(1, ' => Processing ' + node.length + ' entries');
				var added = '';
				var items = [];
				for (var i = 0; i < node.length; i++) items.push('');
				var currentPrefix = '';
				for (var i = 0; i < node.length; i++) {
					try {
						child = node[i];
						pos = child.selectSingleNode('./key[@name="opusflags"]/value[@name="Position"]');
						if (!pos) continue;
						pos = parseInt(pos.text, 10);
						label = child.selectSingleNode('./value[@name="@default"]');
						isStartSubMenu = child.selectSingleNode('./key[@name="opusflags"]/value[@name="SubMenu"]');
						isEndSubMenu = child.selectSingleNode('./key[@name="opusflags"]/value[@name="EndSubMenu"]');
						com = isStartSubMenu ? '' : child.selectSingleNode('./key[@name="command"]/value[@name="@default"]');
						com = com ? com.text : "";
						label = label ? label.text.replace(label_cleaner_regex, '').replace(/&+/g, function(match) {
							return (match.length > 1) ? match.substring(1) : '';
						}) : "";

						items[pos] = {
							'start': isStartSubMenu,
							'end': isEndSubMenu,
							'label': label,
							'cmd': com
						};
					}
					catch (err) {
						Log(3, ' => Error while trying to get "' + pos + '" menu : ' + err.description);
						continue;
					}
				}
				for (var i = 0; i < items.length; i++) {
					var instructionsVector = DOpus.NewVector();
					if (!items[i].label && !items[i].cmd) continue;
					if (items[i].start) {
						currentPrefix += (currentPrefix ? sep : '') + items[i].label;
						continue;
					}
					str_instr = '';
					instructions = items[i].cmd.split('\n');
					var not_excluded = true;
					loop: for (var j = 0; j < instructions.length; j++) {
							inst = instructions[j].trim();
							if (!inst) continue;
							if (!nofilter) {
								//exclude for forbidden words
								for (var k = 0; k < user_excluded.length; k++) {
									if (user_excluded(k).test(inst)) {
										not_excluded = false;
										Log(1, ' ==> Excluding "' + items[i].label + '" because instr line = "' + inst + '"');
										break loop;
									}
								}
							}
							str_instr += '; ' + inst;
							// if (inst[0] == '@' && banned_modifiers_regex.test(inst)) continue;
							instructionsVector.push_back(inst);
						}
						// Log(1, items[i].label + ':' + instructionsVector.count);
					if (not_excluded && !instructionsVector.empty) {
						added += '"' + items[i].label + '",';
						items[i].label = (currentPrefix ? (currentPrefix + sep) : '') + items[i].label;
						c_map.set(map_count, DOpus.Create().OrderedMap(
							'label', items[i].label || '',
							'desc', '',
							'hotkey', '',
							'hotkey_s', '',
							'functype', 'normal',
							'instructions', instructionsVector,
							'str_instr', str_instr.slice(2),
							'toolbar', name
						));
						map_count++;
					}
					if (items[i].end) currentPrefix = '';
				}
				if (added) Log(1, ' ==> Added ' + added.slice(0, -1));
			}
		}
	}
}

// Called whenever the user modifies the script's configuration
function OnScriptConfigChange(configChangeData) {
	var v = configChangeData.changed;
	var update;
	outer: for (var i = 0; i < v.length; i++) {
		switch (v(i)) {
			case 'include lister hotkeys':
			case 'include system hotkeys':
			case 'include floating toolbars':
				update = true;
				break;
			case 'nested separator':
			case 'excluded words':
			case 'include all toolbars':
			case 'excluded toolbars':
				if (!update) {
					var listers = DOpus.listers;
					for (var j = 0; j < listers.count; j++) {
						listers(j).vars.Delete('toolbars_map');
						listers(j).vars.Delete('saved_lister_toolbars');
						listers(j).vars.Delete('last_no_filtered');
					}
					listers = null;
					Script.vars.Delete('saved_toolbars');
				}
				update = true;
				break outer;
		}
	}
	if (update) DOpus.SendCustomMsg('ToolbarPalette:update');
	v = null;
}

// Called when a change to the Lister UI occurs
function OnListerUIChange(listerUIChangeData) {
	if (/toolbar/.test(listerUIChangeData.change)) DOpus.SendCustomMsg('ToolbarPalette:toolbars_change', DOpus.Create().Map('lister', String(listerUIChangeData.lister)));
	return;
}

function compileSearch(input, search_flags) {
	Log(1, 'input:"' + input + '"; ' + input.length);
	var pattern = /(?:(\(*)\s*\$(l|d|v|t|h)\s*(==|!=)\s*"(.*?)(^|[^'])?"\s*(\)*))(?:\s*(AND|OR|&&|\|\|)\s*)?/gi;
	var result = [];
	var match;
	var currGroup = [];
	try {
		while ((match = pattern.exec(input)) !== null) {
			for (var i = 0; i < match.length; i++) DOpus.Output('match ' + i + ':' + match[i]);
			var parenOpen = match[1] === '(';
			var parenClose = match[6] === ')';
			var key = match[2] ? match[2].toLowerCase() : null; // Captura la clave ($n, $l, etc.)
			var operator = match[3]; // Captura el operador (=, ==, !=)
			var value = match[4] ? match[4] : ''; // Procesa comillas escapadas
			var sep = match[7] ? match[7].toUpperCase() : null; // Captura el separador AND/OR si existe
			if (match[5]) value += match[5];
			value = value.replace(/'"/g, '\\"').replace(/''/g, '\'');

			switch (sep) {
				case 'AND':
					sep = '&&';
					break;
				case 'OR':
					sep = '||';
					break;
			}
			var currValue = {
				'key': key,
				'value': value,
				'operator': operator,
				'sep': parenOpen ? sep : null
			};
			currGroup.push(currValue);
			if (parenClose || (sep && !parenOpen)) {
				result.push({
					'values': currGroup,
					'sep': sep
				});
				currGroup = [];
			}
		}
		if (currGroup.length > 0) result.push({
			'values': currGroup,
			'sep': sep
		});
		if (result.length == 0) return null;
		var use_DO_wildcards_local = search_flags('wild_btn') && !search_flags('regex_btn') && !search_flags('ww_btn');
		Log(1, '=> use DO wilcards   : ' + use_DO_wildcards_local);
		var conditions = [];

		// Recorrer cada grupo del término de búsqueda
		Log(1, ' groups=' + result.length);
		for (var i = 0; i < result.length; i++) {
			var group = result[i];
			var groupConditions = [];
			Log(1, 'values=' + group.values.length + '; sep=' + group.sep);
			// Recorrer cada valor dentro del grupo
			for (var j = 0; j < group.values.length; j++) {
				var term = group.values[j];
				var condition = compileTerm(term);
				if (!condition) continue;
				groupConditions.push(condition);
				// Agregar el operador lógico (AND/OR) si hay uno
				if (term.sep) {
					groupConditions.push(' ' + term.sep + ' ');
				}
			}

			// Unir condiciones dentro del grupo
			var groupExpression = "(" + groupConditions.join('') + ")";
			if (group.sep != null) groupExpression += ' ' + group.sep + ' ';
			conditions.push(groupExpression);
		}
		Log(1, conditions.join('') + ';');
		// Unir grupos por AND u OR
		return new Function('item', 'return ' + conditions.join('') + ';');
	}
	catch (err) {
		Log(3, ' => Error parsing input : ' + err.description);
		return null;
	}

	function compileTerm(term) {
		if (term.operator != '==' && term.operator != '!=') {
			Log(3, '=> Error parsing input : Operator can only be == or !=');
			return null;
		}

		var field;

		// Determinar el campo en función de la clave
		switch (term.key) {
			case 'l':
				field = 'item.label';
				break;
			case 'd':
				field = 'item.desc';
				break;
			case 'v':
				field = 'item.str_instr';
				break;
			case 't':
				field = 'item.toolbar';
				break;
			case 'h':
				field = 'item.hotkey';
				break;
			default:
				Log(3, '=> Error parsing input : Field can only be l, d, v or t');
				return null;
		}
		var strFilter = term.value;
		if (!strFilter) return field + ' ' + term.operator + ' ""';
		if (!search_flags('wild_btn') || search_flags('regex_btn')) strFilter = strFilter.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
		if (!search_flags('diac_btn')) strFilter = str_tools.RemoveDiacritics(strFilter);
		if (search_flags('ww_btn')) strFilter = '\\\\b' + strFilter + '\\\\b';

		if (use_DO_wildcards_local) {
			if (strFilter.charAt(0) != '*') strFilter = '*' + strFilter;
			if (strFilter.lastIndexOf('*') != strFilter.length - 1) strFilter += '*';
			return (term.operator === '!=' ? '!' : '') + 'FSU.NewWild("' + strFilter + '","' + (search_flags('case_btn') ? 'c' : '') + '").Match(' + field + ')';
		}
		else return (term.operator === '!=' ? '!' : '') + 'new RegExp("' + strFilter + '","' + (search_flags('case_btn') ? '' : 'i') + '").test(' + field + ')';

	}

}

function SetUserExcludedVector() {
	var u_excluded = Script.config['excluded words']; //discard foldercontent
	user_excluded = DOpus.NewVector();
	for (var k = 0; k < u_excluded.length; k++) {
		Log(1, ' => user excluded words   : ' + u_excluded(k));
		user_excluded.push_back(new RegExp('\\b' + u_excluded(k).replace(/[.*+?^${}()|[\]\\]/g, '\\$&') + '\\b', "i"));
	}
	user_excluded.push_back(/^(@ctx|marker)/i); //Discard @ctx modifier,'Marker' commands
	user_excluded.push_back(/^Go.*(FDBBUTTONS|FOLDERCONTENT|DRIVEBUTTONS|TABGROUPLIST|BACKLIST|FORWARDLIST|FTPSITELIST|HISTORYLIST|TABLIST)/i); //Discard some 'Go' commands
	user_excluded.push_back(/^Favorites(?!.*\b(add|edit|alias|ADDDIALOG)\b).*/i); //Discard 'Favorites' commands
	user_excluded.push_back(/^(Show|Prefs|Set).*(PLUGINLIST|STYLELIST|VFSPLUGINLIST|LAYOUTLIST|FORMATLIST)/i); //Discard other dynamic buttons
	user_excluded.push_back(/^FileType.*(CONTEXTFORCE|CONTEXTMENU|CONTEXTOPTIONS|OPENWITHMENU|SENDTOMENU)/i); //Discard 'FileType' commands
	user_excluded.push_back(/^Recent(?!.*\b(CLEAR)\b).*/i); //Discard 'Recent' commands
}

function SetRegeXP() {
	regexps = {};
	regexps['skipped_modifiers'] = /^@(icon|label|color|toggle|hideblock)/i;
	regexps['forbidden_modifiers'] = /^@(disablenosel|enableif|disableif|hideif|hidenosel|showif|filesfromdroponly)/i;
	regexps['command_regex'] = /toolbarpalette/i;
	regexps['comment_line'] = /^\/\//i;
}

function AddToWatch(file, name) {
	if (dlg.main.WatchDir(name, file, 'wsifda') == 0) {
		Log(1, 'Adding a watch event for ' + file);
		dlg.watch_files_ids.insert(name);
		return true;
	}
	else Log(3, 'Unable to set watch event for ' + name);
	return false;
}

function dlgHelper(message, level, parent) {
	var dlg = DOpus.Dlg();
	if (parent) {
		dlg.window = parent;
		dlg.disable_window = parent;
	}
	dlg.message = message;
	dlg.buttons = '&Yes|&No';
	dlg.title = script_name + ' v' + script_version;
	if (level === 0) dlg.icon = 'info';
	else if (level === 1) dlg.icon = 'question';
	else if (level === 2) dlg.icon = 'warning';
	else dlg.icon = 'error';
	dlg.Show();
	return dlg.result;
}

function ParseVersion(version) {
	version = version.replace(/[^0-9.]+/g, '');
	var parts = version.split('.');
	if (parts.length <= 1) return parseFloat(version);
	var first = parts.shift();
	return parseFloat(first + '.' + parts.join(''));
}

function Log(level, text) {
	if (level === 4 || Script.config['log level'] < level) {
		if (level == 1) DOpus.Output('<#%vs_dragdrop_normal_action>DEBUG   => ' + text + '</#>');
		else if (level == 2) DOpus.Output('INFO    => ' + text);
		else if (level === 3) DOpus.Output('<#%vs_dragdrop_warning_action>WARNING => ' + text + '</#>');
		else DOpus.Output('ERROR   => ' + text, true);
	}
}

String.prototype.trim = function() {
	return this.replace(/^[\s\uFEFF\xA0]+|[\s\uFEFF\xA0]+$/g, '');
};

==SCRIPT RESOURCES
<resources>
	<resource name="main" type="dialog">
		<dialog height="222" lang="english" maximize="yes" minimize="yes" resize="yes" width="326">
			<control halign="center" height="8" name="wait_label" resize="yw" title="..." type="static" valign="center" visible="no" width="270" x="16" y="102" />
			<control halign="left" height="12" name="query_edit" resize="w" type="edit" width="176" x="52" y="6" />
			<control height="12" name="clear_btn" resize="x" title="❌" type="button" width="12" x="228" y="6" />
			<control fullrow="yes" height="186" name="listview" resize="wh" type="listview" viewmode="details" width="318" x="4" y="22">
				<columns>
					<item text="Label" />
					<item text="Description" />
					<item text="Type" />
					<item text="Hotkey" />
					<item text="Instructions" />
				</columns>
			</control>
			<control changelinkcolor="no" halign="left" height="8" name="total_title" resize="y" type="markuptext" width="114" x="4" y="210" />
			<control changelinkcolor="no" halign="left" height="8" name="search_btn" resize="t" tips="yes" title="&lt;%ddbi:25&gt;&lt;a id=&quot;search_opt&quot;&gt;&lt;%ddbi:6&gt;&lt;/a&gt;" type="markuptext" width="16" x="34" y="8" />
			<control halign="left" height="8" name="progress_bar" resize="y" type="static" valign="top" visible="no" width="1" x="6" y="210" />
			<control halign="left" height="8" name="progress_track" resize="y" type="static" valign="top" visible="no" width="114" x="6" y="210" />
			<control halign="left" height="8" name="progress_msg" resize="yw" type="static" valign="center" visible="no" width="166" x="134" y="210" />
			<control height="12" image="#powermode" name="adv_btn" title="Advanced Search" type="button" width="15" x="4" y="6" />
			<control changelinkcolor="no" halign="left" height="8" name="diac_btn" resize="x" tips="yes" title="&lt;a id=&quot;link&quot;&gt;ůü&lt;/a&gt;" type="markuptext" width="10" x="298" y="8" />
			<control changelinkcolor="no" halign="left" height="8" name="ww_btn" resize="x" tips="yes" title="&lt;a id=&quot;link&quot;&gt;ww&lt;/a&gt;" type="markuptext" width="10" x="312" y="8" />
			<control changelinkcolor="no" halign="left" height="8" name="regex_btn" resize="x" tips="yes" title="&lt;a id=&quot;regex&quot;&gt;•✱&lt;/a&gt;" type="markuptext" width="10" x="270" y="8" />
			<control changelinkcolor="no" halign="left" height="8" name="case_btn" resize="x" tips="yes" title="&lt;a id=&quot;case&quot;&gt;Aa&lt;/a&gt;" type="markuptext" width="10" x="284" y="8" />
			<control changelinkcolor="no" halign="left" height="8" name="wild_btn" resize="x" tips="yes" title="&lt;a id=&quot;wild&quot; text=&quot;moco&quot;&gt;✱&lt;/a&gt;" type="markuptext" width="8" x="258" y="8" />
			<control changelinkcolor="no" halign="left" height="8" name="refresh_btn" resize="xt" tips="yes" title="&lt;a id=&quot;refresh&quot;&gt;&lt;%oned:4&gt;&lt;/a&gt;" type="markuptext" width="10" x="244" y="8" />
			<control changelinkcolor="no" halign="left" height="8" name="edit_btn" resize="t" tips="yes" title="&lt;a id=&quot;edit&quot;&gt;&lt;%ddbi:105&gt;&lt;/a&gt;" type="markuptext" width="10" x="22" y="8" />
			<control halign="right" height="8" name="search_mode" resize="yw" type="markuptext" width="236" x="66" y="210" />
			<control changelinkcolor="no" halign="left" height="8" name="config_btn" resize="xyt" tips="yes" title="&lt;a id=&quot;refresh&quot;&gt;&lt;%ddbi:78&gt;&lt;/a&gt;" type="markuptext" width="10" x="312" y="210" />
		</dialog>
	</resource>
	<resource type="strings">
		<strings lang="english">
			<string id="config_allow_multiple">Set to True to allow multiple commands running simultaneously per lister.</string>
			<string id="config_esc_toclose">Close with Escape key</string>
			<string id="config_excluded_toolbars">List of toolbar names that will be ignored by the command.</string>
			<string id="config_excluded_words">If a command has one of this words, it will be excluded from list.</string>
			<string id="config_include_all">Set to True to include all the toolbars, even the unloaded ones.</string>
			<string id="config_include_context_menu">Set to True to include context menu entries for &quot;All Files and Folders&quot;</string>
			<string id="config_include_docks">Set to True to include buttons from floating toolbars.</string>
			<string id="config_include_hotkeys">Set to True to include entries from lister hotkeys.</string>
			<string id="config_include_system_hotkeys">Set to True to include entries from system hotkeys.</string>
			<string id="config_loadsave_position">Set to True to save and load the dialog&apos;s last used position.</string>
			<string id="config_log_level">Logging level to be displayed. OFF to show only errors.  
DEBUG to show all messages. 
STANDARD to show only the most relevant information.
WARNING to show messages that needs your attention.</string>
			<string id="config_max_width_desc">Max width for Descrption column. Set to 0 to always autosize to content.</string>
			<string id="config_max_width_functype">Max width for Function Type column. Set to 0 to always autosize to content.</string>
			<string id="config_max_width_hotkey">Max width for Hotkey column. Set to 0 to always autosize to content.</string>
			<string id="config_max_width_instr">Max width for Instructions column. Set to 0 to always autosize to content.</string>
			<string id="config_max_width_label">Max width for Label column. Set to 0 to always autosize to content.</string>
			<string id="config_nested_separator">Separator to use in anidated menus.</string>
			<string id="config_no_exit_after_run">Keep open after run</string>
			<string id="flag_case_sensitive">Match case</string>
			<string id="flag_force_execution">Force execution</string>
			<string id="flag_ignore_diacritics">Ignore diacritics</string>
			<string id="flag_include_description">Search also in Description</string>
			<string id="flag_no_filter">No filter instructions</string>
			<string id="flag_no_pass_items">Don&apos;t pass selected items</string>
			<string id="flag_whole_words">Whole words</string>
			<string id="flag_wildcards">Use Opus wildcards</string>
			<string id="label_adv_search">Advanced Search</string>
			<string id="label_diac_enabled">Diacritics allowed</string>
			<string id="label_dlg_question">The modifier %s is used in this instruction. It may not run properly in this context.
Do you still want to execute it?</string>
			<string id="label_dlg_remember">&amp;Remember chosen answer during this session</string>
			<string id="label_edit_menu">Edit Menu</string>
			<string id="label_filtering">Filtering...</string>
			<string id="label_menu_open">Open toolbar&apos;s location</string>
			<string id="label_menu_show">Show toolbar in current lister</string>
			<string id="label_msg_dlg">Search in :</string>
			<string id="label_regex_enabled">Regular expressions</string>
			<string id="label_sug_filter">Search by</string>
			<string id="label_sug_filter_adv">Use $(l|d|v|t|h)(==|!=)&quot;value&quot;. OR|AND and basic logical combinations are supported</string>
			<string id="label_use_DO_wildcards">Use DOpus &amp;pattern matching syntax when possible</string>
			<string id="msg_copy_instructions">Command instructions copied to clipboard!</string>
			<string id="msg_delmenuentry">Really delete &quot;%1&quot;?</string>
			<string id="msg_menulabeldlg">Valid variables are</string>
			<string id="msg_new_version">ToolbarPalette %1 is available for download</string>
			<string id="msg_newmenu">Name for the new entry:</string>
			<string id="msg_notab">This command needs to be run with a open lister!</string>
			<string id="msg_wait_results">Building toolbars map. Please wait...</string>
		</strings>
		<strings lang="esm">
			<string id="config_allow_multiple">Establecer en True para permitir múltiples comandos ejecutándose simultáneamente por Listado.</string>
			<string id="config_esc_toclose">Cerrar con tecla Escape</string>
			<string id="config_excluded_toolbars">Lista de nombres de barras que serán ignoradas por el comando.</string>
			<string id="config_excluded_words">Si un comando tiene alguna de estas palabras, será excluido de la lista.</string>
			<string id="config_include_all">Establecer en True para incluir todas las barras de herramientas, incluso las que no están cargadas.</string>
			<string id="config_include_context_menu">Establecer en True para incluir entradas en el menú contextual</string>
			<string id="config_include_docks">Establecer en True para incluir botones de las barras de herramientas flotantes.</string>
			<string id="config_include_hotkeys">Establecer en True para incluir entradas de las teclas rápidas del Listado.</string>
			<string id="config_include_system_hotkeys">Set to True to include entries from system hotkeys.</string>
			<string id="config_loadsave_position">Establecer en True para guardar y cargar la última posición utilizada del diálogo.</string>
			<string id="config_log_level">Nivel de registro a mostrar. OFF para mostrar solo errores. 
DEBUG para mostrar todos los mensajes. 
STANDARD para mostrar solo la información más relevante. 
WARNING para mostrar mensajes que necesitan su atención.</string>
			<string id="config_max_width_desc">Max width for Descrption column. Set to 0 to always autosize to content.</string>
			<string id="config_max_width_functype">Ancho máximo para la columna Tipo de Función. Establezca en 0 para que siempre se ajuste automáticamente al contenido.</string>
			<string id="config_max_width_hotkey">Ancho máximo para la columna Atajo de Teclas. Establezca en 0 para que siempre se ajuste automáticamente al contenido.</string>
			<string id="config_max_width_instr">Ancho máximo para la columna Instrucciones. Establezca en 0 para que siempre se ajuste automáticamente al contenido.</string>
			<string id="config_max_width_label">Ancho máximo para la columna Etiqueta. Establezca en 0 para que siempre se ajuste automáticamente al contenido.</string>
			<string id="config_nested_separator">Separador a utilizar en menús anidados.</string>
			<string id="config_no_exit_after_run">Mantener abierto tras ejecutar</string>
			<string id="flag_case_sensitive">Coincidir mayúsculas</string>
			<string id="flag_force_execution">Forzar ejecución</string>
			<string id="flag_ignore_diacritics">Ignorar diacriticos</string>
			<string id="flag_include_description">Incluir descripción al buscar</string>
			<string id="flag_no_filter">Sin filtros en instrucciones</string>
			<string id="flag_no_pass_items">No pasar elementos seleccionados</string>
			<string id="flag_whole_words">Palabras completas</string>
			<string id="flag_wildcards">Usar comodínes</string>
			<string id="label_adv_search">Búsqueda avanzada</string>
			<string id="label_diac_enabled">Permitir diacríticos</string>
			<string id="label_dlg_question">Se detectó el modificador &apos;%s&apos;. Es posible que no funcione correctamente en este contexto.
¿Aún desea ejecutarla?</string>
			<string id="label_dlg_remember">&amp;Recordar la respuesta elegida durante esta sesión</string>
			<string id="label_edit_menu">Editar Menú</string>
			<string id="label_filtering">Filtrando...</string>
			<string id="label_menu_open">Abrir la ubicación de la barra de herramientas</string>
			<string id="label_menu_show">Mostrar la barra de herramientas en el Listado actual</string>
			<string id="label_msg_dlg">Buscar en:</string>
			<string id="label_regex_enabled">Expresiones regulares</string>
			<string id="label_sug_filter">Buscar por</string>
			<string id="label_sug_filter_adv">Utilice $(l|d|v|t|h)(==|!=)&quot;valor&quot;. Se admiten combinaciones lógicas básicas con OR | AND</string>
			<string id="label_use_DO_wildcards">Usar sintaxis de comodines de DO&amp;pus cuando sea posible</string>
			<string id="msg_copy_instructions">Instrucciones de %s copiadas al portapapeles!</string>
			<string id="msg_delmenuentry">¿Desea eliminar realmente &quot;%1&quot;?</string>
			<string id="msg_menulabeldlg">Las variables válidas son</string>
			<string id="msg_new_version">ToolbarPalette %1 está disponible para descargar</string>
			<string id="msg_newmenu">Nombre para la nueva entrada:</string>
			<string id="msg_notab">¡Este comando necesita ser ejecutado con un Listado abierto!</string>
			<string id="msg_wait_results">Construyendo el mapa de barras de herramientas. Por favor, espere...</string>
		</strings>
		<strings lang="deutsch">
			<string id="config_allow_multiple">Auf &quot;wahr&quot; setzen, um mehrere Befehle gleichzeitig im Lister anzuwenden.</string>
			<string id="config_esc_toclose">Schließen mit Escape</string>
			<string id="config_excluded_toolbars">Eine Liste von Symbolleistennamen, die durch diesen Befehl ignoriert werden.</string>
			<string id="config_excluded_words">Bei Anwesenheit eines dieser Schlüsselwörter wird der Befehl aus der Liste ausgeschlossen.</string>
			<string id="config_include_all">Auf &quot;wahr&quot; setzen, um alle Symbolleisten zu verwenden, einschließlich der nicht geladenen.</string>
			<string id="config_include_context_menu">Auf &quot;wahr&quot; setzen, um alle Kontextmenüeinträge für &quot;alle Dateien und Ordner&quot; einzuschließen</string>
			<string id="config_include_docks">Auf &quot;wahr&quot; setzen, um Schalter von schwebenden Symbolleisten einzuschließen.</string>
			<string id="config_include_hotkeys">Auf &quot;wahr&quot; setzen, um Listerhotkeys einzuschließen.</string>
			<string id="config_include_system_hotkeys">Auf &quot;wahr&quot; setzen, um Systemhotkeys einzuschließen.</string>
			<string id="config_loadsave_position">Auf &quot;wahr&quot; setzen, um die letzte Position des Dialogfensters zu speichern bzw. zu laden.</string>
			<string id="config_log_level">Das angezeigte Logginglevel. AUS zeigt nur Fehlermeldungen.  
DEBUG, um alle Meldungen zu zeigen. 
STANDARD, um nur die relevantesten Meldungen zu zeigen.
WARNUNG, um wichtige Meldungen zu zeigen.</string>
			<string id="config_max_width_desc">Maximalbreite für die Spalte &quot;Beschreibung&quot;. Der Wert 0 bewirkt die automatische Größenanpassung.</string>
			<string id="config_max_width_functype">Maximalbreite für die Spalte &quot;Funktion&quot;. Der Wert 0 bewirkt die automatische Größenanpassung.</string>
			<string id="config_max_width_hotkey">Maximalbreite für die Spalte &quot;Hotkey&quot;. Der Wert 0 bewirkt die automatische Größenanpassung.</string>
			<string id="config_max_width_instr">Maximalbreite für die Spalte &quot;Instruktionen&quot;. Der Wert 0 bewirkt die automatische Größenanpassung.</string>
			<string id="config_max_width_label">Maximalbreite für die Spalte &quot;Label&quot;. Der Wert 0 bewirkt die automatische Größenanpassung.</string>
			<string id="config_nested_separator">Trennsymbol für den Gebrauch in animierten Menüs.</string>
			<string id="config_no_exit_after_run">Nach Ausführung offen halten</string>
			<string id="flag_case_sensitive">Groß-/Kleinschreibung beachten</string>
			<string id="flag_force_execution">Ausführung erzwingen</string>
			<string id="flag_ignore_diacritics">Diakritische Zeichen ignorieren</string>
			<string id="flag_include_description">Auch in der Beschreibung suchen</string>
			<string id="flag_no_filter">Keine Filteranweisungen</string>
			<string id="flag_no_pass_items">Ausgewählte Elemente nicht übergeben</string>
			<string id="flag_whole_words">Ganze Wörter</string>
			<string id="flag_wildcards">DOpus Schema/Syntax verwenden</string>
			<string id="label_adv_search">Fortgeschrittene Suche</string>
			<string id="label_diac_enabled">Diakritische Zeichen erlaubt</string>
			<string id="label_dlg_question">Der Modifier %s wird in diesem Befehl benutzt. Möglicherweise funktioniert er in diesem Kontext nicht einwandfrei
Möchten Sie den Befehl dennoch ausführen?</string>
			<string id="label_dlg_remember">&amp;Auswahl merken erhält die gewählte Antwort in dieser Sitzung</string>
			<string id="label_edit_menu">Menü bearbeiten</string>
			<string id="label_filtering">Filterung...</string>
			<string id="label_menu_open">Öffne den Ort der Symbolleiste</string>
			<string id="label_menu_show">Zeige die Symbolleiste im aktiven Lister</string>
			<string id="label_msg_dlg">Suche in :</string>
			<string id="label_regex_enabled">Reguläre Ausdrücke (REGEX)</string>
			<string id="label_sug_filter">Suche mittels</string>
			<string id="label_sug_filter_adv">Verwende $(l|d|v|t|h)(==|!=)&quot;value&quot;. OR|AND und elementare logische Kombinationen werden unterstützt</string>
			<string id="label_use_DO_wildcards">Verwende passendes DOpus &amp;Schema/Syntax falls möglich</string>
			<string id="msg_copy_instructions">Kommandoanweisungen werden in die Zwischenablage kopiert!</string>
			<string id="msg_delmenuentry">&quot;%1&quot; wirklich löschen?</string>
			<string id="msg_menulabeldlg">Gültige Variablen sind</string>
			<string id="msg_new_version">ToolbarPalette %1 steht zum Download bereit</string>
			<string id="msg_newmenu">Name des neuen Eintrags:</string>
			<string id="msg_notab">Dieser Befehl erfordert einen geöffneten Lister! </string>
			<string id="msg_wait_results">Symbolleisten-Map wird erstellt, bitte warten...</string>
		</strings>
	</resource>
	<resource name="search_in_flags" type="dialog">
		<dialog height="110" lang="english" standard_buttons="ok" width="222">
			<control height="10" name="check0" type="check" width="160" x="22" y="14" />
			<control height="10" name="check1" type="check" width="160" x="22" y="30" />
			<control height="10" name="check2" type="check" width="160" x="22" y="46" />
			<control height="10" name="check3" type="check" width="160" x="22" y="62" />
			<control halign="left" height="8" name="static1" title="Search in :" type="static" valign="top" width="166" x="6" y="4" />
			<control height="10" name="check4" type="check" width="160" x="22" y="78" />
		</dialog>
	</resource>
	<resource name="msg_ask_run" type="dialog">
		<dialog height="68" lang="english" resize="yes" width="258">
			<control changelinkcolor="no" halign="left" height="30" name="label_question" resize="wh" type="markuptext" width="250" x="4" y="4" />
			<control height="10" name="remember_chk" resize="yw" title="Remember chosed answer during this session" type="check" width="250" x="2" y="38" />
			<control close="0" height="14" name="no_btn" resize="xy" title="No" type="button" width="50" x="204" y="50" />
			<control close="1" default="yes" height="14" name="yes_btn" resize="xy" title="Yes" type="button" width="50" x="152" y="50" />
		</dialog>
	</resource>
	<resource name="config_main" type="dialog">
		<dialog height="90" lang="english" maximize="yes" minimize="yes" resize="yes" width="342">
			<control default="yes" height="14" name="save_btn" resize="x" title="&amp;Save" type="button" width="42" x="250" y="2" />
			<control halign="left" height="20" name="label" resize="yw" type="markuptext" width="330" x="6" y="66" />
			<control halign="left" height="46" multiline="yes" name="command" resize="wsh" type="edit" width="226" x="112" y="18" />
			<control changelinkcolor="no" enable="no" halign="left" height="8" name="del_btn" resize="t" tips="yes" title="&lt;a id=&quot;del_btn&quot;&gt;&lt;%ddbi:74&gt;&lt;/a&gt;" type="markuptext" width="8" x="19" y="4" />
			<control changelinkcolor="no" halign="left" height="8" name="add_btn" resize="t" tips="yes" title="&lt;a id=&quot;add_btn&quot;&gt;&lt;%ddbi:140&gt;&lt;/a&gt;" type="markuptext" width="8" x="6" y="4" />
			<control height="14" name="cancel_btn" resize="x" title="Cancel" type="button" width="42" x="296" y="2" />
			<control changelinkcolor="no" halign="left" height="8" name="up_btn" resize="t" tips="yes" title="&lt;a id=&quot;up_btn&quot;&gt;&lt;%ddbi:22&gt;&lt;/a&gt;" type="markuptext" width="8" x="32" y="4" />
			<control changelinkcolor="no" halign="left" height="8" name="down_btn" resize="t" tips="yes" title="&lt;a id=&quot;down_btn&quot;&gt;&lt;%ddbi:23&gt;&lt;/a&gt;" type="markuptext" width="8" x="45" y="4" />
			<control height="46" name="list" resize="wsh" type="listbox" width="104" x="4" y="18" />
		</dialog>
	</resource>
</resources>
