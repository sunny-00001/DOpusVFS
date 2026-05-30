/* Columns Viewer Command for Directory Opus
**GUI for view/copy values for almost all available columns**
Columns Viewer 2.0 © 2024 by Christian Arellano García is licensed under an MIT license. 
    This script is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
*/
var script_label = 'ColViewer';
var script_name = 'ColViewer';
var script_version = '2.0a';

function OnInit(initData) {
	initData.name = script_label;
	initData.version = script_version;
	initData.copyright = '(c) 2024 Christian Arellano García';
	initData.url = 'https://resource.dopus.com/t/columns-viewer/47072';
	initData.desc = 'GUI for view/copy values for almost all available columns';
	initData.default_enable = true;
	initData.min_version = '13.11';
	initData.config_desc = DOpus.Create().OrderedMap();
	initData.config_groups = DOpus.Create().OrderedMap();
	initData.config_group_order = DOpus.NewVector('General', 'UI');

	AddConfig('log level', DOpus.Create().Vector(2, 'debug', 'standard', 'warning', 'off'), DOpus.strings.Get('debug'), 'General');
	AddConfig('ignored_script_columns', DOpus.Create().Vector(), DOpus.strings.Get('ignored_script_columns'), 'General');
	// AddConfig('dialog size mode', DOpus.Create().Vector(0, 'auto', 'last used'), DOpus.strings.Get('size_mode'), 'UI');

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
	cmd.method = 'OnColumnsViewer';
	cmd.desc = 'GUI for view/copy values for almost all available columns';
	cmd.label = script_label;
	cmd.template = 'FILE,REGEX/S,CASE/S,WHOLEWORDS/S,DIACRITICS/S,WILDCARDS/S,ADVSEARCH/S,INCLUDE/K[grp:script,grp:evaluator,grp:shell,grp:audio,grp:general,grp:datetime,grp:filesize,grp:paths,grp:docs,grp:pictures,grp:video,grp:exe],EXCLUDE/k[grp:script,grp:evaluator,grp:shell,grp:audio,grp:general,grp:datetime,grp:filesize,grp:paths,grp:docs,grp:pictures,grp:video,grp:exe]';
	cmd.hide = false;
	cmd.icon = 'metapane';
}
var FSU, str_tools, strings;

function OnColumnsViewer(scriptCmdData) {

	// ======= GET FILE ARG ==========================================
	FSU = DOpus.FSUtil();
	str_tools = DOpus.Create().StringTools();
	var match = scriptCmdData.cmdline.match(new RegExp(script_name + '.*BUILDMAP=(.*)'));
	if (match) {
		Log(1, 'uid : ' + match[1]);
		var file = Script.vars.Exists('item:' + match[1]) ? checkFile(FSU.Resolve(Script.vars.Get('item:' + match[1]), 'cj'), false) : null;
		if (!file) {
			Log(4, 'Unexpected call to this arg, you should not use it by itself!');
			return;
		}
		var columns_map = BuildColumnsList(file, match[1]);
		DOpus.SendCustomMsg(script_name + ':' + match[1], columns_map);
		return;
	}
	DOpus.ClearOutput();
	Log(1, 'cmdline   : ' + scriptCmdData.cmdline);
	var tab = scriptCmdData.func.sourcetab;
	var file;
	// If no provided, use first selected
	if (scriptCmdData.func.args.got_arg.file)
		var file = scriptCmdData.func.args.file;
	else if (tab && tab.selected.count > 0) var file = tab.selected(0);
	if (!file) return;
	var cmd = DOpus.Create().Command();
	var uid = new Date().getTime();
	if (scriptCmdData.func.args.got_arg.include) Script.vars.Set(script_name + ':' + uid + ':include', DOpus.NewVector(scriptCmdData.func.args.include.split(',')));
	if (scriptCmdData.func.args.got_arg.exclude) Script.vars.Set(script_name + ':' + uid + ':exclude', DOpus.NewVector(scriptCmdData.func.args.exclude.split(',')));

	var dlg = {};
	dlg.main = tab ? tab.Dlg() : DOpus.Dlg();
	dlg.main.title = script_label + ' v' + script_version + ' - ' + file;
	dlg.main.icon = DOpus.LoadImage(FSU.Resolve('/home\\dopusrt.exe') + ',2');
	dlg.main.template = 'main';
	dlg.main.want_resize = true;
	dlg.main.Create();

	var columns_map, cols_enum;

	// ======= VARS FOR CONTROLS ==========================================
	dlg.query_edit = dlg.main.Control('query_edit');
	dlg.categories = dlg.main.Control('categories');
	dlg.listview = dlg.main.Control('listview');
	dlg.total_info = dlg.main.Control('total_info');
	dlg.copy_btn = dlg.main.Control('copy_btn');
	dlg.adv_btn = dlg.main.Control('adv_btn');
	dlg.search_btn = dlg.main.Control('search_btn');
	dlg.regex_btn = dlg.main.Control('regex_btn');
	dlg.case_btn = dlg.main.Control('case_btn');
	dlg.diac_btn = dlg.main.Control('diac_btn');
	dlg.ww_btn = dlg.main.Control('ww_btn');
	dlg.wild_btn = dlg.main.Control('wild_btn');
	dlg.status_bar = dlg.main.Control('status_bar');
	dlg.search_mode = dlg.main.Control('search_mode');
	dlg.tooltip_btn = dlg.main.Control('tooltip_btn');
	dlg.abort_btn = dlg.main.Control('abort_btn');
	dlg.skip_btn = dlg.main.Control('skip_btn');
	dlg.progress_bar = dlg.main.Control('progress_bar');
	dlg.progress_track = dlg.main.Control('progress_track');

	var category_all = str_tools.LanguageStr(2135);
	var total_count;

	// ======= GET TRANSLATED STRINGS FOR UI ==========================================
	dlg.copy_btn.label = str_tools.LanguageStr('CopySelection').replace('&', '');
	strings = DOpus.Create().Map(
		'label_name', str_tools.LanguageStr(310),
		'label_label', str_tools.LanguageStr(457),
		'label_header', str_tools.LanguageStr(5599),
		'label_type', str_tools.LanguageStr(312),
		'label_value', str_tools.LanguageStr(6501),
		'copy_str', str_tools.LanguageStr(646),
		'column_str', '<b><#%vs_listview_header_text>' + str_tools.LanguageStr(5015) + ' : </#></b>',
		'options_str', str_tools.LanguageStr(1097),
		'hidden_str', str_tools.LanguageStr(1308).slice(6).toLowerCase(),
		'loading_str', str_tools.LanguageStr(8376),
		'adv_search', DOpus.strings.get('label_adv_search'),
		'label_msg_DO_pattern', str_tools.LanguageStr(9338),
		'label_regex', DOpus.strings.get('label_regex_enabled'),
		'label_case', str_tools.LanguageStr(9341),
		'label_ww', str_tools.LanguageStr(9340),
		'label_ignore_diacritics', str_tools.LanguageStr(29499),
		'label_use_DO_wildcards', DOpus.strings.Get('label_use_DO_wildcards'),
		'label_diac_enabled', DOpus.strings.get('label_diac_enabled'),
		'label_sug_filter_adv', DOpus.strings.get('label_sug_filter_adv'),
		'label_sug_filter', DOpus.strings.get('label_sug_filter'),
		'msg_copy', DOpus.strings.get('msg_copy'));
	with(dlg.listview.columns) {
		GetColumnAt(0).name = strings('label_name'); //name or keyword
		GetColumnAt(1).name = strings('label_label'); //Label
		GetColumnAt(2).name = strings('label_header'); // Header
		GetColumnAt(3).name = strings('label_type'); //Type
		GetColumnAt(4).name = strings('label_value'); //Value
	}

	// ======= SET HOTKEYS ==========================================
	dlg.main.AddHotkey('refresh_key', 'F5');
	dlg.main.AddHotkey('close_key', 'Escape');
	dlg.main.AddHotkey('focus_edit_key', 'F3');
	dlg.main.AddHotkey('set_all_key', 'F4');
	dlg.main.AddHotkey('categories_focus_key', 'Shift+F4');
	dlg.main.AddHotkey('wild_btn', 'Alt+W');
	dlg.main.AddHotkey('regex_btn', 'Alt+G');
	dlg.main.AddHotkey('case_btn', 'Alt+C');
	dlg.main.AddHotkey('diac_btn', 'Alt+D');
	dlg.main.AddHotkey('ww_btn', 'Alt+L');
	// ======= PROGRESS SETTINGS ==========================================
	dlg.progress_track.bg = '#%vs_progress_background';
	dlg.progress_bar.fg = '#%jobsbar_text';
	dlg.progress_bar.style = 'b';
	dlg.progress_bar.bg = '#%vs_progress_bar_normal';

	// ======= FINAL ARRANGEMENTS ==========================================
	dlg.tooltip_btn.label = '✱ : ' + strings('label_msg_DO_pattern') + ' (Alt+W)\n•✱ : ' + strings('label_regex') + ' (Alt+G)\nAa : ' + strings('label_case') +
		' (Alt+C)\nůü : ' + strings('label_diac_enabled') + ' (Alt+D)\nww : ' + strings('label_ww') + ' (Alt+L)\n';

	var font_id = dlg.main.CreateFont('Segoe UI', 0, 'b');
	dlg.regex_btn.SetFont(font_id);
	dlg.case_btn.SetFont(font_id);
	dlg.diac_btn.SetFont(font_id);
	dlg.ww_btn.SetFont(font_id);
	dlg.wild_btn.SetFont(font_id);
	dlg.regex_btn.autosize();
	dlg.case_btn.autosize();
	dlg.diac_btn.autosize();
	dlg.ww_btn.autosize();
	dlg.wild_btn.autosize();

	dlg.main.AddCustomMsg(script_name + ':' + uid, true);
	if (!RunBuild()) {
		Log(4, 'Unable to run the command!');
		EndScript();
		return;
	}

	dlg.main.LoadPosition(script_name);
	var adv_search = scriptCmdData.func.args.got_arg.advsearch;
	var search_flags = DOpus.Create().OrderedMap();
	search_flags('regex_btn') = scriptCmdData.func.args.got_arg.regex;
	search_flags('case_btn') = scriptCmdData.func.args.got_arg['case'];
	search_flags('diac_btn') = scriptCmdData.func.args.got_arg.diacritics;
	search_flags('ww_btn') = scriptCmdData.func.args.got_arg.wholewords;
	search_flags('wild_btn') = scriptCmdData.func.args.got_arg.wildcards;
	if (search_flags('regex_btn') || search_flags('ww_btn')) search_flags('wild_btn') = false;
	var search_in_flags = (Script.Vars.Exists('search_in_flags')) ? Script.Vars.Get('search_in_flags') : 1; //default to just name
	var copy_menu = DOpus.Create().Vector(strings('copy_str') + ' ' + strings('label_name'), strings('copy_str') + ' ' + strings('label_value'), strings('copy_str') + ' ' + strings('label_name') + '+' + strings('label_value'), strings('copy_str') + ' ' + strings('label_label') + '+' + strings('label_value'), strings('copy_str') + ' ' + strings('label_header') + '+' + strings('label_value'), str_tools.LanguageStr('CopyAll'));
	var search_menu = DOpus.Create().Vector(DOpus.strings.get('label_search_name'), DOpus.strings.get('label_search_label'), DOpus.strings.get('label_search_header'), DOpus.strings.get('label_search_value'));
	var wild_obj = FSU.NewWild();
	var regex, search_mode;
	var match_map = {
		wild_all: function(name, label, header, value) {
			return wild_obj.Match(name) || wild_obj.Match(label) || wild_obj.Match(header) || wild_obj.Match(value);
		},
		wild_name: function(name, label, header, value) {
			return wild_obj.Match(name);
		},
		wild_label: function(name, label, header, value) {
			return wild_obj.Match(label);
		},
		wild_name_label: function(name, label, header, value) {
			return wild_obj.Match(name) || wild_obj.Match(label);
		},
		wild_value: function(name, label, header, value) {
			return wild_obj.Match(value);
		},
		wild_name_value: function(name, label, header, value) {
			return wild_obj.Match(name) || wild_obj.Match(value);
		},
		wild_label_value: function(name, label, header, value) {
			return wild_obj.Match(label) || wild_obj.Match(value);
		},
		wild_name_label_value: function(name, label, header, value) {
			return wild_obj.Match(name) || wild_obj.Match(label) || wild_obj.Match(value);
		},
		wild_header: function(name, label, header, value) {
			return wild_obj.Match(header);
		},
		wild_name_header: function(name, label, header, value) {
			return wild_obj.Match(name) || wild_obj.Match(header);
		},
		wild_label_header: function(name, label, header, value) {
			return wild_obj.Match(label) || wild_obj.Match(header);
		},
		wild_name_label_header: function(name, label, header, value) {
			return wild_obj.Match(name) || wild_obj.Match(label) || wild_obj.Match(header);
		},
		wild_value_header: function(name, label, header, value) {
			return wild_obj.Match(header) || wild_obj.Match(value);
		},
		wild_name_value_header: function(name, label, header, value) {
			return wild_obj.Match(name) || wild_obj.Match(header) || wild_obj.Match(value);
		},
		wild_label_value_header: function(name, label, header, value) {
			return wild_obj.Match(label) || wild_obj.Match(header) || wild_obj.Match(value);
		},
		regex_all: function(name, label, header, value) {
			return regex.test(name) || regex.test(label) || regex.test(header) || regex.test(value);
		},
		regex_name: function(name, label, header, value) {
			return regex.test(name);
		},
		regex_label: function(name, label, header, value) {
			return regex.test(label);
		},
		regex_header: function(name, label, header, value) {
			return regex.test(header);
		},
		regex_value: function(name, label, header, value) {
			return regex.test(value);
		},
		regex_name_label_value: function(name, label, header, value) {
			return regex.test(name) || regex.test(label) || regex.test(value);
		},
		regex_name_label: function(name, label, header, value) {
			return regex.test(name) || regex.test(label);
		},
		regex_name_header: function(name, label, header, value) {
			return regex.test(name) || regex.test(header);
		},
		regex_name_value: function(name, label, header, value) {
			return regex.test(name) || regex.test(value);
		},
		regex_label_header: function(name, label, header, value) {
			return regex.test(label) || regex.test(header);
		},
		regex_name_label_header: function(name, label, header, value) {
			return regex.test(name) || regex.test(label) || regex.test(header);
		},
		regex_label_value: function(name, label, header, value) {
			return regex.test(label) || regex.test(value);
		},
		regex_value_header: function(name, label, header, value) {
			return regex.test(header) || regex.test(value);
		},
		regex_name_value_header: function(name, label, header, value) {
			return regex.test(name) || regex.test(header) || regex.test(value);
		},
		regex_label_value_header: function(name, label, header, value) {
			return regex.test(label) || regex.test(header) || regex.test(value);
		}
	};

	var label_var, name_var, header_var, value_var;

	// dlg.main.FlushMsg();

	ChangeSearchModifiers();
	//resize the dialog based on user choice
	updateQueryCueText(search_in_flags);
	var sel_items = DOpus.Create().Vector();
	var msg, clip;
	var step_count = 0;
	var progress_steps = 1;
	dlg.main.Show();

	LoadThumbnail(file);
	Log(1, 'dialog started');
	SetPos_Progress();
	while (true) {
		msg = dlg.main.GetMsg();
		if (!msg.result) break;

		switch (msg.event) {
			case 'resize':
				SetPos_Progress();
				break;
			case 'custom':
				// Log(1, 'name : ' + msg.name + ' object : ' + (msg.object ? 'yes' : 'no'));
				if (msg.object && msg.name == script_name + ':' + uid) {
					if (msg.object.Exists('message'))
						UpdateProgress(msg.object);
					else {
						//map retrieved
						columns_map = msg.object;
						cols_enum = new Enumerator(columns_map);
						FillCategories();
						updateList();
						ToggleControls(true);
						UpdateStatusBar();
						dlg.query_edit.focus = true;
					}
				}
				break;
			case 'focus':
				if (msg.control == 'listview') {
					dlg.main.AddHotkey('copy_key', 'ctrl+c');
					dlg.main.AddHotkey('sel_all_key', 'ctrl+a');
				}
				else {
					dlg.main.DelHotkey('copy_key');
					dlg.main.DelHotkey('sel_all_key');
				}
				if (msg.control == 'query_edit' && adv_search) dlg.main.AddHotkey('enter_key', 'enter');
				else dlg.main.DelHotkey('enter_key');
				break;
			case 'selchange':
				if (msg.control == 'listview') {
					sel_items = dlg.listview.value;
					dlg.copy_btn.visible = !sel_items.empty && dlg.listview.focus;
				}
				else if (msg.control == 'categories') updateList();
				break;
			case 'drop':
				if (!msg.object.empty) {
					try {
						tab = DOpus.listers.lastactive.activetab;
						new_file = checkFile(msg.object.front(), true, dlg);
						if (new_file == null) continue;
						dlg.main.FlushMsg();

						Rebuild_ColMap(new_file);
					}
					catch (err) {
						Log(4, 'An error ocurred when trying to load a new item : ' + err);
					}
				}
				break;
			case 'click':
				if (msg.control == 'copy_btn') copyToClip(sel_items, 6);
				else if (msg.control == 'clear_btn') dlg.query_edit.value = '';
				else if (msg.control == 'refresh_btn') Rebuild_ColMap();
				else if (msg.control == 'adv_btn') {
					adv_search = !adv_search;
					dlg.search_btn.enabled = !adv_search;
					dlg.search_mode.label = adv_search ? strings('adv_search') : '';
					updateQueryCueText(search_in_flags);
				}
				else if (msg.control == 'search_btn') {
					var dlgMenu = DOpus.Dlg();
					dlgMenu.title = script_label + ' v' + script_version;
					dlgMenu.template = 'search_in_flags';
					dlgMenu.window = dlg.main;
					dlgMenu.disable_window = dlg.main;
					dlgMenu.Create();
					dlgMenu.Control('static1').label = DOpus.strings.Get('label_msg_dlg');
					for (var i = 0; i < search_menu.length; i++) {
						with(dlgMenu.Control('check' + i)) {
							label = search_menu(i);
							value = search_in_flags & (1 << i);
						}
					}

					dlgMenu.RunDlg();

					if (dlgMenu.result) {
						setSearchFlags(dlgMenu);
						updateList();
					}
				}
				else if (msg.control == 'skip_btn') Script.vars.Set(script_name + ':' + uid + ':skip', true);
				else if (msg.control == 'abort_btn') Script.vars.Set(script_name + ':' + uid + ':abort', true);
				else if (search_flags.Exists(msg.control)) { //search flags buttons
					ChangeSearchModifiers(msg.control);
					if (dlg.query_edit.value) dlg.main.SetTimer(10, 'update_list_timer');
				}
				break;
			case 'rclick':
				if (dlg.listview.focus && sel_items.count > 0) {
					var dlgMenu = DOpus.Dlg();
					dlgMenu.choices = copy_menu;
					dlgMenu.menu = 0;
					var menuReturn = dlgMenu.Show();
					copyToClip(sel_items, menuReturn);
				}
				break;
			case 'editchange':
				if (msg.control == 'query_edit' && (!adv_search || !dlg.query_edit.value)) dlg.main.SetTimer((dlg.query_edit.value != '') ? 250 : 30, 'update_list_timer');
				break;
			case 'timer':
				if (msg.control == 'update_list_timer') {
					dlg.main.KillTimer('update_list_timer');
					if (dlg.listview.enabled) updateList();
				}
				break;
			case 'hotkey':
				if (dlg.listview.focus && msg.control === 'copy_key' && sel_items.count > 0) copyToClip(sel_items, 6);
				else if (dlg.listview.focus && msg.control === 'sel_all_key' && total_count) dlg.listview.SelectRange(0, total_count);
				else if (dlg.query_edit.focus && msg.control === 'enter_key') updateListAdv();
				else if (msg.control === 'focus_edit_key') dlg.query_edit.focus = true;
				else if (msg.control === 'categories_focus_key') dlg.categories.focus = true;
				else if (msg.control === 'set_all_key') dlg.categories.value = 0;
				else if (msg.control === 'close_key') dlg.main.EndDlg(0);
				else if (msg.control === 'refresh_key') Rebuild_ColMap();
				else if (search_flags.Exists(msg.control)) {
					ChangeSearchModifiers(msg.control);
					if (dlg.query_edit.value) dlg.main.SetTimer(10, 'update_list_timer');
				}
				break;
		}
	}
	//save dlg size and position 
	dlg.main.SavePosition(script_name);

	strings = null;

	columns_map = null;
	cols_enum = null;
	dlg = null;
	EndScript();
	Log(2, '=== COMMAND FINISHED ===');
	CollectGarbage();
	return;

	function EndScript() {
		tab = null;
		file = null;
		FSU = null;
		str_tools = null;
		Script.Vars.Delete('item:' + uid);
		Script.Vars.Delete(script_name + ':' + uid + ':include');
		Script.Vars.Delete(script_name + ':' + uid + ':exclude');
		Script.Vars.Delete(script_name + ':' + uid + ':abort');
		Script.Vars.Delete(script_name + ':' + uid + ':skip');
	}

	function ToggleControls(enable_list) {
		dlg.query_edit.enabled = enable_list;
		dlg.categories.enabled = enable_list;
		dlg.listview.enabled = enable_list;
		dlg.total_info.enabled = enable_list;
		dlg.copy_btn.enabled = enable_list;
		dlg.adv_btn.enabled = enable_list;
		dlg.search_btn.enabled = enable_list;
		dlg.regex_btn.enabled = enable_list;
		dlg.case_btn.enabled = enable_list;
		dlg.diac_btn.enabled = enable_list;
		dlg.ww_btn.enabled = enable_list;
		dlg.wild_btn.enabled = enable_list;
		// dlg.status_bar.enabled = enable_list;
		dlg.search_mode.visible = enable_list;
		dlg.tooltip_btn.enabled = enable_list;
		dlg.abort_btn.visible = !enable_list;
		dlg.skip_btn.visible = !enable_list;
		dlg.progress_bar.visible = !enable_list;
		dlg.progress_track.visible = !enable_list;

		if (enable_list) {
			progress_steps = 0;
			dlg.progress_bar.cx = 0;
		}
		else SetPos_Progress();

	}

	function updateList() {
		if (adv_search) {
			updateListAdv();
			return;
		}
		dlg.listview.redraw = false;
		dlg.listview.RemoveItem(-1);
		var strFilter = dlg.query_edit.value;
		var category = dlg.categories.value.name;
		if (strFilter) {
			var use_DO_wildcards_local = search_flags('wild_btn') && !search_flags('regex_btn') && !search_flags('ww_btn');
			Log(1, '=> use DO wilcards   : ' + use_DO_wildcards_local);

			if (!search_flags('wild_btn') && !search_flags('regex_btn')) strFilter = strFilter.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
			if (!search_flags('diac_btn')) strFilter = str_tools.RemoveDiacritics(strFilter);
			if (search_flags('ww_btn')) strFilter = '\\b' + strFilter + '\\b';
			var search_mode_local;
			if (use_DO_wildcards_local) {
				search_mode_local = 'wild_' + search_mode;
				wild_obj.parse('*' + strFilter + '*', search_flags('case_btn') ? 'c' : '');
			}
			else {
				search_mode_local = 'regex_' + search_mode;
				try {
					regex = new RegExp(strFilter, search_flags('case_btn') ? "" : "i");
				}
				catch (err) {
					regex = new RegExp(strFilter.replace(/[.*+?^${}()|[\]\\]/g, '\\$&'), search_flags('case_btn') ? "" : "i");
				}
			}

			Log(1, '=> search mode       : ' + search_mode_local);
		}
		var total_sel = 0;
		var row, label, name, header, value, item_name, map_cat;
		cols_enum.moveFirst();
		for (; !cols_enum.atEnd(); cols_enum.moveNext()) {
			map_cat = cols_enum.item();
			// Log(1, 'map_cat:' + map_cat + ':' + columns_map(map_cat).size);
			if (category != category_all && map_cat != category) continue;
			for (var e = new Enumerator(columns_map(map_cat)); !e.atEnd(); e.moveNext()) {
				item_name = e.item();
				// Log(1, 'item_name:' + item_name + ':' + columns_map(map_cat)(item_name).size);
				label = columns_map(map_cat)(item_name)(label_var);
				name = columns_map(map_cat)(item_name).Exists(name_var) ? columns_map(map_cat)(item_name)(name_var) : item_name;
				header = columns_map(map_cat)(item_name).Exists(header_var) ? columns_map(map_cat)(item_name)(header_var) : label;
				value = columns_map(map_cat)(item_name).Exists(value_var) ? columns_map(map_cat)(item_name)(value_var) : columns_map(map_cat)(item_name)('value');
				if (!strFilter || match_map[search_mode_local](name, label, header, value)) {

					row = dlg.listview.getItemAt(dlg.listview.AddItem(item_name));
					if (row) {
						row.subitems(0) = columns_map(map_cat)(item_name)('label');
						row.subitems(1) = columns_map(map_cat)(item_name)('header');
						row.subitems(2) = map_cat;
						row.subitems(3) = columns_map(map_cat)(item_name)('value');
					}

					total_sel++;
				}
			}

		}
		dlg.listview.columns.AutoSize();
		dlg.listview.redraw = true;
		dlg.listview.value = 0;

		dlg.total_info.label = strings('column_str') + total_sel + ' / ' + total_count;
		return;
	}

	function updateListAdv() {
		dlg.listview.redraw = false;
		dlg.listview.RemoveItem(-1);
		var strFilter = dlg.query_edit.value.trim();
		var category = dlg.categories.value.name;
		if (strFilter) var compiledSearch = compileSearch(strFilter, search_flags);

		var total_sel = 0;
		var row, item_name, map_cat, item;
		cols_enum.moveFirst();
		try {
			for (; !cols_enum.atEnd(); cols_enum.moveNext()) {
				map_cat = cols_enum.item();
				if (category != category_all && map_cat != category) continue;
				for (var e = new Enumerator(columns_map(map_cat)); !e.atEnd(); e.moveNext()) {
					item_name = e.item();
					item = {
						'label': columns_map(map_cat)(item_name)(label_var),
						'name': columns_map(map_cat)(item_name).Exists(name_var) ? columns_map(map_cat)(item_name)(name_var) : item_name,
						'header': columns_map(map_cat)(item_name).Exists(header_var) ? columns_map(map_cat)(item_name)(header_var) : columns_map(map_cat)(item_name)(label_var),
						'type': map_cat,
						'value': columns_map(map_cat)(item_name).Exists(value_var) ? columns_map(map_cat)(item_name)(value_var) : columns_map(map_cat)(item_name)('value')
					}
					if (!strFilter || (compiledSearch && compiledSearch(item))) {

						row = dlg.listview.getItemAt(dlg.listview.AddItem(item_name));
						if (row) {
							row.subitems(0) = columns_map(map_cat)(item_name)('label');
							row.subitems(1) = columns_map(map_cat)(item_name)('header');
							row.subitems(2) = map_cat;
							row.subitems(3) = columns_map(map_cat)(item_name)('value');
						}

						total_sel++;
					}
				}

			}
		}
		catch (err) {
			Log(3, ' => Error filtering list : ' + err.description);
		}
		dlg.listview.columns.AutoSize();
		dlg.listview.redraw = true;
		dlg.listview.value = 0;

		dlg.total_info.label = strings('column_str') + total_sel + ' / ' + total_count;
		return;
	}

	function copyToClip(sel_items, mode) {
		var ini = new Date();
		var clip = '';
		var getClipContent; // Función que definirá cómo generar el contenido según el modo

		// Definir cómo se genera el contenido según el valor de 'mode'
		switch (mode) {
			case 1:
				getClipContent = function(item) {
					return item.name + '\r\n';
				};
				break;
			case 2:
				getClipContent = function(item) {
					return item.subitems(3) + '\r\n';
				};
				break;
			case 3:
				getClipContent = function(item) {
					return item.name + '\t' + item.subitems(3) + '\r\n';
				};
				break;
			case 4:
				getClipContent = function(item) {
					return item.subitems(0) + '\t' + item.subitems(3) + '\r\n';
				};
				break;
			case 5:
				getClipContent = function(item) {
					return item.subitems(1) + '\t' + item.subitems(3) + '\r\n';
				};
				break;
			case 6:
				getClipContent = function(item) {
					return item.name + '\t' + item.subitems(0) + '\t' + item.subitems(1) + '\t' + item.subitems(2) + '\t' + item.subitems(3) + '\r\n';
				};
				break;
			default:
				getClipContent = function(item) {
					return ''; // Si el modo es desconocido o 0, no hacemos nada.
				};
		}

		for (var i = 0; i < sel_items.length; i++)
			clip += getClipContent(sel_items(i));

		if (clip) {
			try {
				DOpus.SetClip(clip.slice(0, -2)); // Eliminar el último '\r\n'
				DOpus.Notify(script_label + ' v' + script_version, strings('msg_copy'), 'n');
			}
			catch (err) {
				Log(3, 'An error occurred when trying to set clipboard content: ' + err);
			}
		}
		Log(1, ' => Done in ' + (new Date() - ini) + 'ms');
		getClipContent = null;
		return;
	}

	function RunBuild() {
		Script.Vars.Set('item:' + uid, file);
		return cmd.RunCommandAsync(script_name + ' BUILDMAP=' + uid);
	}

	function Rebuild_ColMap(new_file) {
		try {
			Script.Vars.Delete('item:' + uid);
			Script.Vars.Delete(script_name + ':' + uid + ':abort');
			Script.Vars.Delete(script_name + ':' + uid + ':skip');
			// uid = new Date().getTime();
			if (new_file) file = new_file;
			Log(2, 'Loading new item : ' + file);
			if (!RunBuild()) {
				Log(4, 'Unable to run command for map!');
				return;
			}
			dlg.listview.RemoveItem(-1);
			dlg.main.title = script_label + ' v' + script_version + ' - ' + file;
			LoadThumbnail(file);
			columns_map = null;
			cols_enum = null;

			ToggleControls(false);
		}
		catch (err) {
			Log(4, 'An error ocurred when trying to refresh the list content : ' + err.description);
		}
		return;
	}

	function setSearchFlags(dlg) {
		var s = 0;
		for (var i = 0; i < search_menu.count; i++) {
			if (dlg.Control('check' + i).value) s += 1 << i;
		}
		if (s == 0) s = 1; //always have at least one flag active
		search_in_flags = s;
		Log(1, 'Set search_in_flags  : ' + search_in_flags);
		Script.Vars.Set('search_in_flags', s);
		Script.Vars('search_in_flags').persist = true;
		updateQueryCueText(s);
		UpdateStatusBar();
	}

	function UpdateProgress(msg_map) {

		progress_steps = msg_map('max_steps');

		step_count = msg_map('step');
		dlg.status_bar.label = msg_map('message');
		dlg.progress_bar.cx = ~~((dlg.progress_track.cx - 4) / progress_steps * step_count);
		Log(1, 'step:' + step_count + '; msg:' + msg_map('message') + '; xyz:' + dlg.progress_bar.x + ';' + dlg.progress_bar.y + ';' + dlg.progress_bar.cx + ';' + dlg.progress_bar.cy);
		// Log(1, 'progress_track xyz:' + dlg.progress_track.x + ';' + dlg.progress_track.y + ';' + dlg.progress_track.cx + ';' + dlg.progress_track.cy);

		dlg.skip_btn.visible = msg_map('skip');
	}

	function SetPos_Progress() {
		dlg.progress_track.y = dlg.search_mode.y;
		dlg.progress_bar.cy = dlg.progress_track.cy - 4;
		dlg.progress_bar.x = dlg.progress_track.x + 2;
		dlg.progress_bar.y = dlg.progress_track.y + 2;
	}

	function FillCategories() {
		cols_enum.moveFirst();
		dlg.categories.RemoveItem(-1);
		dlg.categories.AddItem(category_all);
		total_count = 0;
		for (; !cols_enum.atEnd(); cols_enum.moveNext()) {
			dlg.categories.AddItem(cols_enum.item());
			total_count += columns_map(cols_enum.item()).count;
		}

		dlg.categories.value = 0;
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
		dlg.wild_btn.label = '<a id="link">' + (search_flags('wild_btn') ? '<#!HOTLIGHT>' : '<#!BTNTEXT>') + '✱</#></a>';
		dlg.case_btn.label = '<a id="link">' + (search_flags('case_btn') ? '<#!HOTLIGHT>' : '<#!BTNTEXT>') + 'Aa</#></a>';
		dlg.regex_btn.label = '<a id="link">' + (search_flags('regex_btn') ? '<#!HOTLIGHT>' : '<#!BTNTEXT>') + '•✱</#></a>';
		dlg.diac_btn.label = '<a id="link">' + (search_flags('diac_btn') ? '<#!HOTLIGHT>' : '<#!BTNTEXT>') + 'ůü</#></a>';
		dlg.ww_btn.label = '<a id="link">' + (search_flags('ww_btn') ? '<#!HOTLIGHT>' : '<#!BTNTEXT>') + 'ww</#></a>';
		UpdateStatusBar();
		label_var = 'label' + (!search_flags('diac_btn') ? '_d' : '');
		name_var = 'name' + (!search_flags('diac_btn') ? '_d' : '');
		header_var = 'header' + (!search_flags('diac_btn') ? '_d' : '');
		value_var = 'value' + (!search_flags('diac_btn') ? '_d' : '');
		return;
	}

	function UpdateStatusBar() {
		var l = (search_flags('wild_btn') && !search_flags('regex_btn') && !search_flags('ww_btn')) ? strings('label_msg_DO_pattern') : '';
		if (search_flags('regex_btn')) l = l ? (l + ', ' + strings('label_regex').toLowerCase()) : strings('label_regex');
		if (search_flags('ww_btn')) l = l ? (l + ', ' + strings('label_ww').toLowerCase()) : strings('label_ww');
		if (search_flags('case_btn')) l = l ? (l + ', ' + strings('label_case').toLowerCase()) : strings('label_case');
		if (!search_flags('diac_btn')) l = l ? (l + ', ' + strings('label_ignore_diacritics').toLowerCase()) : strings('label_ignore_diacritics');
		dlg.status_bar.label = l;
		Log(1, '=> search_in_flags   : ' + search_in_flags);
		switch (search_in_flags) {
			case 15:
				search_mode = 'all'; //search in all available fields
				break;
			case 1:
				search_mode = 'name';
				break;
			case 2:
				search_mode = 'label';
				break;
			case 3:
				search_mode = 'name_label';
				break;
			case 4:
				search_mode = 'header';
				break;
			case 5:
				search_mode = 'name_header';
				break;
			case 6:
				search_mode = 'label_header';
				break;
			case 7:
				search_mode = 'name_label_header';
				break;
			case 8:
				search_mode = 'value';
				break;
			case 9:
				search_mode = 'name_value';
				break;
			case 10:
				search_mode = 'label_value';
				break;
			case 11:
				search_mode = 'name_label_value';
				break;
			case 12:
				search_mode = 'header_value';
				break;
			case 13:
				search_mode = 'name_header_value';
				break;
			case 14:
				search_mode = 'label_header_value';
				break;
		}
	}

	function SizeDlg(dlg, mode) {
		Log(1, 'Resize mode :' + ((mode == 0) ? ' Autosize' : ' Last saved'));
		if (mode == 0) autoSizeDlg(dlg);
		else dlg.LoadPosition('CommandViewers');
		return;

		function autoSizeDlg() {
			Log(2, 'Auto sizing dialog window based on current list content...');
			dlg.position = 'absolute';
			var lista = dlg.Control('listview');
			var w = lista.x * 2 + 50; //Add extra 50 px to width
			var bottom_h = dlg.cy - lista.y - lista.cy;
			for (var i = 0; i < lista.columns.count; i++) w += lista.columns.getColumnAt(i).width;
			var sysinfo = DOpus.Create().SysInfo();
			var area = sysinfo.WorkAreas(sysinfo.MouseMonitor);
			//don't make the dialog width bigger than 7/8 of screen size
			var max_width = Math.floor(area.width * 7 / 8);
			//don't make the dialog height bigger than 7/8 of screen size
			var max_height = Math.floor(area.height * 7 / 8);
			// 28 is an arbitrary row height
			var h = (lista.count + 1) * 28 + lista.y + bottom_h;
			if (h < dlg.cy) h = dlg.cy;
			else if (h > max_height) h = max_height;
			if (w > max_width) w = max_width;
			dlg.cx = w;
			dlg.cy = h;
			var x = area.left + Math.floor((area.width - w) / 2);
			var y = area.top + Math.floor((area.height - h) / 2);
			dlg.x = x;
			dlg.y = y;
			sysinfo = null;
			area = null;
			Log(1, 'Dialog metrics: X=' + x + '     Y=' + y + '     W=' + w + '     H=' + h);
		}
	}

	function checkFile(item, isdrop, parent) {
		if (!item || !FSU.Exists(item)) {
			Log(3, alert(dlg ? dlg.main : tab, DOpus.strings.get('msg_no_file')));
			return null;
		}
		if (DOpus.TypeOf(item) != 'object.Item' || isdrop) item = FSU.GetItem(item);
		if (item.fileattr.offline) {
			Log(3, alert(dlg ? dlg.main : tab, DOpus.strings.get('msg_no_file')));
			return null;
		}
		Log(2, 'file      : ' + item + '(' + DOpus.TypeOf(item) + ')');
		return item;
	}

	function LoadThumbnail(file) {
		var thumb = DOpus.LoadThumbnail(file, 2000, '', '', 'i');
		dlg.main.control('thumbnail').label = thumb ? thumb : '';
		thumb = null;
	}
}

function BuildColumnsList(item, uid) {
	var main_map = DOpus.Create().Map();
	if (!item || DOpus.TypeOf(item) != 'object.Item') return main_map;
	var this_pc = !item.realpath.test_root;
	Log(1, 'BUILD => item name : "' + item.name + '"; path: "' + item.path + '"');
	Log(1, 'BUILD => This PC : ' + this_pc);
	var metadata_type = item.metadata.def_value;
	var include_lists = BuildIncludedList();
	Log(1, 'included groups : ' + include_lists('groups').size);
	Log(1, 'included props : ' + include_lists('include').size);
	Log(1, 'included wild : ' + include_lists('include_wild').size);
	Log(1, 'excluded wild : ' + include_lists('exclude_wild').size);
	var ini = new Date();
	var helper;
	var max_steps = include_lists('groups').count;
	if (this_pc) max_steps++;
	var step = 0;
	var groups = DOpus.Create().Map('general', str_tools.LanguageStr(5029),
		'datetime', str_tools.LanguageStr(5142),
		'filesize', str_tools.LanguageStr(5143),
		'audio', str_tools.LanguageStr(5031),
		'docs', str_tools.LanguageStr(5033),
		'pics', str_tools.LanguageStr(5040),
		'video', str_tools.LanguageStr(5083),
		'exe', str_tools.LanguageStr(5032),
		'thispc', str_tools.LanguageStr(23155),
		'paths', str_tools.LanguageStr(5144));
	//  ██████╗ ███████╗███╗   ██╗███████╗██████╗  █████╗ ██╗     
	// ██╔════╝ ██╔════╝████╗  ██║██╔════╝██╔══██╗██╔══██╗██║     
	// ██║  ███╗█████╗  ██╔██╗ ██║█████╗  ██████╔╝███████║██║     
	// ██║   ██║██╔══╝  ██║╚██╗██║██╔══╝  ██╔══██╗██╔══██║██║     
	// ╚██████╔╝███████╗██║ ╚████║███████╗██║  ██║██║  ██║███████╗
	//  ╚═════╝ ╚══════╝╚═╝  ╚═══╝╚══════╝╚═╝  ╚═╝╚═╝  ╚═╝╚══════╝
	if (include_lists('groups').Exists('grp:general')) {
		step++;
		DOpus.SendCustomMsg(script_name + ':' + uid, DOpus.Create().Map('message', 'Getting General Columns...', 'skip', false, 'max_steps', max_steps, 'step', step));
		var general_map = DOpus.Create().Map();
		try {
			if (include_lists('include').Exists('label')) {
				helper = '';
				var v = item.Labels('*', 'explicit');
				for (var k = 0; k < v.count; k++)
					helper += v(k) + ';';
				if (helper) helper = helper.slice(0, -1);
				AddToMap(general_map, 'label', str_tools.LanguageStr(457), helper);
			}
			if (include_lists('include').Exists('status')) {
				helper = '';
				var v = item.Labels(str_tools.LanguageStr(8776));
				for (var k = 0; k < v.count; k++)
					helper += v(k) + ';';
				if (helper) helper = helper.slice(0, -1);
				AddToMap(general_map, 'status', str_tools.LanguageStr(564), helper, str_tools.LanguageStr(147));
			}
			if (include_lists('include').Exists('attr')) AddToMap(general_map, 'attr', str_tools.LanguageStr(315), item.attr_text, str_tools.LanguageStr(15));
			if (include_lists('include').Exists('owner')) AddToMap(general_map, 'owner', str_tools.LanguageStr(347), item.ShellProp('System.FileOwner', 'r'));
			if (include_lists('include').Exists('streamcount')) AddToMap(general_map, 'streamcount', str_tools.LanguageStr(30307), FSU.GetADSNames(item).count);
			if (metadata_type != 'none') {
				if (include_lists('include').Exists('desc')) {
					helper = item.metadata.other.usercomment || '';
					helper += (item.metadata.other.autodesc ? ((helper ? ' - ' : '') + item.metadata.other.autodesc) : '');
					AddToMap(general_map, 'desc', str_tools.LanguageStr(146), helper);
				}
				if (include_lists('include').Exists('rating')) AddToMap(general_map, 'rating', str_tools.LanguageStr(420), item.metadata.other.rating);
				helper = '';
				if (include_lists('include').Exists('keywords')) {
					for (var k = 0; k < item.metadata.tags.count; k++) helper += item.metadata.tags(k) + ';';
					if (helper) helper = helper.slice(0, -1);
					AddToMap(general_map, 'keywords', str_tools.LanguageStr(28074), helper);
				}
				if (include_lists('include').Exists('target')) AddToMap(general_map, 'target', str_tools.LanguageStr(553), item.metadata.other.target);
				if (include_lists('include').Exists('type')) AddToMap(general_map, 'type', str_tools.LanguageStr(312), item.metadata.other.target_type);
				if (include_lists('include').Exists('userdesc')) AddToMap(general_map, 'userdesc', str_tools.LanguageStr(140), item.metadata.other.usercomment);
			}
		}
		catch (err) {
			Log(3, 'BUILD => Error when trying to read a general property :' + err);
		}
		finally {
			Log(2, 'BUILD => General   : ' + general_map.count + ' columns added');
			if (!general_map.empty) main_map.Set(groups('general'), general_map);
			general_map = null;
		}
	}
	// ██████╗  █████╗ ████████╗███████╗    ██╗████████╗██╗███╗   ███╗███████╗
	// ██╔══██╗██╔══██╗╚══██╔══╝██╔════╝   ██╔╝╚══██╔══╝██║████╗ ████║██╔════╝
	// ██║  ██║███████║   ██║   █████╗    ██╔╝    ██║   ██║██╔████╔██║█████╗  
	// ██║  ██║██╔══██║   ██║   ██╔══╝   ██╔╝     ██║   ██║██║╚██╔╝██║██╔══╝  
	// ██████╔╝██║  ██║   ██║   ███████╗██╔╝      ██║   ██║██║ ╚═╝ ██║███████╗
	// ╚═════╝ ╚═╝  ╚═╝   ╚═╝   ╚══════╝╚═╝       ╚═╝   ╚═╝╚═╝     ╚═╝╚══════╝

	if (Script.vars.Exists(script_name + ':' + uid + ':abort') || !Script.vars.Exists('item:' + uid)) return abortBuild();
	if (include_lists('groups').Exists('grp:datetime')) {
		step++;
		DOpus.SendCustomMsg(script_name + ':' + uid, DOpus.Create().Map('message', 'Getting Date/Time Columns...', 'skip', false, 'step', step, 'max_steps', max_steps));

		var date_map = DOpus.Create().Map();
		try {
			helper = item.access;
			if (include_lists('include').Exists('accesseddate')) AddToMap(date_map, 'accesseddate', str_tools.LanguageStr(318), helper.Format('d', 'n'), str_tools.LanguageStr(18));
			if (include_lists('include').Exists('accesed')) AddToMap(date_map, 'accesed', str_tools.LanguageStr(567), helper.Format('', 's'), str_tools.LanguageStr(22));
			if (include_lists('include').Exists('accessedtime')) AddToMap(date_map, 'accessedtime', str_tools.LanguageStr(319), helper.Format('t', 's'), str_tools.LanguageStr(19));
			helper = item.create;
			if (include_lists('include').Exists('createddate')) AddToMap(date_map, 'createddate', str_tools.LanguageStr(316), helper.Format('d', 'n'), str_tools.LanguageStr(16));
			if (include_lists('include').Exists('created')) AddToMap(date_map, 'created', str_tools.LanguageStr(566), helper.Format('', 's'), str_tools.LanguageStr(21));
			if (include_lists('include').Exists('createdtime')) AddToMap(date_map, 'createdtime', str_tools.LanguageStr(317), helper.Format('t', 's'), str_tools.LanguageStr(17));
			helper = item.modify;
			if (include_lists('include').Exists('modifieddate')) AddToMap(date_map, 'modifieddate', str_tools.LanguageStr(313), helper.Format('d', 'n'), str_tools.LanguageStr(13));
			if (include_lists('include').Exists('modified')) AddToMap(date_map, 'modified', str_tools.LanguageStr(565), helper.Format('', 's'), str_tools.LanguageStr(20));
			if (include_lists('include').Exists('modifiedtime')) AddToMap(date_map, 'modifiedtime', str_tools.LanguageStr(314), helper.Format('t', 's'), str_tools.LanguageStr(14));
		}
		catch (err) {
			Log(3, 'BUILD => Error when trying to read a date property :' + err.description);
		}
		finally {
			Log(2, 'BUILD => Date/Time : ' + date_map.count + ' columns added');
			if (!date_map.empty) main_map.Set(groups('datetime'), date_map);
			date_map = null;
		}
	}
	// ███████╗██╗██╗     ███████╗███████╗██╗███████╗███████╗
	// ██╔════╝██║██║     ██╔════╝██╔════╝██║╚══███╔╝██╔════╝
	// █████╗  ██║██║     █████╗  ███████╗██║  ███╔╝ █████╗  
	// ██╔══╝  ██║██║     ██╔══╝  ╚════██║██║ ███╔╝  ██╔══╝  
	// ██║     ██║███████╗███████╗███████║██║███████╗███████╗
	// ╚═╝     ╚═╝╚══════╝╚══════╝╚══════╝╚═╝╚══════╝╚══════╝

	if (Script.vars.Exists(script_name + ':' + uid + ':abort') || !Script.vars.Exists('item:' + uid)) return abortBuild();
	if (include_lists('groups').Exists('grp:filesize')) {
		step++;
		DOpus.SendCustomMsg(script_name + ':' + uid, DOpus.Create().Map('message', 'Getting Size Columns...', 'skip', false, 'step', step, 'max_steps', max_steps));

		var size_map = DOpus.Create().Map();
		try {
			helper = item.size;
			if (include_lists('include').Exists('sizekb')) AddToMap(size_map, 'sizekb', str_tools.LanguageStr(353), (helper / 1024) + ' ' + str_tools.LanguageStr(1542), str_tools.LanguageStr(11));
			if (include_lists('include').Exists('sizeauto')) AddToMap(size_map, 'sizeauto', str_tools.LanguageStr(348), helper.fmt);
			if (include_lists('include').Exists('size')) AddToMap(size_map, 'size', str_tools.LanguageStr(311), helper);
		}
		catch (err) {
			Log(3, 'BUILD => Error when trying to read a size property :' + err.description);
		}
		finally {
			Log(2, 'BUILD => Size      : ' + size_map.count + ' columns added');
			if (!size_map.empty) main_map.Set(groups('filesize'), size_map);
			size_map = null;
		}
	}
	//  █████╗ ██╗   ██╗██████╗ ██╗ ██████╗ 
	// ██╔══██╗██║   ██║██╔══██╗██║██╔═══██╗
	// ███████║██║   ██║██║  ██║██║██║   ██║
	// ██╔══██║██║   ██║██║  ██║██║██║   ██║
	// ██║  ██║╚██████╔╝██████╔╝██║╚██████╔╝
	// ╚═╝  ╚═╝ ╚═════╝ ╚═════╝ ╚═╝ ╚═════╝ 
	if (Script.vars.Exists(script_name + ':' + uid + ':abort') || !Script.vars.Exists('item:' + uid)) return abortBuild();

	if (metadata_type != 'none') {
		if (include_lists('groups').Exists('grp:audio')) {
			step++;
			DOpus.SendCustomMsg(script_name + ':' + uid, DOpus.Create().Map('message', 'Getting Audio Columns...', 'skip', false, 'step', step, 'max_steps', max_steps));
			var audio_map = DOpus.Create().Map();
			try {
				helper = item.metadata.audio_text;
				if (include_lists('include').Exists('mp3albumartist')) AddToMap(audio_map, 'mp3albumartist', str_tools.LanguageStr(138), helper.mp3albumartist);
				if (include_lists('include').Exists('mp3artists')) AddToMap(audio_map, 'mp3artists', str_tools.LanguageStr(35), helper.mp3artist);
				if (include_lists('include').Exists('mp3year')) AddToMap(audio_map, 'mp3year', str_tools.LanguageStr(37), helper.mp3year);
				if (include_lists('include').Exists('mp3bpm')) AddToMap(audio_map, 'mp3bpm', str_tools.LanguageStr(116), helper.mp3bpm);
				if (include_lists('include').Exists('initialkey')) AddToMap(audio_map, 'initialkey', str_tools.LanguageStr(441), helper.initialkey);
				if (include_lists('include').Exists('mp3encoder')) AddToMap(audio_map, 'mp3encoder', str_tools.LanguageStr(393), helper.mp3encoder, str_tools.LanguageStr(89));
				if (include_lists('include').Exists('mp3comment')) AddToMap(audio_map, 'mp3comment', str_tools.LanguageStr(38), helper.mp3comment);
				if (include_lists('include').Exists('compilation')) AddToMap(audio_map, 'compilation', str_tools.LanguageStr(572), helper.compilation);
				if (include_lists('include').Exists('composers')) AddToMap(audio_map, 'composers', str_tools.LanguageStr(436), helper.composers);
				if (include_lists('include').Exists('conductors')) AddToMap(audio_map, 'conductors', str_tools.LanguageStr(437), helper.conductors);
				if (include_lists('include').Exists('audiocodec')) AddToMap(audio_map, 'audiocodec', str_tools.LanguageStr(92), helper.audiocodec);
				if (include_lists('include').Exists('copyright')) AddToMap(audio_map, 'copyright', str_tools.LanguageStr(344), helper.copyright);
				if (include_lists('include').Exists('duration')) AddToMap(audio_map, 'duration', str_tools.LanguageStr(50), helper.duration, str_tools.LanguageStr(350));
				if (include_lists('include').Exists('releasedate')) AddToMap(audio_map, 'releasedate', str_tools.LanguageStr(447), helper.releasedate, str_tools.LanguageStr(123));
				if (include_lists('include').Exists('mp3genre')) AddToMap(audio_map, 'mp3genre', str_tools.LanguageStr(33), helper.mp3genre);
				if (include_lists('include').Exists('mp3info')) AddToMap(audio_map, 'mp3info', str_tools.LanguageStr(46), helper.mp3info);
				if (include_lists('include').Exists('mp3mode')) AddToMap(audio_map, 'mp3mode', str_tools.LanguageStr(32), helper.mp3mode);
				if (include_lists('include').Exists('mp3disc')) AddToMap(audio_map, 'mp3disc', str_tools.LanguageStr(458), helper.mp3disc, str_tools.LanguageStr(134));
				if (include_lists('include').Exists('mp3track')) AddToMap(audio_map, 'mp3track', str_tools.LanguageStr(371), helper.mp3track, str_tools.LanguageStr(70));
				if (include_lists('include').Exists('picdepth')) AddToMap(audio_map, 'picdepth', str_tools.LanguageStr(327), helper.picdepth, str_tools.LanguageStr(27));
				if (include_lists('include').Exists('mp3encodingsoftware')) AddToMap(audio_map, 'mp3encodingsoftware', str_tools.LanguageStr(570), helper.mp3encodingsoftware, str_tools.LanguageStr(150));
				if (include_lists('include').Exists('mp3drm')) AddToMap(audio_map, 'mp3drm', str_tools.LanguageStr(418), helper.mp3drm);
				if (include_lists('include').Exists('publisher')) AddToMap(audio_map, 'publisher', str_tools.LanguageStr(434), helper.publisher);
				if (include_lists('include').Exists('mp3bitrate')) AddToMap(audio_map, 'mp3bitrate', str_tools.LanguageStr(330), helper.mp3bitrate, str_tools.LanguageStr(30));
				if (include_lists('include').Exists('mp3samplerate')) AddToMap(audio_map, 'mp3samplerate', str_tools.LanguageStr(331), helper.mp3samplerate, str_tools.LanguageStr(31));
				if (include_lists('include').Exists('mp3title')) AddToMap(audio_map, 'mp3title', str_tools.LanguageStr(34), helper.mp3title);
				if (include_lists('include').Exists('mp3album')) AddToMap(audio_map, 'mp3album', str_tools.LanguageStr(36), helper.mp3album);
			}
			catch (err) {
				Log(3, 'BUILD => Error when trying to read an audio property :' + err);
			}
			finally {
				Log(2, 'BUILD => Audio     : ' + audio_map.count + ' columns added');
				if (!audio_map.empty) main_map.Set(groups('audio'), audio_map);
				audio_map = null;
			}
		}
		// ██████╗  ██████╗  ██████╗███████╗
		// ██╔══██╗██╔═══██╗██╔════╝██╔════╝
		// ██║  ██║██║   ██║██║     ███████╗
		// ██║  ██║██║   ██║██║     ╚════██║
		// ██████╔╝╚██████╔╝╚██████╗███████║
		// ╚═════╝  ╚═════╝  ╚═════╝╚══════╝

		if (Script.vars.Exists(script_name + ':' + uid + ':abort') || !Script.vars.Exists('item:' + uid)) return abortBuild();
		if (include_lists('groups').Exists('grp:docs')) {
			step++;
			DOpus.SendCustomMsg(script_name + ':' + uid, DOpus.Create().Map('message', 'Getting Document Columns...', 'skip', false, 'step', step, 'max_steps', max_steps));
			var doc_map = DOpus.Create().Map();
			try {
				helper = item.metadata.doc_text;
				if (include_lists('include').Exists('author')) AddToMap(doc_map, 'author', str_tools.LanguageStr(363), helper.author);
				if (include_lists('include').Exists('category')) AddToMap(doc_map, 'category', str_tools.LanguageStr(366), helper.category);
				if (include_lists('include').Exists('comments')) AddToMap(doc_map, 'comments', str_tools.LanguageStr(368), helper.comments);
				if (include_lists('include').Exists('companyname')) AddToMap(doc_map, 'companyname', str_tools.LanguageStr(45), helper.companyname, str_tools.LanguageStr(345));
				if (include_lists('include').Exists('copyright')) AddToMap(doc_map, 'copyright', str_tools.LanguageStr(44), helper.copyright);
				if (include_lists('include').Exists('creator')) AddToMap(doc_map, 'creator', str_tools.LanguageStr(426), helper.creator);
				if (include_lists('include').Exists('doccreateddate')) AddToMap(doc_map, 'doccreateddate', str_tools.LanguageStr(403), helper.doccreateddate, str_tools.LanguageStr(103));
				if (include_lists('include').Exists('docedittime')) AddToMap(doc_map, 'docedittime', str_tools.LanguageStr(105), helper.docedittime);
				if (include_lists('include').Exists('doclastsavedby')) AddToMap(doc_map, 'doclastsavedby', str_tools.LanguageStr(406), helper.doclastsavedby, str_tools.LanguageStr(106));
				if (include_lists('include').Exists('doclastsaveddate')) AddToMap(doc_map, 'doclastsaveddate', str_tools.LanguageStr(404), helper.doclastsaveddate, str_tools.LanguageStr(104));
				if (include_lists('include').Exists('pages')) AddToMap(doc_map, 'pages', str_tools.LanguageStr(367), helper.pages);
				if (include_lists('include').Exists('producer')) AddToMap(doc_map, 'producer', str_tools.LanguageStr(427), helper.producer);
				if (include_lists('include').Exists('subject')) AddToMap(doc_map, 'subject', str_tools.LanguageStr(365), helper.subject);
				if (include_lists('include').Exists('title')) AddToMap(doc_map, 'title', str_tools.LanguageStr(364), helper.title);
			}
			catch (err) {
				Log(3, 'BUILD => Error when trying to read a document property :' + err);
			}
			finally {
				Log(2, 'BUILD => Documents : ' + doc_map.count + ' columns added');
				if (!doc_map.empty) main_map.Set(groups('docs'), doc_map);
				doc_map = null;
			}
		}
		// ██████╗ ██╗ ██████╗████████╗██╗   ██╗██████╗ ███████╗███████╗
		// ██╔══██╗██║██╔════╝╚══██╔══╝██║   ██║██╔══██╗██╔════╝██╔════╝
		// ██████╔╝██║██║        ██║   ██║   ██║██████╔╝█████╗  ███████╗
		// ██╔═══╝ ██║██║        ██║   ██║   ██║██╔══██╗██╔══╝  ╚════██║
		// ██║     ██║╚██████╗   ██║   ╚██████╔╝██║  ██║███████╗███████║
		// ╚═╝     ╚═╝ ╚═════╝   ╚═╝    ╚═════╝ ╚═╝  ╚═╝╚══════╝╚══════╝

		if (Script.vars.Exists(script_name + ':' + uid + ':abort') || !Script.vars.Exists('item:' + uid)) return abortBuild();
		if (include_lists('groups').Exists('grp:pictures')) {
			step++;
			DOpus.SendCustomMsg(script_name + ':' + uid, DOpus.Create().Map('message', 'Getting Picture Columns...', 'skip', false, 'step', step, 'max_steps', max_steps));

			var image_map = DOpus.Create().Map();
			try {
				helper = item.metadata.image_text;
				if (include_lists('include').Exists('35mmfocallength')) AddToMap(image_map, '35mmfocallength', str_tools.LanguageStr(386), helper['35mmfocallength'], str_tools.LanguageStr(83));
				if (include_lists('include').Exists('altitude')) AddToMap(image_map, 'altitude', str_tools.LanguageStr(101), helper['altitude']);
				if (include_lists('include').Exists('apertureval')) AddToMap(image_map, 'apertureval', str_tools.LanguageStr(357), helper['apertureval']);
				if (include_lists('include').Exists('cameramake')) AddToMap(image_map, 'cameramake', str_tools.LanguageStr(53), helper['cameramake']);
				if (include_lists('include').Exists('cameramodel')) AddToMap(image_map, 'cameramodel', str_tools.LanguageStr(54), helper['cameramodel']);
				if (include_lists('include').Exists('colormodel')) AddToMap(image_map, 'colormodel', str_tools.LanguageStr(29389), helper['colormodel']);
				if (include_lists('include').Exists('contrast')) AddToMap(image_map, 'contrast', str_tools.LanguageStr(383), helper['contrast']);
				if (include_lists('include').Exists('coords')) AddToMap(image_map, 'coords', str_tools.LanguageStr(100), helper['coords']);
				if (include_lists('include').Exists('datedigitized')) AddToMap(image_map, 'datedigitized', str_tools.LanguageStr(425), helper['datedigitized'], str_tools.LanguageStr(122));
				if (include_lists('include').Exists('datetaken')) AddToMap(image_map, 'datetaken', str_tools.LanguageStr(356), helper['datetaken'], str_tools.LanguageStr(55));
				if (include_lists('include').Exists('datetimecreated')) AddToMap(image_map, 'datetimecreated', str_tools.LanguageStr(30276), helper['datetimecreated'], str_tools.LanguageStr(30275));
				if (include_lists('include').Exists('datetimeoriginal')) AddToMap(image_map, 'datetimeoriginal', str_tools.LanguageStr(30274), helper['datetimeoriginal'], str_tools.LanguageStr(30273));
				if (include_lists('include').Exists('digitalzoom')) AddToMap(image_map, 'digitalzoom', str_tools.LanguageStr(387), helper['digitalzoom'], str_tools.LanguageStr(84));
				if (include_lists('include').Exists('exposurebias')) AddToMap(image_map, 'exposurebias', str_tools.LanguageStr(60), helper['exposurebias']);
				if (include_lists('include').Exists('flash')) AddToMap(image_map, 'flash', str_tools.LanguageStr(61), helper['flash']);
				if (include_lists('include').Exists('fnumber')) AddToMap(image_map, 'fnumber', str_tools.LanguageStr(77), helper['fnumber']);
				if (include_lists('include').Exists('imagequality')) AddToMap(image_map, 'imagequality', str_tools.LanguageStr(120), helper['imagequality']);
				if (include_lists('include').Exists('picwidth')) AddToMap(image_map, 'picwidth', str_tools.LanguageStr(25), helper['picwidth']);
				if (include_lists('include').Exists('picheight')) AddToMap(image_map, 'picheight', str_tools.LanguageStr(26), helper['picheight']);
				if (include_lists('include').Exists('picsize')) AddToMap(image_map, 'picsize', str_tools.LanguageStr(28), helper['picsize']);
				if (include_lists('include').Exists('mp3artist')) AddToMap(image_map, 'mp3artist', str_tools.LanguageStr(35), helper['mp3artist']);
				if (include_lists('include').Exists('copyright')) AddToMap(image_map, 'copyright', str_tools.LanguageStr(44), helper['copyright']);
				if (include_lists('include').Exists('shutterspeed')) AddToMap(image_map, 'shutterspeed', str_tools.LanguageStr(57), helper['shutterspeed']);
				if (include_lists('include').Exists('isospeed')) AddToMap(image_map, 'isospeed', str_tools.LanguageStr(58), helper['isospeed']);
				if (include_lists('include').Exists('whitebalance')) AddToMap(image_map, 'whitebalance', str_tools.LanguageStr(59), helper['whitebalance']);
				if (include_lists('include').Exists('scenecapturetype')) AddToMap(image_map, 'scenecapturetype', str_tools.LanguageStr(78), helper['scenecapturetype']);
				if (include_lists('include').Exists('latitude')) AddToMap(image_map, 'latitude', str_tools.LanguageStr(98), helper['latitude']);
				if (include_lists('include').Exists('longitude')) AddToMap(image_map, 'longitude', str_tools.LanguageStr(99), helper['longitude']);
				if (include_lists('include').Exists('scenemode')) AddToMap(image_map, 'scenemode', str_tools.LanguageStr(118), helper['scenemode']);
				if (include_lists('include').Exists('macromode')) AddToMap(image_map, 'macromode', str_tools.LanguageStr(119), helper['macromode']);
				if (include_lists('include').Exists('picphysx')) AddToMap(image_map, 'picphysx', str_tools.LanguageStr(136), helper['picphysx']);
				if (include_lists('include').Exists('picphysy')) AddToMap(image_map, 'picphysy', str_tools.LanguageStr(137), helper['picphysy']);
				if (include_lists('include').Exists('lensmodel')) AddToMap(image_map, 'lensmodel', str_tools.LanguageStr(152), helper['lensmodel']);
				if (include_lists('include').Exists('lensmake')) AddToMap(image_map, 'lensmake', str_tools.LanguageStr(153), helper['lensmake']);
				if (include_lists('include').Exists('author')) AddToMap(image_map, 'author', str_tools.LanguageStr(363), helper['author']);
				if (include_lists('include').Exists('title')) AddToMap(image_map, 'title', str_tools.LanguageStr(364), helper['title']);
				if (include_lists('include').Exists('subject')) AddToMap(image_map, 'subject', str_tools.LanguageStr(365), helper['subject']);
				if (include_lists('include').Exists('rotation')) AddToMap(image_map, 'rotation', str_tools.LanguageStr(382), helper['rotation']);
				if (include_lists('include').Exists('saturation')) AddToMap(image_map, 'saturation', str_tools.LanguageStr(384), helper['saturation']);
				if (include_lists('include').Exists('sharpness')) AddToMap(image_map, 'sharpness', str_tools.LanguageStr(385), helper['sharpness']);
				if (include_lists('include').Exists('software')) AddToMap(image_map, 'software', str_tools.LanguageStr(417), helper['software']);
				if (include_lists('include').Exists('picres')) AddToMap(image_map, 'picres', str_tools.LanguageStr(30007), helper['picres']);
				if (include_lists('include').Exists('exposureprogram')) AddToMap(image_map, 'exposureprogram', str_tools.LanguageStr(375), helper['exposureprogram'], str_tools.LanguageStr(74));
				if (include_lists('include').Exists('exposuretime')) AddToMap(image_map, 'exposuretime', str_tools.LanguageStr(377), helper['exposuretime'], str_tools.LanguageStr(76));
				if (include_lists('include').Exists('focallength')) AddToMap(image_map, 'focallength', str_tools.LanguageStr(373), helper['focallength'], str_tools.LanguageStr(72));
				if (include_lists('include').Exists('imagedesc')) AddToMap(image_map, 'imagedesc', str_tools.LanguageStr(563), helper['imagedesc'], str_tools.LanguageStr(24));
				if (include_lists('include').Exists('instructions')) AddToMap(image_map, 'instructions', str_tools.LanguageStr(571), helper['instructions'], str_tools.LanguageStr(151));
				if (include_lists('include').Exists('meteringmode')) AddToMap(image_map, 'meteringmode', str_tools.LanguageStr(374), helper['meteringmode'], str_tools.LanguageStr(73));
				if (include_lists('include').Exists('picresx')) AddToMap(image_map, 'picresx', str_tools.LanguageStr(369), helper['picresx'], str_tools.LanguageStr(68));
				if (include_lists('include').Exists('picresy')) AddToMap(image_map, 'picresy', str_tools.LanguageStr(370), helper['picresy'], str_tools.LanguageStr(69));
				if (include_lists('include').Exists('subjectdistance')) AddToMap(image_map, 'subjectdistance', str_tools.LanguageStr(376), helper['subjectdistance'], str_tools.LanguageStr(75));
				if (include_lists('include').Exists('aspectratio')) AddToMap(image_map, 'aspectratio', str_tools.LanguageStr(416), helper['aspectratio'], str_tools.LanguageStr(113));
				if (include_lists('include').Exists('picdepth')) AddToMap(image_map, 'picdepth', str_tools.LanguageStr(327), helper['picdepth'], str_tools.LanguageStr(27));
			}
			catch (err) {
				Log(3, 'BUILD => Error when trying to read an image property :' + err);
			}
			finally {
				Log(2, 'BUILD => Pictures  : ' + image_map.count + ' columns added');
				if (!image_map.empty) main_map.Set(groups('pics'), image_map);
				image_map = null;
			}
		}
		// ██╗   ██╗██╗██████╗ ███████╗ ██████╗ 
		// ██║   ██║██║██╔══██╗██╔════╝██╔═══██╗
		// ██║   ██║██║██║  ██║█████╗  ██║   ██║
		// ╚██╗ ██╔╝██║██║  ██║██╔══╝  ██║   ██║
		//  ╚████╔╝ ██║██████╔╝███████╗╚██████╔╝
		//   ╚═══╝  ╚═╝╚═════╝ ╚══════╝ ╚═════╝ 

		if (Script.vars.Exists(script_name + ':' + uid + ':abort') || !Script.vars.Exists('item:' + uid)) return abortBuild();
		if (include_lists('groups').Exists('grp:video')) {
			step++;
			DOpus.SendCustomMsg(script_name + ':' + uid, DOpus.Create().Map('message', 'Getting Video Columns...', 'skip', false, 'step', step, 'max_steps', max_steps));

			var video_map = DOpus.Create().Map();
			try {
				helper = item.metadata.video_text;
				if (include_lists('include').Exists('audiocount')) AddToMap(video_map, 'audiocount', str_tools.LanguageStr(29378), helper['audiocount'], str_tools.LanguageStr(29388));
				if (include_lists('include').Exists('subtitlecount')) AddToMap(video_map, 'subtitlecount', str_tools.LanguageStr(29382), helper['subtitlecount'], str_tools.LanguageStr(29393));
				if (include_lists('include').Exists('videocount')) AddToMap(video_map, 'videocount', str_tools.LanguageStr(29384), helper['videocount'], str_tools.LanguageStr(29395));
				if (include_lists('include').Exists('aspectratio')) AddToMap(video_map, 'aspectratio', str_tools.LanguageStr(416), helper['aspectratio'], str_tools.LanguageStr(113));
				if (include_lists('include').Exists('mp3bitrate')) AddToMap(video_map, 'mp3bitrate', str_tools.LanguageStr(330), helper['mp3bitrate'], str_tools.LanguageStr(30));
				if (include_lists('include').Exists('picdepth')) AddToMap(video_map, 'picdepth', str_tools.LanguageStr(327), helper['picdepth'], str_tools.LanguageStr(27));
				if (include_lists('include').Exists('channel')) AddToMap(video_map, 'channel', str_tools.LanguageStr(450), helper['channel'], str_tools.LanguageStr(126));
				if (include_lists('include').Exists('datarate')) AddToMap(video_map, 'datarate', str_tools.LanguageStr(395), helper['datarate'], str_tools.LanguageStr(91));
				if (include_lists('include').Exists('picheight')) AddToMap(video_map, 'picheight', str_tools.LanguageStr(326), helper['picheight'], str_tools.LanguageStr(26));
				if (include_lists('include').Exists('picphyssize')) AddToMap(video_map, 'picphyssize', str_tools.LanguageStr(459), helper['picphyssize'], str_tools.LanguageStr(135));
				if (include_lists('include').Exists('recordingtime')) AddToMap(video_map, 'recordingtime', str_tools.LanguageStr(454), helper['recordingtime'], str_tools.LanguageStr(130));
				if (include_lists('include').Exists('picwidth')) AddToMap(video_map, 'picwidth', str_tools.LanguageStr(325), helper['picwidth'], str_tools.LanguageStr(25));
				if (include_lists('include').Exists('mp3samplerate')) AddToMap(video_map, 'mp3samplerate', str_tools.LanguageStr(331), helper['mp3samplerate'], str_tools.LanguageStr(31));
				if (include_lists('include').Exists('fourcc')) AddToMap(video_map, 'fourcc', str_tools.LanguageStr(558), helper['fourcc'], str_tools.LanguageStr(142));
				if (include_lists('include').Exists('framerate')) AddToMap(video_map, 'framerate', str_tools.LanguageStr(394), helper['framerate'], str_tools.LanguageStr(90));
				if (include_lists('include').Exists('ishd')) AddToMap(video_map, 'ishd', str_tools.LanguageStr(451), helper['ishd'], str_tools.LanguageStr(127));
				if (include_lists('include').Exists('mp3mode')) AddToMap(video_map, 'mp3mode', str_tools.LanguageStr(332), helper['mp3mode'], str_tools.LanguageStr(32));
				if (include_lists('include').Exists('isrepeat')) AddToMap(video_map, 'isrepeat', str_tools.LanguageStr(452), helper['isrepeat'], str_tools.LanguageStr(128));
				if (include_lists('include').Exists('station')) AddToMap(video_map, 'station', str_tools.LanguageStr(455), helper['station'], str_tools.LanguageStr(131));
				if (include_lists('include').Exists('videocodec')) AddToMap(video_map, 'videocodec', str_tools.LanguageStr(397), helper['videocodec'], str_tools.LanguageStr(93));
				if (include_lists('include').Exists('alllangs')) AddToMap(video_map, 'alllangs', str_tools.LanguageStr(29375), helper['alllangs'], str_tools.LanguageStr(7572));
				if (include_lists('include').Exists('audiolangs')) AddToMap(video_map, 'audiolangs', str_tools.LanguageStr(29377), helper['audiolangs'], str_tools.LanguageStr(29387));
				if (include_lists('include').Exists('hdrtypes')) AddToMap(video_map, 'hdrtypes', str_tools.LanguageStr(29380), helper['hdrtypes'], str_tools.LanguageStr(29390));
				if (include_lists('include').Exists('subtitlelangs')) AddToMap(video_map, 'subtitlelangs', str_tools.LanguageStr(29381), helper['subtitlelangs'], str_tools.LanguageStr(29392));
				if (include_lists('include').Exists('videolangs')) AddToMap(video_map, 'videolangs', str_tools.LanguageStr(29383), helper['videolangs'], str_tools.LanguageStr(29394));
				if (include_lists('include').Exists('picsize')) AddToMap(video_map, 'picsize', str_tools.LanguageStr(28), helper['picsize']);
				if (include_lists('include').Exists('mp3genre')) AddToMap(video_map, 'mp3genre', str_tools.LanguageStr(33), helper['mp3genre']);
				if (include_lists('include').Exists('mp3title')) AddToMap(video_map, 'mp3title', str_tools.LanguageStr(34), helper['mp3title']);
				if (include_lists('include').Exists('mp3artist')) AddToMap(video_map, 'mp3artist', str_tools.LanguageStr(35), helper['mp3artist']);
				if (include_lists('include').Exists('mp3year')) AddToMap(video_map, 'mp3year', str_tools.LanguageStr(37), helper['mp3year']);
				if (include_lists('include').Exists('audiocodec')) AddToMap(video_map, 'audiocodec', str_tools.LanguageStr(92), helper['audiocodec']);
				if (include_lists('include').Exists('episodename')) AddToMap(video_map, 'episodename', str_tools.LanguageStr(124), helper['episodename']);
				if (include_lists('include').Exists('broadcastdate')) AddToMap(video_map, 'broadcastdate', str_tools.LanguageStr(129), helper['broadcastdate']);
				if (include_lists('include').Exists('duration')) AddToMap(video_map, 'duration', str_tools.LanguageStr(350), helper['duration']);
				if (include_lists('include').Exists('mp3drm')) AddToMap(video_map, 'mp3drm', str_tools.LanguageStr(418), helper['mp3drm']);
				if (include_lists('include').Exists('publisher')) AddToMap(video_map, 'publisher', str_tools.LanguageStr(434), helper['publisher']);
				if (include_lists('include').Exists('composers')) AddToMap(video_map, 'composers', str_tools.LanguageStr(436), helper['composers']);
				if (include_lists('include').Exists('conductors')) AddToMap(video_map, 'conductors', str_tools.LanguageStr(437), helper['conductors']);
				if (include_lists('include').Exists('director')) AddToMap(video_map, 'director', str_tools.LanguageStr(444), helper['director']);
				if (include_lists('include').Exists('releasedate')) AddToMap(video_map, 'releasedate', str_tools.LanguageStr(447), helper['releasedate']);
				if (include_lists('include').Exists('credits')) AddToMap(video_map, 'credits', str_tools.LanguageStr(449), helper['credits']);

			}
			catch (err) {
				Log(3, 'BUILD => Error when trying to read a video property :' + err);
			}
			finally {
				Log(2, 'BUILD => Video     : ' + video_map.count + ' columns added');
				if (!video_map.empty) main_map.Set(groups('video'), video_map);
				video_map = null;
			}
		}
		// ███████╗██╗  ██╗███████╗
		// ██╔════╝╚██╗██╔╝██╔════╝
		// █████╗   ╚███╔╝ █████╗  
		// ██╔══╝   ██╔██╗ ██╔══╝  
		// ███████╗██╔╝ ██╗███████╗
		// ╚══════╝╚═╝  ╚═╝╚══════╝
		if (Script.vars.Exists(script_name + ':' + uid + ':abort') || !Script.vars.Exists('item:' + uid)) return abortBuild();
		if (include_lists('groups').Exists('grp:exe')) {
			step++;
			DOpus.SendCustomMsg(script_name + ':' + uid, DOpus.Create().Map('message', 'Getting Programs Columns...', 'skip', false, 'step', step, 'max_steps', max_steps));

			var exe_map = DOpus.Create().Map();
			try {
				helper = item.metadata.exe_text;
				if (include_lists('include').Exists('moddesc')) AddToMap(exe_map, 'moddesc', str_tools.LanguageStr(340), helper['moddesc'], str_tools.LanguageStr(40));
				if (include_lists('include').Exists('modversion')) AddToMap(exe_map, 'modversion', str_tools.LanguageStr(341), helper['modversion'], str_tools.LanguageStr(41));
				if (include_lists('include').Exists('prodname')) AddToMap(exe_map, 'prodname', str_tools.LanguageStr(342), helper['prodname'], str_tools.LanguageStr(42));
				if (include_lists('include').Exists('prodversion')) AddToMap(exe_map, 'prodversion', str_tools.LanguageStr(343), helper['prodversion'], str_tools.LanguageStr(43));
				if (include_lists('include').Exists('copyright')) AddToMap(exe_map, 'copyright', str_tools.LanguageStr(344), helper['copyright'], str_tools.LanguageStr(44));
				if (include_lists('include').Exists('companyname')) AddToMap(exe_map, 'companyname', str_tools.LanguageStr(345), helper['companyname'], str_tools.LanguageStr(45));

			}
			catch (err) {
				Log(3, 'BUILD => Error when trying to read a program property :' + err);
			}
			finally {
				Log(2, 'BUILD => Programs  : ' + exe_map.count + ' columns added');
				if (!exe_map.empty) main_map.Set(groups('exe'), exe_map);
				exe_map = null;
			}
		}
	}
	if (this_pc) {
		try {
			if (Script.vars.Exists(script_name + ':' + uid + ':abort') || !Script.vars.Exists('item:' + uid)) return abortBuild();
			step++;
			DOpus.SendCustomMsg(script_name + ':' + uid, DOpus.Create().Map('message', 'Getting This PC Columns...', 'skip', false, 'step', step, 'max_steps', max_steps));

			var mapa = DOpus.Create().Map();
			var FSO = new ActiveXObject('Scripting.FileSystemObject');
			var drive = FSO.GetDrive(FSO.GetDriveName(item));
			var total_size = drive.TotalSize;
			var free_size = drive.FreeSpace;
			var used_size = total_size - free_size;

			helper = drive.VolumeName;
			main_map(groups('filesize'))('size')('value') = total_size;
			main_map(groups('filesize'))('sizeauto')('value') = FSU.NewFileSize(total_size).fmt;
			main_map(groups('filesize'))('sizekb')('value') = (total_size / 1024) + ' ' + str_tools.LanguageStr(1542);

			AddToMap(mapa, 'sh:freespace', str_tools.LanguageStr(5755), FSU.NewFileSize(free_size).fmt);
			AddToMap(mapa, 'sh:filesys', str_tools.LanguageStr(5756), String(drive.FileSystem));
			AddToMap(mapa, 'sh:usedspace', str_tools.LanguageStr(5841), FSU.NewFileSize(used_size).fmt);
			AddToMap(mapa, 'sh:freepercent', str_tools.LanguageStr(5758), Math.round(free_size * 100 / total_size) + '%');
			AddToMap(mapa, 'sh:usedpercent', str_tools.LanguageStr(5757), Math.round(used_size * 100 / total_size) + '%');

		}
		catch (err) {
			Log(3, 'BUILD => Error when trying to read a program property :' + err);
		}
		finally {
			FSO = null;
			drive = null;
			Log(2, 'BUILD => This PC  : ' + mapa.count + ' columns added');
			if (!mapa.empty) main_map.Set(groups('thispc'), mapa);
			mapa = null;
		}

	}
	//  ██████╗ ████████╗██╗  ██╗███████╗██████╗      ██████╗ ██████╗ ██╗     ███████╗
	// ██╔═══██╗╚══██╔══╝██║  ██║██╔════╝██╔══██╗    ██╔════╝██╔═══██╗██║     ██╔════╝
	// ██║   ██║   ██║   ███████║█████╗  ██████╔╝    ██║     ██║   ██║██║     ███████╗
	// ██║   ██║   ██║   ██╔══██║██╔══╝  ██╔══██╗    ██║     ██║   ██║██║     ╚════██║
	// ╚██████╔╝   ██║   ██║  ██║███████╗██║  ██║    ╚██████╗╚██████╔╝███████╗███████║
	//  ╚═════╝    ╚═╝   ╚═╝  ╚═╝╚══════╝╚═╝  ╚═╝     ╚═════╝ ╚═════╝ ╚══════╝╚══════╝

	if (Script.vars.Exists(script_name + ':' + uid + ':abort') || !Script.vars.Exists('item:' + uid)) return abortBuild();

	var DOvar = 'columnsviewer';
	var cols_keys_arr, trs, t;

	var filter = DOpus.Create().Filter();

	outer: for (var h = 0; h < 4; h++) {
		DOpus.Vars.Delete(DOvar);

		if (h === 0 && include_lists('groups').Exists('grp:script')) {
			step++;
			t = 'Script';
			Log(1, 'BUILD => Getting ' + t + ' Columns');
			DOpus.SendCustomMsg(script_name + ':' + uid, DOpus.Create().Map('message', 'Getting ' + t + ' Columns...', 'skip', true, 'step', step, 'max_steps', max_steps));
			trs = str_tools.LanguageStr(30087);
			cols_keys_arr = parseScriptXmls(item.name, DOvar, include_lists('include_wild'), include_lists('exclude_wild'));
		}
		else if (h === 1 && include_lists('groups').Exists('grp:evaluator')) {
			step++;
			t = 'Evaluator';
			Log(1, 'BUILD => Getting ' + t + ' Columns');
			DOpus.SendCustomMsg(script_name + ':' + uid, DOpus.Create().Map('message', 'Getting ' + t + ' Columns...', 'skip', true, 'step', step, 'max_steps', max_steps));
			trs = str_tools.LanguageStr(28574);
			cols_keys_arr = parseEvaluatorXml(item.name, DOvar, include_lists('include_wild'), include_lists('exclude_wild'));
		}
		else if (h === 2 && !this_pc) {
			t = 'Special';
			Log(1, 'BUILD => Getting ' + t + ' Columns');
			// DOpus.SendCustomMsg(script_name + ':' + uid, DOpus.Create().Map('message', 'Getting ' + t + ' Columns...', 'skip', true, 'step', step));
			trs = null;
			var obj = {};
			if (include_lists('groups').Exists('grp:general')) {
				if (include_lists('include').Exists('availability')) obj['availability'] = {
					'header': str_tools.LanguageStr(145),
					'group': groups('general')
				};
				if (include_lists('include').Exists('perms')) obj['perms'] = {
					'header': str_tools.LanguageStr(29391),
					'group': groups('general')
				};
			}
			if (include_lists('groups').Exists('grp:filesize')) {
				if (include_lists('include').Exists('filecounttotal')) obj['filecounttotal'] = {
					'label': str_tools.LanguageStr(388),
					'header': str_tools.LanguageStr(86),
					'group': groups('filesize')
				};
				if (include_lists('include').Exists('dircounttotal')) obj['dircounttotal'] = {
					'label': str_tools.LanguageStr(389),
					'header': str_tools.LanguageStr(85),
					'group': groups('filesize')
				};
				if (include_lists('include').Exists('filecount')) obj['filecount'] = {
					'label': str_tools.LanguageStr(390),
					'header': str_tools.LanguageStr(88),
					'group': groups('filesize')
				};
				if (include_lists('include').Exists('dircount')) obj['dircount'] = {
					'label': str_tools.LanguageStr(391),
					'header': str_tools.LanguageStr(87),
					'group': groups('filesize')
				};
			}
			if (include_lists('groups').Exists('grp:exe') && include_lists('include').Exists('signer'))
				obj['signer'] = {
					'header': str_tools.LanguageStr(30308),
					'group': groups('exe')
				}

			cols_keys_arr = getSpecialsValues(item.name, DOvar, obj, include_lists('include_wild'), include_lists('exclude_wild'));
		}
		else if (include_lists('groups').Exists('grp:shell')) {
			step++;
			t = 'Shell';
			Log(1, 'BUILD => Getting ' + t + ' Columns');
			DOpus.SendCustomMsg(script_name + ':' + uid, DOpus.Create().Map('message', 'Getting ' + t + ' Columns...', 'skip', true, 'step', step, 'max_steps', max_steps));
			trs = str_tools.LanguageStr(28611);
			cols_keys_arr = parseShellXml(item.name, DOvar, include_lists('include_wild'), include_lists('exclude_wild'));
		}
		var cols_values = [];
		if (cols_keys_arr && cols_keys_arr[0]) {
			//Evaluator mode
			try {
				if (Script.vars.Exists(script_name + ':' + uid + ':abort') || !Script.vars.Exists('item:' + uid)) return abortBuild();
				else if (Script.vars.Exists(script_name + ':' + uid + ':skip')) continue outer;

				if (filter.Set(cols_keys_arr[0])) {
					var dummy = item.matchFilter(filter);
					DOpus.Delay(20);

					if (DOpus.Vars.Exists(DOvar)) {
						cols_values = DOpus.Vars.Get(DOvar);
						cols_values = cols_values.split('#_#');
					}
					else
						Log(3, 'BUILD => DOpus var ' + DOvar + ' doesn\'t exist! Skipping ' + t + ' columns');
				}
			}

			catch (e) {
				Log(3, 'BUILD => Error when trying to get values for ' + t + ' columns : ' + e);
				continue outer;
			}
			if (cols_values.length > 0) {
				cols_keys_arr = cols_keys_arr.slice(1);
				if (cols_keys_arr.length == cols_values.length) {
					var values_map = DOpus.Create().Map();
					for (var i = 0; i < cols_keys_arr.length; i++) {
						try {
							Log(1, 'BUILD => Adding ' + t + ' column :' + cols_keys_arr[i]['value']);
							if (cols_values[i] == '<novalue>') cols_values[i] = '';
							values_map(cols_keys_arr[i]['value']) = DOpus.Create().Map(
								'name_d', str_tools.RemoveDiacritics(cols_keys_arr[i]['value']),
								'label', cols_keys_arr[i]['name'],
								'label_d', str_tools.RemoveDiacritics(cols_keys_arr[i]['name']),
								'header', 'header' in cols_keys_arr[i] ? cols_keys_arr[i]['header'] : cols_keys_arr[i]['name'],
								'header_d', str_tools.RemoveDiacritics('header' in cols_keys_arr[i] ? cols_keys_arr[i]['header'] : cols_keys_arr[i]['name']),
								'value', cols_values[i],
								'value_d', str_tools.RemoveDiacritics(cols_values[i])
							);
							if (!trs && 'group' in cols_keys_arr[i]) {
								// Log(1, 'Group ' + cols_keys_arr[i]['group']);
								if (!main_map.Exists(cols_keys_arr[i]['group'])) {
									main_map.Set(cols_keys_arr[i]['group'], DOpus.Create().Map());
								}
								main_map(cols_keys_arr[i]['group']).Set(cols_keys_arr[i]['value'], values_map(cols_keys_arr[i]['value']));
							}
						}
						catch (e) {
							Log(3, 'BUILD => Error when trying to get a value for ' + t + ' column: ' + err.description);
							continue;
						}
					}
					Log(2, 'BUILD => ' + t + '    : ' + values_map.count + ' columns added');
					if (trs) main_map.Set(trs, values_map);
				}
				else Log(3, 'BUILD => Error when trying to get values for ' + t + ' columns: Array values doesn\'t match with array names : ' + cols_keys_arr.length + '==' + cols_values.length);
			}
		}

	}

	cols_values = null;
	cols_keys_arr = null;
	values_map = null;

	// ██████╗  █████╗ ████████╗██╗  ██╗███████╗
	// ██╔══██╗██╔══██╗╚══██╔══╝██║  ██║██╔════╝
	// ██████╔╝███████║   ██║   ███████║███████╗
	// ██╔═══╝ ██╔══██║   ██║   ██╔══██║╚════██║
	// ██║     ██║  ██║   ██║   ██║  ██║███████║
	// ╚═╝     ╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚══════╝

	if (Script.vars.Exists(script_name + ':' + uid + ':abort') || !Script.vars.Exists('item:' + uid)) return abortBuild();
	if (include_lists('groups').Exists('grp:paths')) {
		step++;
		DOpus.SendCustomMsg(script_name + ':' + uid, DOpus.Create().Map('message', 'Getting Path Columns...', 'skip', false, 'step', step, 'max_steps', max_steps));
		var path_map = DOpus.Create().Map();
		try {

			if (include_lists('include').Exists('name')) AddToMap(path_map, 'name', str_tools.LanguageStr(310), this_pc ? helper : item.name, str_tools.LanguageStr(10));
			if (include_lists('include').Exists('ext')) AddToMap(path_map, 'ext', str_tools.LanguageStr(323), item.ext.slice(1), str_tools.LanguageStr(23));
			if (include_lists('include').Exists('extdir')) AddToMap(path_map, 'extdir', str_tools.LanguageStr(381), (item.is_dir) ? str_tools.LanguageStr(2132) : item.ext.slice(1), str_tools.LanguageStr(23));
			helper = item.realpath;
			if (include_lists('include').Exists('fullpath')) AddToMap(path_map, 'fullpath', str_tools.LanguageStr(144), helper);
			if (include_lists('include').Exists('pathlen')) AddToMap(path_map, 'pathlen', str_tools.LanguageStr(143), String(helper).length);
			if (include_lists('include').Exists('path')) AddToMap(path_map, 'path', str_tools.LanguageStr(39), helper.pathpart);

			if (helper.test_parent) helper = FSU.NewPath(helper.pathpart);
			if (include_lists('include').Exists('parent')) AddToMap(path_map, 'parent', str_tools.LanguageStr(410), item.realpath.test_parent ? helper.filepart : '');
			if (include_lists('include').Exists('parentlocation')) AddToMap(path_map, 'parentlocation', str_tools.LanguageStr(412), helper.test_parent ? (helper.filepart + ' (' + helper.pathpart + ')') : '', str_tools.LanguageStr(110));
			if (include_lists('include').Exists('parentpath')) AddToMap(path_map, 'parentpath', str_tools.LanguageStr(411), helper.test_parent ? helper.pathpart : '', str_tools.LanguageStr(111));
		}
		catch (err) {
			Log(3, 'BUILD => Error when trying to read a document property :' + err);
		}
		finally {
			Log(2, 'BUILD => Paths     : ' + path_map.count + ' columns added');
			if (!path_map.empty) main_map.Set(groups('paths'), path_map);
			path_map = null;

		}
	}
	return abortBuild();

	function abortBuild() {
		Log(2, 'BUILD => Building column\'s list finished: ' + (new Date() - ini) + ' ms');

		return main_map;
	}

	function AddToMap(map, key, label, value, header) {
		map(key) = DOpus.Create().Map();
		map(key).Set('label', label);
		map(key).Set('label_d', str_tools.RemoveDiacritics(label));
		if (header) {
			map(key).Set('header', header);
			map(key).Set('header_d', str_tools.RemoveDiacritics(header));
		}
		else map(key).Set('header', label);
		if (value == undefined) value = '';
		else map(key).Set('value_d', str_tools.RemoveDiacritics(value));
		map(key).Set('value', value);
	}

	function BuildIncludedList() {
		var mpa = DOpus.Create().Map('groups', DOpus.Create().StringSetI(), 'include', DOpus.Create().StringSetI(), 'include_wild', DOpus.NewVector(), 'exclude_wild', DOpus.NewVector());

		var grp_regex = /^grp:(general|datetime|filesize|audio|docs|pictures|video|exe|script|evaluator|shell)$/i;

		var props = DOpus.Create().Map('grp:general', DOpus.Create().StringSetI('label', 'status', 'attr', 'owner', 'streamcount', 'desc', 'rating', 'keywords', 'target', 'type', 'userdesc', 'availability', 'perms'),
			'grp:datetime', DOpus.Create().StringSetI('accesseddate', 'accesed', 'accessedtime', 'createddate', 'created', 'createdtime', 'modifieddate', 'modified', 'modifiedtime'),
			'grp:filesize', DOpus.Create().StringSetI('sizekb', 'sizeauto', 'size', 'filecounttotal', 'dircounttotal', 'filecount', 'dircount'),
			'grp:audio', DOpus.Create().StringSetI('mp3albumartist', 'mp3artists', 'mp3year', 'mp3bpm', 'initialkey', 'mp3encoder', 'mp3comment', 'compilation', 'composers', 'conductors', 'audiocodec', 'copyright', 'duration', 'releasedate',
				'mp3genre', 'mp3info', 'mp3mode', 'mp3disc', 'mp3track', 'picdepth', 'mp3encodingsoftware', 'mp3drm', 'publisher', 'mp3bitrate', 'mp3samplerate', 'mp3title', 'mp3album'),
			'grp:docs', DOpus.Create().StringSetI('author', 'category', 'comments', 'companyname', 'copyright', 'creator', 'doccreateddate', 'docedittime', 'doclastsavedby', 'doclastsaveddate', 'pages', 'producer', 'subject', 'title'),
			'grp:pictures', DOpus.Create().StringSetI('35mmfocallength', 'altitude', 'apertureval', 'cameramake', 'cameramodel', 'colormodel', 'contrast', 'coords', 'datedigitized', 'datetaken', 'datetimecreated', 'datetimeoriginal', 'digitalzoom',
				'exposurebias', 'flash', 'fnumber', 'imagequality', 'picwidth', 'picheight', 'picsize', 'mp3artist', 'copyright', 'shutterspeed', 'isospeed', 'whitebalance', 'scenecapturetype', 'latitude', 'longitude',
				'scenemode', 'macromode', 'picphysx', 'picphysy', 'lensmodel', 'lensmake', 'author', 'title', 'subject', 'rotation', 'saturation', 'sharpness', 'software', 'picres', 'exposureprogram', 'exposuretime',
				'focallength', 'imagedesc', 'instructions', 'meteringmode', 'picresx', 'picresy', 'subjectdistance', 'aspectratio', 'picdepth'),
			'grp:video', DOpus.Create().StringSetI('audiocount', 'subtitlecount', 'videocount', 'aspectratio', 'mp3bitrate', 'picdepth', 'channel', 'datarate', 'picheight', 'picphyssize', 'recordingtime', 'picwidth', 'mp3samplerate', 'fourcc',
				'framerate', 'ishd', 'mp3mode', 'isrepeat', 'station', 'videocodec', 'alllangs', 'audiolangs', 'hdrtypes', 'subtitlelangs', 'videolangs', 'picsize', 'mp3genre', 'mp3title', 'mp3artist', 'mp3year',
				'audiocodec', 'episodename', 'broadcastdate', 'duration', 'mp3drm', 'publisher', 'composers', 'conductors', 'director', 'releasedate', 'credits'),
			'grp:exe', DOpus.Create().StringSetI('moddesc', 'modversion', 'prodname', 'prodversion', 'copyright', 'companyname', 'signer'),
			'grp:paths', DOpus.Create().StringSetI('name', 'ext', 'extdir', 'fullpath', 'pathlen', 'path', 'parent', 'parentlocation', 'parentpath'));
		var props_enum = new Enumerator(props);
		var wild = FSU.NewWild();
		var grp;
		if (Script.vars.Exists(script_name + ':' + uid + ':include')) {
			var list = Script.vars.Get(script_name + ':' + uid + ':include');
			Log(1, 'include : ' + list.size);
			for (var i = list.length - 1; i >= 0; i--) {
				Log(1, 'include : ' + list(i));
				if (grp_regex.test(list(i))) {
					mpa('groups').insert(list(i));
					if (props.Exists(list(i))) mpa('include').merge(props(list(i)));
				}
				else {
					mpa('include_wild').push_back(FSU.NewWild(list(i)));
					if (wild.Parse(list(i))) {
						props_enum.moveFirst();
						for (; !props_enum.atEnd(); props_enum.moveNext()) {
							grp = props_enum.item();
							for (var j = props(grp).size - 1; j >= 0; j--) {
								if (wild.match(props(grp)(j))) {
									mpa('groups').insert(grp);
									mpa('include').insert(props(grp)(j));
								}
							}

						}
					}
				}
			}
		}
		if (mpa('include').empty)
			mpa('include').assign(['35mmfocallength', 'accesed', 'accesseddate', 'accessedtime', 'alllangs', 'altitude', 'apertureval', 'aspectratio', 'attr', 'audiocodec',
				'audiocount', 'audiolangs', 'author', 'availability', 'broadcastdate', 'cameramake', 'cameramodel', 'category', 'channel', 'colormodel', 'comments', 'companyname', 'compilation',
				'composers', 'conductors', 'contrast', 'coords', 'copyright', 'created', 'createddate', 'createdtime', 'creator', 'credits', 'datarate', 'datedigitized', 'datetaken', 'datetimecreated',
				'datetimeoriginal', 'desc', 'digitalzoom', 'dircount', 'dircounttotal', 'director', 'doccreateddate', 'docedittime', 'doclastsavedby', 'doclastsaveddate', 'duration', 'episodename',
				'exposurebias', 'exposureprogram', 'exposuretime', 'ext', 'extdir', 'filecount', 'filecounttotal', 'flash', 'fnumber', 'focallength', 'fourcc', 'framerate', 'fullpath', 'hdrtypes',
				'imagedesc', 'imagequality', 'initialkey', 'instructions', 'ishd', 'isospeed', 'isrepeat', 'keywords', 'label', 'latitude', 'lensmake', 'lensmodel', 'longitude', 'macromode',
				'meteringmode', 'moddesc', 'modified', 'modifieddate', 'modifiedtime', 'modversion', 'mp3album', 'mp3albumartist', 'mp3artist', 'mp3artists', 'mp3bitrate', 'mp3bpm', 'mp3comment',
				'mp3disc', 'mp3drm', 'mp3encoder', 'mp3encodingsoftware', 'mp3genre', 'mp3info', 'mp3mode', 'mp3samplerate', 'mp3title', 'mp3track', 'mp3year', 'name', 'owner', 'pages', 'parent',
				'parentlocation', 'parentpath', 'path', 'pathlen', 'perms', 'picdepth', 'picheight', 'picphyssize', 'picphysx', 'picphysy', 'picres', 'picresx', 'picresy', 'picsize', 'picwidth',
				'prodname', 'producer', 'prodversion', 'publisher', 'rating', 'recordingtime', 'releasedate', 'rotation', 'saturation', 'scenecapturetype', 'scenemode', 'sharpness', 'shutterspeed',
				'signer', 'size', 'sizeauto', 'sizekb', 'software', 'station', 'status', 'streamcount', 'subject', 'subjectdistance', 'subtitlecount', 'subtitlelangs', 'target', 'title', 'type',
				'userdesc', 'videocodec', 'videocount', 'videolangs', 'whitebalance']);
		if (mpa('groups').empty) mpa('groups').assign(['grp:general', 'grp:datetime', 'grp:filesize', 'grp:docs', 'grp:audio', 'grp:pictures', 'grp:video', 'grp:exe', 'grp:script', 'grp:evaluator', 'grp:shell']);

		if (Script.vars.Exists(script_name + ':' + uid + ':exclude')) {
			var list = Script.vars.Get(script_name + ':' + uid + ':exclude');
			Log(1, 'exclude : ' + list.size);
			for (var i = list.length - 1; i >= 0; i--) {
				Log(1, 'exclude : ' + list(i));
				if (grp_regex.test(list(i))) {
					mpa('groups').erase(list(i));
					if (props.Exists(list(i))) {
						for (var j = props(list(i)).size - 1; j >= 0; j--)
							mpa('include').remove(props(grp)(j));
					}
				}
				else {
					mpa('exclude_wild').push_back(FSU.NewWild(list(i)));
					if (wild.Parse(list(i))) {
						var j = 0;
						while (j < mpa('include').size) {
							if (wild.match(mpa('include')(j))) mpa('include').remove(props(grp)(j));
							else j++;
						}
					}
				}
			}
		}
		list = null;
		grp_regex = null;
		props = null;
		return mpa;
	}

}

function parseShellXml(item, varname, include_wild, exclude_wild) {
	var results = [''];
	try {
		var shell_cols_file = DOpus.Aliases('dopuslocaldata').path + '\\State Data\\shellcolumns.osd';
		var shell_props_file = DOpus.Aliases('dopusdata').path + '\\ConfigFiles\\shellprops.oxc';
		Log(1, 'BUILD => Shell     : Parsing Shell Properties columns xml file: ' + shell_cols_file);
		Log(1, 'BUILD => Shell     : Parsing Shell Properties columns xml file: ' + shell_props_file);
		if (!FSU.Exists(shell_cols_file) || !FSU.Exists(shell_props_file)) return results;

		var xmlDocShell = new ActiveXObject('Msxml2.DOMDocument');
		var xmlPropsShell = new ActiveXObject('Msxml2.DOMDocument');
		xmlDocShell.load(shell_cols_file);
		xmlPropsShell.load(shell_props_file);

		// Verify if the files have been loaded correctly
		if (xmlDocShell.parseError.errorCode == 0 || xmlPropsShell.parseError.errorCode == 0) {
			//Get all shellprops
			var shellprops = xmlPropsShell.selectNodes('//prefs/shellprops/prop');
			// var cmdline_vec = [];
			var cmdline = '';
			for (var i = 0; i < shellprops.length; i++) {
				try {
					var pkey = shellprops[i].getAttribute('pkey');
					pkey = pkey.split(',');
					shellcolumn = xmlDocShell.selectSingleNode("//shellcolumns/column[shell/scid/@fmtid='" + pkey[0] + "' and shell/scid/@pid='" + pkey[1] + "']");
					if (!shellcolumn) continue;
					var keyword = shellcolumn.getAttribute('key');
					var title = shellcolumn.selectSingleNode('shell/title').text;
					Log(1, 'BUILD => Shell     : Found "' + keyword + '" column');
					if (title && keyword) {
						if (!KeywordinLists(keyword, include_wild, exclude_wild)) continue;
						cmdline += '(IsSet("sh:' + keyword + '") ? Val("sh:' + keyword + '") as str : "<novalue>") + "#_#" + ';

						results.push({
							'name': title,
							'value': keyword
						});
					}
					else
						Log(1, 'BUILD => Shell     : "' + keyword + '" does not seems valid');
				}
				catch (err) {
					continue;
				}
			}
			shellprops = null;
			shellcolumn = null;
			if (cmdline) cmdline = '=$glob:' + varname + ' = ' + cmdline.slice(0, -11) + ';';
			results[0] = cmdline;
		}
		else
			Log(3, 'BUILD => Shell     : Error while parsing ' + shell_cols_file + ';' + shell_props_file);
		xmlDocShell = null;
		xmlPropsShell = null;
	}
	catch (err) {
		Log(3, 'BUILD => Shell     : Error while trying to get shell columns info : ' + err);
	};
	return results;
}

function parseEvaluatorXml(item, varname, include_wild, exclude_wild) {
	var results = [''];
	try {
		var ev_col_file = DOpus.Aliases('dopusdata').path + '\\ConfigFiles\\evalcols.oxc';
		Log(1, 'BUILD => Evaluator : Parsing Evaluator columns xml file:' + ev_col_file);
		if (!FSU.Exists(ev_col_file)) return results;
		var xmlDocEv = new ActiveXObject('Msxml2.DOMDocument');
		xmlDocEv.load(ev_col_file);
		if (xmlDocEv.parseError.errorCode == 0) {
			var evalcolumns = xmlDocEv.selectNodes('//prefs/evalcolumns/column');
			var cmdline = '';
			for (var i = 0; i < evalcolumns.length; i++) {
				// Get keyword, header and title
				var keyword = evalcolumns[i].getAttribute('keyword');
				var header = evalcolumns[i].getAttribute('header');
				var title = evalcolumns[i].getAttribute('title');
				Log(1, 'BUILD => Evaluator : Founded column "' + keyword + '"');

				if (title && keyword) {
					if (!KeywordinLists(keyword, include_wild, exclude_wild)) continue;
					cmdline += '(IsSet("eval:' + keyword + '") ? Val("eval:' + keyword + '") as str: "<novalue>") + "#_#" + ';

					results.push({
						'name': title,
						'header': (header) ? header : title,
						'value': keyword
					});
				}
				else
					Log(1, 'BUILD => Evaluator : "' + keyword + '" columns does not seems valid');
			}
			if (cmdline)
				cmdline = '=$glob:' + varname + ' = ' + cmdline.slice(0, -11) + ';';

			results[0] = cmdline;
		}
		else
			Log(3, 'BUILD => Evaluator : Error parsing Evaluator xml file');
		xmlDocEv = null;
	}
	catch (err) {
		Log(3, 'BUILD => Evaluator : Error while trying to get Evaluator columns info : ' + err);
	};
	return results;
}

function getSpecialsValues(item, varname, colsObj) {
	var results = [''];
	var cmdline = '';
	for (var key in colsObj) {
		try {
			cmdline += '(' + (key.indexOf(':') != -1 ? ('Val("' + key + '")') : key) + ' as str) + "#_#" + ';

			Log(1, 'BUILD => header:' + colsObj[key].header + '; group:' + colsObj[key].group + '; key:' + key);
			results.push({
				'name': colsObj[key].label ? colsObj[key].label : colsObj[key].header,
				'header': colsObj[key].header,
				'group': colsObj[key].group,
				'value': key
			});
		}
		catch (err) {
			Log(3, 'BUILD => Special  : Error while trying to get value for ' + key + ' : ' + err.description);
			continue;
		};
	}
	if (cmdline) {
		cmdline = '=$glob:' + varname + ' = ' + cmdline.slice(0, -11) + ';';

	}
	results[0] = cmdline;
	return results;
}

function parseScriptXmls(item, varname, include_wild, exclude_wild) {
	var results = [''];
	try {
		var scr_cols_file = DOpus.Aliases('dopusdata').path + '\\ConfigFiles\\scriptcolumns.oxc';
		var scr_addins_file = DOpus.Aliases('dopusdata').path + '\\ConfigFiles\\scriptaddins.oxc';
		var scr_prefs_file = DOpus.Aliases('dopuslocaldata').path + '\\State Data\\scriptprefs.osd';
		Log(1, 'BUILD => Script    : Parsing script columns xml file:' + scr_addins_file);
		Log(1, 'BUILD => Script    : Parsing script columns xml file:' + scr_cols_file);
		Log(1, 'BUILD => Script    : Parsing script columns xml file:' + scr_prefs_file);
		if (!FSU.Exists(scr_addins_file) || !FSU.Exists(scr_cols_file) || !FSU.Exists(scr_prefs_file)) return results;
		var xmlDocAddins = new ActiveXObject('Msxml2.DOMDocument');
		var xmlDocCols = new ActiveXObject('Msxml2.DOMDocument');
		var xmlDocPrefs = new ActiveXObject('Msxml2.DOMDocument');
		var scripts_path = DOpus.Aliases('scripts').path + '\\';
		var ignored_scripts = DOpus.Create().StringSetI(Script.config.ignored_script_columns);
		var labels, main, id, name, path, result, colNames;
		// Load XML content from files
		xmlDocAddins.load(scr_addins_file);
		xmlDocCols.load(scr_cols_file);
		xmlDocPrefs.load(scr_prefs_file);

		// Verify if the files have been loaded correctly
		if (xmlDocAddins.parseError.errorCode == 0 && xmlDocCols.parseError.errorCode == 0 && xmlDocPrefs.parseError.errorCode == 0) {
			var filter = DOpus.Create().Filter();
			var filters_map = {
				'datetime': ' on 2021-01-01 00:00:00',
				'date': ' on 2021-01-01',
				'time': ' on 00:00:00',
				'string': ' "test"',
				'double': ' = 0.0',
				'other': ' = 0',
			};
			var disabledScripts = xmlDocAddins.selectSingleNode('//scriptaddins/disabled');
			// Get all scripts columns in scriptprefs.osd 
			var colScripts = xmlDocPrefs.selectNodes('//scriptprefs/cache/script');
			var cmdline = '';
			for (var i = 0; i < colScripts.length; i++) {
				//Check if columns is empty
				labels = colScripts[i].getAttribute('columns');
				if (!labels) continue;
				main = colScripts[i].getAttribute('name');
				id = colScripts[i].getAttribute('id');
				//Check in scriptaddins.oxc if the script is disabled and exists
				if (!disabledScripts.selectSingleNode("script[@id='" + id + "']")) {
					// Get path value in addins.xml
					pathNode = xmlDocAddins.selectSingleNode("//scriptaddins/idmap/script[@id='" + id + "']");
					if (!pathNode) {
						Log(1, 'BUILD => Script    : "' + name + '" is not listed in scriptaddins.oxc');
						continue;
					}
					//Check if script file is listed as ignored
					if (ignored_scripts.Exists(path) || ignored_scripts.Exists(main)) {
						Log(1, 'BUILD => Script    : "' + path + '" is configured to be ignored');
						continue;
					}
					path = pathNode.getAttribute('path');
					//check if that script file exists
					if (!FSU.Exists(scripts_path + path)) {
						Log(1, 'BUILD => Script    : "' + scripts_path + path + '" does not exists in scripts folder');
						continue;
					}
					colNames = xmlDocCols.selectNodes("//scriptcolumns/col[@script='" + id.slice(1).slice(0, -1) + "']");
					for (var j = 0; j < colNames.length; j++) {
						name = colNames[j].getAttribute('name');
						if (main && name) {
							result = main + '/' + name;
							result = result.replace(/ /g, '');
							for (var key in filters_map) {
								if (filter.Set('script match ' + result + filters_map[key]) && KeywordinLists('scp:' + result, include_wild, exclude_wild)) {
									cmdline += '(Val("scp:' + result + '") as str) + "#_#" + ';

									results.push({
										'name': name,
										'value': result
									});
									break;
								}
							}

						}
					}

				}
				else
					Log(1, 'BUILD => Script    : "' + main + '" is listed as disabled');
			}
			if (cmdline) cmdline = '=$glob:' + varname + ' = ' + cmdline.slice(0, -11) + ';';

			results[0] = cmdline;
		}
		else
			Log(3, 'BUILD => Script    : Error while parsing script columns xml files');
		xmlDocAddins = null;
		xmlDocCols = null;
		xmlPrefsCols = null;
	}
	catch (err) {
		Log(3, 'BUILD => Script    : Error while trying to get script columns info : ' + err);
	};
	ignored_scripts = null;
	return results;
}

function KeywordinLists(keyword, include_list, exclude_list) {
	var s = include_list.empty;
	for (var i = include_list.length - 1; i >= 0; i--) {
		if (include_list(i).match(keyword)) {
			s = true;
			break;
		}
	}
	for (var i = exclude_list.length - 1; i >= 0; i--) {
		if (exclude_list(i).match(keyword)) {
			s = false;
			break;
		}
	}
	return s;
}

function compileSearch(input, search_flags) {
	Log(1, 'input:"' + input + '"; ' + input.length);
	var pattern = /(?:(\(*)\s*\$(n|l|h|v|t)\s*(==|!=)\s*"(.*?)(^|[^'])?"\s*(\)*))(?:\s*(AND|OR|&&|\|\|)\s*)?/gi;
	var result = [];
	var match;
	var currGroup = [];
	try {
		while ((match = pattern.exec(input)) !== null) {
			//	for(var i = 0; i< match.length;i++)DOpus.Output('match ' + i + ':'+match[i]);
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
			// Log(1, '=> parenOpen:' + parenOpen + '\n' +
			// 	'=> parenClose:' + parenClose + '\n' +
			// 	'=> key:' + key + '\n' +
			// 	'=> operator:' + operator + '\n' +
			// 	'=> value:' + value + '\n' +
			// 	'=> sep:' + sep);
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
			case 'n':
				field = 'item.name';
				break;
			case 'l':
				field = 'item.label';
				break;
			case 'h':
				field = 'item.header';
				break;
			case 'v':
				field = 'item.value';
				break;
			case 't':
				field = 'item.type';
				break;
			default:
				Log(3, '=> Error parsing input : Field can only be n, l, h, v or t');
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

function Log(level, text) {
	if (level === 4 || Script.config['log level'] < level) {
		if (level == 1) DOpus.Output('<#%vs_dragdrop_normal_action>DEBUG   => ' + text + '</#>');
		else if (level == 2) DOpus.Output('INFO    => ' + text);
		else if (level === 3) DOpus.Output('<#%vs_dragdrop_warning_action>WARNING => ' + text + '</#>');
		else DOpus.Output('ERROR   => ' + text, true);
	}
}

function alert(parent, message, level) {
	var dlg = DOpus.Dlg();
	if (parent) {
		dlg.window = parent;
		dlg.disable_window = parent;
	}
	dlg.message = message;
	dlg.buttons = '&OK';
	dlg.title = script_label + ' v' + script_version;
	if (level === 0) dlg.icon = 'info';
	else if (level === 1) dlg.icon = 'question';
	else if (level === 2) dlg.icon = 'warning';
	else dlg.icon = 'error';
	dlg.Show();
	dlg = null;
	return message;
}

String.prototype.trim = function() {
	return this.replace(/^[\s\uFEFF\xA0]+|[\s\n\uFEFF\xA0]+$/g, '');
};

==SCRIPT RESOURCES
<resources>
	<resource name="main" type="dialog">
		<dialog dragdrop="yes" height="278" lang="english" maximize="yes" minimize="yes" resize="yes" width="356">
			<control halign="left" height="4" name="progress_bar" resize="y" type="static" valign="center" width="0" x="6" y="270" />
			<control halign="left" height="8" name="progress_track" resize="y" type="static" valign="center" width="122" x="4" y="268" />
			<control height="10" name="abort_btn" resize="y" title="Abort" type="button" width="34" x="72" y="254" />
			<control height="10" name="skip_btn" resize="y" title="Skip" type="button" width="34" x="108" y="254" />
			<control enable="no" halign="left" height="12" name="query_edit" resize="w" tip="Filter entries" type="edit" width="164" x="90" y="4" />
			<control enable="no" fullrow="yes" height="229" multisel="yes" name="listview" resize="wh" sort="yes" type="listview" viewmode="details" width="349" x="3" y="36">
				<columns>
					<item text="Name" />
					<item text="Label" />
					<item text="Header" />
					<item text="Type" />
					<item text="Value" />
				</columns>
			</control>
			<control height="12" name="clear_btn" resize="x" title="❌" type="button" width="12" x="254" y="4" />
			<control enable="no" height="12" name="categories" type="combo" width="102" x="54" y="20" />
			<control enable="no" height="12" name="copy_btn" resize="x" title="Copy Selection" type="button" visible="no" width="77" x="274" y="20" />
			<control changelinkcolor="no" halign="left" height="8" name="case_btn" resize="x" title="&lt;a id=&quot;case&quot;&gt;Aa&lt;/a&gt;" type="markuptext" width="10" x="310" y="6" />
			<control changelinkcolor="no" halign="left" height="8" name="diac_btn" resize="x" title="&lt;a id=&quot;link&quot;&gt;ůü&lt;/a&gt;" type="markuptext" width="10" x="324" y="6" />
			<control changelinkcolor="no" halign="left" height="8" name="ww_btn" resize="x" title="&lt;a id=&quot;link&quot;&gt;ww&lt;/a&gt;" type="markuptext" width="10" x="338" y="6" />
			<control changelinkcolor="no" halign="left" height="8" name="regex_btn" resize="x" title="&lt;a id=&quot;regex&quot;&gt;•✱&lt;/a&gt;" type="markuptext" width="10" x="296" y="6" />
			<control changelinkcolor="no" halign="left" height="8" name="refresh_btn" resize="x" title="&lt;a id=&quot;refresh&quot;&gt;&lt;%oned:4&gt;&lt;/a&gt;" type="markuptext" width="10" x="270" y="6" />
			<control changelinkcolor="no" enable="no" halign="left" height="8" name="search_btn" title="&lt;%ddbi:25&gt;&lt;a id=&quot;search_opt&quot;&gt;&lt;%ddbi:6&gt;&lt;/a&gt;" type="markuptext" width="14" x="70" y="6" />
			<control ellipsis="end" halign="right" height="8" name="status_bar" resize="yws" type="static" valign="center" width="202" x="134" y="268" />
			<control changelinkcolor="no" enable="no" halign="left" height="8" name="total_info" resize="w" type="markuptext" width="110" x="160" y="22" />
			<control border="no" height="2" image="#empty" name="tooltip_btn" resize="x" type="button" width="66" x="282" y="14" />
			<control changelinkcolor="no" halign="left" height="8" name="wild_btn" resize="x" title="&lt;a id=&quot;wild&quot;&gt;✱&lt;/a&gt;" type="markuptext" width="8" x="284" y="6" />
			<control enable="no" height="12" image="#powermode" name="adv_btn" title="Advanced Search" type="button" width="15" x="54" y="4" />
			<control halign="left" height="8" name="search_mode" resize="yws" type="markuptext" width="124" x="4" y="268" />
			<control halign="left" height="30" image="yes" name="thumbnail" type="static" valign="top" width="44" x="6" y="4" />
		</dialog>
	</resource>
	<resource type="strings">
		<strings lang="english">
			<string id="debug">Logging level to be displayed. OFF to show only errors. 
DEBUG to show all messages. 
STANDARD to show only the most relevant information.
WARNING to show messages that needs your attention.</string>
			<string id="ignored_script_columns">List of Script Names (as seen in the &quot;Names&quot; column in Script Management) that you do not wish to use with this command.
All columns for those scripts are going to be ignored.</string>
			<string id="label_adv_search">Advanced Search enabled</string>
			<string id="label_diac_enabled">Diacritics allowed</string>
			<string id="label_msg_dlg">Search in :</string>
			<string id="label_regex_enabled">Regular expressions</string>
			<string id="label_search_header">&amp;Header</string>
			<string id="label_search_label">&amp;Label</string>
			<string id="label_search_name">&amp;Name</string>
			<string id="label_search_value">&amp;Value</string>
			<string id="label_sug_filter">Search by</string>
			<string id="label_sug_filter_adv">Use $(n|l|h|v|t)(==|!=)&quot;value&quot;. OR|AND and basic logical combinations are supported</string>
			<string id="label_use_DO_wildcards">Use DOpus &amp;pattern matching syntax </string>
			<string id="msg_copy">Values copied to clipboard!</string>
			<string id="msg_no_file">No file provided or file is invalid!</string>
			<string id="size_mode">Choose how the dialog window will resize when open.
AUTO : Auto resize based on screen resolution and list dimensions.
LAST USED: Remember the last window size and position.</string>
		</strings>
		<strings lang="esm">
			<string id="debug">Nivel de registro a mostrar. OFF para mostrar solo errores. 
		DEBUG para mostrar todos los mensajes. 
		STANDARD to show only the most relevant information.
 para mostrar solo la información más relevante. 
 WARNING para mostrar mensajes que requieren tu atención.</string>
			<string id="ignored_script_columns">Lista de nombres de scripts (como se ve en la columna &quot;Nombres&quot; en la gestión de scripts) que no deseas usar con este comando. Todas las columnas para esos scripts serán ignoradas.</string>
			<string id="label_adv_search">Búsqueda avanzada activada</string>
			<string id="label_diac_enabled">Diacríticos permitidos</string>
			<string id="label_msg_dlg">Buscar en:</string>
			<string id="label_regex_enabled">Expresiones regulares</string>
			<string id="label_search_header">&amp;Encabezado</string>
			<string id="label_search_label">E&amp;tiqueta</string>
			<string id="label_search_name">&amp;Nombre</string>
			<string id="label_search_value">&amp;Valor</string>
			<string id="label_sug_filter">Buscar por</string>
			<string id="label_sug_filter_adv">Usar $(n|l|h|v|t)(==|!=)&quot;valor&quot;. Se admiten OR|AND y combinaciones lógicas básicas.</string>
			<string id="label_use_DO_wildcards">Usar la sintaxis de coincidencias &amp;DOpus</string>
			<string id="msg_copy">¡Valores copiados al portapapeles!</string>
			<string id="msg_no_file">¡No se proporcionó un archivo o el archivo es inválido!</string>
			<string id="size_mode">Elige cómo se redimensionará la ventana de diálogo al abrirse. 
    AUTO: Redimensionamiento automático según la resolución de la pantalla y las dimensiones de la lista. 
    LAST USED: Recordar el último tamaño y posición de la ventana.</string>
		</strings>
	</resource>
	<resource name="search_in_flags" type="dialog">
		<dialog height="92" lang="english" standard_buttons="ok" width="222">
			<control height="10" name="check0" type="check" width="160" x="22" y="14" />
			<control height="10" name="check1" type="check" width="160" x="22" y="29" />
			<control height="10" name="check2" type="check" width="160" x="22" y="44" />
			<control height="10" name="check3" type="check" width="160" x="22" y="59" />
			<control halign="left" height="8" name="static1" title="Search in :" type="static" valign="top" width="166" x="6" y="4" />
		</dialog>
	</resource>
</resources>
