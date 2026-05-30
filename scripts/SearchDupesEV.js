/* SearchDupes with Everything Command for Directory Opus
**Search for dupes with Everything and DOpus**
SearchDupes © 2024 by Christian Arellano García is licensed under CC BY-NC-ND 4.0 
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 


// CASE 1 : only files selected									=> search dupes globally for the files selected
// CASE 2 : 1 or more files selected, plus 1 or more folders	=> search dupes for the files selected only in the selected folders
// CASE 3 : 1 or more folders selected							=> search dupes included only in those folders
// IF no selection												=> search dupes for all files in source/dest if possible (no recursive)
// IF LOCAL														=> convert CASE 1 to CASE 2 using source path as a folder. Do not combine with others args
// IF READFOLDER												=> convert CASE 2 and 3 to CASE 1
// IF USEDEST													=> add destination items to all cases above
// IF RECURSIVE													=> add recursion for CASE 2 and 3 if READFOLDER is also used
*/

var aborted_files = 0;
var aborted_dirs = false;
var script_name = 'Everything Dupes';
var script_version = '1.6.1';
var FSU;
// Called by Directory Opus to initialize the script
function OnInit(initData) {
	initData.name = script_name;
	initData.version = script_version;
	initData.copyright = '(c) 2024 Christian Arellano García';
	initData.url = 'https://resource.dopus.com/t/everything-dupes-search-for-duplicates-with-dopus-and-everything/47412';
	initData.desc = 'Search for dupes with Everything and Directory Opus';
	initData.default_enable = true;
	initData.min_version = '13.6';
	initData.config_desc = DOpus.Create.Map();
	initData.config_groups = DOpus.Create.Map();
	AddConfig('log level', DOpus.Create.Vector(2, 'debug', 'standard', 'warning', 'off'), DOpus.strings.Get('debug'), 'Script Log');
	AddConfig('collection name', '', DOpus.strings.Get('coll_name'), 'Results Collection');
	AddConfig('use MD5', false, DOpus.strings.Get('use_MD5'), 'Search Options');
	AddConfig('delete on startup', false, DOpus.strings.Get('delete_onstartup'), 'Results Collection');
	AddConfig('create Everything collection', true, DOpus.strings.Get('ev_coll'), 'Search Options');
	AddConfig('max secs timeout', 60, DOpus.strings.Get('max_timeout'), 'Search Options');
	AddConfig('select for delete', true, DOpus.strings.Get('select_delete'), 'Search Options');
	AddConfig('min items to show progress', 10, DOpus.strings.Get('min_progress'), 'Search Options');

	function AddConfig(name, value, desc, group) {
		initData.config[name] = value;
		initData.config_desc(name) = desc;
		initData.config_groups(name) = group;
	}
}

// Called when Directory Opus starts up
function OnStartup(startupData) {
	if (Script.config['delete on startup']) DeleteColl();
}

// Called to add commands to Opus
function OnAddCommands(addCmdData) {
	var cmd = addCmdData.AddCommand();
	cmd.name = 'EVDupes';
	cmd.method = 'OnSearchDupesEV';
	cmd.desc = 'Search for dupes with Everything and Directory Opus';
	cmd.label = script_name;
	cmd.template = 'NODODUPES/S,MD5/O[<yes>,no],IN/K/M,FILES/K/M,PROPERTIES/K[35mmfocallength,accessed,altitude,apertureval,aspectratio,attr,audiocount,author,cameramake,cameramodel,category,companyname,composers,conductors,contrast,copyright,created,datarate,datedigitized,datetaken,digitalzoom,director,doccreateddate,doclastsavedby,doclastsaveddate,duration,exposurebias,exposureprogram,exposuretime,ext,focallength,framerate,keywords,latitude,lensmake,lensmodel,longitude,meteringmode,moddesc,modified,modversion,mp3album,mp3albumartist,mp3artists,mp3bitrate,mp3bpm,mp3genre,mp3title,mp3track,mp3year,name,pages,path,picdepth,picheight,picresx,picresy,picsize,picwidth,prodname,prodversion,publisher,rating,releasedate,saturation,sharpness,size,software,subject,subjectdistance,subtitlecount,title,videocount,whitebalance],READFOLDER/S,USEDEST/S,RECURSIVE/S,LOCAL/S';
	cmd.hide = false;
	cmd.icon = 'dupepane';
}
// Implement the SearchDupesEV command
function OnSearchDupesEV(scriptCmdData) {
	DOpus.ClearOutput();
	//=== CHECK EVERYTHING PROCESS ====================================================================
	try {
		var ev_path;
		var wmi = GetObject('winmgmts://./root/CIMv2');
		var v = wmi.ExecQuery('SELECT ExecutablePath FROM Win32_Process WHERE name="Everything64.exe"');
		for (var e = new Enumerator(v); !e.atEnd(); e.moveNext()) {
			if (!e.item()) continue;
			ev_path = e.item().ExecutablePath;
			break;
		}
		wmi = null;
		FSU = DOpus.FSUtil();
		if (!ev_path) {
			Log(1, 'Probably not enough privileges to query using WMI. Just check if EV is running then') //not enough privileges?
			if (!DOpus.Create.SysInfo().FindProcess('Everything64.exe')) {
				Log(4, 'Everything v1.5 is not running in background!!');
				alert(scriptCmdData.func.sourcetab, DOpus.strings.Get('alert_no_Ev'));
				return;
			}
		}
		else {
			ev_path = String(ev_path).trim();
			Log(1, 'EV Path     : ' + ev_path);	
			v = FSU.GetMetadata(ev_path);
			v = v == 'exe' ? v.exe.prodversion : null;
			if (v != null) {
				Log(1, 'EV version  : ' + v);
				if (v && parseInt(v.replace(/\./g, '')) < 1501370) {
					Log(4, 'This command needs at least Everything v1.5.0.1370 to run!');
					alert(scriptCmdData.func.sourcetab, DOpus.strings.Get('alert_no_minver'));
					return;
				}
			}
			if (!v) {
				Log(4, 'Unable to tell Everything version!!');
				alert(scriptCmdData.func.sourcetab, 'Unable to tell Everything version');
				return;
			}
		}
	}
	catch (err) {
		Log(4, 'Error when trying to get Everything process info! : ' + err);
		return;
	}
	var s_tab = scriptCmdData.func.sourcetab;
	//=== CHECK FOR PATHS TYPES ========================================================================
	//if no tab, path doesn't exists or is not a coll or a file system type, is not valid
	if (!s_tab || !FSU.Exists(s_tab.path) || !/filesys|coll/.test(FSU.PathType(s_tab.path))) s_tab = false;
	//use dest only if valid and by user decision
	//Exit if no valid source path
	if (!s_tab) {
		Log(4, 'Source path must be a valid filesystem folder or a collection!!');
		alert(null, 'Source path must be a valid filesystem folder or a collection!!');
		return;
	}
	var args = scriptCmdData.func.argsmap;
	var props_map = GetProps(args.Exists('PROPERTIES') ? args('PROPERTIES') : '');
	if (props_map.empty) {
		Log(4, 'Unable to continue without a property!!');
		alert(s_tab, 'Unable to continue without a property!!');
		return;
	}
	var ini_time = new Date();
	var d_tab = scriptCmdData.func.desttab;
	if (!d_tab || !FSU.Exists(d_tab.path) || !/filesys|coll/.test(FSU.PathType(d_tab.path)) || d_tab.path != s_tab.path) var use_dest = false;
	else var use_dest = args.Exists('USEDEST');
	//=== INIT VARS ===================================================================================
	var lister = s_tab.lister;
	var cmd = DOpus.Create.Command();
	//=== INIT PROGRESS DIALOG =======================================================================
	var min_progress = Script.config['min items to show progress'];
	if (min_progress <= 0) min_progress = 10;
	var helper = 0;
	if (args.Exists('FILES')) helper = args('FILES').count;
	else {
		helper = s_tab.selected_files.count;
		if (use_dest) helper += d_tab.selected_files.count;
	}
	if (args.Exists('IN')) helper += args('IN').count;
	else {
		helper += s_tab.selected_dirs.count;
		if (use_dest) helper += d_tab.selected_dirs.count;
	}
	if (!helper) {
		Log(4, 'Unable to continue without a file!!');
		alert(s_tab, 'Unable to continue without a file!!');
		return;
	}

	//=== CHECK FOR IN / FILE ARGS =======================================================================
	var read_folder = args.Exists('READFOLDER');
	var recursive = args.Exists('RECURSIVE');
	var local = args.Exists('LOCAL');
	var nododupes = args.Exists('NODODUPES');
	var progress = (helper >= min_progress || read_folder) ? scriptCmdData.func.command.progress : null;
	if (progress) {
		progress.abort = true;
		progress.owned = true;
		progress.bytes = false;
		progress.pause = true;
		progress.delay = false;
		progress.skip = false;
		progress.Init(lister, script_name + ' v' + script_version + ' : Enumerating items...');
		progress.Show();
		progress.SetFiles(helper);
	}
	Log(2, 'cmdline     : ' + scriptCmdData.cmdline);
	Log(1, 'SOURCE TAB  : ' + s_tab.path);
	Log(1, 'INCL. DEST  : ' + use_dest);
	Log(1, 'READFOLDER  : ' + read_folder);
	Log(1, 'RECURSIVE   : ' + recursive);
	Log(1, 'LOCAL       : ' + local);
	Log(1, 'MIN PROGRESS: ' + min_progress);
	Log(1, 'NO DO DUPES : ' + nododupes);
	var files = DOpus.Create.Vector();
	var dirs = DOpus.Create.Vector();
	//=== GETTING SELECTED SOURCE ITEMS ==================================================================
	if (args.Exists('FILES')) {
		Log(1, 'FILES arguments was used. Checking for valid files...');
		if (progress) progress.SetStatus('Enumerating files...');
		//check for valid items
		for (var i = 0; i < args('FILES').count; i++) {
			if (progress) {
				if (progress.GetAbortState(true) === 'a') {
					aborted_files = 1;
					break;
				}
				progress.SetName(args('FILES')(i));
				progress.StepFiles(1);
			}
			item = FSU.GetItem(args('FILES')(i));
			Log(1, '   file => ' + item);
			if (FSU.Exists(item) && !item.is_dir) files.push_back(item);
			else if (progress) progress.SkipFile(false);
		}
	}
	else {
		if (progress) progress.StepFiles(s_tab.selected_files.count);
		files.append(s_tab.selected_files);
	}
	if (args.Exists('IN')) {
		use_dest = false;
		Log(1, 'IN arguments was used. Checking for valid paths...');
		if (progress) progress.SetStatus('Enumerating folders...');
		//check for valid items
		for (var i = 0; i < args('IN').count; i++) {
			if (progress) {
				if (progress.GetAbortState(true) === 'a') {
					aborted_dirs = true;
					break;
				}
				progress.SetName(args('IN')(i));
				progress.StepFiles(1);
			}
			item = FSU.GetItem(args('IN')(i));
			Log(1, '   dir => ' + item.realpath);
			if (FSU.Exists(item) && item.is_dir) {
				if (read_folder) files.append(readFolder(progress, item.realpath, recursive));
				else if (FSU.PathType(item) == 'filesys') dirs.push_back(item.realpath);
			}
			else if (progress) progress.SkipFile(false);
		}
	}
	else {
		if (read_folder) files.append(readFolders(progress, s_tab.selected_dirs, recursive));
		else dirs.append(readFolders(progress, s_tab.selected_dirs, false, true));
	}
	//=== GETTING SELECTED DEST ITEMS ====================================================================
	if (use_dest) {
		Log(1, 'DEST TAB    : ' + d_tab.path);
		if (progress) progress.StepFiles(d_tab.selected_files.count);
		files.append(d_tab.selected_files);
		//read folder contents for files to use, optionally recursive
		if (read_folder) files.append(readFolders(progress, d_tab.selected_dirs, recursive));
		else dirs.append(readFolders(progress, d_tab.selected_dirs, false, true));
	}

	//=== SPECIAL CASES ===========================================================================
	//if there's no folder selected, and LOCAL is set, use source path as a folder for compare
	if (dirs.empty && !files.empty && local && FSU.PathType(s_tab.path) == 'filesys') {
		Log(1, 'Use ' + s_tab.path + ' as a delimiter for results');
		//save source path as a folder, without reading their content
		dirs.push_back(s_tab.path);
	}
	//if there's no items selected, try to get source/dest path content
	if (!aborted_dirs && !aborted_files && dirs.empty && files.empty) {
		Log(3, 'No items to use. Adding source / dest files');
		files.append(s_tab.files);
		if (use_dest) files.append(d_tab.files);
	}
	//delete dupes in files and dirs;
	files.unique();
	dirs.unique();
	//=== BUILDING PROPERTIES =======================================================================
	//continue only if there's items loaded
	if (!dirs.empty || !files.empty) {
		var props = props_map('props');
		var cols = GetColsNames(props);
		var use_MD5 = !args.Exists('MD5') ? (Script.config['use MD5'] && (props.Exists('name') || props.Exists('size'))) : ((typeof args('MD5') === 'string' && args('MD5').toLowerCase() === 'no') ? false : true);
		var EV_has_own_coll = (use_MD5 && nododupes) ? true : Script.config['create Everything collection'];
		var max_timeout_ms = Script.config['max secs timeout']
		if (max_timeout_ms < 0) max_timeout_ms = 0;
		else if (max_timeout_ms > 3600) max_timeout_ms = 3600;
		max_timeout_ms = max_timeout_ms * 1000; //in milliseconds
		if (progress) {
			progress.Restart();
			progress.SetTitle(script_name + ' v' + script_version + ' - Reading : ' + files.count + ' files');
		}
		Log(1, 'USE SIZE    : ' + props.Exists('size'));
		Log(1, 'USE NAME    : ' + props.Exists('name'));
		Log(1, 'USE EXT     : ' + props.Exists('ext'));
		Log(1, 'COLLNAME    : ' + Script.config['collection name']);
		Log(1, 'USE MD5     : ' + use_MD5 + ' (' + Script.config['use MD5'] + ')');
		Log(1, 'EV COLL     : ' + EV_has_own_coll + ' (' + Script.config['create Everything collection'] + ')');
		Log(1, 'SEL TO DEL  : ' + Script.config['select for delete']);
		Log(1, 'MAX TIMEOUT : ' + max_timeout_ms + ' (' + Script.config['max secs timeout'] + ')');
		//if use_name alone, dupes by name no ext
		var dupes_search_mode = (props.Exists('name') && props.count === 1) ? 1 : 0;
		//=== PREPARING COMMAND LINE =======================================================================
		var query = '';
		helper = '';
		var s_case;
		Log(2, 'PROPERTIES  : ' + props.count);
		try {
			//=== CASE 1 =================================================================================
			////<size:file1.size ext:file1.ext>|<size:file2.size ext:file2.ext>... file:
			if (!files.empty && dirs.empty) {
				s_case = files.count + ' file(s) only';
				Log(2, 'CASE 1      : ' + s_case);
				query = GetItemsValues();
				if (query) query += ' file:';
			}
			//=== CASE 2 ===================================================================================
			////"folder1\"|"folder2\"|... <size:file1.size ext:file1.ext>|<size:file2.size ext:file2.ext>...>|<<file1>|<file2>|...>
			else if (!files.empty && !dirs.empty) {
				s_case = files.count + ' file(s), ' + dirs.count + ' folder(s)';
				Log(2, 'CASE 2      : ' + s_case);
				query = GetItemsValues();
				if (query) {
					for (var i = 0; i < dirs.count; i++)
						helper += '"' + dirs(i) + '\\"|';
					query = '< ' + helper.slice(0, -1) + ' ' + query + '>|';
					if (!aborted_files) aborted_files = files.count;
					Log(1, ' => Files   : ' + aborted_files);
					for (var i = 0; i < aborted_files; i++)
						query += '<<' + files(i) + '>|';
					query = query.slice(0, -1) + ' file:';
				}
			}
			//=== CASE 3 ==================================================================================
			////"folder1\"|"folder2\"|... file:dupe:...
			else if (files.empty && !dirs.empty) {
				s_case = dirs.count + ' folder(s), no files';
				Log(2, 'CASE 3      : ' + s_case);
				helper = GetItemsValues(true);
				for (var i = 0; i < dirs.count; i++) {
					item = dirs(i);
					query += '"' + item + '\\"|';
				};
				query = query.slice(0, -1) + ' file:dupe:' + helper;
			}
		}
		catch (err) {
			Log(3, 'Error while parsing query : ' + err.description);
			query = null;
		}
		if (progress) {
			progress.SetFilesProgress(files.count);
			progress.ClearAbortState();
			progress.Hide();
		}
		//=== PREPARING RESULTS COLLECTION =============================================================
		if (query) {
			//Don't print more than 2000 characters at the same time cause otherwise DOpus may crash :)
			Log(1, 'query       : ' + query.substring(0, 2000) + (query.length > 2000 ? ('... (+' + (query.length - 2000) + ' characters)') : ''));
			var cmdline;
			cmd.ClearFiles();
			var str_tools = DOpus.Create.StringTools();
			var coll_name = (Script.config['collection name']) ? ('coll://' + str_tools.MakeLegal(Script.config['collection name'].trim(), 'n')) : 'coll://Everything Dupes';
			Script.vars.Set('collection_name', coll_name);
			Script.vars('collection_name').persist = true;
			coll_name += '/' + DOpus.Create.Date.Format('D#yyyy-MM-dd T#HHmmss');
			//=== BUILDING AND RUNNING THE COMMAND=====================================================

			if (!EV_has_own_coll) { //if Everything results has their own collection, 
				Log(1, 'Skipping Everything collection creation');
				helper = 1;
			}
			else {
				Log(2, 'Everything results will be shown in a separate collection');
				var progress = scriptCmdData.func.command.progress;
				progress.abort = true;
				progress.owned = true;
				progress.bytes = false;
				progress.pause = false;
				progress.delay = false;
				progress.skip = false;
				progress.Init(lister, script_name + ' v' + script_version + ' : Running Everything Search...');
				progress.Show();
				var EV_coll_name = coll_name + '/Everything';
				coll_name += '/DOpus';
				Log(1, 'EV collname : ' + EV_coll_name);
				helper = 0;
				cmdline = 'FIND CLEAR QUERYENGINE=everythingglobal COLLNAME="' + EV_coll_name + '" SHOWRESULTS=source,tab QUERY ';
				Log(1, 'command     : ' + cmdline + '$query');
				if (cmd.RunCommand(cmdline + query)) {
					Log(2, 'Waiting for Everything results...');
					var ini_timeout = new Date();
					var curr_timeout;
					lister.Update();
					s_tab = lister.activetab;
					progress.SetFromTo('Everything Search', s_case, cols);
					do {
						try {
							if (progress.GetAbortState(true) === 'a') {
								cmd.RunCommand('Go TABCLOSE=' + s_tab);
								Log(2, '=> User aborted search...');
								if (!helper) helper = false;
								break;
							}
							curr_timeout = new Date() - ini_timeout;
							if (!s_tab.stats || (max_timeout_ms && curr_timeout > max_timeout_ms)) break;
							progress.SetName('Time passed: ' + curr_timeout + ' ms. Max timeout: ' + max_timeout_ms + ' ms');
							s_tab.Update();
							helper = s_tab.stats.items;
							if (helper > 0) break;
							DOpus.Delay(100);
						}
						catch (err) {
							break;
						}
					} while (true);
					Log(2, 'EV results  : ' + helper + ' items in ' + curr_timeout + ' ms');
					progress.SetFilesProgress(1);
					progress.ClearAbortState();
					progress.Hide();
				}
				if (nododupes && cols) {
					Log(2, 'Add columns : ' + cols);
					cmd.RunCommand('Set COLUMNSADD=+' + cols);
				}
			}
			Log(1, 'DO collname : ' + coll_name);
			//if there's no results, don't perform dupe find, unless EV doesn't have a collection, so the dupe find use the everything query
			if (!nododupes) {
				if (helper !== false) {
					DOpus.Delay(100);
					Log(2, 'Searching with DOpus dupe finder...')
					cmdline = 'IGNORELINKS SHOWRESULTS=dest,tab COLLNAME="' + coll_name + (EV_has_own_coll ? ('" IN "' + EV_coll_name + '"') : ('" QUERYENGINE=everythingglobal QUERY '));
					if (use_MD5) cmdline = 'MD5=cache,85 ' + cmdline;
					if (dupes_search_mode === 1) {
						cmdline = 'FIND CLEAR DUPES NAMEONLY=noext ' + cmdline;
						Log(1, 'SEARCH MODE : nameonly=noext');
					}
					else {
						cmdline = 'FIND CLEAR DUPES COLUMN=' + cols + ' ' + cmdline;
						Log(1, 'SEARCH MODE : columns=' + cols);
					}
					Log(1, 'command     : ' + cmdline + (EV_has_own_coll ? '' : '$query'));
					cmd.RunCommand(cmdline + (EV_has_own_coll ? '' : query));
					if (cmd.results.result != 0) {
						DOpus.Delay(100);
						lister.Update();
						d_tab = lister.desttab;
						//if success in run DOpus DupeFinder
						if (d_tab && d_tab.path == coll_name) {
							if (!d_tab.source) cmd.RunCommand('Set FOCUS=Dest');
							cmd.SetSourceTab(d_tab);
							//Check if active tab is the results collection and has items
							Log(2, 'Total dupes : ' + d_tab.all.count);
							if (d_tab.all.count > 0) {
								cmd.RunCommand('Set GROUPBY=dupes');
								if (cols) {
									Log(2, 'Add columns : ' + cols);
									cmd.RunCommand('Set COLUMNSADD=+' + cols);
								}
								//running with dopusrt.exe so the script doesn't get stuck in here
								if (Script.config['select for delete']) {
									Log(2, 'Showing the select dupes dialog...');
									cmd.RunCommand('Set CHECKBOXMODE=on');
									cmd.RunCommand('/home\\dopusrt.exe /acmd Select DUPES');
								}
							}
							else
								Log(2, 'No items found.');
						}
						else
							Log(3, 'Wrong tab have focus : ' + d_tab.path + '; expected ' + coll_name);
					}
					else Log(4, 'Error running the command for Opus dupe finder');
				}
				else Log(3, 'No results or results tab was closed');
			}
		}
		else
			Log(4, 'No query available! ');
	}
	else {
		Log(3, 'There\'s no items to search!!!');
		alert(s_tab, 'There\'s no items to search!!!');
	}
	props_map = null;
	props = null;
	files = null;
	dirs = null;
	s_tab = null;
	d_tab = null;
	lister = null;
	util = null;
	FSU = null;
	args = null;
	progress = null;
	cmd = null;
	CollectGarbage();
	Log(2, 'COMMAND FINISHED IN ' + (new Date() - ini_time) + ' ms');
	Log(2, '=============================================================');
	return;

	//return desired values from an item
	function GetItemsValues(EVprops_only) {
		var value = '';
		if (EVprops_only) {
			for (var i = 0; i < props.count; i++)
				value += props_map(props(i))('EV_property') + ';';
		}
		else {
			var item_value, item, curr_prop;
			var values_map = DOpus.Create.Map();
			if (progress) progress.SetFiles(files.count);
			outer: for (var j = 0; j < files.count; j++) {
				item = files(j);
				if (progress) {
					progress.SetName(item);
					progress.StepFiles(1);
					progress.SetStatus('Reading ' + cols);
				}
				for (var i = 0; i < props.count; i++) {
					if (progress && progress.GetAbortState(true) === 'a') {
						aborted_files = j;
						break outer;
					}
					curr_prop = props_map(props(i));
					try {
						if (!curr_prop('meta_group')) { //means special treatment
							switch (curr_prop('property')) {
								case 'ext':
									if (item_value = item.ext) item_value = item_value.slice(1);
									break;
								case 'name':
									item_value = item.name_stem_m;
									break;
								case 'size':
									item_value = item.size;
									break;
								case 'path':
									item_value = item.realpath.pathpart;
									break;
								case 'attr':
									if (item_value = item.attr_text) item_value = item_value.replace(/\-/g, '');
									break;
								case 'rating':
									if (item_value = item.metadata.other.rating !== undefined) item_value += '/5';
									break;
								case 'keywords':
									item_value = DOpus.Create.UnorderedSet();
									for (var t = 0; t < item.metadata.tags.count; t++) item_value.insert('"' + item.metadata.tags(t) + '"');
									break;
								case 'created':
								case 'modified':
									item_value = item[curr_prop('property')];
									break;
								default:
									item_value = null;
									Log(2, curr_prop('property') + ' is not a recognized value');
									break;
							}
						}
						else if (curr_prop('property') === 'picsize') {
							if (item_value = item.metadata[curr_prop('meta_group')]['picsize']) item_value = item_value.replace(/(.*) x.*/, "$1");
						}
						else
							item_value = item.metadata[curr_prop('meta_group')][curr_prop('property')];
						if (item_value !== null || item_value !== undefined || item_value !== '') {
							if (!values_map.Exists(curr_prop('property'))) values_map(curr_prop('property')) = DOpus.Create.StringSetI();
							switch (curr_prop('type')) {
								case 'date':
									item_value = item_value.Format('', 'nS');
									values_map(curr_prop('property')).insert(item_value);
									break;
								case 'string':
									if (item_value.indexOf(' ') !== -1) item_value = '"' + item_value + '"';
									values_map(curr_prop('property')).insert(item_value);
									break;
								case 'tags_str':
									values_map(curr_prop('property')).merge(item_value);
									break;
								case 'int':
									if (typeof item_value === 'string') item_value = item_value.replace(/[^\d\-]/g, '');
									if (item_value) values_map(curr_prop('property')).insert(item_value);
									break;
								case 'float':
									if (typeof item_value === 'string') item_value = parseFloat(item_value);
									if (item_value) values_map(curr_prop('property')).insert(item_value);
									break;
							}
						}
					}
					catch (e) {
						Log(2, 'Error retrieving ' + curr_prop('property') + ' for ' + item + ' : ' + e);
						if (progress) progress.SkipFile(false);
						continue;
					}
				}
			}
			for (var e = new Enumerator(values_map); !e.atEnd(); e.moveNext()) {
				item_value = values_map(e.item());
				if (!item_value.empty) {
					value += props_map(e.item())('EV_prefix') + props_map(e.item())('EV_property') + ':';
					for (var i = 0; i < item_value.count; i++)
						value += item_value(i) + ';';
					value = value.slice(0, -1) + ' ';
				}
				else value += '!' + props_map(e.item())('EV_property') + ': ';
			}
			item_value = null;
			values_map = null;
		}
		return value.slice(0, -1);
	}
}
//read multiple folders, optionally recursive, and return a vector of their files as item objects
function readFolders(progress, folders, recursive, dirs_only) {
	var item;
	var res = DOpus.Create.Vector();
	for (var i = new Enumerator(folders); !i.atEnd(); i.moveNext()) {
		item = i.item();
		try {
			if (progress && progress.GetAbortState(true) === 'a') {
				aborted_dirs = true;
				break;
			}
			//we only need the folder itself but it must be a file system folder
			if (dirs_only) {
				if (FSU.PathType(item) !== 'filesys') {
					Log(3, item + ' can\'t be used in this scenario!');
					if (progress) progress.SkipFile(false);
					continue;
				}
				if (progress) {
					progress.SetName(item);
					progress.StepFiles(1);
				}
				res.push_back(item);
			}
			//read folder contents
			else res.append(readFolder(progress, item, recursive));
		}
		catch (err) {
			Log(3, '=> Error when reading ' + item + ' : ' + err.description);
			progress.SkipFile(false);
			continue;
		}
	}
	return res;
}
//read a folder, optionally recursive, and return a vector of their files as item objects
function readFolder(progress, folder, recursive) {
	var item;
	var items = DOpus.Create.Vector();
	try {
		Log(1, 'Reading ' + folder + '. Please wait...');
		//folder must be  a collection or a file system folder
		if (!/filesys|coll/.test(FSU.PathType(folder))) {
			Log(3, item + ' can\'t be used in this scenario!');
			if (progress) progress.SkipFile(false);
			return;
		}
		var f_enum = FSU.ReadDir(folder, (recursive) ? 'r' : '');
		if (progress) progress.SetStatus('Reading ' + folder);
		if (f_enum.error == 0) {
			while (!f_enum.complete) {
				item = f_enum.Next();
				if (item.is_dir) continue;
				if (progress) {
					progress.AddFiles(1);
					if (progress.GetAbortState(true) === 'a') {
						aborted_dirs = true;
						break;
					}
					progress.SetName(item);
					progress.StepFiles(1);
				}
				items.push_back(item);
			}
			f_enum.Close();
		}
		else Log(2, 'Error when trying to read ' + folder);
		f_enum = null;
	}
	catch (err) {
		Log(3, '=> Error when reading ' + folder + ' : ' + err.description);
		progress.SkipFile(false);
	}
	return items;
}

//return DOpus properties names in a map
function GetProps(props) {
	var values_set = DOpus.Create.Map();
	props = props.trim().toLowerCase();
	if (!props) props = 'size,ext';
	props = props.split(',');
	for (var i = 0; i < props.length; i++) {
		switch (props[i]) {
			case 'name':
				values_set('name') = DOpus.Create.Map('is_metaproperty', false, 'meta_group', false, 'EV_property', 'stem', 'property', 'name', 'EV_prefix', '', 'type', 'string');
				break;
			case 'ext':
				values_set('ext') = DOpus.Create.Map('is_metaproperty', false, 'meta_group', false, 'EV_property', 'ext', 'property', 'ext', 'EV_prefix', 'exact:', 'type', 'string');
				break;
			case 'created':
				values_set('created') = DOpus.Create.Map('is_metaproperty', false, 'meta_group', false, 'EV_property', 'dc', 'property', 'created', 'EV_prefix', '', 'type', 'date');
				break;
			case 'modified':
				values_set('modified') = DOpus.Create.Map('is_metaproperty', false, 'meta_group', false, 'EV_property', 'dm', 'property', 'modified', 'EV_prefix', '', 'type', 'date');
				break;
			case 'size':
				values_set('size') = DOpus.Create.Map('is_metaproperty', false, 'meta_group', false, 'EV_property', 'size', 'property', 'size', 'EV_prefix', '', 'type', 'int');
				break;
			case 'path':
				values_set('path') = DOpus.Create.Map('is_metaproperty', false, 'meta_group', false, 'EV_property', 'path', 'property', 'path', 'EV_prefix', '', 'type', 'string');
				break;
			case 'accessed':
				values_set('accessed') = DOpus.Create.Map('is_metaproperty', false, 'meta_group', false, 'EV_property', 'da', 'property', 'accessed', 'EV_prefix', '', 'type', 'date');
				break;
			case 'attr':
				values_set('attr') = DOpus.Create.Map('is_metaproperty', false, 'meta_group', false, 'EV_property', 'attributes', 'property', 'attr', 'EV_prefix', '', 'type', 'string');
				break;
			case 'rating':
				values_set('rating') = DOpus.Create.Map('is_metaproperty', false, 'meta_group', false, 'EV_property', 'rating', 'property', 'rating', 'EV_prefix', '', 'type', 'int');
				break;
			case 'keywords':
				values_set('keywords') = DOpus.Create.Map('is_metaproperty', false, 'meta_group', false, 'EV_property', 'tags', 'property', 'keywords', 'EV_prefix', 'ww:', 'type', 'tags_str');
				break;
				//==========================================
				//t=>Music
			case 'mp3album':
				values_set('mp3album') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'audio', 'EV_property', 'album', 'property', 'mp3album', 'EV_prefix', '', 'type', 'string');
				break;
			case 'mp3albumartist':
				values_set('mp3albumartist') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'audio', 'EV_property', 'albumartist', 'property', 'mp3albumartist', 'EV_prefix', '', 'type', 'string');
				break;
			case 'mp3bpm':
				values_set('mp3bpm') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'audio', 'EV_property', 'beatsperminute', 'property', 'mp3bpm', 'EV_prefix', '', 'type', 'int');
				break;
			case 'mp3track':
				values_set('mp3track') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'audio', 'EV_property', 'track', 'property', 'mp3track', 'EV_prefix', '', 'type', 'int');
				break;
			case 'mp3artists':
				values_set('mp3artists') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'audio', 'EV_property', 'artist', 'property', 'mp3artists', 'EV_prefix', '', 'type', 'string');
				break;
				///////////////////////////
				//t=>Documents
			case 'category':
				values_set('category') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'doc', 'EV_property', 'category', 'property', 'category', 'EV_prefix', '', 'type', 'string');
				break;
			case 'doccreateddate':
				values_set('doccreateddate') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'doc', 'EV_property', 'contentcreated', 'property', 'doccreateddate', 'EV_prefix', '', 'type', 'date');
				break;
			case 'doclastsaveddate':
				values_set('doclastsaveddate') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'doc', 'EV_property', 'datesaved', 'property', 'doclastsaveddate', 'EV_prefix', '', 'type', 'date');
				break;
			case 'doclastsavedby':
				values_set('doclastsavedby') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'doc', 'EV_property', 'lastauthor', 'property', 'doclastsavedby', 'EV_prefix', '', 'type', 'string');
				break;
			case 'pages':
				values_set('pages') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'doc', 'EV_property', 'pagecount', 'property', 'pages', 'EV_prefix', '', 'type', 'int');
				break;
			case 'companyname':
				values_set('companyname') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'doc', 'EV_property', 'company', 'property', 'companyname', 'EV_prefix', '', 'type', 'string');
				break;
				////////////////////////////////
				//t=>Image
			case '35mmfocallength':
				values_set('35mmfocallength') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'image', 'EV_property', '35mmfocallength', 'property', '35mmfocallength', 'EV_prefix', '', 'type', 'float');
				break;
			case 'altitude':
				values_set('altitude') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'image', 'EV_property', 'altitude', 'property', 'altitude', 'EV_prefix', '', 'type', 'int');
				break;
			case 'apertureval':
				values_set('apertureval') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'image', 'EV_property', 'aperture', 'property', 'apertureval', 'EV_prefix', '', 'type', 'int');
				break;
			case 'cameramake':
				values_set('cameramake') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'image', 'EV_property', 'cameramake', 'property', 'cameramake', 'EV_prefix', '', 'type', 'string');
				break;
			case 'cameramodel':
				values_set('cameramodel') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'image', 'EV_property', 'cameramodel', 'property', 'cameramodel', 'EV_prefix', '', 'type', 'string');
				break;
			case 'contrast':
				values_set('contrast') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'image', 'EV_property', 'contrast', 'property', 'contrast', 'EV_prefix', '', 'type', 'string');
				break;
			case 'datedigitized':
				values_set('datedigitized') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'image', 'EV_property', 'date-adquired', 'property', 'datedigitized', 'EV_prefix', '', 'type', 'date');
				break;
			case 'datetaken':
				values_set('datetaken') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'image', 'EV_property', 'date-taken', 'property', 'datetaken', 'EV_prefix', '', 'type', 'date');
				break;
			case 'digitalzoom':
				values_set('digitalzoom') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'image', 'EV_property', 'digital-zoom', 'property', 'digitalzoom', 'EV_prefix', '', 'type', 'int');
				break;
			case 'exposurebias':
				values_set('exposurebias') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'image', 'EV_property', 'exposure-bias', 'property', 'exposurebias', 'EV_prefix', '', 'type', 'int');
				break;
			case 'exposureprogram':
				values_set('exposureprogram') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'image', 'EV_property', 'exposure-program', 'property', 'exposureprogram', 'EV_prefix', '', 'type', 'string');
				break;
			case 'exposuretime':
				values_set('exposuretime') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'image', 'EV_property', 'exposure-time', 'property', 'exposuretime', 'EV_prefix', '', 'type', 'int');
				break;
			case 'focallength':
				values_set('focallength') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'image', 'EV_property', 'focal-length', 'property', 'focallength', 'EV_prefix', '', 'type', 'float');
				break;
			case 'picresx':
				values_set('picresx') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'image', 'EV_property', 'horizontalresolution', 'property', 'picresx', 'EV_prefix', '', 'type', 'int');
				break;
			case 'latitude':
				values_set('latitude') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'image', 'EV_property', 'latitude', 'property', 'latitude', 'EV_prefix', '', 'type', 'int');
				break;
			case 'lensmake':
				values_set('lensmake') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'image', 'EV_property', 'lensmaker', 'property', 'lensmake', 'EV_prefix', '', 'type', 'string');
				break;
			case 'lensmodel':
				values_set('lensmodel') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'image', 'EV_property', 'lensmodel', 'property', 'lensmodel', 'EV_prefix', '', 'type', 'string');
				break;
			case 'longitude':
				values_set('longitude') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'image', 'EV_property', 'longitude', 'property', 'longitude', 'EV_prefix', '', 'type', 'int');
				break;
			case 'meteringmode':
				values_set('meteringmode') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'image', 'EV_property', 'meteringmode', 'property', 'meteringmode', 'EV_prefix', '', 'type', 'string');
				break;
			case 'saturation':
				values_set('saturation') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'image', 'EV_property', 'saturation', 'property', 'saturation', 'EV_prefix', '', 'type', 'string');
				break;
			case 'sharpness':
				values_set('sharpness') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'image', 'EV_property', 'sharpness', 'property', 'sharpness', 'EV_prefix', '', 'type', 'string');
				break;
			case 'software':
				values_set('software') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'image', 'EV_property', 'software', 'property', 'software', 'EV_prefix', '', 'type', 'string');
				break;
			case 'subjectdistance':
				values_set('subjectdistance') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'image', 'EV_property', 'subjectdistance', 'property', 'subjectdistance', 'EV_prefix', '', 'type', 'float');
				break;
			case 'picresy':
				values_set('picresy') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'image', 'EV_property', 'verticalresolution', 'property', 'picresy', 'EV_prefix', '', 'type', 'int');
				break;
			case 'whitebalance':
				values_set('whitebalance') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'image', 'EV_property', 'whitebalance', 'property', 'whitebalance', 'EV_prefix', '', 'type', 'string');
				break;
				////////////////////////////////
				//t=>Video
			case 'audiocount':
				values_set('audiocount') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'video', 'EV_property', 'audiotrackcount', 'property', 'audiocount', 'EV_prefix', '', 'type', 'int');
				break;
			case 'subtitlecount':
				values_set('subtitlecount') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'video', 'EV_property', 'subtitletrackcount', 'property', 'subtitlecount', 'EV_prefix', '', 'type', 'int');
				break;
			case 'videocount':
				values_set('videocount') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'video', 'EV_property', 'videotrackcount', 'property', 'videocount', 'EV_prefix', '', 'type', 'int');
				break;
			case 'director':
				values_set('director') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'video', 'EV_property', 'director', 'property', 'director', 'EV_prefix', '', 'type', 'string');
				break;
			case 'framerate':
				values_set('framerate') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'video', 'EV_property', 'framerate', 'property', 'framerate', 'EV_prefix', '', 'type', 'float');
				break;
			case 'datarate':
				values_set('datarate') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'video', 'EV_property', 'videobitrate', 'property', 'datarate', 'EV_prefix', '', 'type', 'int');
				break;
				////////////////////////////////
				//t=>Programs
			case 'moddesc':
				values_set('moddesc') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'exe', 'EV_property', 'description', 'property', 'moddesc', 'EV_prefix', '', 'type', 'string');
				break;
			case 'modversion':
				values_set('modversion') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'exe', 'EV_property', 'version', 'property', 'modversion', 'EV_prefix', '', 'type', 'float');
				break;
			case 'prodname':
				values_set('prodname') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'exe', 'EV_property', 'productname', 'property', 'prodname', 'EV_prefix', '', 'type', 'string');
				break;
			case 'prodversion':
				values_set('prodversion') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'exe', 'EV_property', 'productversion', 'property', 'prodversion', 'EV_prefix', '', 'type', 'float');
				break;
				//Audio|Video
			case 'duration':
				values_set('duration') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'audio', 'EV_property', 'length', 'property', 'duration', 'EV_prefix', '', 'type', 'int');
				break;
			case 'releasedate':
				values_set('releasedate') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'audio', 'EV_property', 'mediacreated', 'property', 'releasedate', 'EV_prefix', '', 'type', 'date');
				break;
			case 'mp3year':
				values_set('mp3year') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'audio', 'EV_property', 'year', 'property', 'mp3year', 'EV_prefix', '', 'type', 'string');
				break;
			case 'mp3genre':
				values_set('mp3genre') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'audio', 'EV_property', 'genre', 'property', 'mp3genre', 'EV_prefix', '', 'type', 'string');
				break;
			case 'publisher':
				values_set('publisher') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'audio', 'EV_property', 'publisher', 'property', 'publisher', 'EV_prefix', '', 'type', 'string');
				break;
			case 'mp3bitrate':
				values_set('mp3bitrate') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'audio', 'EV_property', 'audiobitrate', 'property', 'mp3bitrate', 'EV_prefix', '', 'type', 'int');
				break;
			case 'mp3title':
				values_set('mp3title') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'audio', 'EV_property', 'title', 'property', 'mp3title', 'EV_prefix', '', 'type', 'string');
				break;
			case 'composers':
				values_set('composers') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'audio', 'EV_property', 'composer', 'property', 'composers', 'EV_prefix', '', 'type', 'string');
				break;
			case 'conductors':
				values_set('conductors') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'audio', 'EV_property', 'conductor', 'property', 'conductors', 'EV_prefix', '', 'type', 'string');
				break;
				//Image|Doc
			case 'subject':
				values_set('subject') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'doc', 'EV_property', 'subject', 'property', 'subject', 'EV_prefix', '', 'type', 'string');
				break;
			case 'author':
				values_set('author') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'doc', 'EV_property', 'authors', 'property', 'author', 'EV_prefix', '', 'type', 'string');
				break;
			case 'title':
				values_set('title') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'doc', 'EV_property', 'title', 'property', 'title', 'EV_prefix', '', 'type', 'string');
				break;
				//Image|Video
			case 'picsize':
				values_set('picsize') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'image_text', 'EV_property', 'dimensions', 'property', 'picsize', 'EV_prefix', '', 'type', 'string');
				break;
			case 'aspectratio':
				values_set('aspectratio') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'image', 'EV_property', 'aspectratio', 'property', 'aspectratio', 'EV_prefix', '', 'type', 'float');
				break;
			case 'picheight':
				values_set('picheight') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'image', 'EV_property', 'height', 'property', 'picheight', 'EV_prefix', '', 'type', 'int');
				break;
			case 'picwidth':
				values_set('picwidth') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'image', 'EV_property', 'width', 'property', 'picwidth', 'EV_prefix', '', 'type', 'int');
				break;
				//Multi
			case 'copyright':
				values_set('copyright') = DOpus.Create.Map('is_metaproperty', true, 'meta_group', 'doc', 'EV_property', 'copyright', 'property', 'copyright', 'EV_prefix', '', 'type', 'string');
				break;
			case 'picdepth':
				values_set('picdepth') = DOpus.Create.Map('is_metaproperty', true, 'image', true, 'EV_property', 'bit-depth', 'property', 'picdepth', 'EV_prefix', '', 'type', 'int');
				break;
			default:
				Log(3, 'Not supported property : ' + props[i]);
		}
	}
	var values = DOpus.Create.UnorderedSet();
	if (values_set.Exists('name')) values.insert('name');
	if (values_set.Exists('ext')) values.insert('ext');
	if (values_set.Exists('size')) values.insert('size');
	if (values_set.Exists('created')) values.insert('created');
	if (values_set.Exists('modified')) values.insert('modified');
	for (var i = 0; i < props.length; i++) {
		if (!values.Exists(props[i]) && values_set.Exists(props[i])) values.insert(props[i]);
	}
	if (!values.empty) values_set('props') = values;
	return values_set;
}

function GetColsNames(props) {
	var props_text = '';
	for (var i = props.count - 1; i >= 0; i--)
		props_text += ',' + props(i);
	props_text = props_text.slice(1);
	Log(2, 'PROPERTIES  : ' + props_text);
	return props_text;
}

//Delete saved collection
function DeleteColl() {
	try {
		if (!Script.vars.Exists('collection_name')) return;
		var coll_name = Script.vars.Get('collection_name');
		if (DOpus.FSUtil().Exists(coll_name)) {
			var cmd = DOpus.Create.Command();
			Log(1, 'Deleting saved collection and their sub collections : ' + coll_name);
			cmd.RunCommand('Delete FILE="' + coll_name + '" QUIET');
			cmd = null;
		}
	}
	catch (e) {
		Log(2, 'Unable to delete collection :' + e.description);
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
	dlg.title = script_name + ' v' + script_version;
	if (level === 0) dlg.icon = 'info';
	else if (level === 1) dlg.icon = 'question';
	else if (level === 2) dlg.icon = 'warning';
	else dlg.icon = 'error';
	dlg.Show();
	dlg = null;
}

function Log(level, text) {
	if (Script.config.debug < level || level == 4) {
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
	<resource type="strings">
		<strings lang="english">
			<string id="alert_no_Ev">Everything process is not running!</string>
			<string id="alert_no_minver">This command needs at least Everything v1.5.0.1366 to run!</string>
			<string id="coll_name">Main collection name to use for results.</string>
			<string id="debug">Logging level to be displayed. OFF to show only errors. 
DEBUG to show all messages. 
STANDARD to show only the most relevant information.
WARNING to show messages that needs your attention.</string>
			<string id="delete_onstartup">If enabled, all results collections will be deleted after start.
It only affects collections created by this command.</string>
			<string id="ev_coll">If enabled, 2 sub-collections will be created, one with Everything results, and one with the DOpus dupes search results.
If disabled, only one subcollection will be used for duplicates.</string>
			<string id="max_timeout">Max seconds to wait for the Everything search results.</string>
			<string id="min_progress">Minimum number of items used by the command to display progress dialogs.
If READFOLDER is used, the dialogs are always displayed.</string>
			<string id="select_delete">Set to True to show the dialog for dupes selection at the end.</string>
			<string id="use_MD5">Use MD5 comparison in Opus dupe finder.
NOTE: Only applies when search includes name/size.</string>
		</strings>
		<strings lang="esm">
			<string id="alert_no_Ev">¡Everything v1.5 no se está ejecutando!</string>
			<string id="alert_no_minver">¡Este comando necesita al menos Everything v.1.5.0.1370 para ejecutarse!</string>
			<string id="coll_name">Nombre de la colección principal a utilizar.</string>
			<string id="debug">El nivel de registro a mostrar. OFF para mostrar solo errores. 
DEBUG para mostrar todos los mensajes. 
STANDARD para mostrar solo la información más relevante. 
WARNING para mostrar mensajes que necesitan tu atención.</string>
			<string id="delete_onstartup">Si se activa, se eliminarán todas las colecciones de resultados trás iniciar DOpus.
Solamente afecta a las colecciones creadas por este comando.</string>
			<string id="ev_coll">Si está activado, se crearán 2 sub colecciones, una con los resultados de Everything, y otra con los resultados de la búsqueda de duplicados en DOpus.
Si está desactivado, sólo se usará una subcolección para los duplicados.</string>
			<string id="max_timeout">Segundos a esperar como máximo por los resultados de la busqueda en Everything.</string>
			<string id="min_progress">Número mínimo de items usados por el comando para mostrar diálogos de progreso.
Si se usa READFOLDER, los diálogos siempre se muestran.</string>
			<string id="select_delete">Establecer a Verdadero para mostrar el diálogo para seleccionar duplicados al finalizar.</string>
			<string id="use_MD5">Usar MD5 en la búsqueda de duplicados en DOpus.
NOTA: Solamente aplica cuando se especifica usar nombres/tamaños.</string>
		</strings>
	</resource>
</resources>
