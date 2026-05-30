/* Meta Wizard Command for Directory Opus
**An intuitive and efficient metadata search and replace**
Meta Wizard © 2024 by Christian Arellano García is licensed under CC BY-NC-ND 4.0 
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
*/

var script_name = 'Meta Wizard';
var script_version = '1.2.0';

// Called by Directory Opus to initialize the script
function OnInit(initData) {
	initData.name = script_name;
	initData.version = script_version;
	initData.copyright = '(c) 2024 Christian Arellano García';
	initData.url = 'https://resource.dopus.com/t/do-meta-wizard-command/48578';
	initData.desc = 'An intuitive and efficient metadata search and replace tool';
	initData.default_enable = true;
	initData.min_version = '13.7.1';
	initData.config_desc = DOpus.Create.Map();
	initData.config_groups = DOpus.Create.Map();
	AddConfig('log level', DOpus.Create.Vector(2, 'DEBUG', 'STANDARD', 'WARNING', 'OFF'), DOpus.strings.Get('debug'), '4. Script Logs');
	AddConfig('Logs Path', '', DOpus.strings.Get('logs_path'), '4. Script Logs');
	AddConfig('After actions', DOpus.Create.Vector(1, 'Nothing', 'Open Logs Path', 'Open Logs Files'), DOpus.strings.Get('after_actions'), '1. General');
	AddConfig('Doc Exts', DOpus.Create.Vector('.doc', '.docx', '.pdf'), DOpus.strings.Get('doc_exts'), '1. General');
	AddConfig('Dialog Mode', DOpus.Create.Vector(2, 'Preview List on Right', 'Preview List on Bottom', 'Preview List on Left'), DOpus.strings.Get('dialog_mode'), '2. Dialog');
	AddConfig('Preview List max column width', 350, DOpus.strings.Get('max_col_width'), '2. Dialog');
	AddConfig('Properties List max column width', 500, DOpus.strings.Get('fp_max_col_width'), '2. Dialog');
	AddConfig('Find', 'alt+f', 'Hotkey for Find Edit Control', '3. Hotkeys');
	AddConfig('Replace', 'alt+r', 'Hotkey for Replace Edit Control', '3. Hotkeys');
	AddConfig('Metadata Filter', 'F3', 'Hotkey for Metadata List filter', '3. Hotkeys');
	AddConfig('Properties Sheet Filter', 'shift+F3', 'Hotkey for Properties Sheet List filter', '3. Hotkeys');
	AddConfig('Case sensitive checkbox', 'alt+c', 'Hotkey for Case sensitive checkbox', '3. Hotkeys');
	AddConfig('Regular expressions checkbox', 'alt+x', 'Hotkey for Regular expressions checkbox', '3. Hotkeys');
	AddConfig('Whole words checkbox', 'alt+w', 'Hotkey for Whole words checkbox', '3. Hotkeys');
	AddConfig('Ignore diacritics checkbox', 'alt+d', 'Hotkey for Ignore diacritics checkbox', '3. Hotkeys');
	AddConfig('Global replace checkbox', 'alt+g', 'Hotkey for Global replace checkbox', '3. Hotkeys');
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
	cmd.name = 'MetaWizard';
	cmd.method = 'OnMetaWizard';
	cmd.desc = 'An intuitive and efficient metadata search and replace tool';
	cmd.label = script_name;
	cmd.template = 'FILES/K/M,NOGUI/S,NOGLOBAL/S,NOEMPTY/S,CASE/S,NODIACRITICS/S,REGEX/S,WHOLEWORDS/S,REPLACE/K,FIND/K,MULTI/S,PROPERTIES/K[tags,rating,usercomment,35mmfocallength,album,albumartist,aperture,artist,author,bpm,cameramake,cameramodel,category,comment,company,composers,conductor,copyright,creator,datedigitized,datetaken,datetimecreated,datetimeoriginal,digitalzoom,directors,discnumber,encoder,encodingsoftware,exposurebias,exposuretime,fnumber,focallength,genre,gpsaltitude,gpslatitude,gpslongitude,imagedesc,initialkey,instructions,isospeed,lastsavedby,lensmake,lensmodel,orientation,producer,producers,publisher,releasedate,shutterspeed,software,subject,subjectdistance,title,track,year]';
	cmd.hide = false;
	cmd.icon = 'find';
}

var g_map, labels, str_tools, files, main_menu_map, translated_str, ini_timer, new_values_map, regexps, str_labels;

function OnMetaWizard(scriptCmdData) {
	DOpus.ClearOutput();

	// ======= GET FILES ARG ==========================================
	Log(2, '======= ' + script_name + ' v' + script_version + ' =====================================');
	Log(2, 'cmdline   : ' + scriptCmdData.cmdline);
	var tab = scriptCmdData.func.sourcetab;
	var args = scriptCmdData.func.argsmap; //arguments used for the command
	files = (args.Exists('FILES')) ? DOpus.Create.Vector(args('FILES')) : ((tab && tab.selected_files.count > 0) ? DOpus.Create.Vector(tab.selected_files) : false);
	if (DOpus.TypeOf(files) != 'object.Vector') return; //files must be a vector to continue
	Log(2, 'files     : ' + files.count);

	// ======= INIT VARS AND PROGRESS DIALOG ==========================================
	ini_timer = new Date(); //time controller var
	var FSU = DOpus.FSUtil();
	g_map = DOpus.Create.Vector(); //main map for metadata values for all files
	var aborted = false; //var to control Abort button in progress dialog
	labels = DOpus.Create.Map(); //map with all data for editable metadata available(label, keyword)
	str_tools = DOpus.Create.StringTools();
	translated_str = DOpus.Create.Map(); //map with useful translated strings
	translated_str('general') = str_tools.LanguageStr(5029);
	translated_str('audio') = str_tools.LanguageStr(5031);
	translated_str('doc') = str_tools.LanguageStr(5033);
	translated_str('image') = str_tools.LanguageStr(5040);
	translated_str('video') = str_tools.LanguageStr(5083);
	translated_str('exe') = str_tools.LanguageStr(5098);
	translated_str('other') = str_tools.LanguageStr(5086);
	translated_str('files') = str_tools.LanguageStr(88);
	translated_str('sizekb') = str_tools.LanguageStr(1542);
	translated_str('set_metadata') = str_tools.LanguageStr(6306);
	var sp = BuildMetaMap(); //build labels map for later info
	var noempty = args.Exists('NOEMPTY');
	var doc_exts = DOpus.Create.StringSetI(Script.config['Doc Exts']);
	var cmd = DOpus.Create.Command(); //used for running actual commands
	var cmdHelper = scriptCmdData.func.command; //used for progress dlg
	cmdHelper.deselect = false;
	cmdHelper.ClearFiles();
	var progress = cmdHelper.progress;
	progress.abort = true;
	progress.owned = true;
	progress.bytes = false;
	progress.pause = true;
	progress.full = false;
	progress.delay = false;
	progress.skip = false;
	progress.Init(tab ? tab.lister : '', script_name + ' v' + script_version);
	progress.SetFiles(files.count);
	progress.Show();
	progress.SetStatus(DOpus.strings.Get('metadata_reading'));
	// ======= VALIDATE FILES AND GET METADATA ==========================================	
	var i = 0;
	while (i < files.count) { //validate files
		if (progress.GetAbortState(true) === 'a') {
			aborted = true;
			break;
		}
		progress.SetName(files(i));
		if (!files(i) || !FSU.Exists(files(i))) { //check if file exists
			Log(3, files(i) + "doesn't exist");
			files.erase(i);
			continue;
		}
		if (DOpus.TypeOf(files(i)) != 'object.Item') { //make sure file is an Item object
			Log(1, 'Converting ' + files(i) + ' to item object');
			files(i) = FSU.GetItem(files(i));
		}
		if (files(i).is_dir || files(i).size == 0 || files(i).metadata + '' == 'none') { //skip the file if no metadata or is a dir
			Log(3, files(i) + ' can\'t be used with this command');
			files.erase(i);
			progress.SkipFile(false);
			continue;
		}

		if (getMetadata(files(i), noempty, (files(i).metadata + '' === 'doc' && !doc_exts.Exists(files(i).ext)) ? true : noempty, sp) === false) {
			files.erase(i);
			progress.SkipFile(false);
			continue;
		}
		i++;
		progress.StepFiles(1);
	}
	doc_exts = null;
	if (!labels.empty && !aborted && !files.empty) { //zero labels means no metadata to use
		BuildMainMenu(); //build menus for keywords

		// ======= INIT MORE VARS  ==========================================

		args('PROPERTIES') = args.Exists('PROPERTIES') ? args('PROPERTIES').toLowerCase().split(',') : []; //Convert PROPERTIES into a array
		regexps = DOpus.Create.Map(); //map with several regexp values
		regexps('year') = /^\d{4}$/; //value is 4 digits
		regexps('sd') = /^(\d*\.?\d+)(mm|m)?$/i; // valid for subjectdistance
		regexps('disc_track') = /^(\d+|\d+\/\d+)$/; // valid for disc/track number
		regexps('multi') = /^(author|tags|category|artist|composers|conductor|directors|producers)$/i; // match for multi values properties
		regexps('checkbox') = /.*_cbx/; // match for multi values properties
		var sel_labels = DOpus.Create.StringSetI(); //Set for labels currently selected
		var find, replace, helper_str;
		var checked_files = files.count;
		var tasks = 0;
		var ch_flags = DOpus.Create.Map('case', args.Exists('CASE') ? true : false,
			'regex', args.Exists('REGEX') ? true : false,
			'ww', args.Exists('WHOLEWORDS') ? true : false,
			'diac', args.Exists('NODIACRITICS') ? true : false,
			'global', args.Exists('NOGLOBAL') ? false : true);
		// ======= NOGUI is used  ==========================================	
		if (args.Exists('NOGUI')) {
			Log(2, 'NOGUI mode in use....');
			var change;
			if (args.Exists('FIND') && args.Exists('REPLACE')) {
				for (var i = 0; i < args('PROPERTIES').length; i++) {
					for (var e = new Enumerator(labels); !e.atEnd(); e.moveNext()) {
						value = e.item();
						if (args('PROPERTIES')[i] == labels(value)('keyword')) {
							sel_labels.insert(value);
							break;
						}
					}
				}

				if (args.Exists('MULTI')) {
					function parseFindReplace(value) {
						value = value.split(',');
						var i = 0;
						while (i < value.length) {
							if (i + 1 < value.length && value[i].slice(-1) === "'") {
								value[i] = value[i].slice(0, -1) + ',' + value[i + 1];
								value.splice(i + 1, 1);
							}
							else i++;
						}
						return value;
					}
					find = parseFindReplace(args('FIND'));
					replace = parseFindReplace(args('REPLACE'));
					if (find.length > replace.length) {
						for (var i = replace.length; i < find.length; i++) {
							replace[i] = replace[replace.length - 1];
						}
					}
				}
				else {
					find = [args('FIND')];
					replace = [args('REPLACE')];
				}
				if (find.length === replace.length) {
					for (var f = 0; f < find.length; f++) {
						change = false;
						Log(2, 'Using find => ' + find[f] + '   replace => ' + replace[f]);
						if (CheckFields(find[f], replace[f], sel_labels.count, checked_files, false)) {
							var pattern = ParsePattern(ch_flags, find[f]);
							if (pattern) {
								for (var i = 0; i < files.length; i++) {
									for (var j = 0; j < sel_labels.length; j++) {
										if (!g_map(i).Exists(sel_labels(j)) || DOpus.TypeOf(g_map(i)(sel_labels(j))) != 'object.Map') continue;
										if (GetNewValue(pattern, replace[f], i, sel_labels(j), true)) change = true;
									}
								}
								if (change) tasks++;
							}
						}
					}
				}
			}
		}
		else {
			Log(1, 'Creating GUI...');
			// ======= BUILDING THE DIALOG ==========================================
			translated_str('metadata') = str_tools.LanguageStr(5653);
			translated_str('empty') = ' ' + str_tools.LanguageStr(28266).replace('&', '');
			translated_str('selected') = str_tools.LanguageStr(28402).replace(/:|&/g, '').toLowerCase();
			translated_str('items') = ' ' + str_tools.LanguageStr(28483).toLowerCase();
			translated_str('replace') = str_tools.LanguageStr(6109).replace('&', '');
			translated_str('find') = str_tools.LanguageStr(5576);
			translated_str('filetype') = str_tools.LanguageStr(9419);
			translated_str('options_str') = str_tools.LanguageStr(1097);
			translated_str('hidden_str') = str_tools.LanguageStr(1308).slice(6).toLowerCase();
			translated_str('results_str') = str_tools.LanguageStr(25055).replace('%ld ', '');
			translated_str('current_tasks') = DOpus.strings.Get('current_tasks');
			translated_str('close_msg') = DOpus.strings.Get('close_msg');
			translated_str('undo_msg') = DOpus.strings.Get('undo_msg');
			translated_str('yes_str') = str_tools.LanguageStr(5639);
			translated_str('no_str') = str_tools.LanguageStr(5640);

			new_values_map = DOpus.Create.Map(); //map wit all the calculated values for all files
			var dlg = DOpus.Dlg();
			dlg.title = script_name + ' v' + script_version + ' - ' + files.count + ' ' + ((files.count == 1) ? str_tools.LanguageStr(2102).toLowerCase() : translated_str('files').toLowerCase());
			dlg.template = Script.config['Dialog Mode'];
			dlg.icon = DOpus.LoadImage(FSU.Resolve('/home\\dopus.exe') + ',1');
			dlg.detach = true;
			dlg.want_close = true;
			dlg.Create();

			//Naming controls for quick reference
			var find_edit = dlg.Control('find_edit');
			var replace_edit = dlg.Control('replace_edit');
			var filter_metalist_edit = dlg.Control('filter_metalist_edit');
			var metadata_list_status = dlg.Control('metadata_list_status');
			var metadata_list = dlg.Control('metadata_list'); //list is the main listview which contain the metadata labels for selection
			var files_list = dlg.Control('files_list'); //files_list is the listview which contain the filenames
			var filter_filepropslist_edit = dlg.Control('filter_filepropslist_edit');
			var fileprops_list_status = dlg.Control('fileprops_list_status');
			var fileprops_static = dlg.Control('fileprops_static');
			var fileprops_list = dlg.Control('fileprops_list');
			var undo_btn = dlg.Control('undo_btn');
			var add_task_btn = dlg.Control('add_task_btn');
			var apply_btn = dlg.Control('apply_btn');
			var status_title = dlg.Control('status_title');
			var tasks_number_title = dlg.Control('tasks_number_title');
			var case_cbx = dlg.Control('case_cbx');
			var regex_cbx = dlg.Control('regex_cbx');
			var ww_cbx = dlg.Control('ww_cbx');
			var diac_cbx = dlg.Control('diac_cbx');
			var global_cbx = dlg.Control('global_cbx');

			case_cbx.value = ch_flags('case');
			regex_cbx.value = ch_flags('regex');
			ww_cbx.value = ch_flags('ww');
			diac_cbx.value = ch_flags('diac');
			global_cbx.value = ch_flags('global'); //checkbox for global replacement

			if (args.Exists('FIND')) find_edit.value = args('FIND');
			if (args.Exists('REPLACE')) replace_edit.value = args('REPLACE');
			//Create 2 groups in metadata_list, Checked and Metadata
			metadata_list.AddGroup(str_tools.LanguageStr(8778), 1);
			metadata_list.AddGroup(translated_str('metadata'), 0);
			metadata_list.EnableGroupView(true);

			//Fill metadata_list with the obtained labels
			for (var e = new Enumerator(labels); !e.atEnd(); e.moveNext()) {
				label = e.item();
				fila = metadata_list.GetItemAt(metadata_list.AddItem('', i + 1, 0));
				fila.subitems(0) = label;
				fila.subitems(1) = labels(label)('type');
				//check if the keyword was provided
				if (!args.Exists('PROPERTIES')) continue;
				for (var i = 0; i < args('PROPERTIES').length; i++) {
					if (args('PROPERTIES')[i] == labels(label)('keyword')) {
						fila.checked = 1;
						fila.group = 1;
						sel_labels.insert(label);
						break;
					}
				}
				new_values_map(label) = DOpus.Create.Map();
			}
			metadata_list.columns.AutoSize();

			//fileprops_list is the listview which contain all the metadata for the specified file
			fileprops_list.columns.SetDisplayOrder(1, 0, 2);
			//Create 2 groups in fileprops_list, Non-editable Properties-1 and Extended Properties-0
			fileprops_list.AddGroup(str_tools.LanguageStr(5660), 1);
			fileprops_list.AddGroup(str_tools.LanguageStr(6411), 2);
			fileprops_list.EnableGroupView(true);

			// ======= GET TRANSLATED STRINGS FOR UI ==========================================
			metadata_list_status.label = labels.count + ' ' + translated_str('items') + '\n' + sel_labels.count + ' / ' + metadata_list.count + ' ' + translated_str('selected');
			dlg.Control('replace_title').label = '<b><#%vs_listview_header_text>' + translated_str('replace') + '</#></b>';
			dlg.Control('find_title').label = '<b><#%vs_listview_header_text>' + translated_str('find') + '</#></b>';
			dlg.Control('fileprops_list_title').label = '<b><#%vs_listview_header_text>' + str_tools.LanguageStr(29005) + ' : </#></b>';
			dlg.Control('preview_title').label = '<b><#%vs_listview_header_text>' + str_tools.LanguageStr(29004) + ' : </#></b>';
			dlg.Control('metadata_list_title').label = '<b><#%vs_listview_header_text>' + translated_str('metadata') + ' : </#></b>';
			add_task_btn.label = str_tools.LanguageStr(5743);
			apply_btn.label = str_tools.LanguageStr(5654);
			undo_btn.label = str_tools.LanguageStr(5958);
			dlg.Control('invert_btn').label = str_tools.LanguageStr(23136);
			dlg.Control('selnone_btn').label = str_tools.LanguageStr(23135);
			dlg.Control('selall_btn').label = str_tools.LanguageStr(23134);
			filter_metalist_edit.cuetext = str_tools.LanguageStr(6384);
			filter_filepropslist_edit.cuetext = str_tools.LanguageStr(9402);
			//case,whole words, regex, ignore diacritics
			var search_menu = DOpus.Create.Vector(str_tools.LanguageStr(9341), str_tools.LanguageStr(9340), str_tools.LanguageStr(6086), str_tools.LanguageStr(29499));

			case_cbx.label = search_menu(0);
			ww_cbx.label = search_menu(1);
			regex_cbx.label = search_menu(2);
			diac_cbx.label = search_menu(3);
			global_cbx.label = str_tools.LanguageStr(2340).replace('&', '');

			translated_str('find') = translated_str('find').slice(0, -1);
			translated_str('replace') = translated_str('replace').slice(0, -1);
			metadata_list.columns.GetColumnAt(1).name = str_tools.LanguageStr(6500); //Name
			metadata_list.columns.GetColumnAt(2).name = translated_str('filetype'); //FileType Group
			fileprops_list.columns.GetColumnAt(0).name = str_tools.LanguageStr(6501); //Value col
			fileprops_list.columns.GetColumnAt(1).name = str_tools.LanguageStr(24249); //Property col
			fileprops_list.columns.GetColumnAt(2).name = str_tools.LanguageStr(2094); //New col
			files_list.columns.GetColumnAt(0).name = str_tools.LanguageStr(29065); //File Name col
			// ======= SET HOTKEYS ==========================================
			setHotkeys();
			// ======= FINAL ARRANGEMENTS ==========================================
			var max_col_width = Script.config['Preview List max column width'];
			var fp_max_col_width = Script.config['Properties List max column width'];
			var last_used_props = DOpus.Create.Map(); //map for last properties changed. Used when undoing
			var sel_file = -1; //index for selected row in files_list
			var search_flags = (Script.Vars.Exists('search_flags')) ? Script.Vars.Get('search_flags') : 0;
			var first = true; //bool to build files_list on the first run only
			var build_files_list_cols = true; //bool to control update type for files_list
			var item_targeted = false; //bool to control update item only in files_list
			var fg_color = DOpus.Create.SysInfo.DarkMode ? '#00ff00' : '#2d792b';
			dlg.Control('search_icon').label = '#0:find';
			dlg.Control('search2_icon').label = '#0:find';
			metadata_list_status.style = 'b';
			fileprops_list_status.style = 'b';

			dlg.LoadPosition('MetaWizard');
			progress.SetFilesProgress(files.count);
			progress.Hide();
			tasks_number_title.label = '<b>' + translated_str('current_tasks') + ' : </b> ' + tasks;
			dlg.SetTaskbarGroup('MetaWizard');
			dlg.Show();
			find_edit.focus = true;
			Log(1, 'DONE!!! ' + (new Date() - ini_timer) + ' ms');

			while (true) {
				var msg = dlg.GetMsg();
				if (!msg.result) break;

				switch (msg.event) {
					case 'editchange':
						switch (msg.control) {
							case 'find_edit': //find/replace fields was edited
							case 'replace_edit':
								setTimerFilesList(false, false);
								break;
							case 'filter_metalist_edit': //metadata filter was edited
								dlg.SetTimer((filter_metalist_edit.value) ? 250 : 50, 'update_metadata_list_timer');
								break;
							case 'filter_filepropslist_edit': //fileprops filter was edited
								dlg.SetTimer((filter_filepropslist_edit.value) ? 250 : 50, 'update_fileprops_list_timer');
								break;
						}
						break;
					case 'click':
						switch (msg.control) {
							case 'keywords_btn': // keywords button was pressed
								if (showMainMenu(dlg) == true) setTimerFilesList(false, false);
								replace_edit.focus = true;
								break;
							case 'selall_btn': //Select All button was pressed
								checkMetadataList(1, true);
								setTimerFilesList(true, false);
								break;
							case 'selnone_btn': //Select None button was pressed
								checkMetadataList(0, true);
								setTimerFilesList(true, false);
								break;
							case 'invert_btn': //Invert Selection button was pressed
								checkMetadataList(-1, true);
								setTimerFilesList(true, false);
								break;
							case 'clear_ml_filter_btn': //Clear Filter button for metadata was pressed
								filter_metalist_edit.value = '';
								break;
							case 'clear_fpl_filter_btn': //Clear Filter for fileprops_list button was pressed
								if (filter_filepropslist_edit.value) filter_filepropslist_edit.value = '';
								break;
							case 'glyph_fpfilter_btn': //Glyph button for filter fileprops_list was pressed
								var dlgMenu = DOpus.Dlg();
								dlgMenu.title = script_name;
								dlgMenu.message = translated_str('options_str') + ':';
								dlgMenu.choices = search_menu;
								dlgMenu.list = DOpus.Create.Vector(search_flags & (1 << 0), search_flags & (1 << 1), search_flags & (1 << 2), search_flags & (1 << 3));
								dlgMenu.window = dlg;
								dlgMenu.disable_window = dlg;
								dlgMenu.Show();

								if (dlgMenu.result) {
									search_flags = setSearchFlags(dlgMenu.list);
									FilterFilePropsList();
								}
								break;
							case 'add_task_btn': //add/OK button was pressed
								if (setNewValues()) {
									tasks++;
									tasks_number_title.label = '<b>' + translated_str('current_tasks') + ' : </b> ' + tasks;
									undo_btn.enabled = true;
								}
								resetFields();
								break;
							case 'undo_btn': //Undo button was pressed
								if (UndoValues()) {
									tasks--;
									last_used_props.Clear();
									undo_btn.enabled = false;
									tasks_number_title.label = '<b>' + translated_str('current_tasks') + ' : </b> ' + tasks;
									apply_btn.enabled = CheckFields(find_edit.value, replace_edit.value, sel_labels.count, checked_files, true);
								}
								break;
							case 'apply_btn': //Apply button was pressed
								if (translated_str('status') == '' && setNewValues()) tasks++;
								dlg.EndDlg(1);
								break;
							default: //A checkbox for some search flag was checked/unchecked
								if (regexps('checkbox').test(msg.control)) {
									ch_flags(msg.control.slice(0, -4)) = msg.data;
									setTimerFilesList(false, false);
								}
								break;
						}
						break;
					case 'dblclk':
						//Double clicking an item in metadata list
						if (msg.control == 'metadata_list' && metadata_list.focus && metadata_list.value.index > -1) {
							checkMetadataList(-1, false);
							setTimerFilesList(true, false);
						}
						break;
					case 'selchange':
						//User select a file from files list
						if (msg.control == 'files_list' && files_list.value.index != sel_file) {
							if (files_list.value.index > -1) {
								sel_file = files_list.value.index;
								filter_filepropslist_edit.value = '';
							}
						}
						break;
					case 'checked':
						switch (msg.control) {
							case 'metadata_list': //Some item in metadata_list was checked/unchecked
								checkMetadataList(2, true);
								setTimerFilesList(true, false);
								break;

							case 'files_list': //Some item in files list was checked/unchecked
								if (msg.index >= 0)
									g_map(msg.index)('is_checked') = msg.checked != 1 ? false : true;
								checked_files = 0;
								for (var i = 0; i < files_list.count; i++) {
									fila = files_list.GetItemAt(i);
									if (fila.checked == 1) checked_files++;
								}
								setTimerFilesList(false, msg.index);
								break;
						}
						break;
					case 'timer':
						switch (msg.control) {
							case 'update_files_list_timer': //A timer for the files_list 
								dlg.KillTimer('update_files_list_timer');
								updateFilesList(first ? true : build_files_list_cols, item_targeted);
								if (sel_file == -1) files_list.value = 0;
								else dlg.SetTimer(250, 'update_fileprops_list_timer');
								break;
							case 'update_metadata_list_timer': //A timer for the filter metadata_list field 
								dlg.KillTimer('update_metadata_list_timer');
								metadata_list_status.label = FilterMetadataList(filter_metalist_edit.value, metadata_list, sel_labels);
								break;
							case 'update_fileprops_list_timer': //A timer for the filter fileprops_list field 
								dlg.KillTimer('update_fileprops_list_timer');
								FilterFilePropsList();
								break;
						}
						break;
					case 'hotkey':
						switch (msg.control) {
							case 'find_edit_hotkey': //A timer for the files_list 
								find_edit.focus = true;
								break;
							case 'replace_edit_hotkey': //A timer for the filter metadata_list field 
								replace_edit.focus = true;
								break;
							case 'filter_filepropslist_hotkey': //A timer for the filter fileprops_list field 
								filter_filepropslist_edit.focus = true;
								break;
							case 'filter_metalist_hotkey': //A timer for the filter fileprops_list field 
								filter_metalist_edit.focus = true;
								break;
							case 'case_sensitive_hotkey':
								case_cbx.value = !case_cbx.value;
								break;
							case 'regular_expressions_hotkey':
								regex_cbx.value = !regex_cbx.value;
								break;
							case 'whole_words_hotkey':
								ww_cbx.value = !ww_cbx.value;
								break;
							case 'ignore_diacritics_hotkey':
								diac_cbx.value = !diac_cbx.value;
								break;
							case 'global_replace_hotkey':
								global_cbx.value = !global_cbx.value;
								break;
						}
						break;
					case 'close': //user close the dialog
						if (tasks > 0 && dlg.Request(translated_str('close_msg').replace('%s', tasks), translated_str('yes_str') + '|' + translated_str('no_str'), script_name) === 0)
							continue;
						tasks = 0;
						dlg.EndDlg(0);
						break;
				}
			}
			//save dlg size and position 
			Log(1, 'Saving dialog size/pos...');
			dlg.SavePosition('MetaWizard');
			metadata_list = null;
			files_list = null;
			fileprops_list = null;
			search_menu = null;
			last_used_props = null;
			find_edit = null;
			replace_edit = null;
			dlg = null;
		}
		Log(2, 'tasks : ' + tasks);
		if (tasks > 0) {
			Log(2, '=============================================================');
			Log(2, 'Setting Changed Metadata....');
			var logs_success = true;
			//Setting logs files
			var logs_path = Script.config['Logs Path'];
			//If not set, save into Logs folder
			if (!logs_path) logs_path = '/profile';
			logs_path += '\\DOpus_F&RLogs';
			try {
				logs_path = FSU.Resolve(logs_path);
				Log(1, 'Logs Path : ' + logs_path);
				if (!FSU.Exists(logs_path)) {
					Log(1, 'Creating folder ' + logs_path);
					cmd.RunCommand('CreateFolder NAME="' + logs_path + '" READAUTO=no');
				}
			}
			catch (err) {
				Log(4, 'Unable to create the folder ' + logs_path);
				logs_success = false;
			}
			if (logs_success) {
				var back_log_item = FSU.GetItem(logs_path + '\\Backup_' + DOpus.Create.Date().Format('D#yyyy-MM-dd-T#HH;mm;ss') + '.txt');
				var file_log_item = FSU.GetItem(logs_path + '\\Files_' + DOpus.Create.Date().Format('D#yyyy-MM-dd-T#HH;mm;ss') + '.txt');
				Log(2, 'FileLog   : ' + file_log_item);
				Log(2, 'BackupLog : ' + back_log_item);
				var backFile, logFile;
				progress.Restart();
				progress.ClearAbortState();
				progress.SetFiles(files.count);
				progress.Show();
				outer: for (var i = 0; i < g_map.length; i++) {
					cmdline = '';
					cmdline_bak = '';
					progress.SetName(files(i));
					progress.SetStatus(translated_str('set_metadata') + ' : ' + (i + 1) + ' / ' + files.count + ' ' + translated_str('files'));
					for (var e = new Enumerator(g_map(i)); !e.atEnd(); e.moveNext()) {
						label = e.item();
						if (progress.GetAbortState(true) === 'a') {
							aborted = true;
							break outer;
						}
						if (DOpus.TypeOf(g_map(i)(label)) === 'object.Map') {
							keyword = labels(label)('keyword');
							old_value = g_map(i)(label)('orig_value') + '';
							new_value = g_map(i)(label)('value') + '';
							if (old_value == new_value) continue;
							cmdline_bak += (!old_value) ? (' ' + keyword) : (' "' + keyword + ':' + old_value.replace(/"/g, '""') + '"');
							cmdline += (new_value == '') ? (' ' + keyword) : (' "' + keyword + ':' + new_value.replace(/"/g, '""') + '"');
						}
					}
					if (cmdline) {
						cmdline = 'SetAttr FILE="' + files(i) + '" META' + cmdline;
						cmdline_bak = 'SetAttr FILE="' + files(i) + '" META' + cmdline_bak;
						try {
							if (cmd.RunCommand(cmdline) !== 0) {
								if (!logFile) logFile = file_log_item.Open('wa');
								if (logFile.error == 0) logFile.Write(cmdline + '\r\n');
							}
							else Log(3, cmdline + ' couldn\'t be executed');
							if (!backFile) backFile = back_log_item.Open('wa');
							if (backFile.error == 0) backFile.Write(cmdline_bak + '\r\n');
						}
						catch (error) {
							Log(3, 'Error while adding command line ' + cmdline + ' : ' + error);
						}
					}
					progress.StepFiles(1);
				}
				if (backFile) backFile.Close();
				if (logFile) logFile.Close();
				progress.SetFilesProgress(files.count);
				progress.Hide();

				//Open logs files based on user's call
				helper_str = Script.config['After actions'];

				if (FSU.Exists(logs_path) && helper_str == 1) cmd.RunCommand('Go "' + logs_path + '" NEWTAB=findexisting OPENINDUAL');
				else {
					if (helper_str == 2 && FSU.Exists(back_log_item)) cmd.RunCommand('"' + back_log_item + '"');
					if (helper_str == 2 && FSU.Exists(file_log_item)) cmd.RunCommand('"' + file_log_item + '"');
				}
			}
			else
				alert(DOpus.strings.Get('alert_no_logs'), 'error');
		}

		sel_labels = null;
		back_log_item = null;
		file_log_item = null;
		logFile = null;
		backFile = null;
	}
	else {
		Log(4, 'Unable to continue without a file or property!');
		alert(DOpus.strings.Get('alert_no_file'), 'error');
		return;
	}
	if (aborted) Log(3, 'User aborted the command!');
	cmd = null;
	cmdHelper = null;
	tab = null;
	str_labels = null;
	sp = null;
	progress = null;
	labels = null;
	main_menu_map = null;
	str_tools = null;
	g_map = null;
	files = null;
	FSU = null;
	translated_str = null;
	new_values_map = null;
	ini_timer = null;
	args = null;
	regexps = null;
	Log(2, '=============== COMMAND FINISHED ==========================');
	CollectGarbage();
	return;

	function UndoValues() {
		if (tasks <= 0) return false;
		Log(2, 'Undoing last values....');
		if (dlg.Request(translated_str('undo_msg'), translated_str('yes_str') + '|' + translated_str('no_str'), script_name + ' - Undo Last Task') == 1) {
			try {
				for (var e = new Enumerator(last_used_props); !e.atEnd(); e.moveNext()) {
					var label = e.item();
					// Log(1, '---Property : ' + label);
					for (var i = 0; i < last_used_props(label).count; i++) {
						var item = last_used_props(label)(i);
						if (!g_map(item).Exists(label) || DOpus.TypeOf(g_map(item)(label)) != 'object.Map') continue;
						// Log(1, '------' + files(item) + '=> last_value : "' + g_map(item)(label)('last_value') + '"; value : "' + g_map(item)(label)('value')) + '"';
						g_map(item)(label)('value') = g_map(item)(label)('last_value');
					}
				}
				return true;
			}
			catch (err) {
				Log(3, 'Error on undone : ' + err);
				return false;
			}
		}
		return false;
	}

	function setNewValues() {
		Log(2, 'Storing new values...');
		var item;
		try {
			for (var i = 0; i < sel_labels.count; i++) {
				// Log(1, '---Property : ' + sel_labels(i));
				var files_set = DOpus.Create.Vector();
				for (var l = new Enumerator(new_values_map(sel_labels(i))); !l.atEnd(); l.moveNext()) {
					item = l.item();
					if (g_map(item)('is_checked') && g_map(item).Exists(sel_labels(i))) {
						// Log(1, '------' + files(item) + '=> last_value : "' + g_map(item)(sel_labels(i))('last_value') + '"; value : "' + g_map(item)(sel_labels(i))('value') + '"; new : "' + new_values_map(sel_labels(i))(item) + '"');
						g_map(item)(sel_labels(i))('last_value') = g_map(item)(sel_labels(i))('value');
						g_map(item)(sel_labels(i))('value') = new_values_map(sel_labels(i))(item);
						files_set.push_back(item);
					}
				}
				last_used_props(sel_labels(i)) = files_set;
			}
			item = null;
			files_set = null;
			return true;
		}
		catch (err) {
			Log(3, 'Unable to add new task : ' + err);
			return false;
		}
	}

	function setTimerFilesList(update_cols, is_targeted) {
		build_files_list_cols = update_cols;
		item_targeted = is_targeted;
		dlg.SetTimer(350, 'update_files_list_timer');
	}

	function updateFilesList(rebuild_cols, target) { //Fill the list with filenames
		find = find_edit.value.clean();
		replace = replace_edit.value.clean();
		var calculate = CheckFields(find, replace, sel_labels.count, checked_files, true);
		Log(1, 'Updating files list : first:' + first + ';\trebuild_cols:' + rebuild_cols + ';\tcalculate:' + calculate + ';\ttarget:' + target);
		files_list.redraw = false;
		var fila;
		var pattern = calculate ? ParsePattern(ch_flags, find) : null;
		if (rebuild_cols && target == false) {
			Log(1, '---Rebuilding columns...');
			for (var i = files_list.columns.count - 1; i > 0; i--) {
				// Log(1, files_list.columns.GetColumnAt(i).name);
				files_list.columns.DeleteColumn(i);
			}

			for (var i = 0; i < sel_labels.length; i++)
				files_list.columns.AddColumn(sel_labels(i));
		}
		if (pattern) Log(1, '---Calculating new values' + (target !== false ? (' for ' + target) : '') + ' ;\tpattern : ' + pattern);
		if (target !== false) {
			try {
				fila = files_list.GetItemAt(target);
				for (var i = 0; i < sel_labels.length; i++) {
					fila.subitems(i) = GetNewValue(pattern, replace, target, sel_labels(i));
				}
			}
			catch (err) {
				Log(3, '---Error setting columns for item ' + target + ':' + err);
			}
		}
		else {
			for (var i = 0; i < files.length; i++) {
				if (first) {
					fila = files_list.GetItemAt(files_list.AddItem(files(i).name));
					fila.checked = 1;
				}
				else fila = files_list.GetItemAt(i);
				for (var j = 0; j < sel_labels.length; j++) {
					if (!g_map(i).Exists(sel_labels(j)) || DOpus.TypeOf(g_map(i)(sel_labels(j))) != 'object.Map') continue;
					fila.subitems(j) = GetNewValue(pattern, replace, i, sel_labels(j));
				}
			}
			files_list.columns.AutoSize();
			if (max_col_width > 0) {
				for (var i = 0; i < files_list.columns.count; i++) {
					with(files_list.columns.GetColumnAt(i)) {
						if (width > max_col_width) width = max_col_width;
					}
				}
			}
		}
		apply_btn.enabled = tasks > 0 || calculate;
		add_task_btn.enabled = calculate;
		status_title.label = translated_str('status');
		if (first) first = false;
		files_list.redraw = true;

		Log(1, 'Updating files list done with ' + calculate);
		return calculate;
	}

	function resetFields() {
		Log(1, 'Resetting fields...');
		checkMetadataList(0, true);
		for (var e = new Enumerator(labels); !e.atEnd(); e.moveNext())
			new_values_map(e.item()).Clear();
		find_edit.value = '';
		replace_edit.value = '';
		setTimerFilesList(true, false);
		Log(1, 'Resetting fields done');
		return;
	}

	//use it for keep in sync checked/unchecked items in metadata list with sel_labels set
	function checkMetadataList(mode, isControl) {
		Log(1, 'checkMetadataList mode : ' + mode);
		var fila, ch;
		var count = isControl === true ? metadata_list.count : 1;
		if (mode == -1) ch = function(val) {
			return val == 1 ? 0 : 1;
		};
		else ch = mode;
		metadata_list.redraw = false;
		for (var i = 0; i < count; i++) {
			fila = (isControl) ? metadata_list.getItemAt(i) : metadata_list.value;
			if (mode != 2) fila.checked = (mode == -1) ? ch(fila.checked) : ch;
			if (fila.checked == 1) sel_labels.insert(fila.subitems(0));
			else {
				sel_labels.erase(fila.subitems(0));
				new_values_map(fila.subitems(0)).Clear(); //clear the map with all the calculated values
			}
			fila.group = fila.checked;
		}
		metadata_list.columns.GetColumnAt(0).sort = 1;
		metadata_list.redraw = true;
		metadata_list_status.label = labels.count + ' ' + translated_str('items') + '\n' + sel_labels.count + ' / ' + metadata_list.count + ' ' + translated_str('selected');
		return;
	}
	//filter content list when user search for some value
	function FilterFilePropsList() {
		Log(1, 'Filtering properties sheet list...');
		var strFilter, fila, value, subvalue, label, group;
		var c = 0;
		var h = 0;
		fileprops_list.redraw = false;
		var fila;
		var strFilter = filter_filepropslist_edit.value;
		// fileprops_static.readonly = false;
		fileprops_static.label = '';
		fileprops_list.RemoveItem(-1);
		if (sel_file >= 0 && sel_file < g_map.count) {
			if (strFilter != '') {
				var nodiac = (search_flags & 1 << 3);
				// Log(1, 'case:' + (search_flags & 1 << 0) + ';whole:' + (search_flags & 1 << 1) + ';regex:' + (search_flags & 1 << 2) + ';nodiac' + (search_flags & 1 << 3));
				if (nodiac) strFilter = str_tools.RemoveDiacritics(strFilter);
				if (!(search_flags & 1 << 2))
					strFilter = strFilter.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
				if (search_flags & 1 << 1)
					strFilter = '\\b' + strFilter + '\\b';
				try {
					var pattern = new RegExp(strFilter, (search_flags & 1 << 0) ? '' : 'i');
				}
				catch (err) {
					var pattern = new RegExp(strFilter.replace(/[.*+?^${}()|[\]\\]/g, '\\$&'), (search_flags & 1 << 0) ? '' : 'i');
				}
				Log(1, '---Updating Content List for ' + files(sel_file) + ':' + pattern);

			}

			fileprops_static.label = files(sel_file) + (String(files(sel_file)).length > 80 ? (' <b>(' + translated_str(files(sel_file).metadata) + ')</b>') :
				('\n<b>' + translated_str('filetype') + ' : </b>' + translated_str(files(sel_file).metadata)));

			for (var e = new Enumerator(g_map(sel_file)); !e.atEnd(); e.moveNext()) {
				try {
					label = e.item();
					if (label == 'is_checked') continue; //skip the 'is_checked' property
					editable = DOpus.TypeOf(g_map(sel_file)(label)) == 'object.Map' ? 1 : 2;
					if (DOpus.TypeOf(g_map(sel_file)(label)) === 'object.Map') { //is a editable value
						value = g_map(sel_file)(label)('value');
						group = 1;
						subvalue = new_values_map(label).Exists(sel_file) ? new_values_map(label)(sel_file) : '';
					}
					else {
						value = g_map(sel_file)(label);
						group = 2;
						subvalue = false;
					}
					if (strFilter == '' || pattern.test((nodiac) ? str_tools.RemoveDiacritics(value) : value) || pattern.test((nodiac) ? str_tools.RemoveDiacritics(label) : label)) {
						fila = fileprops_list.GetItemAt(fileprops_list.AddItem(value, c, group));
						fila.subitems(0) = label;
						if (subvalue && g_map(sel_file)('is_checked') && sel_labels.Exists(label)) {
							fila.fg = fg_color;
							fila.subitems(1) = subvalue;
						}
						c++;
					}
					else h++;
				}
				catch (err) {
					Log(3, 'fileprops_list : ' + err);
					continue;
				}
			}
			fileprops_list.columns.AutoSize();
			if (fp_max_col_width > 0) {
				for (var i = 0; i < fileprops_list.columns.count; i++) {
					with(fileprops_list.columns.GetColumnAt(i)) {
						if (width > fp_max_col_width) width = fp_max_col_width;
					}
				}
			}
		}
		fileprops_list.redraw = true;
		fileprops_list_status.label = c + ' ' + translated_str('results_str') + (h > 0 ? ('\n(' + h + ' ' + translated_str('hidden_str') + ')') : '');
		fila = null;
		pattern = null;
		Log(1, 'Filtering properties sheet list done');
		return;
	}

	function setHotkeys() {
		var hotkeys = DOpus.Create.Map('find_edit_hotkey', Script.config['Find'],
			'replace_edit_hotkey', Script.config['Replace'],
			'filter_metalist_hotkey', Script.config['Metadata Filter'],
			'filter_filepropslist_hotkey', Script.config['Properties Sheet Filter'],
			'case_sensitive_hotkey', Script.config['Case sensitive checkbox'],
			'regular_expressions_hotkey', Script.config['Regular expressions checkbox'],
			'whole_words_hotkey', Script.config['Whole words checkbox'],
			'ignore_diacritics_hotkey', Script.config['Ignore diacritics checkbox'],
			'global_replace_hotkey', Script.config['Global replace checkbox']);
		for (var h = new Enumerator(hotkeys); !h.atEnd(); h.moveNext()) {
			try {
				if (hotkeys(h.item()) !== '') dlg.AddHotkey(h.item(), hotkeys(h.item()));
			}
			catch (err) {
				Log(3, 'Error trying to set hotkey ' + h.item());
				continue;
			}
		}
		h = null;
		hotkeys = null;
		return;
	}

}

function GetNewValue(pattern, replace, item, label, true_set) {
	var value, new_value = '';
	var match = false;
	if (pattern != null && g_map(item)('is_checked')) {
		try {
			value = g_map(item)(label)('value') + '';
			ex_rep = expandReplace(replace, item);
			//if pattern match and expanded replace is not empty
			if (value.match(pattern) && ex_rep) {
				// Log(1, files(item) + ' : ' + label + ' ...Exists and Match : ' + value);
				new_value = (ex_rep == '{empty}') ? '' : avoidDupeValues(value.replace(pattern, ex_rep), labels(label)('keyword'));
				if (FormatType(new_value, labels(label)('type_raw'))) {
					match = true;
					if (true_set) g_map(item)(label)('value') = new_value;
				}
				else new_value = value;
			}
			else new_value = value;
		}
		catch (err) {
			Log(3, "Couldn't retrieved the value for the property : " + err);
			new_value = value;
		}
	}
	if (!true_set) {
		new_values_map(label)(item) = new_value;
		return new_value;
	}
	else return match;
}

function ParsePattern(ch_map, find) {
	if (ch_map('diac')) find = str_tools.RemoveDiacritics(find);
	if (!ch_map('regex')) find = find.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
	if (ch_map('ww')) find = '\\b' + find + '\\b';
	var flags = ch_map('global') ? 'g' : '';
	var f = flags + ((ch_map('case')) ? '' : 'i');
	//disable 'g' flag if find value is '.*' only.Useful for avoid continuous repeat 
	//if some properties has values but others not
	if (find == '.*') f = f.replace('g', '');
	Log(1, 'Parse pattern case : ' + ch_map('case') + ';\twhole : ' + ch_map('ww') + ';\tregex : ' + ch_map('regex') + ';\tdiacr : ' + ch_map('diac') + ';\tflags : ' + f);
	try {
		return new RegExp(find, f);
	}
	catch (error) {
		Log(3, 'Error parsing the pattern : ' + error);
		return false;
	}
}
//expand keywords used as replace value
function expandReplace(value, pos) {
	var regex = /\{.*?\}/g;
	var label, item_val;
	var m = value.match(regex) || [];
	if (m) {
		for (var i = 0; i < m.length; i++) {
			if (m[i] == '{empty}') return '{empty}'; //there's no need to look for other keywords if {empty} is present
			if (label = keywordToLabel(m[i])) {
				if (g_map(pos).Exists(label)) {
					item_val = (labels.Exists(label)) ? g_map(pos)(label)('value') : g_map(pos)(label);
					// Log(1, 'expand    : ' + m[i] + '=' + label + '=' + item_val);
					value = value.replace(m[i], item_val);
				}
				else if (m[i] == '{date}') {
					value = value.replace(m[i], DOpus.Create.Date().Format('D#yyyy-MM-dd'));
					// Log(1, 'expand    : ' + m[i] + '=' + label + '=' + value);
				}
				else if (m[i] == '{time}') {
					value = value.replace(m[i], DOpus.Create.Date().Format('T#HH:mm:ss'));
					// Log(1, 'expand    : ' + m[i] + '=' + label + '=' + value);
				}
				else {
					// Log(1, 'expand    : Recognized keyword but empty');
					value = value.replace(m[i], '');
				}

			}
		}
	}
	return value.clean();
}

//filter metadata list when user search for some value
function FilterMetadataList(strFilter, list, sel_labels) {
	Log(1, 'Filtering Metadata List : ' + strFilter);
	list.redraw = false;
	var fila, label;
	var i = 0;
	if (strFilter != '') var regex = new RegExp(str_tools.RemoveDiacritics(strFilter).replace(/[.*+?^${}()[\]\\]/g, '\\$&'), 'i');
	list.RemoveItem(-1);
	for (var e = new Enumerator(labels); !e.atEnd(); e.moveNext()) {
		label = e.item();
		if (sel_labels.exists(label)) {
			fila = list.GetItemAt(list.AddItem('', i + 1, 1));
			fila.checked = 1;
		}
		else if (strFilter == '' || regex.test(str_tools.RemoveDiacritics(label))) fila = list.GetItemAt(list.AddItem('', i + 1, 0));
		else continue;
		fila.subitems(0) = label;
		fila.subitems(1) = labels(label)('type');
		i++;
	}
	list.redraw = true;
	Log(1, 'Filtering Metadata List done');
	return labels.count + ' ' + translated_str('items') + '\n' + sel_labels.count + ' / ' + list.count + ' ' + translated_str('selected');
}

//use it to decide OK button enabled state
function CheckFields(find, replace, sel_labels, sel_files, set_status) {
	Log(1, 'Checking Fields :  metadata selected  : ' + sel_labels + ';\tfind : ' + find + ';\treplace : ' + replace + ';\tsel_files : ' + sel_files + ';\tset_status : ' + set_status);
	if (sel_labels == 0) {
		if (set_status) translated_str('status') = '<#%error_text>' + translated_str('metadata') + ': 0 ' + translated_str('selected');
		else Log(1, 'No valid properties was passed');
		return false;
	}
	if (sel_files == 0) {
		if (set_status) translated_str('status') = '<#%error_text>0 ' + translated_str('files') + ' ' + translated_str('selected');
		else Log(1, 'No FILES was passed');
		return false;
	}
	find = find.clean();
	if (find == '') {
		if (set_status) translated_str('status') = '<#%error_text>' + translated_str('empty') + ' ' + translated_str('find');
		else Log(1, 'No valid FIND value was passed');
		return false;
	}
	replace = replace.clean();
	if (replace == '') {
		if (set_status) translated_str('status') = '<#%error_text>' + translated_str('empty') + ' ' + translated_str('replace');
		else Log(1, 'No valid REPLACE value was passed');
		return false;
	}
	if (set_status) translated_str('status') = '';
	return true;
}

function BuildMetaMap() {
	str_labels = DOpus.Create.Map(
		'general', DOpus.Create.Map(
			'labels', DOpus.Create.Vector(str_tools.LanguageStr(29076), str_tools.LanguageStr(3896).slice(8), str_tools.LanguageStr(318), str_tools.LanguageStr(316), str_tools.LanguageStr(313), str_tools.LanguageStr(566), str_tools.LanguageStr(565),
				str_tools.LanguageStr(567), str_tools.LanguageStr(319), str_tools.LanguageStr(317), str_tools.LanguageStr(314), str_tools.LanguageStr(323),
				str_tools.LanguageStr(310), str_tools.LanguageStr(144), str_tools.LanguageStr(559), str_tools.LanguageStr(39), str_tools.LanguageStr(353),
				str_tools.LanguageStr(348), str_tools.LanguageStr(315), str_tools.LanguageStr(145), str_tools.LanguageStr(146), str_tools.LanguageStr(457),
				str_tools.LanguageStr(347), str_tools.LanguageStr(564), str_tools.LanguageStr(420), str_tools.LanguageStr(102), str_tools.LanguageStr(140)),
			'keywords', DOpus.Create.Vector('date', 'time', 'accesseddate', 'createddate',
				'modifieddate', 'created', 'modified', 'accesed', 'accessedtime',
				'createdtime', 'modifiedtime', 'ext', 'file', 'filepath',
				'pathlen', 'path', 'sizekb', 'size', 'attr',
				'availability', 'desc', 'label', 'owner', 'status', 'rating', 'tags', 'userdesc'),
			'editable', 3),
		'audio', DOpus.Create.Map(
			'labels', DOpus.Create.Vector(str_tools.LanguageStr(36), str_tools.LanguageStr(138), str_tools.LanguageStr(35), str_tools.LanguageStr(116), str_tools.LanguageStr(436),
				str_tools.LanguageStr(437), str_tools.LanguageStr(344), str_tools.LanguageStr(458), str_tools.LanguageStr(89), str_tools.LanguageStr(570),
				str_tools.LanguageStr(33), str_tools.LanguageStr(441), str_tools.LanguageStr(38), str_tools.LanguageStr(34), str_tools.LanguageStr(447),
				str_tools.LanguageStr(371), str_tools.LanguageStr(37), str_tools.LanguageStr(434), str_tools.LanguageStr(92), str_tools.LanguageStr(327),
				str_tools.LanguageStr(330), str_tools.LanguageStr(572), str_tools.LanguageStr(50), str_tools.LanguageStr(32), str_tools.LanguageStr(46),
				str_tools.LanguageStr(418), str_tools.LanguageStr(331)),
			'keywords', DOpus.Create.Vector('album', 'albumartist', 'artist', 'bpm', 'composers',
				'conductor', 'copyright', 'discnumber', 'encoder', 'encodingsoftware',
				'genre', 'initialkey', 'comment', 'title', 'releasedate',
				'track', 'year', 'publisher',

				'audiocodec', 'picdepth',
				'mp3bitrate', 'compilation', 'duration', 'mp3mode', 'mp3info',
				'mp3drm', 'mp3samplerate'),
			'property', DOpus.Create.Vector('mp3album', 'mp3albumartist', 'mp3artists', 'mp3bpm', 'composers',
				'conductors', 'copyright', 'mp3disc', 'mp3encoder', 'mp3encodingsoftware',
				'mp3genre', 'initialkey', 'mp3comment', 'mp3title', 'releasedate',
				'mp3track', 'mp3year', 'publisher',
				'audiocodec', 'picdepth', 'mp3bitrate', 'compilation', 'duration',
				'mp3mode', 'mp3info', 'mp3drm', 'mp3samplerate'),
			'editable', 18,
			'type', DOpus.Create.Vector('string', 'string', 'string', 'int', 'string',
				'string', 'string', 'discnumber', 'string', 'string',
				'string', 'string', 'string', 'string', 'datetime',
				'track', 'year', 'string', 'string', 'string',
				'string', 'string', 'string', 'string', 'int',
				'int', 'string')),
		'doc', DOpus.Create.Map(
			'labels', DOpus.Create.Vector(str_tools.LanguageStr(363), str_tools.LanguageStr(366), str_tools.LanguageStr(368), str_tools.LanguageStr(45), str_tools.LanguageStr(44),
				str_tools.LanguageStr(426), str_tools.LanguageStr(406), str_tools.LanguageStr(427), str_tools.LanguageStr(365), str_tools.LanguageStr(364),
				str_tools.LanguageStr(403), str_tools.LanguageStr(105), str_tools.LanguageStr(404), str_tools.LanguageStr(367)),
			'keywords', DOpus.Create.Vector('author', 'category', 'comment', 'company', 'copyright',
				'creator', 'lastsavedby', 'producer', 'subject', 'title',
				'doccreateddate', 'docedittime', 'doclastsaveddate', 'pages'),
			'property', DOpus.Create.Vector('author', 'category', 'comments', 'companyname', 'copyright',
				'creator', 'doclastsavedby', 'producer', 'subject', 'title',

				'doccreateddate', 'docedittime', 'doclastsaveddate', 'pages'),
			'editable', 10,
			'type', DOpus.Create.Vector('string', 'string', 'string', 'string', 'string',
				'string', 'string', 'string', 'string', 'string',
				'string', 'string', 'string', 'string')),
		'image', DOpus.Create.Map(
			'labels', DOpus.Create.Vector(str_tools.LanguageStr(386), str_tools.LanguageStr(101), str_tools.LanguageStr(357), str_tools.LanguageStr(53), str_tools.LanguageStr(54),
				str_tools.LanguageStr(425), str_tools.LanguageStr(356), str_tools.LanguageStr(387), str_tools.LanguageStr(60), str_tools.LanguageStr(377),
				str_tools.LanguageStr(77), str_tools.LanguageStr(373), str_tools.LanguageStr(563), str_tools.LanguageStr(571), str_tools.LanguageStr(359),
				str_tools.LanguageStr(98), str_tools.LanguageStr(99), str_tools.LanguageStr(382), str_tools.LanguageStr(358), str_tools.LanguageStr(417),
				str_tools.LanguageStr(376), str_tools.LanguageStr(363), str_tools.LanguageStr(44), str_tools.LanguageStr(365), str_tools.LanguageStr(364),
				str_tools.LanguageStr(152), str_tools.LanguageStr(119), str_tools.LanguageStr(30275), str_tools.LanguageStr(30273), str_tools.LanguageStr(29389), str_tools.LanguageStr(383), str_tools.LanguageStr(100),
				str_tools.LanguageStr(375), str_tools.LanguageStr(61), str_tools.LanguageStr(120), str_tools.LanguageStr(153), str_tools.LanguageStr(374),
				str_tools.LanguageStr(136), str_tools.LanguageStr(137), str_tools.LanguageStr(30007), str_tools.LanguageStr(369), str_tools.LanguageStr(370),
				str_tools.LanguageStr(384), str_tools.LanguageStr(78), str_tools.LanguageStr(118), str_tools.LanguageStr(385), str_tools.LanguageStr(360),
				str_tools.LanguageStr(416), str_tools.LanguageStr(35), str_tools.LanguageStr(327), str_tools.LanguageStr(26), str_tools.LanguageStr(28), str_tools.LanguageStr(25)),
			'keywords', DOpus.Create.Vector('35mmfocallength', 'gpsaltitude', 'aperture', 'cameramake', 'cameramodel',
				'datedigitized', 'datetaken', 'digitalzoom', 'exposurebias', 'exposuretime',
				'fnumber', 'focallength', 'imagedesc', 'instructions', 'isospeed',
				'gpslatitude', 'gpslongitude', 'orientation', 'shutterspeed', 'software',
				'subjectdistance', 'author', 'copyright', 'subject', 'title',
				'lensmake', 'lensmodel',

				'datetimecreated', 'datetimeoriginal', 'colormodel', 'contrast', 'coords',
				'exposureprogram', 'flash', 'imagequality', 'macromode', 'meteringmode',
				'picphysx', 'picphysy', 'picres', 'picresx', 'picresy',
				'saturation', 'scenecapturetype', 'scenemode', 'sharpness', 'whitebalance',
				'aspectratio', 'mp3artist', 'picdepth', 'picheight', 'picsize', 'picwidth'),
			'property', DOpus.Create.Vector('35mmfocallength', 'altitude', 'apertureval', 'cameramake', 'cameramodel',
				'datedigitized', 'datetaken', 'digitalzoom', 'exposurebias', 'exposuretime',
				'fnumber', 'focallength', 'imagedesc', 'instructions', 'isospeed',
				'latitude', 'longitude', 'rotation', 'shutterspeed', 'software',
				'subjectdistance', 'author', 'copyright', 'subject', 'title',
				'lensmake', 'lensmodel', 'datetimecreated', 'datetimeoriginal', 'colormodel', 'contrast', 'coords',
				'exposureprogram', 'flash', 'imagequality', 'macromode', 'meteringmode',
				'picphysx', 'picphysy', 'picres', 'picresx', 'picresy',
				'saturation', 'scenecapturetype', 'scenemode', 'sharpness', 'whitebalance',
				'aspectratio', 'mp3artist', 'picdepth', 'picheight', 'picsize', 'picwidth'),
			'editable', 27,
			'type', DOpus.Create.Vector('number', 'number', 'number_text', 'string', 'string',
				'datetime', 'datetime', 'digitalzoom', 'number', 'number',
				'number', 'number', 'string', 'string', 'int',
				'gpslatitude', 'gpslongitude', 'orientation', 'number_text', 'string',
				'subjectdistance', 'string', 'string', 'string', 'string',
				'string', 'string', 'datetime', 'datetime', 'string', 'string', 'string',
				'string', 'string', 'string', 'string', 'string',
				'string', 'string', 'string', 'string', 'number',
				'string', 'string', 'string', 'string', 'string',
				'string', 'string', 'string', 'string', 'string', 'string')),
		'video', DOpus.Create.Map(
			'labels', DOpus.Create.Vector(str_tools.LanguageStr(363), str_tools.LanguageStr(444), str_tools.LanguageStr(436), str_tools.LanguageStr(437), str_tools.LanguageStr(35),
				str_tools.LanguageStr(33), str_tools.LanguageStr(34), str_tools.LanguageStr(37), str_tools.LanguageStr(447), str_tools.LanguageStr(434),
				str_tools.LanguageStr(427), str_tools.LanguageStr(89),
				str_tools.LanguageStr(327), str_tools.LanguageStr(330), str_tools.LanguageStr(129), str_tools.LanguageStr(450), str_tools.LanguageStr(449),
				str_tools.LanguageStr(395), str_tools.LanguageStr(28), str_tools.LanguageStr(350), str_tools.LanguageStr(124), str_tools.LanguageStr(558),
				str_tools.LanguageStr(394), str_tools.LanguageStr(26), str_tools.LanguageStr(451), str_tools.LanguageStr(332), str_tools.LanguageStr(459),
				str_tools.LanguageStr(454), str_tools.LanguageStr(452), str_tools.LanguageStr(331), str_tools.LanguageStr(455), str_tools.LanguageStr(397),
				str_tools.LanguageStr(325), str_tools.LanguageStr(29375), str_tools.LanguageStr(29377), str_tools.LanguageStr(29380), str_tools.LanguageStr(418),
				str_tools.LanguageStr(29381), str_tools.LanguageStr(29383), str_tools.LanguageStr(364), str_tools.LanguageStr(29378), str_tools.LanguageStr(29382),
				str_tools.LanguageStr(29384), str_tools.LanguageStr(416), str_tools.LanguageStr(92)),
			'property', DOpus.Create.Vector('author', 'director', 'composers', 'conductors', 'mp3artist',
				'mp3genre', 'mp3title', 'mp3year', 'releasedate', 'publisher',
				'producers', 'encodedby',
				'picdepth', 'mp3bitrate', 'broadcastdate', 'channel', 'credits',
				'datarate', 'picsize', 'duration', 'episodename', 'fourcc',
				'framerate', 'picheight', 'ishd', 'mp3mode', 'picphyssize',
				'recordingtime', 'isrepeat', 'mp3samplerate', 'station', 'videocodec',
				'picwidth', 'alllangs', 'audiolangs', 'hdrtypes', 'mp3drm',
				'subtitlelangs', 'videolangs', 'title', 'audiocount', 'subtitlecount',
				'videocount', 'aspectratio', 'audiocodec'),
			'keywords', DOpus.Create.Vector('author', 'directors', 'composers', 'conductor', 'artist',
				'genre', 'title', 'year', 'releasedate', 'publisher',
				'producers', 'encoder',

				'picdepth', 'mp3bitrate', 'broadcastdate', 'channel', 'credits',
				'datarate', 'picsize', 'duration', 'episodename', 'fourcc',
				'framerate', 'picheight', 'ishd', 'mp3mode', 'picphyssize',
				'recordingtime', 'isrepeat', 'mp3samplerate', 'station', 'videocodec',
				'picwidth', 'alllangs', 'audiolangs', 'hdrtypes', 'mp3drm',
				'subtitlelangs', 'videolangs', 'title', 'audiocount', 'subtitlecount',
				'videocount', 'aspectratio', 'audiocodec'),
			'editable', 12,
			'type', DOpus.Create.Vector('string', 'string', 'string', 'string', 'string',
				'string', 'string', 'year', 'datetime', 'string',
				'string', 'string',
				'string', 'string', 'string', 'string', 'string',
				'string', 'string', 'string', 'string', 'string',
				'string', 'string', 'string', 'string', 'string',
				'string', 'string', 'string', 'string', 'string',
				'string', 'string', 'string', 'string', 'string',
				'string', 'string', 'string', 'string', 'string',
				'string', 'string', 'string')),
		'exe', DOpus.Create.Map(
			'labels', DOpus.Create.Vector(str_tools.LanguageStr(40), str_tools.LanguageStr(41), str_tools.LanguageStr(42), str_tools.LanguageStr(43), str_tools.LanguageStr(44), str_tools.LanguageStr(45)),
			'keywords', DOpus.Create.Vector('moddesc', 'modversion', 'prodname', 'prodversion', 'copyright', 'companyname'),
			'property', DOpus.Create.Vector('moddesc', 'modversion', 'prodname', 'prodversion', 'copyright', 'companyname'),
			'editable', 0,
			'type', DOpus.Create.Vector('string', 'string', 'string', 'string', 'string', 'string')));
	var sp = DOpus.Create.Map('encoder', translated_str('audio') + ', ' + translated_str('video'), 'genre', translated_str('audio') + ', ' + translated_str('video'), 'artist', translated_str('audio') + ', ' + translated_str('video'), 'author', translated_str('doc') + ', ' + translated_str('image') + ', ' + translated_str('video'), 'comment', translated_str('audio') + ', ' + translated_str('doc'),
		'composers', translated_str('audio') + ', ' + translated_str('video'), 'copyright', translated_str('audio') + ', ' + translated_str('doc') + ', ' + translated_str('image'), 'publisher', translated_str('audio') + ', ' + translated_str('video'),
		'releasedate', translated_str('audio') + ', ' + translated_str('video'), 'subject', translated_str('doc') + ', ' + translated_str('image'), 'title', translated_str('audio') + ', ' + translated_str('doc') + ', ' + translated_str('image') + ', ' + translated_str('video'), 'year', translated_str('audio') + ', ' + translated_str('video'));
	Log(1, 'Map done in ' + (new Date() - ini_timer) + ' ms');
	return sp;
}

function getMetadata(item, noempty, isdoc, sp) {
	var m, value, meta_type, mtype, mlabels, mkeywords, mproperty;
	var keywords = DOpus.Create.Map();
	var mgeneral = str_labels('general')('labels');
	var valid = true;
	value = '';
	//===tags
	m = item.metadata.tags;
	for (var k = 0; k < m.count; k++) value += m(k) + ';';
	if (value || !noempty) {
		if (!labels.Exists(mgeneral(25))) labels.Set(mgeneral(25), DOpus.Create.Map('keyword', 'tags', 'type_raw', 'string', 'type', translated_str('general')));
		value = value.slice(0, -1);
		keywords(mgeneral(25)) = DOpus.Create.Map('orig_value', value, 'value', value, 'last_value', '');
	}
	//===rating
	m = item.metadata.other;
	value = m.rating || 0;
	if (value || !noempty) {
		if (!labels.Exists(mgeneral(24))) labels.Set(mgeneral(24), DOpus.Create.Map('keyword', 'rating', 'type_raw', 'rating', 'type', translated_str('general')));
		keywords(mgeneral(24)) = DOpus.Create.Map('orig_value', value, 'value', value, 'last_value', '');
	}
	//===usercomment
	value = m.usercomment || '';
	if (value || !noempty) {
		if (!labels.Exists(mgeneral(26))) labels.Set(mgeneral(26), DOpus.Create.Map('keyword', 'usercomment', 'type_raw', 'string', 'type', translated_str('general')));
		keywords(mgeneral(26)) = DOpus.Create.Map('orig_value', value, 'value', value, 'last_value', '');
	}

	meta_type = item.metadata + '';
	if (meta_type != 'other') {
		m = str_labels(meta_type);
		mtype = m('type');
		mlabels = m('labels');
		mkeywords = m('keywords');
		mproperty = m('property');
		for (var i = 0; i < m('editable'); i++) {
			if (mproperty(i) == 'author') value = item.metadata['doc']['author'] || '';
			else if (mtype(i) === 'gpslatitude' || mtype(i) === 'gpslongitude' || mtype(i) === 'number_text') value = item.metadata[meta_type + '_text'][mproperty(i)] || '';
			else value = item.metadata[meta_type][mproperty(i)] || ''
			if (value) {
				switch (mtype(i)) {
					case 'datetime':
						value = value.Format('D#yyyy-MM-dd T#HH:mm:ss');
						break;
					case 'number':
						value = parseFloat(value);
						break;
					case 'number_text':
						value = value.replace(/[^\d.]/g, '');
						break;
					case 'int':
						value = value.replace(/\D/g, '');
						break;
					case 'gpslatitude':
					case 'gpslongitude':
						value = value.replace(/ /g, '').replace(/\.0+/g, '');
						break;
				}
			}
			if (value || !isdoc) {
				if (!labels.Exists(mlabels(i))) labels.Set(mlabels(i), DOpus.Create.Map('keyword', mkeywords(i), 'type_raw', mtype(i), 'type', sp.Exists(mkeywords(i)) ? sp(mkeywords(i)) : translated_str(meta_type)));
				keywords(mlabels(i)) = DOpus.Create.Map('orig_value', value, 'value', value, 'last_value', '');
			}
		}
	}
	if (!keywords.empty) {
		keywords('is_checked') = true; //is the file checked in the list
		// Date/time
		keywords(mgeneral(2)) = item.access.Format('D#yyyy-MM-dd'); //accesseddate 
		keywords(mgeneral(3)) = item.create.Format('D#yyyy-MM-dd'); //createddate 
		keywords(mgeneral(4)) = item.modify.Format('D#yyyy-MM-dd'); //modifieddate 
		keywords(mgeneral(5)) = item.create.Format('D#yyyy-MM-dd T#HH:mm:ss'); //created 
		keywords(mgeneral(6)) = item.modify.Format('D#yyyy-MM-dd T#HH:mm:ss'); //modified 
		keywords(mgeneral(7)) = item.access.Format('D#yyyy-MM-dd T#HH:mm:ss'); //accesed 
		keywords(mgeneral(8)) = item.access.Format('T#HH:mm:ss'); //accessedtime 
		keywords(mgeneral(9)) = item.create.Format('T#HH:mm:ss'); //createdtime 
		keywords(mgeneral(10)) = item.modify.Format('T#HH:mm:ss'); //modifiedtime 
		// Path
		keywords(mgeneral(11)) = item.ext.slice(1); //ext 
		keywords(mgeneral(12)) = item.name; //file 
		value = item.realpath;
		keywords(mgeneral(13)) = value + ''; //filepath 
		keywords(mgeneral(14)) = String(value).length; //pathlen 
		keywords(mgeneral(15)) = value.pathpart; //path 
		// Size
		keywords(mgeneral(16)) = (item.size / 1024) + ' ' + translated_str('sizekb'); //sizekb 
		keywords(mgeneral(17)) = item.size.fmt; //size 
		//  General
		keywords(mgeneral(18)) = item.attr_text; //attr 
		// if (value = item.ShellProp('System.OfflineAvailability', 'r')) keywords(mgeneral(19)) = value; //availability 
		if (value = item.metadata.other.desc) keywords(mgeneral(20)) = value; //desc 
		value = '';
		var l = item.Labels('*', 'explicit');
		for (var k = 0; k < l.count; k++) value += l(k) + ';';
		if (value) keywords(mgeneral(21)) = value.slice(0, -1); //label 
		if (value = item.ShellProp('System.FileOwner', 'r')) keywords(mgeneral(22)) = value; //owner 
		value = '';
		var l = item.Labels('Status');
		for (var k = 0; k < l.count; k++) value += l(k) + ';';
		if (value) keywords(mgeneral(23)) = value.slice(0, -1); //status 
		if (meta_type != 'other') {
			for (var i = m('editable'); i < mlabels.length; i++) {
				if (mtype(i) == 'datetime') value = item.metadata[meta_type][mproperty(i)] || '';
				else value = item.metadata[meta_type + '_text'][mproperty(i)] || '';
				if (value) keywords(mlabels(i)) = (mtype(i) == 'datetime') ? value.Format('D#yyyy-MM-dd T#HH:mm:ss') : value;
			}
		}
		g_map.push_back(keywords);
		Log(2, 'Getting metadata for ' + item + '(' + meta_type + ') --- DONE : ' + (new Date() - ini_timer) + ' ms');
	}
	else {
		Log(3, "Discarding " + item + " since doesn't have editable metadata!");
		valid = false;
	}
	mtype = null;
	mproperty = null;
	mkeywords = null;
	mlabels = null;
	keyword = null;
	m = null;
	return valid;
}
//show the main menu for variables insertion
function showMainMenu(parent_dlg) {
	var main_menu = DOpus.Dlg();
	main_menu.window = parent_dlg;
	main_menu.disable_window = parent_dlg;
	main_menu.choices = main_menu_map('main');
	main_menu.menu = 0;
	res = main_menu.Show();
	main_menu = null;
	if (res == 1) {
		parent_dlg.Control('replace_edit').value += ' {empty}';
		return true;
	}
	else if (res > 0)
		return showSubMenu(parent_dlg, main_menu_map(main_menu_map('main')(res - 1)));
	return false;
}
//show the selected submenu for variables selection
function showSubMenu(parent_dlg, vec) {
	var main_menu = DOpus.Dlg();
	main_menu.window = parent_dlg;
	main_menu.disable_window = parent_dlg;
	main_menu.choices = vec;
	main_menu.menu = 0;
	r = main_menu.Show();
	main_menu = null;
	if (r > 0) {
		parent_dlg.Control('replace_edit').value += ' ' + main_menu_map('labels')(vec(r - 1));
		return true;
	}
	return false;
}

function BuildMainMenu() {
	main_menu_map = DOpus.Create.Map();
	//date,path,general,audio,docs,pictures,video,programs
	var datetime = str_tools.LanguageStr(5142) + '\tᐅ';
	var path = str_tools.LanguageStr(5144) + '\tᐅ';
	var label;
	var key = str_tools.LanguageStr(862); //Delete
	var v = DOpus.Create.Vector(datetime, path, translated_str('general') + '\tᐅ', translated_str('audio') + '\tᐅ', translated_str('doc') + '\tᐅ', translated_str('image') + '\tᐅ', translated_str('video') + '\tᐅ', translated_str('exe') + '\tᐅ');
	v.sort();
	v.insert(0, key);
	v.insert(1, '-');
	main_menu_map.Set('main', v);
	var l = DOpus.Create.Map();
	var k = DOpus.Create.Map();
	for (var e = new Enumerator(str_labels); !e.atEnd(); e.moveNext()) {
		for (var i = 0; i < str_labels(e.item())('labels').length; i++) {
			label = str_labels(e.item())('labels')(i);
			key = '{' + str_labels(e.item())('keywords')(i) + '}';
			l.Set(label, key);
			k.Set(key, label);
		}
	}
	main_menu_map.Set('labels', l);
	main_menu_map.Set('keywords', k);
	putinMap(str_labels('general')('labels'), datetime, 0, 10);
	putinMap(str_labels('general')('labels'), path, 11, 15);
	putinMap(str_labels('general')('labels'), translated_str('general') + '\tᐅ', 16, 26);
	putinMap(str_labels('audio')('labels'), translated_str('audio') + '\tᐅ', -1);
	putinMap(str_labels('doc')('labels'), translated_str('doc') + '\tᐅ', -1);
	putinMap(str_labels('image')('labels'), translated_str('image') + '\tᐅ', -1);
	putinMap(str_labels('video')('labels'), translated_str('video') + '\tᐅ', -1);
	putinMap(str_labels('exe')('labels'), translated_str('exe') + '\tᐅ', -1);
	v = null;
}

function putinMap(v, l, i, f) {
	if (i == -1) {
		v.sort();
		main_menu_map.Set(l, v);
	}
	else {
		var vec = DOpus.Create.Vector();
		vec.assign(v, i, f);
		vec.sort();
		main_menu_map.Set(l, vec);
	}
}

function keywordToLabel(keyword) {
	if (main_menu_map('keywords').Exists(keyword))
		return main_menu_map('keywords')(keyword);
	return false;
}

function avoidDupeValues(value, keyword) {
	if (!regexps('multi').test(keyword)) return value;
	var value_set = DOpus.Create.Vector(value.replace(/ *; */g, ';').split(';'));
	var value_str = '';
	value_set.unique();
	for (var i = 0; i < value_set.length; i++) {
		if (value_set(i) == '') continue;
		value_str += value_set(i) + '; ';
	}
	value_set = null;
	return value_str.slice(0, -2);
}

function FormatType(value, type) {
	if (value === '') return true;
	switch (type) {
		case 'datetime':
			return true;
			break;
		case 'int':
			return !isNaN(parseInt(value, 10));
			break;
		case 'number':
		case 'number_text':
			return !isNaN(parseFloat(value));
			break;
		case 'digitalzoom':
			if (value === 'off') return true;
			else return !isNaN(parseFloat(value));
			break;
		case 'rating':
			value = parseInt(value, 10);
			return !isNaN(value) && value >= 0 && value <= 5;
			break;
		case 'year':
			return regexps('year').test(value);
			break;
		case 'orientation':
			value = parseInt(value, 10);
			return !isNaN(value) && (value === 0 || value === 90 || value === 180 || value === 270);
			break;
		case 'gpslatitude':
			return true;
			break;
		case 'gpslongitude':
			return true;
			break;
		case 'subjectdistance':
			return regexps('sd').test(value);
			break;
		case 'discnumber':
		case 'track':
			return regexps('disc_track').test(value);
			break;
		default:
			return true;
			break;
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

function alert(message, icon) {
	var dlg = DOpus.Dlg();
	dlg.message = message;
	dlg.buttons = '&OK';
	dlg.title = script_name + ' v' + script_version;
	if (/warning|error|info|question/.test(icon))
		dlg.icon = icon;
	dlg.Show();
	dlg = null;
}

String.prototype.clean = function() {
	return this.replace(/^[\s\uFEFF\xA0]+|[\s\uFEFF\xA0]+$/g, '').replace(/\s+/g, ' ');
};

function setSearchFlags(flags) {
	var s = 0;
	for (var i = 0; i < flags.length; i++)
		if (flags(i) == true) s += 1 << i;
	Script.Vars.Set('search_flags', s);
	if (!Script.Vars('search_flags').persist) Script.Vars('search_flags').persist = true;
	return s;
}


==SCRIPT RESOURCES
<resources>
	<resource type="strings">
		<strings lang="english">
			<string id="after_actions">Choose among several options to be performed after the command has been completed.</string>
			<string id="alert_no_file">Can&apos;t continue without a file!</string>
			<string id="alert_no_logs">Can&apos;t continue without the logs folder!!!</string>
			<string id="close_msg">There are %s task(s) pending. Do you really want to discard them?</string>
			<string id="current_tasks">Tasks</string>
			<string id="debug">Logging level to be displayed. OFF to show only errors. 
DEBUG to show all messages. 
STANDARD to show only the most relevant information.
WARNING to show messages that needs your attention.</string>
			<string id="dialog_mode">Select the dialog&apos;s UI you want to use.</string>
			<string id="doc_exts">File extensions that will be considered as &quot;true&quot; documents (e.g. can have Author, Subject, etc).</string>
			<string id="fp_max_col_width">Maximum column width in Properties Sheet List. 0 to always autosize.</string>
			<string id="logs_path">Folder path where log files will be stored.</string>
			<string id="max_col_width">Maximum column width in Preview List. 0 to always autosize.</string>
			<string id="metadata_reading">Checking for valid items and retrieving metadata...</string>
			<string id="undo_msg">Really undo the last task? This action can&apos;t be undone. </string>
		</strings>
		<strings lang="esm">
			<string id="after_actions">Decidir entre varias opciones a realizar tras finalizar el comando.</string>
			<string id="alert_no_file">!No se puede continuar sin un archivo!</string>
			<string id="alert_no_logs">No se puede continuar sin una carpeta para los registros</string>
			<string id="close_msg">Hay %s tarea(s) pendiente(s) por aplicar. ¿Desea salir y descartarlas?</string>
			<string id="current_tasks">Tareas</string>
			<string id="debug">El nivel de registro a mostrar. OFF para mostrar solo errores. 
DEBUG para mostrar todos los mensajes. 
STANDARD para mostrar solo la información más relevante. 
WARNING para mostrar mensajes que necesitan tu atención.</string>
			<string id="dialog_mode">Selecciona el diseño para el diálogo a utilizar</string>
			<string id="doc_exts">Extensiones de archivos que serán considerados como documentos &quot;verdaderos&quot; (ej. pueden tener Autor, Tema, etc).</string>
			<string id="fp_max_col_width">Máximo ancho de columna en la lista de Hoja de Propiedades. 0 para autoredimensionar siempre.</string>
			<string id="logs_path">Ruta de la carpeta donde se guardará los archivos de registro.</string>
			<string id="max_col_width">Máximo ancho de columna en la lista de Vista Previa. 0 para autoredimensionar siempre.</string>
			<string id="metadata_reading">Comprobando archivos válidos y obteniendo metadatos...</string>
			<string id="undo_msg">¿Desea deshacer la última tarea? Esta acción no se puede deshacer. </string>
		</strings>
	</resource>
	<resource name="1" type="dialog">
		<dialog height="336" lang="english" maximize="yes" minimize="yes" resize="yes" title="Find and Replace" width="672">
			<control height="8" name="find_title" title="&lt;b&gt;Find :&lt;/b&gt;" type="markuptext" width="54" x="4" y="4" />
			<control halign="left" height="12" multiline="yes" name="find_edit" type="edit" width="277" x="60" y="2" />
			<control height="8" name="replace_title" title="&lt;b&gt;Replace :&lt;/b&gt;" type="markuptext" width="54" x="4" y="18" />
			<control halign="left" height="12" multiline="yes" name="replace_edit" type="edit" width="263" x="74" y="16" />
			<control height="12" name="keywords_btn" title="▼" type="button" width="15" x="60" y="16" />
			<control height="10" name="case_cbx" title="Case sensitive" type="check" width="109" x="4" y="30" />
			<control height="10" name="ww_cbx" title="Whole words" type="check" width="108" x="122" y="30" />
			<control height="10" name="regex_cbx" title="Regular expressions" type="check" width="109" x="4" y="42" />
			<control height="10" name="diac_cbx" title="Ignore diacritics" type="check" width="212" x="122" y="42" />
			<control height="10" name="global_cbx" title="Global replacement" type="check" width="96" x="238" y="30" />
			<control halign="left" height="12" image="yes" name="search_icon" type="static" valign="center" width="15" x="4" y="64" />
			<control halign="left" height="12" name="filter_metalist_edit" type="edit" width="158" x="22" y="64" />
			<control height="12" name="clear_ml_filter_btn" title="❌" type="button" width="15" x="180" y="64" />
			<control halign="left" height="16" name="metadata_list_status" type="static" valign="center" width="134" x="202" y="60" />
			<control height="14" name="selall_btn" title="Select All" type="button" width="78" x="98" y="216" />
			<control height="14" name="selnone_btn" title="Select None" type="button" width="78" x="178" y="216" />
			<control height="14" name="invert_btn" title="Invert Selection" type="button" width="78" x="258" y="216" />
			<control checkboxes="auto" fullrow="yes" height="137" name="metadata_list" type="listview" viewmode="details" width="332" x="4" y="77">
				<columns>
					<item text=" " />
					<item text="label" />
					<item text="type" />
				</columns>
			</control>
			<control height="14" name="add_task_btn" resize="xy" title="Add Task" type="button" width="50" x="566" y="320" />
			<control enable="no" height="14" name="undo_btn" resize="xy" title="Undo" type="button" width="50" x="514" y="320" />
			<control height="8" name="status_title" resize="yw" title="&lt;#%error_text&gt;status" type="markuptext" width="506" x="4" y="326" />
			<control halign="left" height="12" image="yes" name="search2_icon" type="static" valign="center" width="15" x="342" y="12" />
			<control halign="left" height="12" name="filter_filepropslist_edit" type="edit" width="164" x="368" y="12" />
			<control height="12" name="clear_fpl_filter_btn" title="❌" type="button" width="15" x="532" y="12" />
			<control checkboxes="auto" height="80" name="files_list" nosortheader="yes" resize="wh" type="listview" viewmode="details" width="664" x="4" y="238">
				<columns>
					<item text="Filename" />
				</columns>
			</control>
			<control halign="left" height="16" name="fileprops_list_status" resize="w" type="static" valign="center" width="114" x="554" y="8" />
			<control changelinkcolor="no" height="8" name="glyph_fpfilter_btn" title="&lt;a id=&quot;glyph&quot;&gt;&lt;%ddbi:6&gt;&lt;/a&gt;" type="markuptext" width="10" x="358" y="14" />
			<control editlabels="yes" fullrow="yes" height="182" name="fileprops_list" resize="w" sort="yes" type="listview" viewmode="details" width="328" x="340" y="44">
				<columns>
					<item text="Value" />
					<item text="Label" />
					<item text="New Value" />
				</columns>
			</control>
			<control height="8" name="preview_title" title="&lt;b&gt;Preview :&lt;/b&gt;" type="markuptext" width="80" x="4" y="228" />
			<control height="8" name="metadata_list_title" title="&lt;b&gt;Metadata :&lt;/b&gt;" type="markuptext" width="80" x="4" y="54" />
			<control height="14" name="apply_btn" resize="xy" title="Apply" type="button" width="50" x="618" y="320" />
			<control height="8" name="fileprops_list_title" title="&lt;b&gt;File Properties :&lt;/b&gt;" type="markuptext" width="80" x="340" y="2" />
			<control height="16" name="fileprops_static" resize="w" type="markuptext" width="328" x="340" y="26" />
			<control changelinkcolor="no" height="8" icon="1" name="tasks_number_title" resize="x" title="&lt;b&gt;Current tasks : &lt;/b&gt;" type="markuptext" width="96" x="572" y="228" />
		</dialog>
	</resource>
	<resource name="0" type="dialog">
		<dialog height="336" lang="english" maximize="yes" minimize="yes" resize="yes" title="Find and Replace" width="668">
			<control height="8" name="find_title" title="&lt;b&gt;Find :&lt;/b&gt;" type="markuptext" width="54" x="4" y="4" />
			<control halign="left" height="12" multiline="yes" name="find_edit" type="edit" width="276" x="60" y="2" />
			<control height="8" name="replace_title" title="&lt;b&gt;Replace :&lt;/b&gt;" type="markuptext" width="54" x="4" y="18" />
			<control halign="left" height="12" multiline="yes" name="replace_edit" type="edit" width="262" x="74" y="16" />
			<control height="12" name="keywords_btn" title="▼" type="button" width="15" x="60" y="16" />
			<control height="10" name="case_cbx" title="Case sensitive" type="check" width="116" x="4" y="30" />
			<control height="10" name="ww_cbx" title="Whole words" type="check" width="110" x="122" y="30" />
			<control height="10" name="regex_cbx" title="Regular expressions" type="check" width="116" x="4" y="42" />
			<control height="10" name="diac_cbx" title="Ignore diacritics" type="check" width="206" x="122" y="42" />
			<control height="10" name="global_cbx" title="Global replacement" type="check" width="97" x="238" y="30" />
			<control halign="left" height="12" image="yes" name="search_icon" type="static" valign="center" width="15" x="4" y="64" />
			<control halign="left" height="12" name="filter_metalist_edit" type="edit" width="158" x="22" y="64" />
			<control height="12" name="clear_ml_filter_btn" title="❌" type="button" width="15" x="180" y="64" />
			<control halign="left" height="14" name="metadata_list_status" type="static" valign="center" width="134" x="202" y="62" />
			<control height="14" name="selall_btn" title="Select All" type="button" width="78" x="98" y="196" />
			<control height="14" name="selnone_btn" title="Select None" type="button" width="78" x="178" y="196" />
			<control height="14" name="invert_btn" title="Invert Selection" type="button" width="78" x="258" y="196" />
			<control checkboxes="auto" fullrow="yes" height="117" name="metadata_list" type="listview" viewmode="details" width="332" x="4" y="77">
				<columns>
					<item text=" " />
					<item text="label" />
					<item text="type" />
				</columns>
			</control>
			<control height="14" name="add_task_btn" resize="xy" title="Add Task" type="button" width="50" x="562" y="320" />
			<control enable="no" height="14" name="undo_btn" resize="xy" title="Undo" type="button" width="50" x="510" y="320" />
			<control height="8" name="status_title" resize="yw" title="&lt;#%error_text&gt;status" type="markuptext" width="164" x="342" y="324" />
			<control halign="left" height="12" image="yes" name="search2_icon" type="static" valign="center" width="15" x="6" y="218" />
			<control halign="left" height="12" name="filter_filepropslist_edit" type="edit" width="148" x="32" y="218" />
			<control height="12" name="clear_fpl_filter_btn" title="❌" type="button" width="15" x="180" y="218" />
			<control checkboxes="auto" height="304" name="files_list" nosortheader="yes" resize="wh" type="listview" viewmode="details" width="324" x="340" y="14">
				<columns>
					<item text="Filename" />
				</columns>
			</control>
			<control changelinkcolor="no" height="8" name="glyph_fpfilter_btn" title="&lt;a id=&quot;glyph&quot;&gt;&lt;%ddbi:6&gt;&lt;/a&gt;" type="markuptext" width="10" x="22" y="220" />
			<control editlabels="yes" fullrow="yes" height="84" name="fileprops_list" resize="h" sort="yes" type="listview" viewmode="details" width="332" x="4" y="250">
				<columns>
					<item text="Value" />
					<item text="Label" />
					<item text="New Value" />
				</columns>
			</control>
			<control height="8" name="preview_title" title="&lt;b&gt;Preview :&lt;/b&gt;" type="markuptext" width="80" x="340" y="3" />
			<control height="8" name="metadata_list_title" title="&lt;b&gt;Metadata :&lt;/b&gt;" type="markuptext" width="80" x="4" y="54" />
			<control height="14" name="apply_btn" resize="xy" title="Apply" type="button" width="50" x="614" y="320" />
			<control height="8" name="fileprops_list_title" title="&lt;b&gt;File Properties :&lt;/b&gt;" type="markuptext" width="92" x="4" y="208" />
			<control halign="left" height="14" name="fileprops_list_status" type="static" valign="center" width="134" x="202" y="216" />
			<control height="16" name="fileprops_static" type="markuptext" width="332" x="4" y="232" />
			<control changelinkcolor="no" height="8" icon="1" name="tasks_number_title" resize="x" title="&lt;b&gt;Current tasks : &lt;/b&gt;" type="markuptext" width="96" x="568" y="3" />
		</dialog>
	</resource>
	<resource name="2" type="dialog">
		<dialog height="298" lang="english" maximize="yes" minimize="yes" resize="yes" title="Find and Replace" width="692">
			<control height="8" name="find_title" title="&lt;b&gt;Find :&lt;/b&gt;" type="markuptext" width="58" x="4" y="4" />
			<control halign="left" height="12" multiline="yes" name="find_edit" resize="w" type="edit" width="287" x="63" y="2" />
			<control height="8" name="replace_title" title="&lt;b&gt;Replace :&lt;/b&gt;" type="markuptext" width="58" x="4" y="18" />
			<control halign="left" height="12" multiline="yes" name="replace_edit" resize="w" type="edit" width="272" x="78" y="16" />
			<control height="12" name="keywords_btn" title="▼" type="button" width="15" x="63" y="16" />
			<control height="10" name="case_cbx" title="Case sensitive" type="check" width="126" x="4" y="30" />
			<control height="10" name="ww_cbx" title="Whole words" type="check" width="112" x="136" y="30" />
			<control height="10" name="regex_cbx" title="Regular expressions" type="check" width="126" x="4" y="42" />
			<control height="10" name="diac_cbx" title="Ignore diacritics" type="check" width="166" x="136" y="42" />
			<control height="10" name="global_cbx" title="Global replacement" type="check" width="96" x="254" y="30" />
			<control halign="left" height="12" image="yes" name="search_icon" resize="x" type="static" valign="center" width="15" x="354" y="12" />
			<control halign="left" height="12" name="filter_metalist_edit" resize="x" type="edit" width="159" x="372" y="12" />
			<control height="12" name="clear_ml_filter_btn" resize="x" title="❌" type="button" width="15" x="531" y="12" />
			<control halign="left" height="16" name="metadata_list_status" resize="x" type="static" valign="center" width="137" x="550" y="8" />
			<control height="14" name="selall_btn" resize="x" title="Select All" type="button" width="78" x="449" y="162" />
			<control height="14" name="selnone_btn" resize="x" title="Select None" type="button" width="78" x="529" y="162" />
			<control height="14" name="invert_btn" resize="x" title="Invert Selection" type="button" width="78" x="609" y="162" />
			<control checkboxes="auto" fullrow="yes" height="135" name="metadata_list" resize="x" type="listview" viewmode="details" width="333" x="354" y="25">
				<columns>
					<item text=" " />
					<item text="label" />
					<item text="type" />
				</columns>
			</control>
			<control height="14" name="add_task_btn" resize="xy" title="Add Task" type="button" width="50" x="248" y="282" />
			<control enable="no" height="14" name="undo_btn" resize="xy" title="Undo" type="button" width="50" x="196" y="282" />
			<control height="8" name="status_title" resize="yw" title="&lt;#%error_text&gt;status" type="markuptext" width="190" x="4" y="286" />
			<control halign="left" height="12" image="yes" name="search2_icon" resize="x" type="static" valign="center" width="15" x="355" y="186" />
			<control halign="left" height="12" name="filter_filepropslist_edit" resize="x" type="edit" width="151" x="381" y="186" />
			<control height="12" name="clear_fpl_filter_btn" resize="x" title="❌" type="button" width="15" x="531" y="186" />
			<control checkboxes="auto" height="216" name="files_list" nosortheader="yes" resize="wh" type="listview" viewmode="details" width="346" x="4" y="64">
				<columns>
					<item text="Filename" />
				</columns>
			</control>
			<control halign="left" height="16" name="fileprops_list_status" resize="x" type="static" valign="center" width="137" x="550" y="182" />
			<control changelinkcolor="no" height="8" name="glyph_fpfilter_btn" resize="x" title="&lt;a id=&quot;glyph&quot;&gt;&lt;%ddbi:6&gt;&lt;/a&gt;" type="markuptext" width="10" x="371" y="188" />
			<control editlabels="yes" fullrow="yes" height="78" name="fileprops_list" resize="xh" sort="yes" type="listview" viewmode="details" width="333" x="355" y="218">
				<columns>
					<item text="Value" />
					<item text="Label" />
					<item text="New Value" />
				</columns>
			</control>
			<control height="8" name="preview_title" title="&lt;b&gt;Preview :&lt;/b&gt;" type="markuptext" width="80" x="4" y="54" />
			<control height="8" name="metadata_list_title" resize="x" title="&lt;b&gt;Metadata :&lt;/b&gt;" type="markuptext" width="80" x="354" y="2" />
			<control height="14" name="apply_btn" resize="xy" title="Apply" type="button" width="50" x="300" y="282" />
			<control height="8" name="fileprops_list_title" resize="x" title="&lt;b&gt;File Properties :&lt;/b&gt;" type="markuptext" width="80" x="355" y="176" />
			<control height="16" name="fileprops_static" resize="x" type="markuptext" width="333" x="355" y="200" />
			<control changelinkcolor="no" height="8" icon="1" name="tasks_number_title" resize="x" title="&lt;b&gt;Current tasks : &lt;/b&gt;" type="markuptext" width="96" x="254" y="54" />
		</dialog>
	</resource>
</resources>
