/* Filter by Column for Directory Opus
**Powerful filtering/searching tool for all columns**
Filter by Column © 2024 by Christian Arellano García is licensed under CC BY-NC-ND 4.0 
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
*/
var script_name = 'FilterbyColumn';
var script_label = 'FBC';
var script_version = '1.8.2';
var g_custom_col = false;
// Called by Directory Opus to initialize the script
function OnInit(initData) {
	initData.name = script_name;
	initData.version = script_version;
	initData.copyright = '(c) 2023-2024 Christian Arellano García';
	initData.desc = 'Powerful filtering/searching tool for all columns';
	initData.url = 'https://resource.dopus.com/t/filter-by-column-powerful-filtering-searching-tool-for-all-columns/47559';
	initData.default_enable = true;
	initData.min_version = '13.14';
	initData.config = DOpus.Create().OrderedMap();
	initData.config_desc = DOpus.Create().OrderedMap();
	initData.config_groups = DOpus.Create().OrderedMap();
	AddConfig('collection name', '', DOpus.strings.Get('collection_name'), '1. General');
	AddConfig('create subcollections', false, DOpus.strings.Get('create_subcollections'), '1. General');
	AddConfig('custom_columns_categories', DOpus.Create().Vector(), DOpus.strings.Get('custom_columns_categories'), '1. General');
	AddConfig('custom_columns_names', DOpus.Create().Vector(), DOpus.strings.Get('custom_columns_names'), '1. General');
	AddConfig('log level', DOpus.Create().Vector(2, 'DEBUG', 'STANDARD', 'WARNING', 'OFF'), DOpus.strings.Get('debug'), '1. General');
	AddConfig('flags_quickkey', '?', DOpus.strings.Get('flags_quickkey'), '2. FAYT Mode');
	AddConfig('default_implicit_column', DOpus.Create().Vector(0, 'name', 'current sorted', 'user_defined_implicit_column value', 'current highlighted'), DOpus.strings.Get('default_implicit_column'), '3. FAYT/Command Mode');
	AddConfig('add column', true, DOpus.strings.Get('add_column'), '3. FAYT/Command Mode');
	AddConfig('enable_wildcards', false, DOpus.strings.Get('enable_wildcards'), '3. FAYT/Command Mode');
	AddConfig('max_history_size', 20, DOpus.strings.Get('max_history_size'), '3. FAYT/Command Mode');
	AddConfig('show nested results with Textual Filters', false, DOpus.strings.Get('nested_in_textualfilters'), '3. FAYT/Command Mode');
	AddConfig('preferred_filter_mode', DOpus.Create().Vector(1, 'Textual Filters', 'Rename Preset'), DOpus.strings.Get('preferred_filter_mode'), '3. FAYT/Command Mode');
	AddConfig('user_defined_implicit_column', '', DOpus.strings.Get('user_defined_implicit_column'), '3. FAYT/Command Mode');
	AddConfig('custom columns obtention', DOpus.Create().Vector(1, 'Textual Filters', 'Rename Preset'), DOpus.strings.Get('preferred_filter_mode'), '4. Dialog Mode');
	AddConfig('list Evaluator columns', true, DOpus.strings.Get('include_evalcols'), '4. Dialog Mode');
	AddConfig('list Script columns', true, DOpus.strings.Get('include_scpcols'), '4. Dialog Mode');
	AddConfig('list Shell columns', true, DOpus.strings.Get('include_shellcols'), '4. Dialog Mode');
	return;

	function AddConfig(name, value, desc, group) {
		initData.config[name] = value;
		initData.config_desc(name) = desc;
		initData.config_groups(name) = group;
	}

}

function OnAddCommands(addCmdData) {
	var cmd = addCmdData.AddCommand();
	cmd.name = script_name;
	cmd.method = 'OnColFilter';
	cmd.desc = 'Powerful filtering/searching tool for all columns';
	cmd.label = script_label;
	cmd.template = 'CONFIG/S,EDITCATEGORIES/S,MULTI/S,IGNOREEMPTY/S,REGEXP/S,WHOLEWORDS/S,CASE/S,NODIACRITICS/S,USEWILD/S,FILTER/O[visible],QUERY/K/R,' +
		'DIALOG/O[filterall,accessed,accesseddate,accessedtime,created,createddate,createdtime,modified,modifieddate,modifiedtime,attr,ext,extdir,sizeauto,size,sizekb,fullpath,' +
		'label,owner,type,perms,availability,streamcount,desc,keywords,name,parent,parentlocation,parentpath,path,pathrel,userdesc,pathlen,copyright,producer,picdepth,mp3bpm,mp3disc,' +
		'mp3disk,mp3track,compilation,mp3album,mp3albumartist,mp3comment,mp3encodingsoftware,mp3info,mp3drm,doccreateddate,docedittime,doclastsaveddate,pages,category,comments,creator,doclastsavedby,' +
		'modversion,prodversion,moddesc,prodname,signer,fontname,datedigitized,datetaken,datetimecreated,datetimeoriginal,35mmfocallength,altitude,apertureval,digitalzoom,exposurebias,exposuretime,' +
		'fnumber,focallength,latitude,longitude,picphysx,picphysy,picresx,picresy,rotation,shutterspeed,subjectdistance,cameramake,cameramodel,colormodel,colorspace,contrast,coords,exposureprogram,' +
		'flash,imagedesc,imagequality,instructions,isospeed,lensmake,lensmodel,macromode,meteringmode,picres,saturation,scenecapturetype,scenemode,sharpness,software,whitebalance,broadcastdate,' +
		'recordingtime,audiocount,channel,datarate,framerate,subtitlecount,videocount,alllangs,audiolangs,credits,director,episodename,fourcc,hdrtypes,ishd,isrepeat,publisher,station,subtitlelangs,' +
		'videocodec,videolangs,encodedby,rating,target,duration,releasedate,mp3bitrate,mp3samplerate,mp3year,audiocodec,composers,conductors,mp3genre,mp3mode,mp3title,mp3type,initialkey,companyname,' +
		'subject,picsize,aspectratiogroup,picphyssize,aspectratio,picheight,picwidth,mp3artists,author,title]';
	cmd.hide = false;
	cmd.icon = 'script';
	var fayt = cmd.fayt;
	fayt.enable = true;
	fayt.key = '$';
	fayt.backcolor = '#ffad5b';
	fayt.textcolor = '#000000';
	fayt.label = script_label;
	fayt.realtime = true;
	fayt.flags = DOpus.Create().Map();
	fayt.flags[(1 << 0)] = DOpus.strings.Get('flag0'); //Case sensitive
	fayt.flags[(1 << 1)] = DOpus.strings.Get('flag1'); //Whole words
	fayt.flags[(1 << 2)] = DOpus.strings.Get('flag2'); //Regular expressions
	fayt.flags[(1 << 3)] = DOpus.strings.Get('flag3'); //Ignore diacritics
	fayt.flags[(1 << 4)] = DOpus.strings.Get('flag4'); //Ignore undefined values
	fayt.flags[(1 << 5)] = DOpus.strings.Get('flag5'); //Filter only visible
	fayt.flags[(1 << 6)] = DOpus.strings.Get('flag6'); //Search SubMode
	fayt.flags[(1 << 7)] = DOpus.strings.Get('flag7'); //Multi Mode
}

var str_tools, g_ini_timer, FSU, custom_name, is_default_filter;
var quickKey, custom_categories_map, custom_names_map;
var g_ext_dir, g_ignore_empty, g_hide_ext, g_colnames_set, g_use_wilcards, g_search_mode, g_flags, g_filter_visible, g_flat_state;

function OnColFilter(scriptFAYTData) {
	if (scriptFAYTData.fayt !== script_name) {
		if (scriptFAYTData.func.args.got_arg.config) {
			scriptFAYTData.func.command.RunCommand('Prefs SCRIPTS=ColSearch.js*');
			return;
		}
		var tab = scriptFAYTData.func.sourcetab;
		if (!FSU) FSU = DOpus.FSUtil();
		if (scriptFAYTData.func.args.got_arg.editcategories) {
			DOpus.ClearOutput();
			editCategoriesDlg(tab);
			Script.Vars.Delete('custom_columns_categories');
			return;
		}
		if (!tab || /(shell|mtp|ftp|plugin)/.test(FSU.PathType(tab.path))) { //don't work in shell, mtp, plugin
			Log(4, '"' + script_label + '" can\'t be used in this path types : ftp, shell, mtp, plugin');
			return;
		}
		if (scriptFAYTData.func.args.got_arg.ignoreempty) g_ignore_empty = true;
		else if (scriptFAYTData.func.args.got_arg.dialog) {
			DOpus.ClearOutput();
			Log(2, '=======' + script_label + ' v' + script_version + '=====================================');
			Log(2, 'FAYT cmdline            : ' + scriptFAYTData.cmdline);
			var query = UseDialogMode(tab, scriptFAYTData.func.command, scriptFAYTData.func.args.dialog);
		}
		else {
			if (!scriptFAYTData.func.args.got_arg.query) {
				Log(4, 'Unable to continue without a query!');
				return;
			}
			var is_multi = scriptFAYTData.func.args.got_arg.multi;
			var query = scriptFAYTData.func.args.query;
			g_flags = 0;
			if (scriptFAYTData.func.args.got_arg['case']) g_flags += (1 << 0);
			if (scriptFAYTData.func.args.got_arg.wholewords) g_flags += (1 << 1);
			if (scriptFAYTData.func.args.got_arg.regexp) g_flags += (1 << 2);
			if (scriptFAYTData.func.args.got_arg.nodiacritics) g_flags += (1 << 3);
			if (scriptFAYTData.func.args.got_arg.usewild) g_use_wilcards = true;
			if (scriptFAYTData.func.args.got_arg.filter) {
				g_search_mode = false;
				if (String(scriptFAYTData.func.args.filter).toLowerCase() === 'visible') g_filter_visible = true;
			}
			else g_search_mode = true;
		}
	}
	else {
		if (scriptFAYTData.quickkey) quickKey = scriptFAYTData.quickkey;
		var query = scriptFAYTData.cmdline;
		if (is_default_filter === undefined || !quickKey) {
			is_default_filter = CheckDefaultFilter();
		}
		if (!quickKey) {
			Log(4, 'Unexpected FAYT         : quick key can\'t be empty!');
			return;
		}

		if (is_default_filter && query.charAt(0) == quickKey) {
			var append_key = quickKey;
			query = query.slice(1);
		}
		else var append_key = '';

		if (query === '' || query === quickKey) scriptFAYTData.tab.UpdateFAYTSuggestions(getSuggestions(append_key + query, true)); //only suggest custom names
		else scriptFAYTData.tab.UpdateFAYTSuggestions(getSuggestions(append_key + (query.charAt(0) === quickKey ? quickKey : ''), false));

		if (!query) return;
		var is_multi = (g_flags & (1 << 7));
		if (is_multi && scriptFAYTData.key !== 'return') return;
		var flags_quickkey = Script.config.flags_quickkey;

		if (quickKey == flags_quickkey) {
			Log(3, 'flags_quickkey can\'t be the same as ' + quickKey);
			flags_quickkey = '';
		}
		if (query == quickKey && scriptFAYTData.key === 'return') { // query is the same key as quick key so Print cols info
			Log(2, 'Printing cols name for all visible columns...');
			printCols(scriptFAYTData.tab.format.columns, scriptFAYTData.tab);
			return;
		}
		g_search_mode = false;
		if (query.charAt(0) == quickKey) {
			g_search_mode = true;
			query = query.slice(1);
		}
		g_flags = scriptFAYTData.flags;
		if (g_flags & (1 << 6)) g_search_mode = !g_search_mode; //toggle between filter and search
		var tab = scriptFAYTData.tab;
		if (flags_quickkey && query == flags_quickkey && scriptFAYTData.key === 'return') { //user activated flags dialog window
			Log(2, 'Flags configuration activated');
			configFlags(tab.lister, g_flags);
			return;
		}
		if (!FSU) FSU = DOpus.FSUtil();
		if (!tab || /(ftp|shell|mtp|plugin)/.test(FSU.PathType(tab.path))) { //don't work in shell, mtp, plugin
			Log(4, '"' + script_label + '" can\'t be used in this path types : ftp, shell, mtp, plugin');
			return;
		}
		g_ignore_empty = (g_flags & 1 << 4) ? true : false;
		if (g_flags & (1 << 5) && tab.stats.items > 0) g_filter_visible = true;
	}
	g_ini_timer = new Date();
	if (query && query !== quickKey) {
		if (!Script.Vars.Exists('no_clear_output')) DOpus.ClearOutput();
		if (!g_hide_ext) g_hide_ext = tab.format.hide_ext;
		if (!str_tools) str_tools = DOpus.Create().StringTools();
		if (is_multi && /&&|\|\|/.test(query)) UseMultiTextualFilters(tab, query, !g_search_mode);
		else {
			if (g_use_wilcards === undefined) g_use_wilcards = Script.config.enable_wildcards;
			var ColObj = getColObj(tab, query);
			if (ColObj !== null) {
				if (scriptFAYTData.fayt) {
					if (m = query.match(/^(.*?(?:==|!=|>=|<=|=|!)*\{)/)) {
						// Log(1, m[1] + ' match with reference');
						tab.UpdateFAYTSuggestions(getColumnSuggestions(append_key + (query.charAt(0) === quickKey ? quickKey : ''), m[1]));
					}
					if (scriptFAYTData.key !== 'return') //Real time till here
						return;
				}
				if (Script.config['log level'] < 2) {
					Log(2, '=======' + script_label + ' v' + script_version + '=====================================');
					Log(2, 'FAYT cmdline            : ' + scriptFAYTData.cmdline);
					Log(2, 'active tab              : ' + tab.path);
					Log(2, 'quickKey                : ' + quickKey);
					Log(2, 'is default filter       : ' + is_default_filter);
					Log(2, 'query                   : "' + query + '"');
					Log(1, 'flags_quickkey          : ' + flags_quickkey);
					Log(1, 'default_implicit_column : ' + Script.config.default_implicit_column);
					Log(1, 'user_implicit_column    : ' + Script.config.user_defined_implicit_column);
					Log(1, 'preferred_filter_mode   : ' + (Script.config.preferred_filter_mode === 0 ? 'Textual Filters' : 'Rename Preset'));
					Log(1, 'nested with Text. Filt. : ' + Script.config['show nested results with Textual Filters']);
					Log(1, 'Add column              : ' + Script.config['add column']);
					Log(1, 'collection name         : ' + Script.config['collection name']);
					Log(1, 'create subcollections   : ' + Script.config['create subcollections']);
					Log(1, 'Case sensitive          : ' + (g_flags & (1 << 0) ? 'true' : 'false'));
					Log(1, 'Whole words             : ' + (g_flags & (1 << 1) ? 'true' : 'false'));
					Log(1, 'Regular expressions     : ' + (g_flags & (1 << 2) ? 'true' : 'false'));
					Log(1, 'Ignore diacritics       : ' + (g_flags & (1 << 3) ? 'true' : 'false'));
					Log(1, 'Ignore undefined values : ' + g_ignore_empty);
					Log(1, 'Use wilcards            : ' + g_use_wilcards);
					Log(2, 'Search Mode             : ' + g_search_mode);
					Log(2, '==================================================================');
				}
				var queryObj = getQueryObj(query, ColObj); // Get converted query, raw query and operators from user input
				if (queryObj !== null) {
					var cmd = DOpus.Create().Command();
					cmd.SetSourceTab(tab);
					if (g_search_mode) UseTextualFilters(tab, null, cmd, queryObj, ColObj, false);
					else {
						if (g_filter_visible) {
							Log(2, 'Using only currently visible items...');
							var items = GetItemsVector(tab.all);
						}
						else var items = GetItemsVector(tab.all, tab.hidden);
						if (!items.empty) {
							if (!g_custom_col) g_use_wilcards ? UseTextualFilters(tab, items, cmd, queryObj, ColObj, true) : UseBuiltinMethod(tab, items, cmd, queryObj, ColObj);
							else Script.config.preferred_filter_mode === 0 ? UseTextualFilters(tab, items, cmd, queryObj, ColObj, true) : UseRenamePreset(tab, items, cmd, queryObj, ColObj);
						}
						items = null;
					}
					saveQuery(query, custom_name ? ColObj('name') : '');
					queryObj = null;
					cmd = null;
				}
				else Log(4, 'Input is not valid!');
				ColObj = null;
			}
			else Log(3, 'Unable to retrieve data for column!');
		}
		CollectGarbage();
	}
	Log(2, 'COMMAND FINISHED       : ' + (new Date() - g_ini_timer) + ' ms');
	Log(2, '===================================================================');
	tab = null;
	FSU = null;
	str_tools = null;
	g_ini_timer = null;
	g_colnames_set = null;
	return;
}

//Filter function for built-in columns
function UseBuiltinMethod(tab, items, cmd, queryObj, ColObj) {
	Log(2, 'FILTERING IN COLUMN "' + ColObj('name') + '" FOR: ' + items.count + ' ITEMS');
	g_flat_state = cmd.IsSet('Set FLATVIEW=grouped');
	var res = DOpus.Create().StringSetI();
	var item, add, query, operator, item_value, item_path;
	operator = queryObj.operator;
	query = queryObj.query;
	category = ColObj('category');
	if (operator == '') operator = '=';
	if (operator == '=' || operator == '!') operator += '=';
	if (ColObj('name') === 'extdir' && !g_ext_dir) g_ext_dir = str_tools.LanguageStr(2132);
	var busy_indicator = DOpus.Create().BusyIndicator();
	busy_indicator.abort = true;
	if (busy_indicator.Init(tab, str_tools.LanguageStr(5101))) busy_indicator.Show();
	for (var i = 0; i < items.count; i++) {
		if (busy_indicator.abort) break;
		item = items(i);
		item_value = Global_GetBuiltinValue(item, ColObj('name'), ColObj, tab.path + '');
		if (item_value === null) {
			Log(3, '   => Error reading ' + item);
			continue;
		}
		if (queryObj.is_reference) {
			query = Global_GetBuiltinValue(item, queryObj.query, queryObj.raw_query, tab.path + '');
			if (query === null) {
				Log(3, '   => Error reading ' + item);
				continue;
			}
		}
		if (category === 'duration')
			add = compare(item_value, query, operator);
		else if (category === 'number')
			add = compare(item_value, query, operator);
		else if (category === 'size')
			add = compareSizes(item_value, query, operator);
		else if (category === 'date')
			add = compareDates(item_value, query, queryObj.is_reference ? 'whole' : queryObj.raw_query, operator);
		else {
			if (DOpus.TypeOf(query) === 'object.Vector') add = compareGroups(item_value, query, operator, g_flags & (1 << 3));
			else {
				if (ColObj('name') === 'name') add = compareStrings(g_hide_ext ? item_value.name_stem_m : item_value.name, query, operator);
				else add = compareStrings(item_value, query, operator);
			}
		}
		if (add === true) {
			res.insert(item.def_value);
			try {
				if (item.nested || (g_flat_state && item.path.def_value != tab.path.def_value)) {
					item_path = item.path;
					while (item_path.def_value != tab.path.def_value) {
						res.insert(item_path.def_value);
						if (!item_path.Parent()) break;
					}
				}
			}
			catch (err) {
				Log(3, 'Error retrieving parents for ' + item + ' : ' + err.description);
			}
		}
	}
	if (!res.empty) {
		Log(2, 'RESULTS                 : ' + res.count);
		cmd.SetFiles(res);
		cmd.RunCommand('SELECT FROMSCRIPT=unhide HIDEUNSEL DESELECTNOMATCH');
		cmd.RunCommand('SELECT FIRST MAKEVISIBLE=inmediate');
	}
	else {
		Log(2, 'No results could be found');
		cmd.RunCommand('SELECT NONE HIDEUNSEL');
	}
	res = null;
	item = null;
	return;
}

function UseRenamePreset(tab, items, cmd, queryObj, ColObj) {
	Log(2, 'FILTERING VIA RENAME IN COLUMN "' + ColObj('name') + '" FOR: ' + items.count + ' ITEMS');
	g_flat_state = cmd.IsSet('Set FLATVIEW=grouped');
	var fromlib = FSU.PathType(tab.path) === 'lib';
	var res = DOpus.Create().StringSetI();
	var DOvar = 'fbc_values';
	var item, value;
	var map_values = Global_GetRenamePresetValues(items, cmd, ColObj('name'), DOvar);
	if (!map_values.empty) {
		var map_items = DOpus.Create().OrderedMap();
		if (fromlib)
			for (var i = items.length - 1; i >= 0; i--) {
				map_items(items(i).realpath) = items(i);
			}
		var e = new Enumerator(map_values);
		for (; !e.atEnd(); e.moveNext()) {
			a = false;
			try {
				item = e.item();
				value = map_values(item);
				if (!value && (queryObj.operator === '==' || queryObj.operator === '!=') && queryObj.query === '')
					a = (queryObj.operator === '==');
				else if (value) {
					value = value.replace(/[\u200E\u200F\u202A-\u202E\u2066-\u2069]/g, '');
					if (ColObj('category') !== 'string') value = convertType(ColObj('category'), value);
					switch (ColObj('category')) {
						case 'date':
							a = compareDates(value, queryObj.query, queryObj.raw_query, queryObj.operator);
							break;
						case 'duration':
						case 'number':
							a = compare(value, queryObj.query, queryObj.operator);
							break;
						case 'size':
							a = compareSizes(value, queryObj.query, queryObj.operator);
							break;
						default:
							a = compareStrings(value, queryObj.query, queryObj.operator);
					}
				}
			}
			catch (err) {
				Log(3, 'Error when trying to get ' + ColObj('name') + ' info for ' + item);
			}
			if (a) {
				try {
					item = fromlib ? map_items(item) : tab.all(item);
					res.insert(item);
					if (item.nested || (g_flat_state && item.path.def_value != tab.path.def_value)) {
						helper = item.path;
						while (helper.def_value != tab.path.def_value) {
							res.insert(helper.def_value);
							if (!helper.Parent()) break;
						}
					}
				}
				catch (err) {
					Log(3, 'Error retrieving parents for ' + e.item() + ' : ' + err.description);
				}
			}
		}
	}
	if (!res.empty) {
		Log(2, 'RESULTS                 : ' + res.count);
		cmd.SetFiles(res);
		cmd.RunCommand('SELECT FROMSCRIPT=unhide HIDEUNSEL DESELECTNOMATCH');
		cmd.RunCommand('SELECT FIRST MAKEVISIBLE=inmediate');
	}
	else {
		Log(2, 'No results could be found');
		cmd.RunCommand('SELECT NONE HIDEUNSEL');
	}
	//Delete the global var used
	DOpus.Vars.Delete(DOvar);
	//Delete the renmae preset created
	DOvar = DOpus.Aliases('dopusdata').path + '\\Rename Presets\\' + DOvar + '.orp';
	cmd.ClearFiles();
	cmd.RunCommand('DELETE "' + DOvar + '" NORECYCLE FORCE QUIET');
	map_values = null;
	map_items = null;
	res = null;
	return;
}

function UseDialogMode(tab, cmd, arg_values, ignoreempty) {
	var tab_path = tab.path + '';
	if (Script.Vars.Exists(tab_path)) return null;
	Log(2, 'DIALOG                  : ' + arg_values);
	var colname, show_all = false;
	var colheader, colcategory, search_option;
	if (custom_names_map === undefined) custom_names_map = Script.config.custom_columns_names.empty ? null :
		(Script.Vars.Exists('custom_columns_names') ? Script.Vars.Get('custom_columns_names') : getCustomMap('custom_columns_names', true, true));
	if (typeof arg_values === 'string') {
		arg_values = arg_values.toLowerCase().split(',');
		for (var i = 0; i < arg_values.length; i++) {
			if (arg_values[i] === 'filterall') show_all = true;
			else if (custom_names_map !== null && custom_names_map.Exists(arg_values[i])) {
				Log(1, 'Found custom name ' + arg_values[i] + ' => ' + custom_names_map(arg_values[i]));
				colname = custom_names_map(arg_values[i]);
			}
			else if (!isNaN(arg_values[i])) { //a number referring column position in Lister
				try {
					colname = tab.format.columns(arg_values[i]).name;
				}
				catch (err) {}
			}
			else colname = arg_values[i];
		}
	}
	if (!colname) {
		Log(3, 'No valid column was provided. Using "name" as default');
		colname = 'name';
	}
	var g_fromlib = FSU.PathType(tab.path) === 'lib';
	if (!str_tools) str_tools = DOpus.Create().StringTools();
	custom_categories_map = getCustomMap('custom_columns_categories', false, true);
	var saved_categories = Script.Vars.Exists('saved_categories') ? Script.Vars.Get('saved_categories') : DOpus.Create().Map();
	var map_columns = MapColHeaders();
	Log(1, 'list Evaluator columns  : ' + Script.config['list Evaluator columns']);
	Log(1, 'list Shell columns      : ' + Script.config['list Shell columns']);
	Log(1, 'list Script columns     : ' + Script.config['list Script columns']);

	Log(1, 'Checking "' + colname + '" column data...');
	if (colname.indexOf(':') !== -1) {
		colheader = colname.slice(0, 3);
		colheader = colheader === 'scp' ? ' (Script)' : colheader.indexOf('sh') === 0 ? ' (Shell)' : ' (Evaluator)';
		try {
			colheader = tab.format.columns(colname).header + colheader;
		}
		catch (err) {
			colheader = colname + colheader;
		}
		map_columns.Set(colheader, colname);
		map_columns.Set(colname, null);
	}
	else if (!map_columns.Exists(colname)) {
		Log(3, 'No valid column was provided. Using "name" as default');
		colname = 'name';
	}
	arg_values = null;
	Script.Vars.Set('no_clear_output', true);
	if (!g_hide_ext) g_hide_ext = tab.format.hide_ext;
	var dlg = {};
	dlg.gui = tab.Dlg();
	dlg.gui.template = 'filter_ui';
	dlg.gui.title = script_label + ' v' + script_version + ' : ' + tab_path;
	// dlg.gui.disable_window = tab;
	dlg.gui.icon = DOpus.LoadImage(FSU.Resolve('/home\\dopusrt.exe') + ',2');
	dlg.gui.Create();
	dlg.name = dlg.gui.Control('name_combo');
	dlg.query_edit = dlg.gui.Control('query_edit');
	dlg.clear_btn = dlg.gui.Control('clear_btn');
	dlg.category = dlg.gui.Control('category_combo');
	dlg.listview = dlg.gui.Control('listview');
	dlg.list_and = dlg.gui.Control('use_and_checkbox');
	dlg.more = dlg.gui.Control('more_options');
	dlg.message = dlg.gui.Control('message');
	dlg.total_title = dlg.gui.Control('total_title');
	dlg.to_coll = dlg.gui.Control('tocoll_btn');
	dlg.sel_btn = dlg.gui.Control('sel_btn');
	dlg.refresh_btn = dlg.gui.Control('refresh_btn');
	dlg.save_btn = dlg.gui.Control('save_cat_btn');
	dlg.edit_btn = dlg.gui.Control('edit_cat_btn');
	dlg.search_btn = dlg.gui.Control('search_btn');
	dlg.regex_btn = dlg.gui.Control('regex_btn');
	dlg.case_btn = dlg.gui.Control('case_btn');
	dlg.diac_btn = dlg.gui.Control('diac_btn');
	dlg.ww_btn = dlg.gui.Control('ww_btn');
	dlg.ignore_btn = dlg.gui.Control('ignore_btn');
	var font_id = dlg.gui.CreateFont('Segoe UI', 0, 'b');
	dlg.regex_btn.SetFont(font_id);
	dlg.case_btn.SetFont(font_id);
	dlg.diac_btn.SetFont(font_id);
	dlg.ww_btn.SetFont(font_id);
	dlg.regex_btn.autosize();
	dlg.case_btn.autosize();
	dlg.diac_btn.autosize();
	dlg.ww_btn.autosize();
	dlg.sel_btn.SetFont(font_id);
	dlg.sel_btn.autosize();
	dlg.refresh_btn.x = dlg.sel_btn.x + dlg.sel_btn.cx + 5;
	dlg.refresh_btn.autosize();
	dlg.to_coll.autosize();
	dlg.save_btn.autosize();
	dlg.edit_btn.autosize();
	dlg.ignore_btn.autosize();
	var multimap = DOpus.Create().OrderedMap();
	var colObj = DOpus.Create().Map();
	var values_map = DOpus.Create().OrderedMap();
	var sel_values = DOpus.Create().UnorderedSet();
	var nested_items = DOpus.Create().OrderedMap();
	var curr_checked_items = DOpus.Create().StringSet();
	var curr_visible_items = DOpus.Create().StringSet();
	var curr_item_parents = DOpus.Create().StringSet();
	var curr_checked_parents = DOpus.Create().StringSet();
	var collection_name = getCollPath(script_name);
	multimap('search_options') = DOpus.Create().Vector('Filter using FAYT Syntax', 'Filter as text');
	multimap('regex_btn') = false;
	multimap('case_btn') = false;
	multimap('diac_btn') = false;
	multimap('ww_btn') = false;
	multimap('items_trs') = ' ' + str_tools.LanguageStr(28483).toLowerCase(); //Items
	var g_unique_code, from_subdialog;
	var g_is_curr_tab = true;
	var regex_path_lib = /^lib:.*\/\?[a-z0-9]+$/;

	if (FSU.ComparePath(collection_name, tab.path, 'p')) { //tab.path is the same as collection
		dlg.to_coll.enabled = false;
		collection_name = '';
	}
	dlg.listview.AddGroup('Checked', 1);
	dlg.listview.AddGroup('Values', 0);
	dlg.listview.EnableGroupView(true);
	dlg.listview.columns.GetColumnAt(0).name = str_tools.LanguageStr(24250); //Value
	dlg.gui.Control('category_title').label = '<#%vs_listview_header_text>' + str_tools.LanguageStr(12) + ' :</#>'; //Type
	dlg.gui.Control('category_title').autosize();
	dlg.gui.AddHotkey('focus_name', 'f2');
	dlg.gui.AddHotkey('focus_query', 'f3');
	dlg.gui.AddHotkey('custom_dialog', 'f4');
	dlg.gui.AddHotkey('refresh', 'f5');
	dlg.gui.AddHotkey('esc', 'Escape');
	dlg.gui.AddHotkey('sel_all', 'alt+1');
	dlg.gui.AddHotkey('sel_invert', 'alt+2');
	dlg.gui.AddHotkey('sel_none', 'alt+3');
	dlg.gui.AddHotkey('save_coll_close', 'f9');
	dlg.gui.AddHotkey('save_coll', 'shift+f9');

	dlg.gui.WatchTab(tab, 'navigate,activate,srcdst');
	dlg.gui.LoadPosition('fbc_dialog_simple');
	dlg.gui.Show();
	if (Script.config['list Evaluator columns']) {
		arg_values = GetEvaluatorColumns(saved_categories, map_columns);
		if (!arg_values.empty) map_columns.merge(arg_values);
	}
	if (!g_fromlib && Script.config['list Shell columns']) { //no shell columns in libraries
		arg_values = GetShellColumns(saved_categories, map_columns);
		if (!arg_values.empty) map_columns.merge(arg_values);
	}
	if (Script.config['list Script columns']) {
		arg_values = GetScriptColumns(saved_categories, map_columns);
		if (!arg_values.empty) map_columns.merge(arg_values);
	}
	Log(1, 'Total listed columns    : ' + map_columns.count);
	Script.Vars.Set('saved_categories', saved_categories);
	var map_columns_enum = new Enumerator(map_columns);
	updateNameCombo(dlg.name.value, true);
	if (!colheader) {
		Log(4, 'Fatal error while loading column headers. Probably related to a translation issue.');
		return null;
	};
	dlg.gui.SetTaskbarGroup(script_name);
	Script.Vars.Set(tab_path, tab_path);

	LoadValues(true, show_all, '', false);
	var msg, g_query, max_item_length;
	multimap('customize_trs') = str_tools.LanguageStr(490).replace('&', '') + '...'; //C&ustomize...
	multimap('between_trs') = str_tools.LanguageStr(2163) + '...'; //Between
	multimap('string') = DOpus.Create().Vector('Start with...', 'Not start with...', multimap('customize_trs'));
	multimap('number') = DOpus.Create().Vector(multimap('between_trs'), multimap('customize_trs'));
	multimap('size') = DOpus.Create().Vector(multimap('between_trs'), multimap('customize_trs'));
	multimap('duration') = DOpus.Create().Vector(multimap('between_trs'), multimap('customize_trs'));
	multimap('date') = DOpus.Create().Vector(str_tools.LanguageStr(2195), str_tools.LanguageStr(2196), 'This week', 'This month', 'This year', str_tools.LanguageStr(9312) + '...', str_tools.LanguageStr(9310) + '...', 'Time ago...', multimap('between_trs'), multimap('customize_trs'));
	multimap('months') = DOpus.Create().Vector('January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December');
	multimap('days') = DOpus.Create().Vector(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31);

	while (true) {
		msg = dlg.gui.GetMsg();
		if (!msg.result) break;
		if (msg.event === 'tab') {
			if (msg.value === 'close') {
				Log(2, 'Tab was closed. Closing dialog..');
				dlg.gui.EndDlg(0);
			}
			tab.Update();
			if (msg.value === 'navigate') {
				if (tab.path + '' !== tab_path) {
					Log(2, 'Path was changed. Closing dialog..');
					dlg.gui.EndDlg(0);
				}
				else {
					Log(2, 'Tab was reloaded. Reloading data...');
					LoadValues(true, false, dlg.query_edit.value, false);
				}
			}
			else if (msg.value === 'activate' || msg.value === 'srcdst') {
				g_is_curr_tab = tab.lister.activetab.def_value == tab.def_value;
				if (!g_is_curr_tab) DisableControls();
				else EnableControls();
			}
		}
		else if (msg.event === 'selchange') {
			if (msg.control === 'name_combo') {
				dlg.gui.KillTimer('update_list_timer');
				colheader = msg.value;
				colname = map_columns(colheader);
				Log(1, 'Changing to column "' + colname + '" ; header : ' + colheader);
				LoadValues(false, false, '', true);
			}
			else if (msg.control === 'category_combo') colcategory = msg.value;
		}
		else if (msg.event === 'editchange') {
			if (msg.control === 'query_edit' && !from_subdialog) {
				dlg.gui.SetTimer((dlg.query_edit.value !== '') ? 250 : 10, 'update_list_timer');
			}
			else if (msg.control === 'name_combo') dlg.gui.SetTimer(500, 'update_name_timer');
		}

		else if (msg.control === 'listview') {
			if (msg.event === 'checked') SelectFromDlg(msg.value, false, dlg.list_and.enabled && dlg.list_and.value);
			else if (msg.event === 'dblclk') SelectFromDlg(msg.value, 2, dlg.list_and.enabled && dlg.list_and.value);
		}
		else if (msg.event === 'timer') {
			if (msg.control === 'update_list_timer') {
				dlg.gui.KillTimer('update_list_timer');
				dlg.sel_btn.enabled = false;
				dlg.refresh_btn.enabled = false;
				if (search_option === 0) updateSugListSyntax(dlg.query_edit.value);
				else updateSugList(dlg.query_edit.value);
				dlg.sel_btn.enabled = true;
				dlg.refresh_btn.enabled = true;
			}
			else if (msg.control === 'update_name_timer') {
				dlg.gui.KillTimer('update_name_timer');
				updateNameCombo(dlg.name.value);
			}
		}

		else if (msg.event === 'hotkey' && g_is_curr_tab) {
			if (msg.control === 'focus_name') dlg.name.focus = true;
			else if (msg.control === 'focus_query') dlg.query_edit.focus = true;
			else if (msg.control === 'esc') {
				dlg.query_edit.value = '';
				dlg.query_edit.focus = true;
			}
			else if (msg.control === 'refresh') LoadValues(msg.qualifiers === 'shift', true, '', false);
			else if (msg.control === 'sel_none') SelectFromDlg(null, 0, dlg.list_and.enabled && dlg.list_and.value);
			else if (msg.control === 'sel_invert') SelectFromDlg(null, 2, dlg.list_and.enabled && dlg.list_and.value);
			else if (msg.control === 'sel_all') SelectFromDlg(null, 1, dlg.list_and.enabled && dlg.list_and.value);
			else if (msg.control === 'save_coll') SaveFilterToCollection(curr_checked_items.empty ? curr_visible_items : curr_checked_items, false);
			else if (msg.control === 'save_coll_close') SaveFilterToCollection(curr_checked_items.empty ? curr_visible_items : curr_checked_items, true);
			else if (msg.control === 'custom_dialog') showSubDialog(multimap(colcategory).length, colcategory);
		}
		else if (msg.event === 'click') {
			if (msg.control === 'tocoll_btn' && msg.focus)
				SaveFilterToCollection(curr_checked_items.empty ? curr_visible_items : curr_checked_items, msg.qualifiers !== 'shift');
			else if (msg.control === 'search_btn' && msg.focus) {
				var dlgMenu = DOpus.Dlg();
				dlgMenu.choices = multimap('search_options');
				dlgMenu.menu = search_option === 1 ? [0, 2] : [2, 0];
				dlgMenu.Show();
				if (dlgMenu.result) {
					search_option = dlgMenu.result - 1;
					Script.Vars.Set('search_option', search_option);
					Script.Vars('search_option').persist = true;
					Log(1, 'New Search Option : ' + multimap('search_options')(search_option));
					dlg.query_edit.cuetext = multimap('search_options')(search_option);
					ChangeSearchModifiers(search_option === 1 || colcategory === 'string');
					dlg.query_edit.value = '';
				}
				dlgMenu = null;
			}
			else if (msg.control === 'use_and_checkbox' && msg.focus) SelectFromDlg(null, false, msg.data);
			else if (msg.control === 'clear_btn' && msg.focus) dlg.query_edit.value = '';
			else if (msg.control === 'edit_cat_btn' && msg.focus) {
				saved_categories = editCategoriesDlg(dlg.gui, saved_categories);
				custom_categories_map = getCustomMap('custom_columns_categories', false, true);
				if ((!saved_categories.Exists(colname) && !custom_categories_map.Exists(colname)) || (saved_categories.Exists(colname) && colcategory !== saved_categories(colname))) {
					Log(2, 'Reloading data with new category...');
					map_columns(colname) = null; //clear values in map to re-read and convert to new category
					LoadValues(false, false, '', false);
				}
			}
			else if (msg.control === 'save_cat_btn' && msg.focus) {
				Log(2, 'Saving type for column "' + colname + '":"' + colcategory + '"');
				try {
					custom_categories_map(colname) = colcategory;
					Script.Vars.Set('custom_columns_categories', custom_categories_map);
					saved_categories.Set(colname, colcategory);
					map_columns(colname) = null; //clear values in map to re-read and convert to new category
					LoadValues(false, false, '', false);
				}
				catch (err) {
					Log(3, 'Unable to save type in config for this column : ' + err.description);
				}
			}
			else if (msg.control === 'more_options') {
				var dlgMenu = DOpus.Dlg();
				dlgMenu.choices = multimap(colcategory);
				dlgMenu.menu = 0;
				showSubDialog(dlgMenu.Show(), colcategory);
				dlgMenu = null;
			}
			else if (msg.control === 'refresh_btn') LoadValues(msg.qualifiers === 'shift', true, '', false);
			else if (msg.control === 'sel_btn') {
				if (msg.value === 'sel_all') SelectFromDlg(null, 1, dlg.list_and.enabled && dlg.list_and.value);
				else if (msg.value === 'sel_none') SelectFromDlg(null, 0, dlg.list_and.enabled && dlg.list_and.value);
				else if (msg.value === 'sel_invert') SelectFromDlg(null, 2, dlg.list_and.enabled && dlg.list_and.value);
			}
			else if (msg.control === 'ignore_btn') {
				g_ignore_empty = !g_ignore_empty;
				dlg.ignore_btn.label = '<a id="ign_empty">' + (g_ignore_empty ? '<%ddbi:133>' : '<%ddbi:133:8>') + '</a>';
				if (dlg.query_edit.value) dlg.gui.SetTimer(20, 'update_list_timer');
			}
			else { //search flags buttons
				multimap(msg.control) = !multimap(msg.control);
				ChangeSearchModifiers(search_option === 1 || colcategory === 'string');
				if (dlg.query_edit.value) dlg.gui.SetTimer(20, 'update_list_timer');
			}
		}

	}
	if (dlg.gui.result === 0) g_query = '';
	dlg.gui.SavePosition('fbc_dialog_simple');
	dlg.gui.DestroyFont(font_id);
	colObj = null;
	dlg = null;
	values_map = null;
	files = null;
	map_enum = null;
	map_columns = null;
	multimap = null;
	nested_items = null;
	curr_visible_items = null;
	curr_item_parents = null;
	curr_checked_parents = null;
	curr_checked_items = null;
	Script.Vars.Delete('no_clear_output');
	Script.Vars.Delete(tab_path);
	Script.Vars.Set('saved_categories', saved_categories);
	Script.Vars('saved_categories').persist = true;
	saved_categories = null;
	return g_query;

	function showSubDialog(entry, category) {
		g_query = '';
		if (entry === 0) return;
		Log(1, 'SHOWING ADVANCED DIALOG => option :' + entry);
		var value;
		if (category === 'date' && entry <= 5) //'Today', 'Yesterday', 'This week', 'This month', 'This year'
			g_query = '==' + multimap(category)(entry - 1).toLowerCase().replace(' ', '');
		else if (category === 'date' && entry <= 7) { //'In month...', 'In day...'
			var dlgMenu = DOpus.Dlg();
			dlgMenu.choices = entry === 6 ? multimap('months') : multimap('days');
			dlgMenu.menu = 0;
			value = dlgMenu.Show();
			dlgMenu = null;
			g_query = value !== 0 ? ('==' + value + (entry === 6 ? 'm' : 'd')) : '';
		}
		else {
			var error = false;
			var customized = multimap(category)(entry - 1) === multimap('customize_trs');
			var between = multimap(category)(entry - 1) === multimap('between_trs');
			var time_ago = multimap(category)(entry - 1) === 'Time ago...';
			var customize_str = customized && category === 'string' && entry > 2;
			var dlg_height = customize_str ? 66 : 42;
			var dlg_width = time_ago ? 176 : 268;

			template = '<resources><resource name="subdialog" type="dialog">';
			template += '<dialog fontsize="0" height="' + dlg_height + '" lang="english" standard_buttons="cancel" width="' + dlg_width + '">';
			if (customized || category === 'string') {
				template += '<control height="40" ' + (customized ? '' : 'enable="no" ') + 'name="operator_combo" type="combo" width="26" x="6" y="8" />';
				template += '<control edit="yes" height="40" name="query_combo" resize="w" type="combo" width="228" x="36" y="8" />';
				if (customize_str) {
					template += '<control height="10" name="casem_chk" title="' + DOpus.strings.Get('flag0') + '" type="check" width="76" x="10" y="22" />';
					template += '<control height="10" name="ign_chk" title="' + DOpus.strings.Get('flag3') + '" type="check" width="76" x="102" y="22" />';
					template += '<control height="10" name="ww_chk" title="' + DOpus.strings.Get('flag1') + '" type="check" width="76" x="182" y="22" />';
					template += '<control height="10" name="regex_chk" title="' + DOpus.strings.Get('flag2') + '" type="check" width="76" x="10" y="34" />';
					template += '<control height="10" name="wild_chk" title="' + DOpus.strings.Get('flag5') + '" type="check" width="100" x="102" y="34" />';
				}
			}
			else if (time_ago) {
				template += '<control name="lower_combo" halign="left" height="12" number="yes" type="edit" width="28" x="28" y="8" />';
				template += '<control name="upper_combo" type="combo" edit="yes" height="40" width="110" x="60" y="8" />';
				template += '<control name="static1" halign="center" type="static" valign="center" height="8" title="&gt;=" width="20" x="6" y="10" />';
			}
			else {
				template += '<control name="lower_combo" edit="yes" height="40" type="combo" width="100" x="6" y="8" />';
				template += '<control name="upper_combo" type="combo" edit="yes" height="40" width="100" x="164" y="8" />';
				template += '<control name="static1" halign="center" type="static" valign="center" height="8" title="' + str_tools.LanguageStr(9200) + '" width="20" x="126" y="10" />';
			}
			dlg_height -= 18;
			dlg_width -= 106;
			template += '<control height="14" name="search_btn" title="' + str_tools.LanguageStr(3688) + '" type="button" width="50" x="' + dlg_width + '" y="' + dlg_height + '" />';
			dlg_width -= 52;
			template += '<control default="yes" height="14" name="filter_btn" title="&amp;' + str_tools.LanguageStr(28808) + '" type="button" width="50" x="' + dlg_width + '" y="' + dlg_height + '" />';
			template += '</dialog></resource></resources>';

			var dlgsub = {};
			try {
				dlgsub.gui = DOpus.Dlg();
				dlgsub.gui.template = template;
				dlgsub.gui.title = script_label + ' v' + script_version + ' - ' + colheader + ' : ' + multimap(category)(entry - 1);
				dlgsub.gui.window = dlg.gui;
				dlgsub.gui.disable_window = dlg.gui;
				dlgsub.gui.Create();
			}
			catch (err) {
				Log(4, 'Unable to create the dialog!');
				error = true;
			}
			if (error) return;
			if (between || time_ago) {
				dlgsub.lower = dlgsub.gui.Control('lower_combo');
				dlgsub.upper = dlgsub.gui.Control('upper_combo');
				if (between) {
					setSubValues(dlgsub.lower);
					setSubValues(dlgsub.upper);
				}
				else {
					dlgsub.upper.AddItem('hours ago from now');
					dlgsub.upper.AddItem('days ago from now');
					dlgsub.upper.AddItem('months ago from now');
					dlgsub.upper.AddItem('years ago from now');
				}
			}
			else {
				dlgsub.query = dlgsub.gui.Control('query_combo');
				setSubValues(dlgsub.query);
			}
			if (customized) {
				dlgsub.operator = dlgsub.gui.Control('operator_combo');
				setOperators(dlgsub.operator, category);
			}
			dlgsub.filter = dlgsub.gui.Control('filter_btn');
			dlgsub.search = dlgsub.gui.Control('search_btn');
			dlgsub.filter.enabled = dlgsub.search.enabled = false;
			dlgsub.gui.WatchTab(tab, 'navigate,activate,srcdst');
			dlgsub.gui.Show();
			var msg, value;
			while (true) {
				msg = dlgsub.gui.GetMsg();
				if (!msg.result) break;
				if (msg.event === 'tab') {
					if (msg.value) dlgsub.gui.EndDlg(0);
				}
				else if (msg.event === 'editchange' || msg.event === 'selchange') {
					if (msg.control === 'query_combo')
						dlgsub.filter.enabled = dlgsub.search.enabled = msg.value ? true : false;
					else if (msg.control === 'lower_combo' || msg.control === 'upper_combo') {
						if (time_ago) value = dlgsub.lower.value && dlgsub.upper.value.name ? true : false;
						else value = dlgsub.lower.value.name && dlgsub.upper.value.name ? true : false;
						dlgsub.filter.enabled = dlgsub.search.enabled = value;
					}
				}
				if (msg.event === 'click') {
					if (msg.control === 'regex_chk' && msg.focus) {
						if (msg.data) dlgsub.gui.Control('wild_chk').value = false;
					}
					else if (msg.control === 'wild_chk' && msg.focus) {
						if (msg.data) dlgsub.gui.Control('regex_chk').value = false;
					}
					else if (msg.control.slice(-4) === '_btn') {
						g_search_mode = msg.control === 'search_btn';
						g_flags = 0;
						if (category === 'string' && entry <= 2) {
							if (g_search_mode) g_flags += (1 << 2);
							else multimap('regex_btn') = true;
							g_query += (entry === 2 ? '!=^' : '==^') + dlgsub.query.value.name;
						}
						else {
							if (customize_str) {
								if (dlgsub.gui.Control('casem_chk').value) g_search_mode ? g_flags += (1 << 0) : multimap('case_btn') = true;
								if (dlgsub.gui.Control('ww_chk').value) g_search_mode ? g_flags += (1 << 1) : multimap('ww_btn') = true;
								if (dlgsub.gui.Control('ign_chk').value) g_search_mode ? g_flags += (1 << 3) : multimap('diac_btn') = false;
								if (dlgsub.gui.Control('wild_chk').value) g_search_mode ? g_use_wilcards = true : '';
								if (dlgsub.gui.Control('regex_chk').value) g_search_mode ? g_flags += (1 << 2) : multimap('regex_btn') = true;
							}
							if (between)
								g_query += '==' + dlgsub.lower.value.name + '..' + dlgsub.upper.value.name;
							else if (time_ago) {
								g_query += '>=' + dlgsub.lower.value + dlgsub.upper.value.name.charAt(0) + 'a';
							}
							else
								g_query += dlgsub.operator.value.name + dlgsub.query.value.name;
						}
						dlgsub.gui.EndDlg(1);
					}
				}
			}
			if (dlgsub.gui.result === 0) g_query = '';
			dlgsub = null;
		}
		if (g_query) {
			Log(1, 'Sending query to command: ' + g_query);
			if (g_search_mode) {
				g_query = colname + g_query;
				dlg.gui.EndDlg(1);
			}
			else {
				updateSugListSyntax(g_query);
				SelectFromDlg(null, 1, dlg.list_and.enabled && dlg.list_and.value);
				g_query = '';
			}
		}
		return;
	}

	function LoadValues(erase_values, refresh_list, query_value, new_column) {
		var success;
		g_ini_timer = new Date();
		try {
			Log(1, '=== LOADING "' + colname + '" PARAMS : erase=' + erase_values + ' ; refresh=' + refresh_list + ' ; new_column=' + new_column + ' ===');
			var unique_code;
			DisableControls();
			dlg.message.enabled = true;
			dlg.message.visible = true;
			dlg.message.label = 'Retrieving values. Please wait...';
			g_flat_state = cmd.IsSet('Set FLATVIEW=grouped');
			Log(1, '   FlatView grouped     : ' + g_flat_state);
			if (map_columns(colname) !== null) {
				Log(1, '   Getting previous saved column data...');
				colObj.assign(map_columns(colname)('colObj'));
				unique_code = map_columns(colname)('unique_code');
				if (!erase_values) {
					Log(1, '   Getting previous saved column values...');
					values_map.assign(map_columns(colname)('values'));
				}
			}
			else {
				Log(1, '   Building new column data object...');
				if (colname.indexOf(':') !== -1) { //Evaluator,script,Shell column
					colObj('category') = custom_categories_map.Exists(colname) ? custom_categories_map(colname) : '';
					colObj('builtin') = false;
					colObj('multi') = false;
					colObj('metatype') = null;
					colObj('property') = null;
				}
				else {
					colObj = getColumnData(colname);
					if (custom_categories_map.Exists(colname)) colObj('category') = custom_categories_map(colname);
				}
				colObj('name') = colname;
				map_columns.Set(colname, DOpus.Create().Map());
				map_columns(colname)('colObj') = DOpus.Create().Map();
				map_columns(colname)('colObj').assign(colObj);
			}
			Log(1, '   => category : ' + colObj('category') + ' ; builtin : ' + colObj('builtin') + ' ; multi : ' + colObj('multi') + ' ; metatype : ' + colObj('metatype') + ' ; property : ' + colObj('property'));

			if (!erase_values) erase_values = CheckNewItems();
			if (erase_values) {
				Log(1, '   Cleaning saved values...');
				sel_values.clear();
				curr_visible_items.clear();
				curr_item_parents.clear();
				curr_checked_items.clear();
				nested_items.clear();
				g_unique_code = new Date().getTime();
			}
			if (new_column) {
				sel_values.clear();
				if (!curr_checked_items.empty) {
					Log(1, '   Assigning checked items to visible items...');
					curr_visible_items.assign(curr_checked_items);
					curr_item_parents.assign(curr_checked_parents);
					curr_checked_items.clear();
				}
			}
			else if (refresh_list) {
				Log(1, '   Refreshing list...');
				// if (g_flat_state) {
				// 	cmd.RunCommand('Set SHOWEVERYTHING=on');
				// 	cmd.RunCommand('Set SHOWEVERYTHING=off');
				// }
				// else
				cmd.RunCommand('Select NOPATTERN SHOWHIDDEN');
				sel_values.clear();
				curr_visible_items.clear();
				curr_item_parents.clear();
				nested_items.clear();
			}

			setCategoryCombo(dlg.category, colObj('category'));
			Log(1, '   Global unique code   : ' + g_unique_code);
			Log(1, '   Column unique code   : ' + unique_code);

			if (unique_code !== g_unique_code) {
				getColValues(colname);
				map_columns(colname)('values') = DOpus.Create().OrderedMap();
				map_columns(colname)('values').assign(values_map);
				map_columns(colname)('unique_code') = g_unique_code;
			}
			values_map.assign(getVisibleItemsMap());
			map_enum = new Enumerator(values_map);
			max_item_length = 0;
			for (; !map_enum.atEnd(); map_enum.moveNext()) {
				var key = map_enum.item();
				if (max_item_length < values_map(key)('list').count) max_item_length = values_map(key)('list').count;
			}
			max_item_length = String(max_item_length).length;
			Log(1, '   max value length     : ' + max_item_length);
			dlg.message.label = '';
			dlg.message.enabled = false;
			dlg.message.visible = false;

			EnableControls();
			dlg.query_edit.cuetext = multimap('search_options')(search_option);

			if (dlg.query_edit.value === query_value) dlg.gui.SetTimer(20, 'update_list_timer');
			else dlg.query_edit.value = query_value;
			dlg.query_edit.focus = true;
			Log(1, '   visible items : ' + curr_visible_items.count + '; item parents : ' + curr_item_parents.count);
			success = true;
		}
		catch (err) {
			Log(3, 'Error loading values for column ' + colname + '. Closing dialog...');
			dlg.gui.EndDlg(0);
		}
		Log(1, '=== LOADED FINISHED IN ' + (new Date() - g_ini_timer) + ' ms ===');
		return success;
	}

	function getColValues(colname) {
		values_map.clear();
		var set_items = GetItemsVector(tab.all, tab.hidden);
		Log(1, '   Getting new column values for "' + colname + '" : ' + set_items.count + ' items');
		if (colObj('builtin')) getBuiltInValues(colname, set_items);
		else getOtherColValues(colname, set_items);
		return;
	}

	function getBuiltInValues(colname, set_items) {
		var ini = new Date();
		var item, helper, value, h, item_value, item_str;
		if (colname === 'extdir' && !g_ext_dir) g_ext_dir = str_tools.LanguageStr(2132);
		else if (colname === 'attr') h = DOpus.Create().Map('r', str_tools.LanguageStr(6135), 'h', str_tools.LanguageStr(6134), 's', str_tools.LanguageStr(1215), 'a', str_tools.LanguageStr(6131),
			'c', str_tools.LanguageStr(1213), 'o', str_tools.LanguageStr(6177), 'i', str_tools.LanguageStr(6178), 'e', str_tools.LanguageStr(1214), 'p', str_tools.LanguageStr(6179));

		for (var i = 0; i < set_items.count; i++) {
			if (dlg.gui.result !== undefined) break;
			value = null;
			item_value = undefined;
			item = set_items(i);
			item_str = g_fromlib ? item.realpath.def_value : item.def_value;
			try {
				if (colname === 'name') {
					value = tab.format.hide_ext ? item.name_stem_m : item.name;
					item_value = item;
				}
				else if (colname === 'ext') {
					if (item_value = item.ext.toLowerCase()) item_value = item_value.slice(1);
					else item_value = ''
				}
				else if (colname === 'extdir') {
					if (item.is_dir) item_value = g_ext_dir;
					else if (item_value = item.ext.toLowerCase()) item_value = item_value.slice(1);
				}
				else if (colname === 'attr') {
					helper = item.attr_text.replace(/\-/g, '');
					if (helper) {
						item_value = null;
						for (var k = 0; k < helper.length; k++) {
							if (!values_map.Exists(h(helper.charAt(k)))) values_map(h(helper.charAt(k))) = DOpus.Create().Map('value', helper.charAt(k), 'list', DOpus.Create().StringSet());
							values_map(h(helper.charAt(k)))('list').insert(item_str);
						}
					}
				}
				else if (colname === 'keywords') {
					helper = item.metadata.tags;
					if (!helper.empty) {
						for (var k = 0; k < helper.count; k++) {
							if (item_value = helper(k).toLowerCase()) {
								if (!values_map.Exists(item_value)) values_map(item_value) = DOpus.Create().Map('value', item_value, 'list', DOpus.Create().StringSet());
								values_map(item_value)('list').insert(item_str);
							}
						}
						item_value = null;
					}
				}
				else if (colname === 'label') {
					helper = item.Labels();
					if (!helper.empty) {
						for (var k = 0; k < helper.count; k++) {
							if (item_value = helper(k).toLowerCase()) {
								if (!values_map.Exists(item_value)) values_map(item_value) = DOpus.Create().Map('value', item_value, 'list', DOpus.Create().StringSet());
								values_map(item_value)('list').insert(item_str);
							}
						}
						item_value = null;
					}
				}
				else if (colname === 'mp3genre') {
					if (item_value = item.metadata.audio.mp3genre) {
						helper = item_value.split(',');
						for (var k = 0; k < helper.length; k++) {
							if (item_value = helper[k].trim()) {
								if (!values_map.Exists(item_value)) values_map(item_value) = DOpus.Create().Map('value', item_value, 'list', DOpus.Create().StringSet());
								values_map(item_value)('list').insert(item_str);
							}
						}
						item_value = null;
					}
				}
				else if (colname === 'parent')
					item_value = item.realpath.Parent().filepart;
				else if (colname === 'parentlocation')
					item_value = item.realpath.Parent().filepart + ' (' + item.realpath.Parent().pathpart + ')';
				else if (colname === 'parentpath')
					item_value = item.realpath.Parent().pathpart;
				else if (colname === 'path')
					item_value = item.realpath.pathpart;
				else if (colname === 'pathlen')
					item_value = String(item).length;
				else if (colname === 'streamcount') {
					helper = FSU.GetADSNames(item);
					item_value = helper.empty ? '' : helper.count;
					helper = null;
				}
				else if (colname === 'pathrel')
					item_value = String(item.realpath).replace(String(tab.path), '');
				else if (colname === 'desc') {
					var helper = item.metadata.other.usercomment;
					item_value = helper ? helper : '';
					helper = item.metadata.other.autodesc;
					item_value += (helper ? ((item_value ? ' - ' : '') + helper) : '');
				}
				else if (colname === 'sizeauto') {
					item_value = item.size;
					value = item_value.fmt;
				}
				else if (colname === 'sizekb') {
					item_value = item.size;
					value = item_value / 1024 + ' kb';
				}
				else if (colname.slice(-4) === 'time' && !colObj('metatype')) {
					item_value = item[colObj('property')];
					value = DOpus.TypeOf(item_value) === 'object.Date' ? item_value.Format('t', 'S') : '';
				}
				else {
					// metatype = colObj('metatype');
					if (!colObj('metatype')) item_value = item[colObj('property')];
					else {
						var item_metadata = item.metadata.def_value;
						if (colObj('metatype') === true || (typeof colObj('metatype') === 'string' && colObj('metatype') == item_metadata)) item_value = item.metadata[item_metadata][colObj('property')];
						// if (colObj('metatype') === true) item_value = item.metadata[item.metadata.def_value][colObj('property')];
						// else if (typeof colObj('metatype') === 'string') item_value = item.metadata[colObj('metatype')][colObj('property')];
					}
					if (item_value !== undefined) {
						if (colObj('multi') === true) {
							var helper = item_value.split(';');
							for (var k = 0; k < helper.length; k++) {
								helper[k] = helper[k].trim();
								if (!values_map.Exists(helper[k])) values_map(helper[k]) = DOpus.Create().Map('value', item_value, 'list', DOpus.Create().StringSet());
								values_map(helper[k])('list').insert(item_str);
							}
							item_value = null;
						}
						else {
							var val_category = DOpus.TypeOf(item_value);
							switch (val_category) {
								case 'rational':
									val_category = 'number';
									break;
								case 'int':
									val_category = 'number,duration';
									break;
								case 'object.FileSize':
									val_category = 'size';
									break;
								case 'object.Date':
									val_category = 'date';
									break;
								default:
									val_category = 'string';
									break;
							}
							if (val_category.indexOf(colObj('category')) == -1) {
								Log(1, 'Converting ' + item + ' to ' + colObj('category'));
								item_value = convertType(colObj('category'), item_value, '', true);
							}
							else value = DOpus.TypeOf(item_value) === 'object.Date' ? item_value.Format('', 'ns') : item_value + '';
						}
					}
					else value = '';
				}
				if (item_value === undefined || item_value === '') value = '<no value>';
				else if (value === null) value = item_value;
				else if (value === '') value = '<no value>';
			}
			catch (err) {
				Log(3, '   => Error reading ' + colname + ' in ' + item + ' : ' + err);
				item_value = undefined;
				value = '<no value>';
			}
			finally {
				if (item_value !== null) {
					if (!values_map.Exists(value)) values_map(value) = DOpus.Create().Map('value', item_value, 'list', DOpus.Create().StringSet());
					values_map(value)('list').insert(item_str);
				}
			}
		}
		h = null;
		helper = null;
		Log(1, '   => Built in cols done in : ' + (new Date() - ini) + ' ms');
		return;
	}

	function getOtherColValues(colname, set_items) {
		var ini = new Date();
		var value, item, regex_helper, item_value;
		var varname = 'fbc_values';
		var is_evaluator = Script.config['custom columns obtention'] === 0;
		var Evmap = is_evaluator ? Global_GetTextualFiltersValues(set_items, cmd, colname, varname) : Global_GetRenamePresetValues(set_items, cmd, colname, varname);

		for (var e = new Enumerator(Evmap); !e.atEnd(); e.moveNext()) {
			item = e.item();
			value = Evmap(item);
			if (!is_evaluator) value = value.replace(/[\u200E\u200F\u202A-\u202E\u2066-\u2069]/g, '');
			if (colcategory === 'duration') {
				if (!regex_helper) regex_helper = /^(\d+)d (\d+):(\d+):(\d+)$|^(\d+):(\d+):(\d+)$|^(\d+):(\d+)$|^(\d+)$/;
				item_value = convertToDuration(value, regex_helper);
			}
			else if (colcategory === 'number') {
				if (!regex_helper) regex_helper = /[^\d,.]/g;
				item_value = convertToNumber(value, regex_helper);
			}
			else if (colcategory === 'size') {
				if (!regex_helper) regex_helper = /^([\d,.]+) *([kb|mb|gb|tb|pb]*)$/i
				item_value = convertToSize(value, regex_helper);
			}
			else if (colcategory === 'date')
				item_value = convertToDate(value, 'sD');
			else item_value = value;
			if (value === '') value = '<no value>';
			if (!values_map.Exists(value)) values_map(value) = DOpus.Create().Map('value', item_value, 'list', DOpus.Create().StringSet());
			values_map(value)('list').insert(item);
		}
		DOpus.Vars.Delete(varname);
		Log(1, '   => Other cols done as ' + colcategory + ' using ' + (is_evaluator ? 'Textual Filters' : 'Rename Preset') + ' in : ' + (new Date() - ini) + ' ms');
		Evmap = null;
		return;
	}

	function setCategoryCombo(control, category) {
		Log(1, '   Setting category...');
		with(control) {
			redraw = false;
			RemoveItem(-1);
			if (category) {
				AddItem(category);
				colcategory = category;
				search_option = Script.Vars.Exists('search_option') ? Script.Vars.Get('search_option') : 0;
			}
			else {
				search_option = 1;
				colcategory = 'string';
				Log(3, '   ' + colname + ' doesn\'t have a registered category!');
				AddItem('string');
				AddItem('number');
				AddItem('date');
				AddItem('size');
				AddItem('duration');
			}
			redraw = true;
			value = 0;
		}
		return;
	}

	function setOperators(control, category) {
		with(control) {
			AddItem('==');
			AddItem('!=');
			if (category !== 'string') {
				AddItem('>');
				AddItem('>=');
				AddItem('<');
				AddItem('<=');
			}
			value = 0;
		}
		return;
	}

	function setSubValues(control) {
		map_enum.moveFirst();
		for (; !map_enum.atEnd(); map_enum.moveNext())
			control.AddItem(map_enum.item());
		return;
	}

	function updateNameCombo(ctrl_value, first) {
		Log(1, '=== UPDATING COLNAMES => value : "' + ctrl_value.name + '" ===');
		if (!first && ctrl_value.index !== -1) return;
		var value = ctrl_value.name;
		var item;
		if (value) var regex = new RegExp(value.replace(/[.*+?^${}()|[\]\\]/g, '\\$&'), 'i');
		dlg.name.redraw = false;
		dlg.name.RemoveItem(-1);
		map_columns_enum.moveFirst();
		for (; !map_columns_enum.atEnd(); map_columns_enum.moveNext()) {
			item = map_columns_enum.item();
			if (typeof map_columns(item) === 'string') {
				if (value === '' || regex.test(item)) {
					dlg.name.AddItem(item);
					if (!colheader && map_columns(item) === colname)
						colheader = item;
				}
			}
		}
		dlg.name.redraw = true;
		dlg.name.label = ctrl_value.name;
		dlg.name.SelectRange(dlg.name.label.length,-1);
		if (first && colheader) dlg.name.value = dlg.name.getItemByName(colheader);
		Log(1, '=== UPDATING COLNAMES => DONE');
		return;
	}

	function updateSugListSyntax(value) {
		Log(1, '=== UPDATING "' + colname + '" WITH FAYT SYNTAX : "' + value + '" ===');
		value = value.trim();
		g_ini_timer = new Date();
		var add, fila, item, item_value, count, query, operator, queryObj;
		if (value !== '') {
			g_flags = 0;
			if (multimap('case_btn')) g_flags += (1 << 0);
			if (multimap('ww_btn')) g_flags += (1 << 1);
			if (multimap('regex_btn')) g_flags += (1 << 2);
			if (!multimap('diac_btn')) g_flags += (1 << 3);
			queryObj = getQueryObj(value, colObj, colcategory, (colcategory === 'string') ? /^(!=|==)$/ : /^(!=|>=|<=|==|<|>)$/);
		}
		else queryObj = null;
		dlg.listview.redraw = false;
		dlg.listview.RemoveItem(-1);
		var total_sel = 0;
		map_enum.moveFirst();
		for (; !map_enum.atEnd(); map_enum.moveNext()) {
			item = map_enum.item();
			count = values_map(item)('list').count;
			if (sel_values.Exists(item)) {
				total_sel += count;
				fila = dlg.listview.getItemAt(dlg.listview.AddItem(item, 0, 1));
				fila.checked = 1;
				fila.subitems(0) = ZeroPad(count, max_item_length);
			}
			else {
				if (queryObj === null || queryObj.is_reference) add = true;
				else {
					item_value = values_map(item)('value');
					if (item_value === undefined) {
						if (!g_ignore_empty) item_value = '';
						else continue;
					}
					query = queryObj.query;
					operator = queryObj.operator;
					if (colcategory === 'duration')
						add = compare(item_value, query, operator);
					else if (colcategory === 'number')
						add = compare(item_value, query, operator);
					else if (colcategory === 'size')
						add = compareSizes(item_value, query, operator);
					else if (colcategory === 'date')
						add = compareDates(item_value, query, queryObj.raw_query, operator);
					else {
						if (DOpus.TypeOf(query) === 'object.Vector') add = compareGroups(item_value, query, operator, !multimap('diac_btn'));
						else {
							if (colname === 'name') add = compareStrings(item, query, operator);
							else add = compareStrings(item_value, query, operator);
						}
					}
				}
				if (add) {
					fila = dlg.listview.getItemAt(dlg.listview.AddItem(item, 0, 0));
					fila.subitems(0) = ZeroPad(count, max_item_length);
				}
			}
		}
		dlg.listview.columns.autosize();
		dlg.listview.columns.GetColumnAt(1).sort = -1;
		dlg.listview.redraw = true;
		dlg.total_title.label = total_sel + ' / ' + curr_visible_items.count + multimap('items_trs') + ' (+' + curr_item_parents.count + ')';
		queryObj = null;
		Log(1, '=== UPDATING DONE : ' + dlg.total_title.label + ' ===================')
		return;
	}

	function updateSugList(value) {
		Log(1, '=== UPDATING "' + colname + '" : ' + value + '" ===');
		dlg.listview.redraw = false;
		dlg.listview.RemoveItem(-1);
		if (value) {
			if (!multimap('diac_btn')) value = str_tools.RemoveDiacritics(value);
			if (!multimap('regex_btn')) value = value.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
			if (multimap('ww_btn')) value = '\\b' + value + '\\b';
			try {
				value = new RegExp(value, multimap('case_btn') ? "" : "i");
			}
			catch (err) {
				value = new RegExp(value.replace(/[.*+?^${}()|[\]\\]/g, '\\$&'), multimap('case_btn') ? "" : "i");
			}
		}
		map_enum.moveFirst();
		var total_sel = 0;
		var count;
		for (; !map_enum.atEnd(); map_enum.moveNext()) {
			item = map_enum.item();
			count = values_map(item)('list').count;
			if (sel_values.Exists(item)) {
				total_sel += count;
				fila = dlg.listview.getItemAt(dlg.listview.AddItem(item, 0, 1));
				fila.checked = 1;
				fila.subitems(0) = ZeroPad(count, max_item_length);

			}
			else if (value === '' || value.test(item)) {
				fila = dlg.listview.getItemAt(dlg.listview.AddItem(item, 0, 0));
				fila.subitems(0) = ZeroPad(count, max_item_length);
			}
		}
		dlg.listview.columns.autosize();
		dlg.listview.columns.GetColumnAt(1).sort = -1;
		dlg.listview.redraw = true;
		dlg.total_title.label = total_sel + ' / ' + curr_visible_items.count + multimap('items_trs') + ' (+' + curr_item_parents.count + ')';
		Log(1, '=== UPDATING DONE : ' + dlg.total_title.label + ' ===================')
		return;
	}

	function SelectFromDlg(value, check, is_and) {
		if (typeof value === 'number' && value >= dlg.listview.count) return;
		Log(1, '=== SELECTING ITEMS => mode : ' + check + ' ===');
		var fila, num_items, num_parents;
		curr_checked_items.clear();
		if (value !== -1) {
			dlg.listview.redraw = false;
			num_items = value === null ? dlg.listview.count : 1;
			for (var i = 0; i < num_items; i++) {
				fila = value === null ? dlg.listview.getItemAt(i) : dlg.listview.getItemByName(value);
				if (check !== false) fila.checked = check === 2 ? (fila.checked === 1 ? 0 : 1) : check;
				if (fila.checked === 1) sel_values.insert(fila.name);
				else sel_values.erase(fila.name);
				fila.group = fila.checked;
			}
			dlg.listview.columns.GetColumnAt(1).sort = -1;
			dlg.listview.redraw = true;
		}
		if (!sel_values.empty) {
			if (is_and) {
				mainloop: for (var i = 0; i < values_map(sel_values(0))('list').length; i++) {
					for (var j = 1; j < sel_values.length; j++) {
						if (!values_map(sel_values(j))('list').Exists(values_map(sel_values(0))('list')(i))) continue mainloop;
					}
					curr_checked_items.insert(values_map(sel_values(0))('list')(i));
				}
			}
			else {
				for (var i = 0; i < sel_values.length; i++) {
					if (curr_checked_items.empty) curr_checked_items.assign(values_map(sel_values(i))('list'));
					else curr_checked_items.merge(values_map(sel_values(i))('list'));
				}
			}
		}
		num_items = 0;
		num_parents = 0;
		curr_checked_parents = DOpus.Create().StringSet();
		var curr_items = DOpus.Create().StringSet();
		if (!curr_checked_items.empty) {
			curr_items.assign(curr_checked_items);
			num_items = curr_checked_items.count;
			if (!nested_items.empty) {
				for (var i = curr_checked_items.length - 1; i >= 0; i--) {
					if (nested_items.Exists(curr_checked_items(i))) {
						curr_checked_parents.merge(nested_items(curr_checked_items(i)));
					}
				}
				if (!curr_checked_parents.empty) {
					num_parents = curr_checked_parents.count;
					curr_items.merge(curr_checked_parents);
				}
			}
		}
		else if (!sel_values.empty)
			cmd.RunCommand('Select NONE HIDEUNSEL');
		else if (!curr_visible_items.empty) {
			num_items = curr_visible_items.count;
			num_parents = curr_item_parents.count;
			curr_items.assign(curr_visible_items);
			curr_items.merge(curr_item_parents);
		}
		if (!curr_items.empty) {
			cmd.SetFiles(curr_items);
			// if (g_flat_state) {
			// 	cmd.RunCommand('Set SHOWEVERYTHING=on');
			// 	cmd.RunCommand('Set SHOWEVERYTHING=off');
			// }
			// else cmd.RunCommand('Select NOPATTERN SHOWHIDDEN');
			cmd.RunCommand('Select FROMSCRIPT=unhide HIDEUNSEL DESELECTNOMATCH');
			cmd.RunCommand('Select FIRST MAKEVISIBLE=inmediate');
		}
		dlg.total_title.label = num_items + ' / ' + curr_visible_items.count + multimap('items_trs') + ' (+' + num_parents + ')';
		Log(2, '=== SELECT DONE : ' + dlg.total_title.label);
		return;
	}

	function getVisibleItemsMap() {
		var key, item, list, item_str, path, path_str, sw;
		tab.Update();
		if (curr_visible_items.empty) {
			for (var i = 0; i < tab.all.count; i++) {
				item = tab.all(i);
				item_str = item.realpath.def_value;
				if (!sw && (item.nested || (g_flat_state && item.path.def_value != tab_path))) {
					path = item.path;
					while (path.def_value != tab_path) {
						//Log(1, 'path:' + path.def_value + '; tabpath:' + tab_path);
						if (g_fromlib && regex_path_lib.test(path.def_value)) break;
						path_str = g_fromlib ? FSU.Resolve(path) : path.def_value;
						if (!nested_items.Exists(item_str)) nested_items(item_str) = DOpus.Create().StringSet();
						nested_items(item_str).insert(path_str);
						//Log(1, 'parent:' + path_str);
						curr_item_parents.insert(path_str);
						if (!path.Parent()) break;
					}
				}
				else if (!sw) sw = true;
				curr_visible_items.insert(item_str);
			}
		}
		var visibles_map = DOpus.Create().OrderedMap();
		for (var e = new Enumerator(values_map); !e.atEnd(); e.moveNext()) {
			key = e.item();
			list = values_map(key)('list');
			for (var i = 0; i < list.length; i++) {
				item = list(i);
				if (curr_visible_items.Exists(item)) {
					if (!visibles_map.Exists(key)) visibles_map(key) = DOpus.Create().Map('value', values_map(key)('value'), 'list', DOpus.Create().StringSet());
					visibles_map(key)('list').insert(item);
				}
			}
		}
		list = null;
		Log(1, '   getVisibleItemsMap IN ' + (new Date() - g_ini_timer) + ' ms ');

		return visibles_map;
	}

	function CheckNewItems() {
		tab.Update();
		for (var i = 0; i < tab.all.count; i++) {
			if (!curr_visible_items.Exists(tab.all(i).realpath.def_value) && !curr_item_parents.Exists(tab.all(i).realpath.def_value)) {
				Log(1, tab.all(i) + ' is a new item : Data needs an update');
				return true;
			}
		}
		return false;
	}

	function DisableControls() {
		Log(1, 'Disabling controls...');
		dlg.name.enabled = false;
		dlg.to_coll.enabled = false;
		dlg.category.enabled = false;
		dlg.save_btn.enabled = false;
		dlg.edit_btn.enabled = false;
		dlg.search_btn.enabled = false;
		dlg.query_edit.enabled = false;
		dlg.clear_btn.enabled = false;
		dlg.list_and.enabled = false;
		ChangeSearchModifiers(false);
		dlg.more.enabled = false;
		dlg.listview.enabled = false;
		dlg.sel_btn.enabled = false;
		dlg.refresh_btn.enabled = false;
		dlg.ignore_btn.enabled = false;
		return;
	}

	function EnableControls() {
		Log(1, 'Enabling controls...');
		dlg.name.enabled = true;
		if (collection_name && !values_map.empty) dlg.to_coll.enabled = true;
		dlg.category.enabled = true;
		dlg.edit_btn.enabled = true;
		dlg.save_btn.enabled = colObj('category') === '';
		dlg.search_btn.enabled = colObj('category') !== '';
		dlg.query_edit.enabled = true;
		dlg.clear_btn.enabled = true;
		dlg.list_and.enabled = colObj('multi');
		dlg.more.enabled = true;
		dlg.listview.enabled = true;
		dlg.sel_btn.enabled = true;
		dlg.refresh_btn.enabled = true;
		ChangeSearchModifiers(search_option === 1 || colcategory === 'string');
		dlg.ignore_btn.enabled = true;
		dlg.ignore_btn.label = '<a id="ign_empty">' + (g_ignore_empty ? '<%ddbi:133>' : '<%ddbi:133:8>') + '</a>';
		return;
	}

	function ChangeSearchModifiers(enable) {
		dlg.regex_btn.enabled = enable;
		dlg.case_btn.enabled = enable;
		dlg.diac_btn.enabled = enable;
		dlg.ww_btn.enabled = enable;
		if (enable) {
			dlg.case_btn.label = '<a id="link">' + (multimap('case_btn') ? '<#!HOTLIGHT>' : '<#!BTNTEXT>') + 'Aa</#></a>';
			dlg.regex_btn.label = '<a id="link">' + (multimap('regex_btn') ? '<#!HOTLIGHT>' : '<#!BTNTEXT>') + '•✱</#></a>';
			dlg.diac_btn.label = '<a id="link">' + (multimap('diac_btn') ? '<#!HOTLIGHT>' : '<#!BTNTEXT>') + 'ůü</#></a>';
			dlg.ww_btn.label = '<a id="link">' + (multimap('ww_btn') ? '<#!HOTLIGHT>' : '<#!BTNTEXT>') + 'ww</#></a>';
		}
		else {
			dlg.case_btn.label = '';
			dlg.regex_btn.label = '';
			dlg.diac_btn.label = '';
			dlg.ww_btn.label = '';
		}
		return;
	}

	function SaveFilterToCollection(item_set, close) {
		if (!item_set.empty) {
			try {
				cmd.SetFiles(item_set);
				if (cmd.filecount) {
					if (FSU.Exists(collection_name)) cmd.RunCommand('@runonce Delete FILE="' + collection_name + '" FORCE QUIET');
					if (cmd.RunCommand('Copy COPYTOCOLL=member CREATEFOLDER="' + collection_name + '"')) {
						Log(2, 'Copied ' + cmd.filecount + ' items to ' + collection_name);
						cmd.RunCommand('@runonce Go "' + collection_name + '" NEWTAB=findexisting' + (!close ? ',nofocus' : ',tofront'));
						if (close) dlg.gui.EndDlg(0);
					}
				}
			}
			catch (err) {
				Log(4, 'Unable to save items to collection! : ' + err.description);
			}
		}
		return;
	}
}

function UseTextualFilters(tab, items, cmd, queryObj, ColObj, isFilter) {
	Log(2, 'FILTERING VIA TEXTUAL FILTERS IN COLUMN "' + ColObj('name') + '" FOR: ' + (!isFilter ? tab.path : (items.count + ' ITEMS')));
	var check_if_empty = false;
	var cmdline;
	if (queryObj.query === '' && (queryObj.operator === '==' || queryObj.operator === '!=')) {
		check_if_empty = true;
		cmdline = (queryObj.operator === '==' ? '!' : '') + 'IsSet("' + ColObj('name') + '")';
	}
	else {
		var cmdline_arr = Global_BuildTextualFilter(queryObj, ColObj, false);
		if (cmdline_arr !== null) {
			cmdline = 'return ' + cmdline_arr[0];
			Log(1, '   Variables            : ' + cmdline_arr[1].count);
			for (var i = cmdline_arr[1].length - 1; i >= 0; i--)
				cmdline = cmdline_arr[1](i) + cmdline;
		}
		cmdline_arr = null;
	}
	if (cmdline) {
		if (g_ignore_empty && !check_if_empty) cmdline = '=if (IsSet("' + ColObj('name') + '")){' + cmdline + ';} else {return false;}';
		else cmdline = '=' + cmdline + ';';
		Log(1, '   Filter defined       : ' + cmdline);
		var collname = getCollPath(script_name, ColObj('name'));
		if (tab.path.def_value == collname && !Script.config['create subcollections']) {
			Log(3, '   Unable to use search mode in this path, since is a results collection from a previous search');
			isFilter = true;
		}
		Global_UseTextualFilter(tab, items, cmd, isFilter, cmdline, collname, ColObj('name'));
	}
	else Log(4, '   Unable to build a filter!!!');
	return;
}

function UseMultiTextualFilters(tab, query, isFilter) {
	Log(2, 'USING MULTI MODE        : "' + query + '"' + '; Filter = ' + isFilter);
	query = query.trim();
	if (query) {
		g_use_wilcards = true;
		var g_parsed_filter = '';
		var g_cols_to_add = '';
		var c, error, g_curr_prefix, g_curr_suffix;
		var quote = false;
		var g_variables = DOpus.Create().Vector();
		var g_curr_query = g_curr_prefix = g_curr_suffix = '';
		var AND = '&&';
		var OR = '||';

		for (var str_pos = 0; str_pos < query.length; str_pos++) {
			c = query.charAt(str_pos);
			if (c === '"') quote = !quote;
			else if (!quote) {
				if (c === '(') g_curr_prefix += c;
				else if (c === ')') g_curr_suffix += c;
				else if (query.substr(str_pos, AND.length) === AND) {
					if (!BuildFilter(AND)) {
						error = true;
						break;
						g_parsed_filter = null;
					}
					str_pos += AND.length - 1;
				}
				else if (query.substr(str_pos, OR.length) === OR) {
					if (!BuildFilter(OR)) {
						error = true;
						break;
						g_parsed_filter = null;
					}
					str_pos += OR.length - 1;
				}
				else g_curr_query += c;
			}
			else
				g_curr_query += c;
		}
		if (!error && g_curr_query) {
			if (!BuildFilter()) g_parsed_filter = null;
		}
		if (g_parsed_filter) {
			var cmd = DOpus.Create().Command();
			cmd.SetSourceTab(tab);
			g_parsed_filter = 'return ' + g_parsed_filter + ';';
			Log(1, '   Variables            : ' + g_variables.count);
			for (var i = g_variables.length - 1; i >= 0; i--)
				g_parsed_filter = g_variables(i) + g_parsed_filter;
			g_parsed_filter = '=' + g_parsed_filter;
			Log(1, '   Filter defined       : ' + g_parsed_filter);
			var collname = getCollPath(script_name, 'FBC Multi');
			if (tab.path + '' === collname && !Script.config['create subcollections']) {
				Log(3, '   Unable to use search mode in this path, since is a results collection from a previous search');
				isFilter = true;
			}
			if (isFilter) {
				if (g_filter_visible) {
					Log(2, '   Using only currently visible items...');
					var items = GetItemsVector(tab.all);
				}
				else var items = GetItemsVector(tab.all, tab.hidden);
			}
			else var items = null;
			Global_UseTextualFilter(tab, items, cmd, isFilter, g_parsed_filter, collname, g_cols_to_add);
			cmd = null;
		}
		else Log(4, '   Unable to build a filter!!!');
		g_variables = null;
	}
	return;

	function BuildFilter(logical_op) {
		var objCol = getColObj(tab, g_curr_query);
		if (objCol === null) return null;
		var objQuery = getQueryObj(g_curr_query, objCol);
		if (objQuery === null) return null;
		var check_if_empty = false;
		if (objQuery.query === '' && (objQuery.operator === '==' || objQuery.operator === '!=')) {
			check_if_empty = true;
			var arr_filter = (objQuery.operator === '==' ? '!' : '') + 'IsSet("' + objCol('name') + '")';
		}
		else var arr_filter = Global_BuildTextualFilter(objQuery, objCol, true);
		if (arr_filter === null) return null;
		g_curr_query = '';
		if (logical_op === AND) logical_op = ' and ';
		else if (logical_op === OR) logical_op = ' or ';
		else logical_op = '';
		if (g_ignore_empty && !check_if_empty) arr_filter[0] = '(IsSet("' + objCol('name') + '") and ' + arr_filter[0] + ')';

		g_parsed_filter += g_curr_prefix + arr_filter[0] + g_curr_suffix + logical_op;
		g_cols_to_add += (g_cols_to_add === '' ? '' : ',') + objCol('name');
		g_curr_prefix = '';
		g_curr_suffix = '';
		if (!arr_filter[1].empty) g_variables.append(arr_filter[1]);
		objQuery = null;
		objCol = null;
		return true;
	}
}

function Global_UseTextualFilter(tab, items, cmd, isFilter, cmdline, collname, cols_to_add) {
	var filter = DOpus.Create().Filter();
	if (filter.Set(cmdline)) {
		try {
			if (isFilter) {
				if (!items.empty) {
					var busy_indicator = DOpus.Create().BusyIndicator();
					busy_indicator.abort = true;
					if (busy_indicator.Init(tab, str_tools.LanguageStr(5101))) busy_indicator.Show();
					g_flat_state = cmd.IsSet('Set FLATVIEW=grouped');
					var regular_items = DOpus.Create().StringSetI();
					var nested_items = DOpus.Create().StringSetI();
					if (Script.config['show nested results with Textual Filters'] && (g_flat_state || tab.all.RemoveNested() > 0)) {
						Log(2, '   Filtering with support for nested results...');
						var item_path, sw;
						for (var i = items.length - 1; i >= 0; i--) {
							if (busy_indicator.abort) break;
							if (!sw && (items(i).nested || (g_flat_state && items(i).path.def_value != tab.path.def_value))) {
								try {
									if (items(i).MatchFilter(filter)) {
										nested_items.insert(items(i).def_value);
										item_path = items(i).path;
										while (item_path.def_value != tab.path.def_value) {
											nested_items.insert(item_path.def_value);
											regular_items.erase(item_path.def_value);
											if (!item_path.Parent()) break;
										}
									}
								}
								catch (err) {
									Log(3, '   Error retrieving parents for ' + items(i) + ' : ' + err.description);
								}
							}
							else {
								if (!sw) sw = true;
								regular_items.insert(items(i).def_value);
							}
						}
					}
					else regular_items.assign(items);
					cmd.SetFiles(regular_items);
					if (!nested_items.empty) {
						cmd.RunCommand('SELECT DESELECTNOMATCH FILTERDEF ' + cmdline);
						cmd.SetFiles(nested_items);
						Log(1, '   Nested results       :' + nested_items.count);
						cmd.RunCommand('Select FROMSCRIPT HIDEUNSEL');
					}
					else cmd.RunCommand('SELECT DESELECTNOMATCH HIDEUNSEL FILTERDEF ' + cmdline);
					cmd.RunCommand('SELECT FIRST MAKEVISIBLE=inmediate');
					busy_indicator.Destroy();
					busy_indicator = null;
					regular_items = null;
					nested_items = null;
				}
			}
			else {
				if (!Script.config['create subcollections']) cmd.RunCommand('Go TABCLOSE=dest PATH="' + collname + '"');
				cmdline = 'FIND IN "' + tab.path + '" RECURSE=yes SHOWRESULTS=dest,tab COLLNAME="' + collname + '" FILTERDEF ' + cmdline;
				if (cmd.RunCommand(cmdline)) {
					cmd.RunCommand('SET FOCUS=Dest');
					if (cols_to_add && Script.config['add column']) {
						tab.lister.Update();
						if (tab.lister.activetab.path == collname) {
							Log(1, '   Adding columns       : ' + cols_to_add);
							cmdline = 'SET COLUMNSADD "' + cols_to_add + '"';
							cmd.SetSourceTab(tab.lister.activetab);
							cmd.RunCommand(cmdline);
						}
					}
				}
				else Log(3, '   Error when searching :' + cmdline);
			}
		}
		catch (err) {
			Log(3, '   Error in the command : ' + err.description);
		}
	}
	else Log(4, '   Filter not valid     : ' + filter.lasterror.message);
	return;
}

function Global_GetBuiltinValue(item, colname, ColObj, tab_path_str) {
	var raw_value, helper, item_metadata;
	try {
		if (colname === 'name')
			return item;
		else if (colname === 'extdir')
			return item.is_dir ? g_ext_dir : item.ext.toLowerCase();
		else if (colname === 'attr')
			return item.attr_text.replace(/\-/g, '');
		else if (colname === 'keywords') {
			raw_value = '';
			for (var k = 0; k < item.metadata.tags.count; k++) raw_value += item.metadata.tags(k) + ';';
			return raw_value.slice(0, -1);
		}
		else if (colname === 'label') {
			raw_value = '';
			var labels = item.Labels();
			for (var k = 0; k < labels.count; k++) raw_value += labels(k) + ';';
			labels = null;
			return raw_value.slice(0, -1);
		}
		else if (colname.slice(-4) === 'time' && !colObj('metatype')) { //created,modify,accesed time
			raw_value = item[ColObj('property')];
			return raw_value.hour * 3600 + raw_value.min * 60 + raw_value.sec;
		}
		else if (colname === 'owner')
			return item.ShellProp('System.FileOwner', 'r');
		else if (colname === 'type')
			return item.ShellProp('System.ItemTypeText', 'r');
		else if (colname === 'parent')
			return item.realpath.Parent().filepart;
		else if (colname === 'parentlocation')
			return item.realpath.Parent().filepart + ' (' + item.realpath.Parent().pathpart + ')';
		else if (colname === 'parentpath')
			return item.realpath.Parent().pathpart;
		else if (colname === 'path')
			return item.realpath.pathpart;
		else if (colname === 'pathlen')
			return String(item).length;
		else if (colname === 'streamcount') {
			helper = FSU.GetADSNames(item);
			return helper.empty ? '' : helper.count;
		}
		else if (colname === 'pathrel')
			return String(item.realpath).replace(tab_path_str, '');
		else if (colname === 'desc') {
			helper = item.metadata.other.usercomment;
			raw_value = helper ? helper : '';
			helper = item.metadata.other.autodesc;
			raw_value += (helper ? ((raw_value ? ' - ' : '') + helper) : '');
			return raw_value;
		}
		else {
			if (!ColObj('metatype')) raw_value = item[ColObj('property')];
			else {
				item_metadata = item.metadata.def_value;
				if (ColObj('metatype') === true || (typeof ColObj('metatype') === 'string' && ColObj('metatype') == item_metadata)) raw_value = item.metadata[item_metadata][ColObj('property')];
				// else if raw_value = item.metadata[ColObj('metatype')][ColObj('property')];
				if (!g_ignore_empty && raw_value === undefined) {
					if (!ColObj('metatype') || ColObj('filetype') === '' || ColObj('filetype').test(item_metadata)) raw_value = '';
				}
			}
			if (raw_value !== undefined) {
				var val_category = DOpus.TypeOf(raw_value);
				switch (val_category) {
					case 'rational':
						val_category = 'number';
						break;
					case 'int':
						val_category = 'number,duration';
						break;
					case 'object.FileSize':
						val_category = 'size';
						break;
					case 'object.Date':
						val_category = 'date';
						break;
					default:
						// case 'string':
						val_category = 'string';
						break;
				}
				if (val_category.indexOf(ColObj('category')) == -1) {
					Log(1, 'Converting ' + item + ' to ' + ColObj('category'));
					raw_value = convertType(ColObj('category'), raw_value, '', true);
				}
			}
			return raw_value;
		}
	}
	catch (err) {
		// Log(3, 'Error retrieving "' + colname + '" property from ' + item + ' : ' + err.description);
		return null;
	}
	return null;
}

function Global_GetTextualFiltersValues(items, cmd, colname, varname) {
	var map_values = DOpus.Create().OrderedMap();
	var cmdline = 'fullpath + "\t" + (IsSet("' + colname + '")?(Val("' + colname + '") as str):"") + "\n"';
	try {
		cmd.SetFiles(items);
		DOpus.Vars.Set(varname, '');
		cmd.RunCommand('Select NONE');
		cmdline = 'Select FILTERDEF =$glob:' + varname + '+=' + cmdline + '; return false;';
		cmd.RunCommand(cmdline);
		DOpus.Delay(20);
		if (DOpus.Vars.Exists(varname)) {
			var value = DOpus.Vars.Get(varname).replace(/[\u200E\u200F\u202A-\u202E\u2066-\u2069]/g, '');
			value = value.split('\n');
			for (var i = 0; i < value.length; i++) {
				if (value[i]) {
					line = value[i].split('\t');
					map_values.Set(line[0], line[1]);
				}
			}
		}
		else
			Log(3, 'DOpus var ' + varname + ' doesn\'t exist!');
	}
	catch (err) {
		Log(3, 'Error retrieving values via textual filters : ' + err.description);
	}
	return map_values;
}

function Global_GetRenamePresetValues(items, cmd, colname, varname) {
	var map_values = DOpus.Create().OrderedMap();
	var item, a;
	cmd.SetFiles(items);
	DOpus.Vars.Delete(varname);
	if (CreateRenamePresetTxt(varname, '{=IsSet("' + colname + '")?Val("' + colname + '"):""=}')) {
		cmd.RunCommand('RENAME PRESET="' + varname + '"');
		DOpus.Delay(20);
		if (DOpus.Vars.Exists(varname)) {
			map_values = DOpus.Vars.Get(varname);
			Log(1, 'ITEMS RETRIEVED         : ' + map_values.count);
		}
		else
			Log(3, 'DOpus var ' + varname + ' doesn\'t exist!');
	}
	else
		Log(3, 'Unable to write rename preset file !!!');
	return map_values;
}

function Global_BuildTextualFilter(queryObj, obj_col, multi) {
	var operator = queryObj.operator;
	var query = queryObj.query;
	var cmdline = '';
	var variables = DOpus.Create().Vector();
	try {
		var col_name = obj_col('name').indexOf(':') !== -1 ? 'Val("' + obj_col('name') + '")' : obj_col('name');
		if (queryObj.is_reference) {
			if (obj_col('category') === 'string' && (operator === '==' || operator == '!='))
				cmdline = 'Val("' + queryObj.raw_query('name') + '") as str)' + operator + '(' + col_name + ' as str)';
			else if (obj_col('category') === 'date')
				cmdline = 'DateDiff("s",(Val("' + queryObj.raw_query('name') + '") as date),(' + col_name + ' as date))' + operator + '0';
			else //duration,size,number
				cmdline = 'Val("' + queryObj.raw_query('name') + '")' + operator + '' + col_name + ')';
		}
		else {
			switch (obj_col('category')) {
				case 'string':
					var flags = (g_flags & (1 << 0) ? 'c' : '') + (g_flags & (1 << 1) ? '' : 'p') + (g_flags & (1 << 3) ? 'i' : '');
					if (DOpus.TypeOf(query) === 'object.Vector') {
						if (g_flags & (1 << 2)) flags += 'r';
						else if (!g_use_wilcards) {
							var wild = FSU.NewWild();
							query[0] = wild.EscapeString(query[0]);
							query[1] = wild.EscapeString(query[1]);
							wild = null;
						}
						if (query[1] === '') {
							query[1] = '*';
							flags = 'p';
						}
						cmdline = ((operator == '!=') ? '!(' : '(') + 'Match(' + col_name + ',"grp:' + query[0] + '","fpi") and Match(' + col_name + ',"' + query[1] + '"';
						cmdline += flags ? (',"' + flags + '"))') : '))';
					}
					else {
						if (g_flags & (1 << 2)) flags += 'r';
						else if (!g_use_wilcards) {
							var wild = FSU.NewWild();
							query = wild.EscapeString(query);
							wild = null;
						}
						cmdline = ((operator == "!=") ? '!' : '') + 'Match(' + col_name + ',"' + query + '"';
						cmdline += flags ? (',"' + flags + '")') : ')';
					}
					break;
				case 'duration':
					var helper = '^(\\d+)d (\\d+):(\\d+):(\\d+)$|^(\\d+):(\\d+):(\\d+)$|^(\\d+):(\\d+)$';
					var varname = multi ? (+new Date * Math.floor(Math.random() * 1000)).toString(36) : '';
					variables.push_back('type' + varname + '=TypeOf(' + col_name + ');if(type' + varname + '=="path" || type' + varname + '=="date" || type' + varname + '=="str"){dur' + varname +
						'=RegEx(' + col_name + ', "' + helper + '", "\\1","e") as uint64 * 86400+RegEx(' + col_name + ', "' + helper + '", "\\2\\5","e") as uint64 * 3600+RegEx(' + col_name + ', "' +
						helper + '", "\\3\\6\\8","e") as uint64 * 60+RegEx(' + col_name + ', "' + helper + '", "\\4\\7\\9","e") as uint64;}else{dur' + varname + ' = ' + col_name + ';};');
					if (DOpus.TypeOf(query) === 'object.Vector') { //query is a range between 2 values (inclusive)
						cmdline += '(dur' + varname + ((operator == "!=") ? '<' : ' >= ') + query(0);
						cmdline += ((operator == "!=") ? ' or ' : ' and ');
						cmdline += 'dur' + varname + ((operator == "!=") ? '>' : ' <= ') + query(1) + ')';
					}
					else {
						cmdline += 'dur' + varname + operator + query;
					}
					break;
				case 'number':
					var varname = multi ? (+new Date * Math.floor(Math.random() * 1000)).toString(36) : '';
					variables.push_back('type' + varname + '=TypeOf(' + col_name + ');if(type' + varname + '=="path" || type' + varname + '=="date" || type' + varname + '=="str"){num' + varname +
						'=RegExS(' + col_name + ', "[^\\d,.]#", "",",",".","e") as double;}else{num' + varname + ' = ' + col_name + ';};');
					if (DOpus.TypeOf(query) === 'object.Vector') { //query is a range between 2 values (inclusive)
						cmdline = '(num' + varname + ((operator == "!=") ? '<' : ' >= ') + query(0);
						cmdline += ((operator == "!=") ? ' or ' : ' and ');
						cmdline += 'num' + varname + ((operator == "!=") ? '>' : ' <= ') + query(1) + ')';
					}
					else {
						cmdline = 'num' + varname + operator + query;
					}
					break;
				case 'size':
					if (DOpus.TypeOf(query) === 'object.Vector') { //query is a range between 2 values (inclusive)
						cmdline = '(' + col_name + ((operator == "!=") ? '<' : ' >= ') + query(0);
						cmdline += ((operator == "!=") ? ' or ' : ' and ');
						cmdline += col_name + ((operator == "!=") ? '>' : ' <= ') + query(1) + ')';
					}
					else {
						cmdline = col_name + operator + query;
					}
					break;
				case 'date':
					switch (queryObj.raw_query) {
						case 'thisyear':
						case 'year':
						case 'lastyear':
						case 'lyear':
							cmdline = 'DatePart((' + col_name + ' as date),"yyyy")' + operator + query(0).year;
							break;
						case 'rwhole':
							DateRange('"s"', query(0).Format("D#yyyy-MM-dd T#HH:mm:ss"), query(1).Format("D#yyyy-MM-dd T#HH:mm:ss"), query(0).sec == 0 ? '"D#yyyy-MM-dd T#HH:mm"' : 'date', query(1).sec == 0 ? '"D#yyyy-MM-dd T#HH:mm"' : 'date');
							break;
						case 'ryear':
						case 'rmyear':
						case 'rdate':
						case 'thismonth':
						case 'month':
						case 'lastmonth':
						case 'lmonth':
						case 'thisweek':
						case 'week':
						case 'lastweek':
						case 'lweek':
							DateRange('"d"', query(0).Format("D#yyyy-MM-dd"), query(1).Format("D#yyyy-MM-dd"), 'date', 'date');
							break;
						case 'today':
						case 'yesterday':
						case 'tomorrow':
						case 'ya':
						case 'Ma':
						case 'da':
						case 'd':
							DateDiff(query.Format('D#yyyy-MM-dd'), '"D#yyyy-MM-dd"', 'd');
							break;
						case 'whole':
						case 'ha':
							DateDiff(query.Format('D#yyyy-MM-dd T#HH:mm:ss'), 'date', 's');
							break;
						case 'sD':
							DateDiff(query.Format('D#yyyy-MM-dd T#HH:mm'), '"D#yyyy-MM-dd T#HH:mm"', 's');
							break;
						case 't':
							DateDiff(query.Format('T#HH:mm:ss'), '"T#HH:mm:ss"', 's');
							break;
						case 'day': //query is a number referring to a day
							DatePart(query, 'day' + (multi ? (+new Date * Math.floor(Math.random() * 1000)).toString(36) : ''), 'd');
							break;
						case 'm': //query is a number referring to a month
							DatePart(query, 'month' + (multi ? (+new Date * Math.floor(Math.random() * 1000)).toString(36) : ''), 'M');
							break;
						case 'tsD': //query is a number referring to seconds in HH:mm
							cmdline += '((DatePart((' + col_name + ' as date),"H") as int * 3600 + DatePart((' + col_name + ' as date),"m") as int * 60 )' + operator + query + ')';
							break;
					}
					break;
			}
		}
	}
	catch (err) {
		Log(3, '   Error building filter     : ' + err.description);
		return null;
	}
	if (!cmdline) return null;
	return [cmdline, variables];

	function DateRange(diff, date1, date2, as_value1, as_value2) {
		switch (operator) {
			case '==':
				cmdline += '(DateDiff(' + diff + ',"' + date1 + '",(' + col_name + ' as ' + as_value1 + '))>=0 and DateDiff(' + diff + ',"' + date2 + '",(' + col_name + ' as ' + as_value2 + '))<=0' + ')';
				break;
			case '!=':
				cmdline += '(DateDiff(' + diff + ',"' + date1 + '",(' + col_name + ' as ' + as_value1 + '))<0 or DateDiff(' + diff + ',"' + date2 + '",(' + col_name + ' as ' + as_value2 + '))>0' + ')';
				break;
			case '>':
			case '<=':
				cmdline += 'DateDiff(' + diff + ',"' + date2 + '",(' + col_name + ' as ' + as_value2 + '))' + operator + '0';
				break;
			case '>=':
			case '<':
				cmdline += 'DateDiff(' + diff + ',"' + date1 + '",(' + col_name + ' as ' + as_value1 + '))' + operator + '0';
				break;
		}
		return;
	}

	function DateDiff(date, as_value, comp) {
		cmdline += 'DateDiff("' + comp + '","' + date + '",(' + col_name + ' as ' + as_value + '))' + operator + '0';
		return;
	}

	function DatePart(value, value_name, type) {
		variables.push_back(value_name + '=DatePart((' + col_name + ' as date),"' + type + '");');
		if (DOpus.TypeOf(value) == 'object.Vector') {
			if (operator == '==') {
				cmdline += '(' + value_name + ' >= ' + value(0) + ' and ' + value_name + ' <= ' + value(1) + ')';
			}
			else {
				cmdline += '(' + value_name + ' < ' + value(0) + ' or ' + value_name + ' > ' + value(1) + ')';
			}
		}
		else {
			cmdline += value_name + operator + value;
		}
		return;
	}
}

function GlobalGetColumnsSet() {
	return DOpus.Create().StringSet('accessed', 'accesseddate', 'created', 'createddate', 'modified', 'modifieddate', 'attr', 'ext', 'sizekb', 'size', 'sizeauto', 'accessedtime',
		'createdtime', 'modifiedtime', 'extdir', 'fullpath', 'owner', 'type', 'perms', 'availability', 'streamcount', 'keywords', 'label', 'name', 'parent',
		'parentlocation', 'parentpath', 'path', 'pathrel', 'desc', 'userdesc', 'pathlen', 'copyright', 'producer', 'picdepth', 'mp3bpm', 'mp3disk',
		'mp3track', 'compilation', 'mp3album', 'mp3albumartist', 'mp3comment', 'mp3encodingsoftware', 'mp3info', 'mp3drm', 'doccreateddate',
		'docedittime', 'doclastsaveddate', 'pages', 'category', 'comments', 'creator', 'doclastsavedby', 'modversion', 'prodversion', 'moddesc', 'prodname', 'signer',
		'fontname', 'datedigitized', 'datetaken', 'datetimecreated', 'datetimeoriginal', '35mmfocallength', 'altitude', 'apertureval', 'digitalzoom', 'exposurebias', 'exposuretime', 'fnumber',
		'focallength', 'latitude', 'longitude', 'picphysx', 'picphysy', 'picresx', 'picresy', 'rotation', 'shutterspeed', 'subjectdistance', 'cameramake',
		'cameramodel', 'colormodel', 'colorspace', 'contrast', 'coords', 'exposureprogram', 'flash', 'imagedesc', 'imagequality', 'instructions', 'isospeed',
		'lensmake', 'lensmodel', 'macromode', 'meteringmode', 'picres', 'saturation', 'scenecapturetype', 'scenemode', 'sharpness', 'software', 'whitebalance',
		'broadcastdate', 'recordingtime', 'audiocount', 'channel', 'datarate', 'framerate', 'subtitlecount', 'videocount', 'director', 'alllangs', 'audiolangs',
		'credits', 'episodename', 'fourcc', 'hdrtypes', 'ishd', 'isrepeat', 'publisher', 'station', 'subtitlelangs', 'videocodec', 'videolangs', 'encodedby',
		'rating', 'target', 'duration', 'releasedate', 'mp3bitrate', 'mp3samplerate', 'mp3year', 'audiocodec', 'composers', 'conductors', 'mp3genre', 'mp3mode',
		'mp3title', 'mp3type', 'initialkey', 'companyname', 'subject', 'picsize', 'aspectratiogroup', 'picphyssize', 'aspectratio', 'picheight', 'picwidth',
		'mp3artists', 'author', 'title');
}
//Get info about the selected column. return null if not valid column
function getColObj(tab, query) {
	Log(2, 'PARSING COL TO OBJECT   : ' + query);
	custom_name = false;
	var category, metatype, property, operator, name, filetype;
	property = metatype = false;
	var col = false;
	query = query.trim();
	//Check if input makes reference to a particular column number
	var match = query.match(/^[cC](\d+) *((>=|=>|<=|=<|!=|==|=|<|>).*)/);
	//input is in the form C[#]<operator>query
	if (match) {
		Log(2, '   Explicit column      : C' + match[1]);
		try {
			if (tab.format.columns(0).name === 'index') //first columns is index so we just ignore it
				col = tab.format.columns(match[1]);
			else col = tab.format.columns(match[1] - 1);
			query = match[2];
		}
		catch (e) {
			Log(3, '   Not such column number could be found!!!');
			col = null;
		}
	}
	else {
		//Check if input makes reference to a custom column name
		if (custom_names_map === undefined) custom_names_map = Script.config.custom_columns_names.empty ? null :
			(Script.Vars.Exists('custom_columns_names') ? Script.Vars.Get('custom_columns_names') : getCustomMap('custom_columns_names', true, true));
		match = query.match(/^(.+?) *((?:>=|=>|<=|=<|!=|==|=|<|>).*)/);
		if (match) {
			Log(2, '   Explicit custom name : ' + match[1]);
			if (custom_names_map !== null && custom_names_map.Exists(match[1])) {
				custom_name = true;
				try {
					col = 'forced';
					name = custom_names_map(match[1]).toLowerCase();
					query = match[2];
				}
				catch (err) {
					Log(3, '   Column "' + match[1] + '" can\'t be found. Check if is a valid column');
					col = null;
				}
			}
			else {
				Log(1, '   "' + match[1] + '" seems is not registered or is invalid. Trying to use it anyway');
				col = 'forced';
				name = match[1].toLowerCase();
				query = match[2];
			}
		}
		if (col === false) { //no explicit reference, use default column
			Log(2, '   Implicit column mode');
			switch (Script.config.default_implicit_column) {
				case 1:
					col = tab.format.sort_field;
					break;
				case 2:
					var f_col = Script.config.user_defined_implicit_column;
					if (f_col) {
						col = 'forced';
						name = f_col.toLowerCase();
					}
					break;
				case 3:
					try {
						if (tab.highlighted.count > 0) {
							name = tab.highlighted[0].highlighted[0].column.toLowerCase();
							col = 'forced';
						}
						else Log(2, '   No highlighted columns available');
					}
					catch (err) {
						Log(1, '   Unable to get highlighted column name : ' + err.description);
						col = false;
					}
					break;
			}
			query = false;
			try {
				if (col === false) {
					Log(1, '   Using Name column as default');
					col = tab.format.columns('name');
				}
			}
			catch (e) {
				Log(2, '   Can\'t retrieve Name column info!');
				col = null;
			}
			custom_name = true;
		}
	}
	match = null;
	if (col === null) return null;
	if (col !== 'forced') name = col.name.toLowerCase();
	switch (name) { //list of unsupported columns
		case 'group':
		case 'md5sum':
		case 'shasum':
		case 'blake3sum':
		case 'crc32sum':
		case 'sha256sum':
		case 'sha512sum':
		case 'thumbnail':
		case 'status':
		case 'sizerel':
		case 'disksizeauto':
		case 'disksize':
		case 'disksizekb':
		case 'disksizerel':
		case 'uncompressedsize':
		case 'dircounttotal':
		case 'dircount':
		case 'filecounttotal':
		case 'filecount':
			Log(3, '   "' + name + '" column is not supported for filter');
			return null;
	}
	if (custom_categories_map === undefined) custom_categories_map = Script.Vars.Exists('custom_columns_categories') ? Script.Vars.Get('custom_columns_categories') : getCustomMap('custom_columns_categories', false, true);
	//check if column is a script/Evaluator/shell/special column
	if (name === 'availability' || name === 'perms' || name === 'signer') g_custom_col = true;
	else if (/^scp:.*$/.test(name)) g_custom_col = 'script';
	else if (/^eval:.*/.test(name)) g_custom_col = 'eval';
	else if (/^sh:.*/.test(name)) g_custom_col = 'shell';
	//Now libraries support shell columns!
	// if (g_custom_col === 'shell' && FSU.PathType(tab.path) === 'lib') {
	// 	Log(3, '   No shell columns available in Libraries!');
	// 	return null;
	// }
	if (custom_categories_map.Exists(name)) {
		category = custom_categories_map(name);
		Log(2, '   Overriding category to ' + category);
	}
	if (typeof g_custom_col == 'string') { //for non built-in columns
		if (!category && g_custom_col === 'eval') category = getCategoryforEvalCol(name.slice(5));
		else {
			Log(2, '   "' + name + '" column doesn\'t have a registered category type. Using "STRING" as default');
			category = 'string';
		}
	}
	else { //Get category for built-in columns
		info = getColumnData(name);
		if (info !== null) {
			if (!category) category = info('category');
			metatype = info('metatype');
			property = info('property');
			filetype = info('filetype');
			info = null;
		}
	}
	if (!category) {
		Log(3, '   Column "' + name + '" doesn\'t have a valid category');
		return null;
	}
	operator = (category === 'string') ? /^(!=|==)$/ : /^(!=|>=|<=|==|<|>)$/;
	name = name.replace(/\s/g, '');
	Log(2, '   name                 : ' + name);
	Log(2, '   category             : ' + category);
	Log(1, '   query                : ' + query);
	Log(1, '   metatype             : ' + metatype);
	Log(1, '   property             : ' + property);
	Log(1, '   filetype             : ' + filetype);
	Log(1, '   operator             : ' + operator);
	Log(2, 'PARSING COL DONE        : ' + (new Date() - g_ini_timer) + ' ms');
	return DOpus.Create().Map(
		'name', name,
		'category', category,
		'query', query,
		'metatype', metatype,
		'property', property,
		'operator', operator,
		'filetype', filetype
	);
}
//Get info about used input
function getQueryObj(query, ColObj, category, obj_operators) {
	//if the input contains reference to a column, get the new input from col obj
	if (ColObj.Exists('query') && ColObj('query') != false) query = ColObj('query');
	Log(2, 'PARSING INPUT TO OBJ    : ' + query);
	var quote = false;
	var operator = '';
	var currentPart = '';
	var helper = '';
	var c, from_dialog;
	var is_reference = false;
	if (category) from_dialog = true;
	else category = ColObj('category');
	if (!obj_operators) obj_operators = ColObj('operator');
	var op_regex = category === 'string' ? /!|=/ : /!|>|<|=/;
	query = query.trim();
	for (var i = 0; i < query.length; i++) {
		c = query.charAt(i);
		if (c === '"') quote = !quote;
		else if (!quote && op_regex.test(c)) operator += c;
		else currentPart += c;
	}
	if (operator === '' && (!ColObj.Exists('query') || ColObj('query') === false)) operator = '=='; //only allow implicit operator if column is implicit
	if (operator === '=' || operator == '!') operator += '=';
	else if (operator === '=>') operator = '>=';
	else if (operator === '=<') operator = '<=';

	if (operator === '' || !obj_operators.test(operator)) {
		if (!from_dialog) Log(3, '   No valid operator detected. Valid operators for ' + category + ' column types are : ' + obj_operators.source.substring(2, obj_operators.source.length - 2));
		return null;
	}
	currentPart = currentPart.trim();
	if (currentPart === '' && (operator === '==' || operator === '!=')) {
		if (category === 'size') currentPart = FSU.NewFileSize(0);
		else if (category === 'duration') currentPart = 0;
		else currentPart = '';
	}
	else {
		if (c = currentPart.match(/^\{(.*)\}$/)) {
			Log(1, '   Checking validity for ' + c[1]);
			helper = getColumnData(c[1]);
			if (helper === null) {
				if (!from_dialog) Log(3, '   Keyword ' + c[1] + ' is not a valid value');
				return null;
			}
			helper('name') = c[1];
			currentPart = c[1];
			is_reference = true;
		}
		else if (category === 'date') { //in case column type is date, make support for some special syntax
			if ((operator == '==' || operator == '!=') && (c = currentPart.match(/^((?:1[6-9]|2[0-9])\d{2}) *(?:-|\.\.) *((?:1[6-9]|2[0-9])\d{2})$/))) { //is a range of 2 years
				Log(1, '   ' + currentPart + ' is a range of 2 years in yyyy format');
				helper = 'ryear';
				currentPart = c;
			}
			else if (/^(1[6-9]|2[0-9])\d{2}$/.test(currentPart)) { //is a 4-digit number
				Log(1, '   ' + currentPart + ' is a year in yyyy format');
				helper = 'year';
			}
			else if ((operator == '==' || operator == '!=') && (c = currentPart.match(/^([1-9]|0[1-9]|1[0-2])[-/]((?:1[6-9]|2[0-9])\d{2}) *(?:\.\.) *([1-9]|0[1-9]|1[0-2])[-/]((?:1[6-9]|2[0-9])\d{2})$/))) { //is a 6-digit number with the format MM-yyyy or MM/yyyy
				Log(1, '   ' + currentPart + ' is a range of 2 dates MM-yyyy');
				helper = 'rmyear';
				currentPart = c;
			}
			else if (c = currentPart.match(/^([1-9]|0[1-9]|1[0-2])[-/]((?:1[6-9]|2[0-9])\d{2})$/)) { //is a 6-digit number with the format MM-yyyy or MM/yyyy
				Log(1, '   ' + currentPart + ' is a 6-digit number with the format MM-yyyy');
				helper = 'month';
				currentPart = c;
			}
			else if ((operator == '==' || operator == '!=') && (c = currentPart.match(/^([12]\d|3[01]|0?[1-9])[-/]([1-9]|0[1-9]|1[0-2])[-/]((?:1[6-9]|2[0-9])\d{2}) *(?:\.\.) *([12]\d|3[01]|0?[1-9])[-/]([1-9]|0[1-9]|1[0-2])[-/]((?:1[6-9]|2[0-9])\d{2})$/))) { //is a range of two dates dd-MM-yyyy
				Log(1, '   ' + currentPart + ' is a range of two dates with the format dd-MM-yyyy');
				helper = 'rdate';
				currentPart = c;
			}
			else if ((operator == '==' || operator == '!=') && (c = currentPart.match(/^((?:1[6-9]|2[0-9])\d{2})[-/]([1-9]|0[1-9]|1[0-2])[-/]([12]\d|3[01]|0?[1-9]) *(?:\.\.) *((?:1[6-9]|2[0-9])\d{2})[-/]([1-9]|0[1-9]|1[0-2])[-/]([12]\d|3[01]|0?[1-9])$/))) { //is a range of two dates yyyy-MM-dd
				Log(1, '   ' + currentPart + ' is a range of two dates with the format yyyy-MM-dd');
				helper = 'rdate';
				currentPart = c;
			}
			else if (c = currentPart.match(/^(\d+) *[hH][aA]$/)) { //is a number followed by 'ha' means x hours ago from now
				Log(1, '   ' + currentPart + ' means ' + c[1] + ' hours ago from now');
				currentPart = c[1];
				helper = 'ha';
			}
			else if (c = currentPart.match(/^(\d+) *[dD][aA]$/)) { //is a number followed by 'da' means x days ago from now
				Log(1, '   ' + currentPart + ' means ' + c[1] + ' days ago from now');
				currentPart = c[1];
				helper = 'da';
			}
			else if (c = currentPart.match(/^(\d+) *[mM][aA]$/)) { //is a number followed by 'ma' means x months ago from now
				Log(1, '   ' + currentPart + ' means ' + c[1] + ' months ago from now');
				currentPart = c[1];
				helper = 'Ma';
			}
			else if (c = currentPart.match(/^(\d+) *[yY][aA]$/)) { //is a number followed by 'ya' means x months ago from now
				Log(1, '   ' + currentPart + ' means ' + c[1] + ' years ago from now');
				currentPart = c[1];
				helper = 'ya';
			}
			else if ((operator == '==' || operator == '!=') && (c = currentPart.match(/^([12]\d|3[01]|0?[1-9]) *(?:-|\.\.) *([12]\d|3[01]|0?[1-9]) *[dD]$/))) { //is [1-31][-..][1-31]d
				Log(1, '   ' + currentPart + ' is a range of days');
				currentPart = c;
				helper = 'day';
			}
			else if (c = currentPart.match(/^([12]\d|3[01]|0?[1-9]) *[dD]$/)) { //is a number between 1 and 31 followed by 'd'
				Log(1, '   ' + c[1] + ' is a number between 1 and 31 followed by d');
				currentPart = c[1];
				helper = 'day';
			}
			else if (c = currentPart.match(/^([1-9]|0[1-9]|1[0-2]) *(?:-|\.\.) *([1-9]|0[1-9]|1[0-2]) *[mM]$/)) { //is a number between 1 and 12 followed by 'm'
				Log(1, '   ' + currentPart + ' is a range of months');
				currentPart = c;
				helper = 'm';
			}
			else if (c = currentPart.match(/^([1-9]|0[1-9]|1[0-2]) *[mM]$/)) { //is a number between 1 and 12 followed by 'm'
				Log(1, '   ' + c[1] + ' is a number between 1 and 12 followed by m');
				currentPart = c[1];
				helper = 'm';
			}
			else if (/^([1-9]|0[1-9]|1[0-2])$/.test(currentPart)) { //is a number between 1 and 12
				Log(1, '   ' + currentPart + ' is a number referring a month');
				helper = 'month';
			}
			else if ((operator == '==' || operator == '!=') && (c = currentPart.match(/^(0?[1-9]|[12][0-9]|3[01])[-/](0?[1-9]|1[0-2])[-/]((?:1[6-9]|2[0-9])\d{2}) (0?\d|1\d|2[0-3]):([0-5]?\d)(?::([0-5]?\d))? *(?:\.\.) *(0?[1-9]|[12][0-9]|3[01])[-/](0?[1-9]|1[0-2])[-/]((?:1[6-9]|2[0-9])\d{2}) (0?\d|1\d|2[0-3]):([0-5]?\d)(?::([0-5]?\d))?$/))) {
				Log(1, '   ' + currentPart + ' is a range of two dates with the format dd-MM-yyyy HH:mm:ss or HH:mm');
				helper = 'rwhole';
				currentPart = c;
			}
			else if ((operator == '==' || operator == '!=') && (c = currentPart.match(/^((?:1[6-9]|2[0-9])\d{2})[-/](0?[1-9]|1[0-2])[-/](0?[1-9]|[12][0-9]|3[01]) (0?\d|1\d|2[0-3]):([0-5]?\d)(?::([0-5]?\d))? *(?:\.\.) *((?:1[6-9]|2[0-9])\d{2})[-/](0?[1-9]|1[0-2])[-/](0?[1-9]|[12][0-9]|3[01]) (0?\d|1\d|2[0-3]):([0-5]?\d)(?::([0-5]?\d))?$/))) {
				Log(1, '   ' + currentPart + ' is a range of two dates with the format yyyy-MM-dd HH:mm:ss or HH:mm');
				helper = 'rwhole';
				currentPart = c;
			}
			else if (c = currentPart.match(/^\d+([/\-:])\d+.*$/)) { //only numbers , '/-:' are present
				Log(1, '   ' + currentPart + ' contains only numbers , /-: are present');
				helper = currentPart;
				currentPart = DOpus.Create().Date(currentPart);
				if (c[1] != ':') helper = (/[:\.]/.test(helper)) ? ((!currentPart.sec) ? 'sD' : 'whole') : 'd'; //value contains date only or date and time
				else helper = (!currentPart.sec) ? 'tsD' : 't'; //is time only, with or without seconds
			}
			else {
				Log(1, '   ' + currentPart + ' is a constant');
				helper = currentPart.toLowerCase(); //is a constant value
				currentPart = '';
			}
		}
		else if ((category == 'number' || category == 'size') && (operator == '==' || operator == '!=') && /-|\.\./.test(currentPart)) {
			//means a range between 2 values
			currentPart = currentPart.split(/-|\.\./);
		}
		else if (category == 'duration') {
			helper = currentPart;
			if ((operator == '==' || operator == '!=') && /-|\.\./.test(currentPart))
				currentPart = currentPart.split(/-|\.\./); //means a range between 2 values
			if (/^(accessedtime|createdtime|modifiedtime)$/.test(ColObj('name'))) {
				if (typeof currentPart == 'object') {
					for (var i = 0; i < currentPart.length; i++)
						if ((currentPart[i].match(/:/g) || []).length < 3) currentPart[i] += ':00';
				}
				else {
					if ((currentPart.match(/:/g) || []).length < 3) currentPart += ':00';
				}
			}
		}
		else if (category === 'string' && ColObj('name') === 'name' && /:/.test(currentPart)) {
			//means filter by filetype group
			currentPart = currentPart.split(':');
		}
		//Convert input to column type
		if (!is_reference) {
			currentPart = convertType(category, currentPart, helper);
			//If can't convert input to selected category
			if (currentPart === null) {
				if (!from_dialog) Log(3, "   Filter input can't be converted to " + category);
				return null;
			}
		}
	}
	if (Script.config['log level'] === 0) {
		if (DOpus.TypeOf(currentPart) === 'object.Vector') {
			if (DOpus.TypeOf(currentPart(0)) === 'object.FileSize') Log(1, '   lower : ' + currentPart(0).fmt + '   upper : ' + currentPart(1).fmt);
			else if (DOpus.TypeOf(currentPart(0)) === 'object.Date') Log(1, '   lower : ' + currentPart(0).Format('', 'n') + '   upper : ' + currentPart(1).Format('', 'n'));
			else Log(1, '   lower : ' + currentPart(0) + '   upper : ' + currentPart(1));
		}
		else if (DOpus.TypeOf(currentPart) == 'object.FileSize') Log(1, '   query                : ' + currentPart.fmt);
		else if (DOpus.TypeOf(currentPart) == 'object.Date') Log(1, '   query                : ' + currentPart.Format('', 'n'));
		else Log(1, '   query                : ' + currentPart);
		Log(1, '   raw_query            : ' + helper);
		Log(1, '   operator             : ' + operator);
	}
	Log(2, 'PARSING INPUT FINISHED : ' + (new Date() - g_ini_timer) + ' ms');
	return {
		'query': currentPart,
		'raw_query': helper,
		'operator': operator,
		'is_reference': is_reference
	};
}

function compare(value1, value2, operator) {
	if (value1 === undefined) return false;
	if (typeof value1 === 'string') value1 = 0;
	if (value2 === undefined || typeof value2 === 'string') value2 = -1;
	if (DOpus.TypeOf(value2) == 'object.Vector') { //value2 is a range between 2 values (inclusive)
		return (operator == "!=") ? !(value2(0) <= value1 && value1 <= value2(1)) : (value2(0) <= value1 && value1 <= value2(1));
	}
	else switch (operator) {
		case '>=':
			return (value1 >= value2);
		case '<=':
			return (value1 <= value2);
		case '>':
			return (value1 > value2);
		case '<':
			return (value1 < value2);
		case '!=':
			return (value1 != value2);
		default:
			return (value1 == value2);
	}
}

function compareSizes(item, query, operator) {
	if (!item && query === '') return (operator === '==');
	if (DOpus.TypeOf(item) !== 'object.FileSize') return false;
	if (DOpus.TypeOf(query) === 'object.Vector') { //query is a range between 2 values (inclusive)
		return (operator == "!=") ? !(compare(item.Compare(query(0)), 0, ">=") && compare(item.Compare(query(1)), 0, "<=")) : (compare(item.Compare(query(0)), 0, ">=") && compare(item.Compare(query(1)), 0, "<="));
	}
	else {
		return compare(item.Compare(query), 0, operator);
	}
}

function compareGroups(itemObj, values, operator, nodiac) {
	if (DOpus.TypeOf(itemObj) !== 'object.Item') return false;
	var name = itemObj.name;
	if (name === undefined) name = '';
	else if (nodiac) name = str_tools.RemoveDiacritics(name);
	var op = (operator === '==') ? true : false;
	if (itemObj.InGroup('disp:' + values[0]) && (values[1] === '' || values[1].test(name))) return op ? true : false;
	return op ? false : true;
}

function compareStrings(value, regex, operator) {
	if (value === undefined) return false;
	else if (g_flags & (1 << 3)) value = str_tools.RemoveDiacritics(value);
	if (operator == '==') return typeof regex === 'string' ? regex == value : regex.test(value);
	else if (operator == '!=') return typeof regex === 'string' ? regex != value : !regex.test(value);
	else return false;
}

function compareDates(item, query, raw_query, operator) {
	if (!item && query === '') return (operator === '==');
	if (DOpus.TypeOf(item) !== 'object.Date') return false;
	// Log(1, 'teim: ' + item + ';query :' + query + '; raw : ' + raw_query);
	try {
		switch (raw_query) {
			case 'thisyear':
			case 'year':
			case 'lastyear':
			case 'lyear':
				return compare(item.year, query(0).year, operator);
			case 'ryear':
				if (operator == '==')
					return compare(item.year, query(0).year, '>=') && compare(item.year, query(1).year, '<=');
				else
					return compare(item.year, query(0).year, '<') || compare(item.year, query(1).year, '>');
			case 'rwhole':
				if (operator == '==')
					return ((item.compare(query(0), query(0).sec == 0 ? 'sD' : '') >= 0) && (item.compare(query(1), query(1).sec == 0 ? 'sD' : '') <= 0));
				else
					return ((item.compare(query(0), query(0).sec == 0 ? 'sD' : '') < 0) || (item.compare(query(1), query(1).sec == 0 ? 'sD' : '') > 0));
				break;
			case 'rmyear':
			case 'rdate':
			case 'lastmonth':
			case 'lmonth':
			case 'thismonth':
			case 'month':
			case 'week':
			case 'thisweek':
			case 'lastweek':
			case 'lweek':
				switch (operator) {
					case '==':
						return ((item.compare(query(0), 'd') >= 0) && (item.compare(query(1), 'd') <= 0));
					case '!=':
						return ((item.compare(query(0), 'd') < 0) || (item.compare(query(1), 'd') > 0));
					case '>':
					case '<=':
						return compare(item.compare(query(1), 'd'), 0, operator);
					case '>=':
					case '<':
						return compare(item.compare(query(0), 'd'), 0, operator);
				}
				break;
			case 'today':
			case 'yesterday':
			case 'tomorrow':
			case 'ya':
			case 'Ma':
			case 'da':
				return compare(item.compare(query, 'd'), 0, operator); //compare dates value only
			case 'whole':
			case 'ha': //query is a date object with time included
				return compare(item.Compare(query, ''), 0, operator);
			case 'day': //query is a number or range referring a day
				if (DOpus.TypeOf(query) == 'object.Vector') {
					if (operator == '==')
						return compare(item.day, query(0), '>=') && compare(item.day, query(1), '<=');
					else
						return compare(item.day, query(0), '<') || compare(item.day, query(1), '>');
				}
				else return compare(item.day, query, operator);
				break;
			case 'm': //query is a number or range referring a month
				if (DOpus.TypeOf(query) == 'object.Vector') {
					if (operator == '==')
						return compare(item.month, query(0), '>=') && compare(item.month, query(1), '<=');
					else
						return compare(item.month, query(0), '<') || compare(item.month, query(1), '>');
				}
				else return compare(item.month, query, operator);
				break;
			case 'tsD': //query is a number referring the total seconds in a time value
				return compare(item.hour * 3600 + item.min * 60, query, operator);
			case 'sD':
			case 'd':
			case 't':
				return compare(item.compare(query, raw_query), 0, operator);
			default:
				return false;
		}
	}
	catch (err) {
		Log(3, 'Error when comparing dates in ' + item + ' : ' + err.description);
		return false;
	}
	return false;
}

function convertType(category, value, cons, str_only) {
	if (value === undefined) return null;
	// Log(1, '   Converting ' + typeof(value) + ' as ' + category);
	try {
		if (typeof value != 'string' && typeof value != 'object') {
			Log(1, 'Converting value to string...');
			switch (DOpus.TypeOf(value)) {
				case 'object.FileSize':
					value = value.def_value;
					break;
				case 'object.Date':
					value = value.Format('', 'ns');
					break;
				default:
					value = String(value);
					break;
			}
		}
		switch (category) {
			case 'number':
				return convertToNumber(value, /[^\d,.]/g);
				break;
			case 'date':
				if (cons === undefined) cons = 'sD';
				return convertToDate(value, cons);
				break;
			case 'duration':
				return convertToDuration(value, /^(\d+)d (\d+):(\d+):(\d+)$|^(\d+):(\d+):(\d+)$|^(\d+):(\d+)$|^(\d+)$/);
			case 'size':
				return convertToSize(value, /^([\d,.]+) *([bytes|kb|mb|gb|tb|pb]*)$/i);
				break;
			case 'string':
				if (str_only) return value;
				if (DOpus.TypeOf(value) === 'object') {
					if (value.length != 2) {
						Log(3, 'Wrong format for input');
						return null;
					}
					if (value[0] === '') {
						Log(3, 'No valid filetype group for comparison');
						return null;
					}
					var vec = DOpus.Create().Vector();
					var r = '';
					for (var i = 0; i < value[0].length; i++)
						r += value[0].charAt(i) + '.*';
					r = new RegExp(r.slice(0, -2), 'i');
					var ft = DOpus.filetypegroups;
					for (var i = 0; i < ft.count; i++) {
						if (r.test(ft(i).display_name)) {
							vec.push_back(ft(i).display_name);
							break;
						}
					}
					ft = null;
					if (vec.empty) {
						Log(3, 'No valid filetype group for comparison');
						return null;
					}
					vec.push_back(BuildStrPattern(value[1]));
					return vec(1) == null ? null : vec;
				}
				else return BuildStrPattern(value);
				break;
			default:
				return null;
		}
	}
	catch (err) {
		Log(3, 'Error while trying to convert "' + value + '" as "' + category + '" : ' + err);
		return null;
	}
}

function BuildStrPattern(value) {
	//Use Textual Filters is off and is not a Script/Eval/Shell column or preferred method is Rename Presets 
	if (!g_search_mode && !g_use_wilcards && (!g_custom_col || Script.config.preferred_filter_mode == 1)) { //Rename Preset is used or is not a script column
		try {
			if (value === '') return value;
			else {
				Log(1, '   Aplying flags to string "' + value + '"');
				if (g_flags & (1 << 3)) value = str_tools.RemoveDiacritics(value);
				if (g_flags & (1 << 1)) var regex = new RegExp('\\b' + ((g_flags & (1 << 2)) ? value : value.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')) + '\\b', (g_flags & (1 << 0)) ? '' : 'i');
				else var regex = new RegExp((g_flags & (1 << 2)) ? value : value.replace(/[.*+?^${}()|[\]\\]/g, '\\$&'), (g_flags & (1 << 0)) ? '' : 'i');
			}
			return regex;
		}
		catch (err) {
			Log(3, '   Error while parsing the string ' + value);
			return null;
		}
	}
	return value;
}

function convertToDate(value, cons) {
	try {
		switch (cons) {
			case 'thisyear':
			case 'year':
			case 'lastyear':
			case 'lyear':
				var ld = DOpus.Create().Date();
				var ud = DOpus.Create().Date();
				ld.month = 1;
				ld.day = 1;
				ud.month = 12;
				ud.day = 31;
				if (cons.charAt(0) == 'l') {
					ld.Sub(1, 'y');
					ud.Sub(1, 'y');
				}
				else if (value !== '')
					ld.year = ud.year = parseInt(value, 10);
				return DOpus.Create().Vector(ld, ud);
			case 'ryear':
				var ld = DOpus.Create().Date('1-1-2001');
				var ud = DOpus.Create().Date('31-12-2001');
				ld.year = parseInt(value[1], 10);
				ud.year = parseInt(value[2], 10);
				if (ld.Compare(ud, 'd') != -1) return null; //not valid range
				return DOpus.Create().Vector(ld, ud);
			case 'rmyear':
				var ld = DOpus.Create().Date('1-1-2001');
				var ud = DOpus.Create().Date('1-1-2001');
				ld.month = parseInt(value[1], 10);
				ld.year = parseInt(value[2], 10);
				ud.month = parseInt(value[3], 10);
				ud.year = parseInt(value[4], 10);
				ud.Add(1, 'M');
				ud.Sub(1, 'd');
				if (ld.Compare(ud) != -1) return null; //not valid range
				return DOpus.Create().Vector(ld, ud);
			case 'rdate':
				var ld = DOpus.Create().Date('1-1-2001');
				var ud = DOpus.Create().Date('1-1-2001');
				ld.month = parseInt(value[2], 10);
				ud.month = parseInt(value[5], 10);
				if (value[1].length == 4) {
					ld.year = parseInt(value[1], 10);
					ld.day = parseInt(value[3], 10);
					ud.year = parseInt(value[4], 10);
					ud.day = parseInt(value[6], 10);
				}
				else {
					ld.year = parseInt(value[3], 10);
					ld.day = parseInt(value[1], 10);
					ud.year = parseInt(value[6], 10);
					ud.day = parseInt(value[4], 10);
				}

				if (ld.Compare(ud) != -1) return null; //not valid range
				return DOpus.Create().Vector(ld, ud);
			case 'rwhole':
				var ld = DOpus.Create().Date('1-1-2001');
				var ud = DOpus.Create().Date('1-1-2001');
				ld.month = parseInt(value[2], 10);
				ud.month = parseInt(value[8], 10);
				if (value[1].length == 4) {
					ld.year = parseInt(value[1], 10);
					ld.day = parseInt(value[3], 10);
					ud.year = parseInt(value[7], 10);
					ud.day = parseInt(value[9], 10);
				}
				else {
					ld.year = parseInt(value[3], 10);
					ld.day = parseInt(value[1], 10);
					ud.year = parseInt(value[9], 10);
					ud.day = parseInt(value[7], 10);
				}
				ld.hour = parseInt(value[4], 10);
				ld.min = parseInt(value[5], 10);
				ld.sec = value[6] == '' ? 0 : parseInt(value[6], 10);
				ud.hour = parseInt(value[10], 10);
				ud.min = parseInt(value[11], 10);
				ud.sec = value[12] == '' ? 0 : parseInt(value[12], 10);
				if (ld.Compare(ud) != -1) return null; //not valid range
				return DOpus.Create().Vector(ld, ud);
			case 'thismonth':
			case 'month':
			case 'lastmonth':
			case 'lmonth':
				var ld = DOpus.Create().Date();
				var ud = DOpus.Create().Date();
				ld.day = 1;
				ud.day = 1;
				if (cons.charAt(0) == 'l') {
					ld.Sub(1, 'M');
					ud.Sub(1, 'd');
				}
				else {
					if (typeof value === 'object') {
						ld.month = ud.month = parseInt(value[1], 10);
						ld.year = ud.year = parseInt(value[2], 10);
					}
					else if (value) ld.month = ud.month = parseInt(value, 10);
					ud.Add(1, 'M');
					ud.Sub(1, 'd');
				}
				return DOpus.Create().Vector(ld, ud);
			case 'thisweek':
			case 'week':
			case 'lastweek':
			case 'lweek':
				var ld = DOpus.Create().Date();
				if (cons.charAt(0) == 'l') ld.Sub(1, 'w');
				ld.Sub((ld.wday === 0) ? 6 : ld.wday - 1, 'd');
				var ud = ld.Clone();
				ud.Add(6,'d');
				return DOpus.Create().Vector(ld, ud);
			case 'today':
				return DOpus.Create().Date();
			case 'yesterday':
				var cd = DOpus.Create().Date();
				cd.Sub(1, 'd');
				return cd;
			case 'tomorrow':
				var cd = DOpus.Create().Date();
				cd.Add(1, 'd');
				return cd;
			case 'ya':
			case 'Ma':
			case 'da':
			case 'ha':
				var cd = DOpus.Create().Date();
				cd.Sub(parseInt(value, 10), cons.charAt(0));
				return cd;
			case 'day':
			case 'm': //returns month|day number as int
				if (typeof value == 'object') { //is an array
					value[1] = parseInt(value[1], 10);
					value[2] = parseInt(value[2], 10);
					if (value[1] >= value[2]) return null; //not valid range
					return DOpus.Create().Vector(value[1], value[2]);
				}
				return parseInt(value, 10);
			case 'tsD':
				//value is a Date object with time only
				return value.hour * 3600 + value.min * 60; //returns total seconds
			case 'whole':
			case 'd':
			case 't':
			case 'sD':
				if (DOpus.TypeOf(value) !== 'object.Date') {
					if (/^\d+([/\-:])\d+.*$/.test(value)) return DOpus.Create().Date(value);
					return null;
				}
				return value; //returns a Date object
			default:
				return null;
		}
	}
	catch (err) {
		Log(3, 'Error converting to date as ' + cons + ':' + err);
		return null;
	}
}

function convertToNumber(value, regex) {
	if (typeof value === 'object') {
		if (value.length > 2) return null; //no  more than 2 values in the range
		var a = DOpus.Create().Vector();
		for (var i = 0; i < 2; i++) {
			Log(1, value[i]);
			v = convertToNumber(value[i], regex);
			if (v === null) return null;
			a.push_back(v);
		}
		if (a(0) >= a(1)) return null; //not valid range
		return a;
	}
	else if (typeof value === 'string') {
		//value = value.replace(regex, '');
		value = value.replace(/,/g, '.').replace(/(?:.*?)(\d+)(?:.*?)\.(.*)/, function(_, p1, p2) {
			return p1 + '.' + p2.replace(/[^\d]/g, '');
		});
		value = parseFloat(value);
		return !isNaN(value) ? value : null;
	}
	else if (!isNaN(value)) return value;
	return null;
}
//convert duration to number (as number of seconds)
function convertToDuration(value, regex) {
	///^(\d+)d (\d+):(\d+):(\d+)$|^(\d+):(\d+):(\d+)$|^(\d+):(\d+)$|^(\d+)$/
	if (typeof value == 'object') { //is a range
		if (value.length > 2) return null; //no  more than 2 values in the range
		var a = DOpus.Create().Vector();
		for (var i = 0; i < 2; i++) {
			v = convertToDuration(value[i], regex);
			if (v == null) return null;
			a.push_back(v);
		}
		if (a(0) >= a(1)) return null; //not valid range
		return a;
	}
	else {
		var match = value.match(regex);
		if (!match) return null;
		var day = parseInt(match[1] || 0, 10);
		var hr = parseInt(match[2] || match[5] || 0, 10);
		var min = parseInt(match[3] || match[6] || match[8] || 0, 10);
		var sec = parseInt(match[4] || match[7] || match[9] || match[10] || 0, 10);
		var totalsec = day * 86400 + hr * 3600 + min * 60 + sec;
		return totalsec;
	}
}

function convertToSize(value, regex) {
	if (typeof value === 'object') {
		if (value.length > 2) return null; //no  more than 2 values in the range
		var a = DOpus.Create().Vector();
		for (var i = 0; i < 2; i++) {
			v = convertToSize(value[i], regex);
			if (v == null) return null;
			a.push_back(v);
		}
		if (a(0).Compare(a(1)) != -1) return null; //not valid range
		return a;
	}
	else {
		var match = value.match(regex);
		if (match) {
			var num = parseFloat(match[1].replace(',', '.'));
			var unit = match[2].toLowerCase();
			switch (unit) {
				case 'kb':
					unit = 1024;
					break;
				case 'mb':
					unit = 1024 * 1024;
					break;
				case 'gb':
					unit = 1024 * 1024 * 1024;
					break;
				case 'tb':
					unit = 1024 * 1024 * 1024 * 1024;
					break;
				case 'pb':
					unit = 1024 * 1024 * 1024 * 1024 * 1024;
					break;
				case 'bytes':
				case '':
					unit = 1;
					break;
				default:
					return null;
			};
			return num ? FSU.NewFileSize(num * unit) : null;
		}
	}
	return null;
}

function CreateRenamePresetTxt(preset, to, single) {
	Log(2, 'Creating rename preset "' + preset + '"');
	Log(1, 'Rename preset TO        : ' + to);
	to = to.replace(/&/g, '&amp;');
	to = to.replace(/"/g, '&quot;');
	to = to.replace(/</g, '&lt;');
	to = to.replace(/>/g, '&gt;');
	to = to.replace(/'/g, '&apos;');
	var content = '<?xml version="1.0" encoding="UTF-8"?>\r\n';
	content += '<rename_preset applynomatch="yes" case="none" script="yes" type="normal" version="13">\r\n<from></from>\r\n<to>';
	content += to + '</to>\r\n<script>@script JScript\r\n';
	if (single)
		content += 'function OnGetNewName(g){\r\nvar tmp = g.newname;\r\nif (!tmp) {DOpus.vars.Delete(&quot;fbc&quot;);return true;}\r\nDOpus.vars.Set(&quot;fbc&quot;, tmp);return true;}';
	else {
		content += 'var map = DOpus.Create().OrderedMap();\r\n';
		content += 'function OnGetNewName(g){map.Set(g.item+&quot;&quot;,g.newname);return true;}\r\n';
		content += 'DOpus.Vars.Set(&quot;' + preset + '&quot;, map);\r\n'
	}
	content += '</script>\r\n</rename_preset>';
	preset = DOpus.Aliases('dopusdata').path + '\\Rename Presets\\' + preset + '.orp';
	var file = FSU.OpenFile(preset, 'w');
	if (file.error != 0) return false;
	try {
		var r = file.Write(content);
		file.Close();
	}
	catch (e) {
		Log(3, 'Unable to write ' + preset + ': ' + e.description);
		r = 0;
	}
	file = null;
	if (r != 0) Log(2, 'Rename preset succesfully created: ' + r + ' bytes');
	return r;
}

function configFlags(parent, flags) {
	var dlg = parent.dlg();
	dlg.template = 'configflags';
	dlg.disable_window = parent;
	dlg.detach = true;
	dlg.Create();
	dlg.top = true;
	dlg.title = script_label + ' v' + script_version + ' - Flags configuration';
	for (var i = 0; i < 8; i++) {
		dlg.Control('flag' + i).label = DOpus.strings.Get('flag' + i);
		dlg.Control('flag' + i).value = flags & (1 << i);
	}
	dlg.Show();
	while (true) {
		msg = dlg.GetMsg();
		if (!msg.result) break;
		if (msg.event == 'click' && msg.control == 'ok_btn') dlg.EndDlg(1);
	}
	if (dlg.result == 1) {
		flags = 0;
		for (var i = 0; i < 8; i++) {
			if (dlg.Control('flag' + i).value) flags += (1 << i);
		}
		Script.UpdateFAYTFlags(script_name, flags);
		Log(2, 'Flags saved in configuration!!');
	}
	dlg = null;
}

function getColumnData(value) {
	Log(2, 'Getting data for column "' + value + '"...');
	switch (value) {
		case 'accessed':
		case 'accesseddate':
			return DOpus.Create().Map('builtin', true, 'multi', false, 'metatype', false, 'property', 'access', 'category', 'date', 'filetype', null);
		case 'created':
		case 'createddate':
			return DOpus.Create().Map('builtin', true, 'multi', false, 'metatype', false, 'property', 'create', 'category', 'date', 'filetype', null);
		case 'modified':
		case 'modifieddate':
			return DOpus.Create().Map('builtin', true, 'multi', false, 'metatype', false, 'property', 'modify', 'category', 'date', 'filetype', null);
		case 'attr':
			return DOpus.Create().Map('builtin', true, 'multi', true, 'metatype', false, 'property', 'attr_text', 'category', 'string', 'filetype', null);
		case 'ext':
		case 'extdir':
			return DOpus.Create().Map('builtin', true, 'multi', false, 'metatype', false, 'property', 'ext', 'category', 'string', 'filetype', null);
		case 'size':
		case 'sizeauto':
		case 'sizekb':
			return DOpus.Create().Map('builtin', true, 'multi', false, 'metatype', false, 'property', 'size', 'category', 'size', 'filetype', null);
			//SPECIAL CASES
		case 'accessedtime':
			return DOpus.Create().Map('builtin', true, 'multi', false, 'metatype', false, 'property', 'access', 'category', 'duration', 'filetype', null);
		case 'createdtime':
			return DOpus.Create().Map('builtin', true, 'multi', false, 'metatype', false, 'property', 'create', 'category', 'duration', 'filetype', null);
		case 'modifiedtime':
			return DOpus.Create().Map('builtin', true, 'multi', false, 'metatype', false, 'property', 'modify', 'category', 'duration', 'filetype', null);
		case 'fullpath':
			return DOpus.Create().Map('builtin', true, 'multi', false, 'metatype', false, 'property', 'realpath', 'category', 'string', 'filetype', null);
		case 'owner':
		case 'type':
		case 'perms':
		case 'availability':
			return DOpus.Create().Map('builtin', false, 'multi', false, 'metatype', false, 'property', '', 'category', 'string', 'filetype', null);
		case 'keywords':
		case 'label':
			return DOpus.Create().Map('builtin', true, 'multi', true, 'metatype', false, 'property', value, 'category', 'string', 'filetype', null);
		case 'name':
		case 'parent':
		case 'parentlocation':
		case 'parentpath':
		case 'path':
		case 'pathrel':
		case 'desc':
			return DOpus.Create().Map('builtin', true, 'multi', false, 'metatype', false, 'property', value, 'category', 'string', 'filetype', null);
		case 'userdesc':
			return DOpus.Create().Map('builtin', true, 'multi', false, 'metatype', 'other', 'property', 'usercomment', 'category', 'string', 'filetype', null);
		case 'pathlen':
		case 'streamcount':
			return DOpus.Create().Map('builtin', true, 'multi', false, 'metatype', false, 'property', '', 'category', 'number', 'filetype', null);
		case 'copyright':
			return DOpus.Create().Map('builtin', true, 'multi', false, 'metatype', true, 'property', value, 'category', 'string', 'filetype', /(doc|exe|image|audio)/);
		case 'producer':
			return DOpus.Create().Map('builtin', true, 'multi', true, 'metatype', 'doc', 'property', value, 'category', 'string', 'filetype', /doc/);
		case 'picdepth':
			return DOpus.Create().Map('builtin', true, 'multi', false, 'metatype', true, 'property', value, 'category', 'number', 'filetype', /(video|image|audio)/);
			//AUDIO
		case 'mp3bpm':
		case 'mp3disc':
		case 'mp3disk':
		case 'mp3track':
			return DOpus.Create().Map('builtin', true, 'multi', false, 'metatype', 'audio', 'property', value, 'category', 'number', 'filetype', /audio/);
		case 'compilation':
		case 'mp3album':
		case 'mp3albumartist':
		case 'mp3comment':
		case 'mp3encodingsoftware':
		case 'mp3info':
		case 'mp3drm':
			return DOpus.Create().Map('builtin', true, 'multi', false, 'metatype', 'audio', 'property', value, 'category', 'string', 'filetype', /audio/);
			//DOCUMENT
		case 'doccreateddate':
		case 'docedittime':
		case 'doclastsaveddate':
			return DOpus.Create().Map('builtin', true, 'multi', false, 'metatype', 'doc', 'property', value, 'category', 'date', 'filetype', /doc/);
		case 'pages':
			return DOpus.Create().Map('builtin', true, 'multi', false, 'metatype', 'doc', 'property', value, 'category', 'number', 'filetype', /doc/);
		case 'category':
			return DOpus.Create().Map('builtin', true, 'multi', true, 'metatype', 'doc', 'property', value, 'category', 'string', 'filetype', /doc/);
		case 'comments':
		case 'creator':
		case 'doclastsavedby':
			return DOpus.Create().Map('builtin', true, 'multi', false, 'metatype', 'doc', 'property', value, 'category', 'string', 'filetype', /doc/);
			//PROGRAM
		case 'modversion':
		case 'prodversion':
			return DOpus.Create().Map('builtin', true, 'multi', false, 'metatype', 'exe', 'property', value, 'category', 'string', 'filetype', /exe/);
		case 'moddesc':
		case 'prodname':
			return DOpus.Create().Map('builtin', true, 'multi', false, 'metatype', 'exe', 'property', value, 'category', 'string', 'filetype', /exe/);
		case 'signer':
			return DOpus.Create().Map('builtin', false, 'multi', false, 'metatype', 'exe', 'property', '', 'category', 'string', 'filetype', /exe/);
		case 'fontname':
			return DOpus.Create().Map('builtin', true, 'multi', false, 'metatype', 'font', 'property', value, 'category', 'string', 'filetype', '');
			//IMAGE
		case 'datedigitized':
		case 'datetaken':
		case 'datetimeoriginal':
		case 'datetimecreated':
			return DOpus.Create().Map('builtin', true, 'multi', false, 'metatype', 'image', 'property', value, 'category', 'date', 'filetype', /image/);
		case '35mmfocallength':
		case 'altitude':
		case 'apertureval':
		case 'digitalzoom':
		case 'exposurebias':
		case 'exposuretime':
		case 'fnumber':
		case 'focallength':
		case 'latitude':
		case 'longitude':
		case 'picphysx':
		case 'picphysy':
		case 'picresx':
		case 'picresy':
		case 'rotation':
		case 'shutterspeed':
		case 'subjectdistance':
			return DOpus.Create().Map('builtin', true, 'multi', false, 'metatype', 'image', 'property', value, 'category', 'number', 'filetype', /image/);
		case 'cameramake':
		case 'cameramodel':
		case 'colormodel':
		case 'colorspace':
		case 'contrast':
		case 'coords':
		case 'exposureprogram':
		case 'flash':
		case 'imagedesc':
		case 'imagequality':
		case 'instructions':
		case 'isospeed':
		case 'lensmake':
		case 'lensmodel':
		case 'macromode':
		case 'meteringmode':
		case 'picres':
		case 'saturation':
		case 'scenecapturetype':
		case 'scenemode':
		case 'sharpness':
		case 'software':
		case 'whitebalance':
			return DOpus.Create().Map('builtin', true, 'multi', false, 'metatype', 'image', 'property', value, 'category', 'string', 'filetype', /image/);
			//VIDEO
		case 'broadcastdate':
			return DOpus.Create().Map('builtin', true, 'multi', false, 'metatype', 'video', 'property', value, 'category', 'date', 'filetype', /video/);
		case 'recordingtime':
			return DOpus.Create().Map('builtin', true, 'multi', false, 'metatype', 'video', 'property', value, 'category', 'duration', 'filetype', /video/);
		case 'audiocount':
		case 'channel':
		case 'datarate':
		case 'framerate':
		case 'subtitlecount':
		case 'videocount':
			return DOpus.Create().Map('builtin', true, 'multi', false, 'metatype', 'video', 'property', value, 'category', 'number', 'filetype', /video/);
		case 'director':
			return DOpus.Create().Map('builtin', true, 'multi', true, 'metatype', 'video', 'property', value, 'category', 'string', 'filetype', /video/);
		case 'alllangs':
		case 'audiolangs':
		case 'credits':
		case 'episodename':
		case 'fourcc':
		case 'hdrtypes':
		case 'ishd':
		case 'isrepeat':
		case 'publisher':
		case 'station':
		case 'subtitlelangs':
		case 'videocodec':
		case 'videolangs':
		case 'encodedby':
			return DOpus.Create().Map('builtin', true, 'multi', false, 'metatype', 'audio', 'property', value, 'category', 'string', 'filetype', /video|audio/);
			//MULTI	
		case 'rating':
			return DOpus.Create().Map('builtin', true, 'multi', false, 'metatype', 'other', 'property', value, 'category', 'number', 'filetype', '');
		case 'target':
			return DOpus.Create().Map('builtin', true, 'multi', false, 'metatype', 'other', 'property', value, 'category', 'string', 'filetype', '');
		case 'duration':
			return DOpus.Create().Map('builtin', true, 'multi', false, 'metatype', 'audio', 'property', value, 'category', 'duration', 'filetype', /(video|audio)/);
		case 'releasedate':
			return DOpus.Create().Map('builtin', true, 'multi', false, 'metatype', 'audio', 'property', value, 'category', 'date', 'filetype', /(video|audio)/);
		case 'mp3bitrate':
		case 'mp3samplerate':
		case 'mp3year':
			return DOpus.Create().Map('builtin', true, 'multi', false, 'metatype', 'audio', 'property', value, 'category', 'number', 'filetype', /(video|audio)/);
		case 'mp3type':
		case 'audiocodec':
		case 'composers':
		case 'conductors':
		case 'mp3mode':
		case 'mp3title':
		case 'initialkey':
			return DOpus.Create().Map('builtin', true, 'multi', false, 'metatype', 'audio', 'property', value, 'category', 'string', 'filetype', /(video|audio)/);
		case 'mp3genre':
			return DOpus.Create().Map('builtin', true, 'multi', true, 'metatype', 'audio', 'property', value, 'category', 'string', 'filetype', /(video|audio)/);
		case 'companyname':
			return DOpus.Create().Map('builtin', true, 'multi', false, 'metatype', 'doc', 'property', value, 'category', 'string', 'filetype', /(doc|exe)/);
		case 'subject':
			return DOpus.Create().Map('builtin', true, 'multi', false, 'metatype', true, 'property', value, 'category', 'string', 'filetype', /(image|doc)/);
		case 'picsize':
		case 'aspectratiogroup':
		case 'picphyssize':
			return DOpus.Create().Map('builtin', true, 'multi', false, 'metatype', 'image_text', 'property', value, 'category', 'string', 'filetype', /(video|image)/);
		case 'aspectratio':
		case 'picheight':
		case 'picwidth':
			return DOpus.Create().Map('builtin', true, 'multi', false, 'metatype', 'image', 'property', value, 'category', 'number', 'filetype', /(video|image)/);
		case 'mp3artists':
			return DOpus.Create().Map('builtin', true, 'multi', true, 'metatype', true, 'property', value, 'category', 'string', 'filetype', /(audio|image)/);
		case 'author':
			return DOpus.Create().Map('builtin', true, 'multi', true, 'metatype', true, 'property', value, 'category', 'string', 'filetype', /(doc|image)/);
		case 'title':
			return DOpus.Create().Map('builtin', true, 'multi', false, 'metatype', true, 'property', value, 'category', 'string', 'filetype', /(doc|image)/);
		default:
			Log(3, value + ' is not a recognized column for this command');
			return null;
	}
}

function saveQuery(query, desc) {
	Log(2, 'Saving query            : ' + query);
	var values = DOpus.Create().Vector(query.trim() + '\t' + desc);
	if (Script.Vars.Exists('saved_queries')) values.append(Script.Vars.Get('saved_queries'));
	values.unique();
	var max = Script.config.max_history_size;
	if (max <= 0) max = 20;
	else if (max > 100) max = 100;
	if (values.count > max) values.resize(max);
	Script.Vars.Set('saved_queries', values);
	Script.Vars('saved_queries').persist = true;
	return;
}

function getCategoryforEvalCol(col_name) {
	Log(2, '   Trying to get category for "' + col_name + '" column from evalcols.oxc...');
	//text,double,signed,notsigned,size,date,time,datetime,rating,graph,percent
	var results = ['string', 'number', 'number', 'number', 'size', 'date', 'date', 'date', false, false, false];
	var type = 0;
	try {
		var ev_col_file = DOpus.Aliases('dopusdata').path + '\\ConfigFiles\\evalcols.oxc';
		Log(1, '   Evaluator : Parsing Evaluator columns xml file:' + ev_col_file);
		if (!FSU.Exists(ev_col_file)) return results;
		var xmlDocEv = new ActiveXObject('Msxml2.DOMDocument');
		xmlDocEv.load(ev_col_file);
		if (xmlDocEv.parseError.errorCode == 0) {
			var evalcolumns = xmlDocEv.selectNodes('//prefs/evalcolumns/column');
			for (var i = 0; i < evalcolumns.length; i++) {
				// Get keyword
				var keyword = evalcolumns[i].getAttribute('keyword');
				if (col_name == keyword) {
					type = evalcolumns[i].getAttribute('type');
					break;
				}
			}
		}
		else
			Log(3, '   Evaluator : Error parsing Evaluator xml file');
		xmlDocEv = null;
	}
	catch (err) {
		Log(3, '   Evaluator : Error while trying to get Evaluator columns info : ' + err);
	};
	return results[type];
}

function GetScriptColumns(saved_categories, cols_map) {
	var results = DOpus.Create().Map();
	try {
		var scr_cols_file = DOpus.Aliases('dopusdata').path + '\\ConfigFiles\\scriptcolumns.oxc';
		var scr_addins_file = DOpus.Aliases('dopusdata').path + '\\ConfigFiles\\scriptaddins.oxc';
		var scr_prefs_file = DOpus.Aliases('dopuslocaldata').path + '\\State Data\\scriptprefs.osd';
		Log(2, 'Reading Script Columns  : Parsing XML files...');
		if (!FSU.Exists(scr_addins_file) || !FSU.Exists(scr_cols_file) || !FSU.Exists(scr_prefs_file)) return results;
		var xmlDocAddins = new ActiveXObject("Msxml2.DOMDocument");
		var xmlDocCols = new ActiveXObject("Msxml2.DOMDocument");
		var xmlDocPrefs = new ActiveXObject("Msxml2.DOMDocument");
		xmlDocAddins.load(scr_addins_file);
		xmlDocCols.load(scr_cols_file);
		xmlDocPrefs.load(scr_prefs_file);

		if (xmlDocAddins.parseError.errorCode == 0 && xmlDocCols.parseError.errorCode == 0 && xmlDocPrefs.parseError.errorCode == 0) {
			var filter = DOpus.Create().Filter();
			var filters_map = {
				'datetime': ' on 2021-01-01 00:00:00',
				'date': ' on 2021-01-01',
				'time': ' on 00:00:00',
				'string': ' "test"',
				'number': ' = 0',
				'double': ' = 0.0',
				'duration': ' = 0',
				'size': ' = 0'
			};
			var labels, main, id, name, keyword, colNames, type;
			var disabledScripts = xmlDocAddins.selectSingleNode("//scriptaddins/disabled");
			var colScripts = xmlDocPrefs.selectNodes("//scriptprefs/cache/script");
			for (var i = 0; i < colScripts.length; i++) {
				labels = colScripts[i].getAttribute("columns");
				if (!labels) continue;
				main = colScripts[i].getAttribute("name");
				id = colScripts[i].getAttribute("id");
				if (!disabledScripts.selectSingleNode("script[@id='" + id + "']")) {
					// Get path value in addins.xml
					pathNode = xmlDocAddins.selectSingleNode("//scriptaddins/idmap/script[@id='" + id + "']");
					if (!pathNode) {
						//Log(1, 'Reading Script Columns  : "' + name + '" is not listed in scriptaddins.oxc');
						continue;
					}
					colNames = xmlDocCols.selectNodes("//scriptcolumns/col[@script='" + id.slice(1).slice(0, -1) + "']");
					for (var j = 0; j < colNames.length; j++) {
						name = colNames[j].getAttribute("name");
						if (main && name) {
							name = main + "/" + name;
							name = name.replace(/ /g, '');
							for (var key in filters_map) {
								if (filter.Set('script match ' + name + filters_map[key])) {
									keyword = 'scp:' + name.toLowerCase();
									if (!cols_map.Exists(keyword)) {
										name += ' (Script)';
										results(name) = keyword;
										results(keyword) = null;
									}
									if (key == 'double') type = 'number';
									else if (key == 'datetime' || key == 'time') type = 'date';
									else type = key;
									if (!custom_categories_map.Exists(keyword)) {
										Log(1, 'Reading Script Columns  : Found "' + keyword + '" column\ttype : ' + type);
										if (!saved_categories.Exists(keyword)) saved_categories.Set(keyword, type);
										custom_categories_map.Set(keyword, type);
									}
									else Log(1, 'Reading Script Columns  : Found "' + keyword + '" column\ttype : ' + custom_categories_map(keyword));
									break;
								}
							}
						}
					}
				}
			}
		}
		else
			Log(3, 'Reading Script Columns  : Error while parsing script columns xml files');
		xmlDocAddins = null;
		xmlDocCols = null;
		xmlPrefsCols = null;
		filter = null;
		filters_map = null;
	}
	catch (err) {
		Log(3, 'Reading Script Columns  : Error while trying to get script columns info : ' + err.description);
	};
	return results;
}

function GetShellColumns(saved_categories, cols_map) {
	var results = DOpus.Create().Map();
	try {
		var shell_cols_file = DOpus.Aliases('dopuslocaldata').path + '\\State Data\\shellcolumns.osd';
		var shell_props_file = DOpus.Aliases('dopusdata').path + '\\ConfigFiles\\shellprops.oxc';
		if (!FSU.Exists(shell_cols_file) || !FSU.Exists(shell_props_file)) return results;
		Log(2, 'Reading Shell Columns   : Parsing XML files...');
		var xmlDocShell = new ActiveXObject("Msxml2.DOMDocument");
		var xmlPropsShell = new ActiveXObject("Msxml2.DOMDocument");
		xmlDocShell.load(shell_cols_file);
		xmlPropsShell.load(shell_props_file);

		// Verify if the files have been loaded correctly
		if (xmlDocShell.parseError.errorCode == 0 || xmlPropsShell.parseError.errorCode == 0) {
			//Get all shellprops
			var title, keyword, shells, type;
			var shellprops = xmlPropsShell.selectNodes("//prefs/shellprops/prop");
			for (var i = 0; i < shellprops.length; i++) {
				try {
					var pkey = shellprops[i].getAttribute("pkey");
					pkey = pkey.split(',');
					shellcolumn = xmlDocShell.selectSingleNode("//shellcolumns/column[shell/scid/@fmtid='" + pkey[0] + "' and shell/scid/@pid='" + pkey[1] + "']");
					if (!shellcolumn) continue;
					var keyword = shellcolumn.getAttribute("key");
					var title = shellcolumn.selectSingleNode("shell/title").text;
					if (title && keyword) {
						keyword = 'sh:' + keyword.toLowerCase();
						if (!cols_map.Exists(keyword)) {
							results(title + ' (Shell)') = keyword;
							results(keyword) = null;
						}
						if (!custom_categories_map.Exists(keyword)) {
							shells = FSU.GetShellPropertyList(title);
							if (!shells.empty) {
								type = shells(0).type;
								if (type === 'datetime') type = 'date';
								Log(1, 'Reading Shell Columns   : Found "' + keyword + '" column\ttype : ' + type);
								if (!saved_categories.Exists(keyword)) saved_categories.Set(keyword, type);
								custom_categories_map.Set(keyword, type);
							}
						}
						else Log(1, 'Reading Shell Columns   : Found "' + keyword + '" column\ttype : ' + custom_categories_map(keyword));
					}
				}
				catch (err) {
					Log(3, 'Reading Shell Columns   : Error retrieving column : ' + err.description);
					continue;
				}
			}
			shellprops = null;
			shellcolumn = null;
			shells = null;
		}
		else
			Log(3, 'Reading Shell Columns   : Error while parsing xml files');
		xmlDocShell = null;
		xmlPropsShell = null;
	}
	catch (err) {
		Log(3, 'Reading Shell Columns   : Error while trying to get shell columns info : ' + err.description);
	};
	return results;
}

function GetEvaluatorColumns(saved_categories, cols_map) {
	var results = DOpus.Create().Map();
	try {
		var ev_col_file = DOpus.Aliases('dopusdata').path + '\\ConfigFiles\\evalcols.oxc';
		if (!FSU.Exists(ev_col_file)) return results;
		Log(2, 'Reading Eval Columns    : Parsing XML file...');
		var xmlDocEv = new ActiveXObject("Msxml2.DOMDocument");
		xmlDocEv.load(ev_col_file);
		if (xmlDocEv.parseError.errorCode == 0) {
			var evalcolumns = xmlDocEv.selectNodes("//prefs/evalcolumns/column");
			var keyword, category, header, title;
			var types_arr = ['string', 'number', 'number', 'number', 'size', 'date', 'date', 'date', null, null, null];
			var forbidden_values = /(ftp:(xfertime|group)|sh:(freespace|filesys|usedpercent|freepercent|netlocation|usedspace)|zip:(compsize|compratio|compmethod|compcrc)|opus7zip:(packed|ratio|crc|block|index|solid|method|mode|link|user|group))/;
			for (var i = 0; i < evalcolumns.length; i++) {
				// Get keyword, category, header and title
				type = evalcolumns[i].getAttribute('type');
				if (type && types_arr[type] !== null) {
					keyword = evalcolumns[i].getAttribute("keyword");
					title = evalcolumns[i].getAttribute("title");
					header = evalcolumns[i].getAttribute("header");
					if (title && keyword) {
						if (forbidden_values.test(evalcolumns[i].text.toLowerCase())) {
							Log(1, 'Reading Eval Columns    : ' + keyword + ' contains forbidden references');
							continue;
						}
						keyword = 'eval:' + keyword.toLowerCase();
						if (!cols_map.Exists(keyword)) {
							header = (header ? header : title) + ' (Evaluator)';
							results(header) = keyword;
							results(keyword) = null;
						}
						if (!custom_categories_map.Exists(keyword)) {
							Log(1, 'Reading Eval Columns    : Found "' + keyword + '" column\ttype : ' + types_arr[type]);
							if (!saved_categories.Exists(keyword)) saved_categories.Set(keyword, types_arr[type]);
							custom_categories_map.Set(keyword, types_arr[type]);
						}
						else Log(1, 'Reading Eval Columns    : Found "' + keyword + '" column\ttype : ' + custom_categories_map(keyword));
					}
					else
						Log(1, 'Reading Eval Columns    : "' + keyword + '" columns does not seems valid');
				}
			}
		}
		else
			Log(3, 'Reading Eval Columns    : Error parsing Evaluator xml file');
		xmlDocEv = null;
	}
	catch (err) {
		Log(3, 'Reading Eval Columns    : Error while trying to get Evaluator columns info : ' + err);
	};
	return results;
}

function getCustomMap(name, verify_names, return_map) {
	var map = DOpus.Create().Map();
	var keys = '';
	var match;
	var regex = (verify_names === true) ? /(>=|=>|<=|=<|!=|=|<|>)/ : /^(string|date|number|duration|size)$/i;
	var m = /^ *"(.+)":"(.+)" *$/;
	var vector = Script.config[name];
	if (!verify_names && Script.Vars.Exists('saved_categories')) map.assign(Script.Vars.Get('saved_categories'));
	for (var i = 0; i < vector.length; i++) {
		try {
			if (vector(i) !== '') {
				var match = vector(i).match(m);
				if (match) {
					match[1] = match[1].trim();
					match[2] = match[2].trim();
					if (verify_names === true) {
						if (regex.test(match[1])) {
							Log(3, match[1] + ' is not a valid value. Contains an operator in its name');
							continue;
						}
						map.Set(match[1], match[2].toLowerCase());
					}
					else { //verify for correct category
						if (!regex.test(match[2])) {
							Log(3, match[2] + ' is not a valid category. Valid types are string, date, number, duration or size');
							continue;
						}
						map.Set(match[1].toLowerCase(), match[2].toLowerCase());
					}
				}
				else Log(3, vector(i) + ' doesn\'t have a valid syntax');
			}
		}
		catch (error) {
			Log(3, '   Error when parsing ' + vector(i) + ' : ' + error);
			continue;
		};
	}
	Script.Vars.Set(name, map);
	return return_map ? map : true;
}

// Called whenever the user modifies the script's configuration
function OnScriptConfigChange(configChangeData) {
	for (var i = 0; i < configChangeData.changed.length; i++) {
		if (configChangeData.changed(i) === 'custom_columns_categories') getCustomMap('custom_columns_categories', false, false);
		else if (configChangeData.changed(i) === 'custom_columns_names') getCustomMap('custom_columns_names', true, false);
	}
}

function editCategoriesDlg(parent, saved_categories) {
	Log(2, script_name + ' v' + script_version + ' : Editing saved categories');
	var save_on_exit;
	if (!saved_categories) {
		save_on_exit = true;
		saved_categories = Script.Vars.Exists('saved_categories') ? Script.Vars.Get('saved_categories') : DOpus.Create().Map();
	}
	if (saved_categories.empty) {
		Log(3, 'Nothing to edit!');
		return;
	}
	var dlgtypes = {};
	dlgtypes.gui = DOpus.Dlg();
	dlgtypes.gui.template = 'category_editor';
	dlgtypes.gui.title = script_label + ' v' + script_version + ' - Category Editor';
	if (parent) {
		dlgtypes.gui.window = parent;
		dlgtypes.gui.disable_window = parent;
	}
	dlgtypes.gui.Create();

	dlgtypes.listview = dlgtypes.gui.Control('cat_listview');
	dlgtypes.name = dlgtypes.gui.Control('cat_colname');
	dlgtypes.type = dlgtypes.gui.Control('cat_combo');
	dlgtypes.ok_btn = dlgtypes.gui.Control('ok_btn');
	dlgtypes.clear_btn = dlgtypes.gui.Control('clear_btn');

	var fila;
	for (var e = new Enumerator(saved_categories); !e.atEnd(); e.moveNext()) {
		fila = dlgtypes.listview.getItemAt(dlgtypes.listview.AddItem(e.item()));
		fila.subitems(0) = saved_categories(e.item());
	}
	dlgtypes.listview.columns.autosize();
	dlgtypes.gui.Show();
	dlgtypes.listview.value = 0;
	var msg, value, are_changed;
	while (true) {
		msg = dlgtypes.gui.GetMsg();
		if (!msg.result) break;
		if (msg.event === 'selchange' && msg.control === 'cat_listview') {
			sel_item = dlgtypes.listview.value;
			if (sel_item.index !== -1) {
				dlgtypes.name.label = sel_item.name;
				dlgtypes.type.value = dlgtypes.type.getItemByName(sel_item.subitems(0));
			}
		}
		else if (msg.event === 'editchange' && msg.control === 'cat_colname') {
			dlgtypes.type.enabled = dlgtypes.ok_btn.enabled = dlgtypes.clear_btn.enabled = msg.value !== '';
		}
		else if (msg.event === 'click') {
			if (msg.control === 'ok_btn' && msg.focus) {
				are_changed = true;
				category = dlgtypes.type.value.name;
				sel_item.subitems(0) = category;
				saved_categories(sel_item.name) = category;
			}
			else if (msg.control === 'clear_btn' && msg.focus) {
				dlgtypes.listview.RemoveItem(sel_item);
				saved_categories.erase(sel_item.name);
				if (dlgtypes.listview.count) dlgtypes.listview.value = 0;
			}
		}
	}
	dlgtypes = null;
	if (are_changed) {
		Script.Vars.Set('saved_categories', saved_categories);
		Script.Vars('saved_categories').persist = true;
	}
	if (save_on_exit) return;
	return saved_categories;
}

function getSuggestions(quick_key, names_only) {
	try {
		var values = DOpus.Create().Vector();
		if (!names_only && Script.Vars.Exists('saved_queries')) {
			var vector = Script.Vars.Get('saved_queries');
			for (var i = 0; i < vector.length; i++) {
				values.push_back(quick_key + vector(i) + '\t(history)');
			}
			vector = null;
		}
		if (custom_names_map === undefined) custom_names_map = Script.config.custom_columns_names.empty ? null : (Script.Vars.Exists('custom_columns_names') ? Script.Vars.Get('custom_columns_names') : getCustomMap('custom_columns_names', true, true));
		if (custom_names_map !== null && !custom_names_map.empty) {
			for (var e = new Enumerator(custom_names_map); !e.atEnd(); e.moveNext()) {
				values.push_back(quick_key + e.item() + '\t' + custom_names_map(e.item()) + ' (custom name)');
			}
			e = null;
		}
	}
	catch (error) {
		Log(3, 'Error when getting suggestions : ' + error.description);
	};
	return values;
}

function getColumnSuggestions(quick_key, append_str) {
	if (!g_colnames_set) g_colnames_set = GlobalGetColumnsSet();
	var values = DOpus.Create().Vector();
	try {
		for (var i = 0; i < g_colnames_set.length; i++)
			values.push_back(quick_key + append_str + g_colnames_set(i) + '}\t(keywords)');
		if (Script.Vars.Exists('saved_queries')) {
			var vector = Script.Vars.Get('saved_queries');
			for (var i = 0; i < vector.length; i++)
				values.push_back(quick_key + vector(i) + '\t(history)');
			vector = null;
		}
	}
	catch (error) {
		Log(3, 'Error when getting suggestions : ' + error.description);
	};
	return values;
}

function CheckDefaultFilter() {
	Log(1, 'Trying to get default filter from prefs.oxc...');
	var result = false;
	try {
		var prefs_file = DOpus.Aliases('dopusdata').path + '\\ConfigFiles\\prefs.oxc';
		var xmlDocEv = new ActiveXObject('Msxml2.DOMDocument');
		xmlDocEv.load(prefs_file);
		if (xmlDocEv.parseError.errorCode == 0) {
			var default_filter = '';
			var fayts = xmlDocEv.selectSingleNode('//prefs/fayt');
			if (fayts) default_filter = fayts.getAttribute('defaultmode');
			Log(1, '   default filter : ' + default_filter);
			if (default_filter === script_name) result = true;
			if (!quickKey) {
				fayts = xmlDocEv.selectSingleNode('//prefs/fayt/modes/mode[@fayt_mode="' + script_name + '"]');
				if (fayts) quickKey = String.fromCharCode(fayts.getAttribute('key'));
			}
		}
		else
			Log(3, 'Error parsing prefs xml file');
		xmlDocEv = null;
	}
	catch (err) {
		Log(3, 'Error while trying to get default filter info : ' + err.description);
	};
	return result;
}

function MapColHeaders() {
	var map = DOpus.Create().Map();
	var helper = str_tools.LanguageStr(100);
	map.set(map.Exists(helper) ? (helper + ' (coords)') : helper, 'coords');
	helper = str_tools.LanguageStr(101);
	map.set(map.Exists(helper) ? (helper + ' (altitude)') : helper, 'altitude');
	helper = str_tools.LanguageStr(145);
	map.set(map.Exists(helper) ? (helper + ' (availability)') : helper, 'availability');
	helper = str_tools.LanguageStr(30307);
	map.set(map.Exists(helper) ? (helper + ' (streams)') : helper, 'streamcount');
	helper = str_tools.LanguageStr(152);
	map.set(map.Exists(helper) ? (helper + ' (lensmodel)') : helper, 'lensmodel');
	helper = str_tools.LanguageStr(153);
	map.set(map.Exists(helper) ? (helper + ' (lensmake)') : helper, 'lensmake');
	helper = str_tools.LanguageStr(25);
	map.set(map.Exists(helper) ? (helper + ' (picwidth)') : helper, 'picwidth');
	helper = str_tools.LanguageStr(26);
	map.set(map.Exists(helper) ? (helper + ' (picheight)') : helper, 'picheight');
	helper = str_tools.LanguageStr(29376);
	map.set(map.Exists(helper) ? (helper + ' (aspectratiogroup)') : helper, 'aspectratiogroup');
	helper = str_tools.LanguageStr(29377);
	map.set(map.Exists(helper) ? (helper + ' (audiolangs)') : helper, 'audiolangs');
	helper = str_tools.LanguageStr(29378);
	map.set(map.Exists(helper) ? (helper + ' (audiocount)') : helper, 'audiocount');
	helper = str_tools.LanguageStr(29389);
	map.set(map.Exists(helper) ? (helper + ' (colormodel)') : helper, 'colormodel');
	helper = str_tools.LanguageStr(29390);
	map.set(map.Exists(helper) ? (helper + ' (hdrtypes)') : helper, 'hdrtypes');
	helper = str_tools.LanguageStr(29391);
	map.set(map.Exists(helper) ? (helper + ' (perms)') : helper, 'perms');
	helper = str_tools.LanguageStr(29381);
	map.set(map.Exists(helper) ? (helper + ' (subtitlelangs)') : helper, 'subtitlelangs');
	helper = str_tools.LanguageStr(29382);
	map.set(map.Exists(helper) ? (helper + ' (subtitlecount)') : helper, 'subtitlecount');
	helper = str_tools.LanguageStr(29383);
	map.set(map.Exists(helper) ? (helper + ' (videolangs)') : helper, 'videolangs');
	helper = str_tools.LanguageStr(29384);
	map.set(map.Exists(helper) ? (helper + ' (videocount)') : helper, 'videocount');
	helper = str_tools.LanguageStr(30007);
	map.set(map.Exists(helper) ? (helper + ' (picres)') : helper, 'picres');
	helper = str_tools.LanguageStr(310);
	map.set(map.Exists(helper) ? (helper + ' (name)') : helper, 'name');
	helper = str_tools.LanguageStr(311);
	map.set(map.Exists(helper) ? (helper + ' (size)') : helper, 'size');
	helper = str_tools.LanguageStr(312);
	map.set(map.Exists(helper) ? (helper + ' (type)') : helper, 'type');
	helper = str_tools.LanguageStr(313);
	map.set(map.Exists(helper) ? (helper + ' (modifieddate)') : helper, 'modifieddate');
	helper = str_tools.LanguageStr(314);
	map.set(map.Exists(helper) ? (helper + ' (modifiedtime)') : helper, 'modifiedtime');
	helper = str_tools.LanguageStr(315);
	map.set(map.Exists(helper) ? (helper + ' (attr)') : helper, 'attr');
	helper = str_tools.LanguageStr(316);
	map.set(map.Exists(helper) ? (helper + ' (createddate)') : helper, 'createddate');
	helper = str_tools.LanguageStr(317);
	map.set(map.Exists(helper) ? (helper + ' (createdtime)') : helper, 'createdtime');
	helper = str_tools.LanguageStr(318);
	map.set(map.Exists(helper) ? (helper + ' (accesseddate)') : helper, 'accesseddate');
	helper = str_tools.LanguageStr(319);
	map.set(map.Exists(helper) ? (helper + ' (accessedtime)') : helper, 'accessedtime');
	helper = str_tools.LanguageStr(323);
	map.set(map.Exists(helper) ? (helper + ' (ext)') : helper, 'ext');
	helper = str_tools.LanguageStr(324);
	map.set(map.Exists(helper) ? (helper + ' (desc)') : helper, 'desc');
	helper = str_tools.LanguageStr(327);
	map.set(map.Exists(helper) ? (helper + ' (picdepth)') : helper, 'picdepth');
	helper = str_tools.LanguageStr(328);
	map.set(map.Exists(helper) ? (helper + ' (picsize)') : helper, 'picsize');
	helper = str_tools.LanguageStr(329);
	map.set(map.Exists(helper) ? (helper + ' (mp3type)') : helper, 'mp3type');
	helper = str_tools.LanguageStr(330);
	map.set(map.Exists(helper) ? (helper + ' (mp3bitrate)') : helper, 'mp3bitrate');
	helper = str_tools.LanguageStr(331);
	map.set(map.Exists(helper) ? (helper + ' (mp3samplerate)') : helper, 'mp3samplerate');
	helper = str_tools.LanguageStr(332);
	map.set(map.Exists(helper) ? (helper + ' (mp3mode)') : helper, 'mp3mode');
	helper = str_tools.LanguageStr(333);
	map.set(map.Exists(helper) ? (helper + ' (mp3genre)') : helper, 'mp3genre');
	helper = str_tools.LanguageStr(364);
	map.set(map.Exists(helper) ? (helper + ' (title)') : helper, 'title');
	helper = str_tools.LanguageStr(334);
	map.set(map.Exists(helper) ? (helper + ' (mp3title)') : helper, 'mp3title');
	helper = str_tools.LanguageStr(335);
	map.set(map.Exists(helper) ? (helper + ' (mp3artists)') : helper, 'mp3artists');
	helper = str_tools.LanguageStr(336);
	map.set(map.Exists(helper) ? (helper + ' (mp3album)') : helper, 'mp3album');
	helper = str_tools.LanguageStr(337);
	map.set(map.Exists(helper) ? (helper + ' (mp3year)') : helper, 'mp3year');
	helper = str_tools.LanguageStr(368);
	map.set(map.Exists(helper) ? (helper + ' (comments)') : helper, 'comments');
	helper = str_tools.LanguageStr(338);
	map.set(map.Exists(helper) ? (helper + ' (mp3comment)') : helper, 'mp3comment');
	helper = str_tools.LanguageStr(339);
	map.set(map.Exists(helper) ? (helper + ' (path)') : helper, 'path');
	helper = str_tools.LanguageStr(340);
	map.set(map.Exists(helper) ? (helper + ' (moddesc)') : helper, 'moddesc');
	helper = str_tools.LanguageStr(341);
	map.set(map.Exists(helper) ? (helper + ' (modversion)') : helper, 'modversion');
	helper = str_tools.LanguageStr(342);
	map.set(map.Exists(helper) ? (helper + ' (prodname)') : helper, 'prodname');
	helper = str_tools.LanguageStr(30308);
	map.set(map.Exists(helper) ? (helper + ' (signer)') : helper, 'signer');
	helper = str_tools.LanguageStr(343);
	map.set(map.Exists(helper) ? (helper + ' (prodversion)') : helper, 'prodversion');
	helper = str_tools.LanguageStr(344);
	map.set(map.Exists(helper) ? (helper + ' (copyright)') : helper, 'copyright');
	helper = str_tools.LanguageStr(345);
	map.set(map.Exists(helper) ? (helper + ' (companyname)') : helper, 'companyname');
	helper = str_tools.LanguageStr(346);
	map.set(map.Exists(helper) ? (helper + ' (mp3info)') : helper, 'mp3info');
	helper = str_tools.LanguageStr(347);
	map.set(map.Exists(helper) ? (helper + ' (owner)') : helper, 'owner');
	helper = str_tools.LanguageStr(348);
	map.set(map.Exists(helper) ? (helper + ' (sizeauto)') : helper, 'sizeauto');
	helper = str_tools.LanguageStr(350);
	map.set(map.Exists(helper) ? (helper + ' (duration)') : helper, 'duration');
	helper = str_tools.LanguageStr(353);
	map.set(map.Exists(helper) ? (helper + ' (sizekb)') : helper, 'sizekb');
	helper = str_tools.LanguageStr(354);
	map.set(map.Exists(helper) ? (helper + ' (cameramake)') : helper, 'cameramake');
	helper = str_tools.LanguageStr(355);
	map.set(map.Exists(helper) ? (helper + ' (cameramodel)') : helper, 'cameramodel');
	helper = str_tools.LanguageStr(356);
	map.set(map.Exists(helper) ? (helper + ' (datetaken)') : helper, 'datetaken');
	helper = str_tools.LanguageStr(30273);
	map.set(map.Exists(helper) ? (helper + ' (datetimeoriginal)') : helper, 'datetimeoriginal');
	helper = str_tools.LanguageStr(30275);
	map.set(map.Exists(helper) ? (helper + ' (datetimecreated)') : helper, 'datetimecreated');
	helper = str_tools.LanguageStr(357);
	map.set(map.Exists(helper) ? (helper + ' (apertureval)') : helper, 'apertureval');
	helper = str_tools.LanguageStr(358);
	map.set(map.Exists(helper) ? (helper + ' (shutterspeed)') : helper, 'shutterspeed');
	helper = str_tools.LanguageStr(359);
	map.set(map.Exists(helper) ? (helper + ' (isospeed)') : helper, 'isospeed');
	helper = str_tools.LanguageStr(360);
	map.set(map.Exists(helper) ? (helper + ' (whitebalance)') : helper, 'whitebalance');
	helper = str_tools.LanguageStr(361);
	map.set(map.Exists(helper) ? (helper + ' (exposurebias)') : helper, 'exposurebias');
	helper = str_tools.LanguageStr(362);
	map.set(map.Exists(helper) ? (helper + ' (flash)') : helper, 'flash');
	helper = str_tools.LanguageStr(363);
	map.set(map.Exists(helper) ? (helper + ' (author)') : helper, 'author');
	helper = str_tools.LanguageStr(365);
	map.set(map.Exists(helper) ? (helper + ' (subject)') : helper, 'subject');
	helper = str_tools.LanguageStr(366);
	map.set(map.Exists(helper) ? (helper + ' (category)') : helper, 'category');
	helper = str_tools.LanguageStr(367);
	map.set(map.Exists(helper) ? (helper + ' (pages)') : helper, 'pages');
	helper = str_tools.LanguageStr(369);
	map.set(map.Exists(helper) ? (helper + ' (picresx)') : helper, 'picresx');
	helper = str_tools.LanguageStr(370);
	map.set(map.Exists(helper) ? (helper + ' (picresy)') : helper, 'picresy');
	helper = str_tools.LanguageStr(371);
	map.set(map.Exists(helper) ? (helper + ' (mp3track)') : helper, 'mp3track');
	helper = str_tools.LanguageStr(373);
	map.set(map.Exists(helper) ? (helper + ' (focallength)') : helper, 'focallength');
	helper = str_tools.LanguageStr(374);
	map.set(map.Exists(helper) ? (helper + ' (meteringmode)') : helper, 'meteringmode');
	helper = str_tools.LanguageStr(375);
	map.set(map.Exists(helper) ? (helper + ' (exposureprogram)') : helper, 'exposureprogram');
	helper = str_tools.LanguageStr(376);
	map.set(map.Exists(helper) ? (helper + ' (subjectdistance)') : helper, 'subjectdistance');
	helper = str_tools.LanguageStr(377);
	map.set(map.Exists(helper) ? (helper + ' (exposuretime)') : helper, 'exposuretime');
	helper = str_tools.LanguageStr(378);
	map.set(map.Exists(helper) ? (helper + ' (fnumber)') : helper, 'fnumber');
	helper = str_tools.LanguageStr(379);
	map.set(map.Exists(helper) ? (helper + ' (scenecapturetype)') : helper, 'scenecapturetype');
	helper = str_tools.LanguageStr(380);
	map.set(map.Exists(helper) ? (helper + ' (pathrel)') : helper, 'pathrel');
	helper = str_tools.LanguageStr(381);
	map.set(map.Exists(helper) ? (helper + ' (extdir)') : helper, 'extdir');
	helper = str_tools.LanguageStr(382);
	map.set(map.Exists(helper) ? (helper + ' (rotation)') : helper, 'rotation');
	helper = str_tools.LanguageStr(383);
	map.set(map.Exists(helper) ? (helper + ' (contrast)') : helper, 'contrast');
	helper = str_tools.LanguageStr(384);
	map.set(map.Exists(helper) ? (helper + ' (saturation)') : helper, 'saturation');
	helper = str_tools.LanguageStr(385);
	map.set(map.Exists(helper) ? (helper + ' (sharpness)') : helper, 'sharpness');
	helper = str_tools.LanguageStr(386);
	map.set(map.Exists(helper) ? (helper + ' (35mmfocallength)') : helper, '35mmfocallength');
	helper = str_tools.LanguageStr(387);
	map.set(map.Exists(helper) ? (helper + ' (digitalzoom)') : helper, 'digitalzoom');
	helper = str_tools.LanguageStr(393);
	map.set(map.Exists(helper) ? (helper + ' (encodedby)') : helper, 'encodedby');
	helper = str_tools.LanguageStr(394);
	map.set(map.Exists(helper) ? (helper + ' (framerate)') : helper, 'framerate');
	helper = str_tools.LanguageStr(395);
	map.set(map.Exists(helper) ? (helper + ' (datarate)') : helper, 'datarate');
	helper = str_tools.LanguageStr(402);
	map.set(map.Exists(helper) ? (helper + ' (keywords)') : helper, 'keywords');
	helper = str_tools.LanguageStr(403);
	map.set(map.Exists(helper) ? (helper + ' (doccreateddate)') : helper, 'doccreateddate');
	helper = str_tools.LanguageStr(404);
	map.set(map.Exists(helper) ? (helper + ' (doclastsaveddate)') : helper, 'doclastsaveddate');
	helper = str_tools.LanguageStr(405);
	map.set(map.Exists(helper) ? (helper + ' (docedittime)') : helper, 'docedittime');
	helper = str_tools.LanguageStr(406);
	map.set(map.Exists(helper) ? (helper + ' (doclastsavedby)') : helper, 'doclastsavedby');
	helper = str_tools.LanguageStr(408);
	map.set(map.Exists(helper) ? (helper + ' (colorspace)') : helper, 'colorspace');
	helper = str_tools.LanguageStr(409);
	map.set(map.Exists(helper) ? (helper + ' (fontname)') : helper, 'fontname');
	helper = str_tools.LanguageStr(410);
	map.set(map.Exists(helper) ? (helper + ' (parent)') : helper, 'parent');
	helper = str_tools.LanguageStr(411);
	map.set(map.Exists(helper) ? (helper + ' (parentpath)') : helper, 'parentpath');
	helper = str_tools.LanguageStr(412);
	map.set(map.Exists(helper) ? (helper + ' (parentlocation)') : helper, 'parentlocation');
	helper = str_tools.LanguageStr(416);
	map.set(map.Exists(helper) ? (helper + ' (aspectratio)') : helper, 'aspectratio');
	helper = str_tools.LanguageStr(417);
	map.set(map.Exists(helper) ? (helper + ' (software)') : helper, 'software');
	helper = str_tools.LanguageStr(418);
	map.set(map.Exists(helper) ? (helper + ' (mp3drm)') : helper, 'mp3drm');
	helper = str_tools.LanguageStr(419);
	map.set(map.Exists(helper) ? (helper + ' (mp3bpm)') : helper, 'mp3bpm');
	helper = str_tools.LanguageStr(420);
	map.set(map.Exists(helper) ? (helper + ' (rating)') : helper, 'rating');
	helper = str_tools.LanguageStr(421);
	map.set(map.Exists(helper) ? (helper + ' (scenemode)') : helper, 'scenemode');
	helper = str_tools.LanguageStr(422);
	map.set(map.Exists(helper) ? (helper + ' (macromode)') : helper, 'macromode');
	helper = str_tools.LanguageStr(423);
	map.set(map.Exists(helper) ? (helper + ' (imagequality)') : helper, 'imagequality');
	helper = str_tools.LanguageStr(425);
	map.set(map.Exists(helper) ? (helper + ' (datedigitized)') : helper, 'datedigitized');
	helper = str_tools.LanguageStr(426);
	map.set(map.Exists(helper) ? (helper + ' (creator)') : helper, 'creator');
	helper = str_tools.LanguageStr(427);
	map.set(map.Exists(helper) ? (helper + ' (producer)') : helper, 'producer');
	helper = str_tools.LanguageStr(434);
	map.set(map.Exists(helper) ? (helper + ' (publisher)') : helper, 'publisher');
	helper = str_tools.LanguageStr(436);
	map.set(map.Exists(helper) ? (helper + ' (composers)') : helper, 'composers');
	helper = str_tools.LanguageStr(437);
	map.set(map.Exists(helper) ? (helper + ' (conductors)') : helper, 'conductors');
	helper = str_tools.LanguageStr(441);
	map.set(map.Exists(helper) ? (helper + ' (initialkey)') : helper, 'initialkey');
	helper = str_tools.LanguageStr(444);
	map.set(map.Exists(helper) ? (helper + ' (director)') : helper, 'director');
	helper = str_tools.LanguageStr(447);
	map.set(map.Exists(helper) ? (helper + ' (releasedate)') : helper, 'releasedate');
	helper = str_tools.LanguageStr(448);
	map.set(map.Exists(helper) ? (helper + ' (episodename)') : helper, 'episodename');
	helper = str_tools.LanguageStr(449);
	map.set(map.Exists(helper) ? (helper + ' (credits)') : helper, 'credits');
	helper = str_tools.LanguageStr(450);
	map.set(map.Exists(helper) ? (helper + ' (channel)') : helper, 'channel');
	helper = str_tools.LanguageStr(451);
	map.set(map.Exists(helper) ? (helper + ' (ishd)') : helper, 'ishd');
	helper = str_tools.LanguageStr(452);
	map.set(map.Exists(helper) ? (helper + ' (isrepeat)') : helper, 'isrepeat');
	helper = str_tools.LanguageStr(453);
	map.set(map.Exists(helper) ? (helper + ' (broadcastdate)') : helper, 'broadcastdate');
	helper = str_tools.LanguageStr(454);
	map.set(map.Exists(helper) ? (helper + ' (recordingtime)') : helper, 'recordingtime');
	helper = str_tools.LanguageStr(455);
	map.set(map.Exists(helper) ? (helper + ' (station)') : helper, 'station');
	helper = str_tools.LanguageStr(457);
	map.set(map.Exists(helper) ? (helper + ' (label)') : helper, 'label');
	helper = str_tools.LanguageStr(458);
	map.set(map.Exists(helper) ? (helper + ' (mp3disk)') : helper, 'mp3disk');
	helper = str_tools.LanguageStr(459);
	map.set(map.Exists(helper) ? (helper + ' (picphyssize)') : helper, 'picphyssize');
	helper = str_tools.LanguageStr(550);
	map.set(map.Exists(helper) ? (helper + ' (picphysx)') : helper, 'picphysx');
	helper = str_tools.LanguageStr(551);
	map.set(map.Exists(helper) ? (helper + ' (picphysy)') : helper, 'picphysy');
	helper = str_tools.LanguageStr(552);
	map.set(map.Exists(helper) ? (helper + ' (mp3albumartist)') : helper, 'mp3albumartist');
	helper = str_tools.LanguageStr(553);
	map.set(map.Exists(helper) ? (helper + ' (target)') : helper, 'target');
	helper = str_tools.LanguageStr(554);
	map.set(map.Exists(helper) ? (helper + ' (userdesc)') : helper, 'userdesc');
	helper = str_tools.LanguageStr(558);
	map.set(map.Exists(helper) ? (helper + ' (fourcc)') : helper, 'fourcc');
	helper = str_tools.LanguageStr(559);
	map.set(map.Exists(helper) ? (helper + ' (pathlen)') : helper, 'pathlen');
	helper = str_tools.LanguageStr(561);
	map.set(map.Exists(helper) ? (helper + ' (fullpath)') : helper, 'fullpath');
	helper = str_tools.LanguageStr(563);
	map.set(map.Exists(helper) ? (helper + ' (imagedesc)') : helper, 'imagedesc');
	helper = str_tools.LanguageStr(565);
	map.set(map.Exists(helper) ? (helper + ' (modified)') : helper, 'modified');
	helper = str_tools.LanguageStr(566);
	map.set(map.Exists(helper) ? (helper + ' (created)') : helper, 'created');
	helper = str_tools.LanguageStr(567);
	map.set(map.Exists(helper) ? (helper + ' (accessed)') : helper, 'accessed');
	helper = str_tools.LanguageStr(570);
	map.set(map.Exists(helper) ? (helper + ' (mp3encodingsoftware)') : helper, 'mp3encodingsoftware');
	helper = str_tools.LanguageStr(571);
	map.set(map.Exists(helper) ? (helper + ' (instructions)') : helper, 'instructions');
	helper = str_tools.LanguageStr(572);
	map.set(map.Exists(helper) ? (helper + ' (compilation)') : helper, 'compilation');
	helper = str_tools.LanguageStr(7572);
	map.set(map.Exists(helper) ? (helper + ' (alllangs)') : helper, 'alllangs');
	helper = str_tools.LanguageStr(92);
	map.set(map.Exists(helper) ? (helper + ' (audiocodec)') : helper, 'audiocodec');
	helper = str_tools.LanguageStr(93);
	map.set(map.Exists(helper) ? (helper + ' (videocodec)') : helper, 'videocodec');
	helper = str_tools.LanguageStr(98);
	map.set(map.Exists(helper) ? (helper + ' (latitude)') : helper, 'latitude');
	helper = str_tools.LanguageStr(99);
	map.set(map.Exists(helper) ? (helper + ' (longitude)') : helper, 'longitude');
	if (!g_colnames_set) g_colnames_set = GlobalGetColumnsSet();
	for (var i = 0; i < g_colnames_set.count; i++) {
		map(g_colnames_set(i)) = null;
	}

	return map;
}

function printCols(cols, tab) {
	var cmd = DOpus.Create().Command();
	cmd.SetSourceTab(tab);
	cmd.RunCommand('Set UTILITY=OtherLog');
	DOpus.ClearOutput();
	DOpus.Output('=======' + script_name + ' v' + script_version + '=====================================');
	var max_length = 0;
	var map = {};
	map['Name'] = 'Header';
	for (var i = 0; i < cols.count; i++) {
		map[cols(i).name] = cols(i).header;
		if (cols(i).name.length > max_length) max_length = cols(i).name.length;
	}
	var name_f;
	for (var name in map) {
		name_f = name;
		while (name_f.length < max_length) {
			name_f += ' ';
		}
		DOpus.Output(name_f + '\t\t | ' + map[name]);
	}
	map = null;
	cmd = null;
	return;
}

String.prototype.trim = function() {
	return this.replace(/^[\s\uFEFF\xA0]+|[\s\uFEFF\xA0]+$/g, '');
};

function Log(level, text) {
	if (level === 4 || Script.config['log level'] < level) {
		if (level == 1) DOpus.Output('<#%vs_dragdrop_normal_action>DEBUG   => ' + text + '</#>');
		else if (level == 2) DOpus.Output('INFO    => ' + text);
		else if (level === 3) DOpus.Output('<#%vs_dragdrop_warning_action>WARNING => ' + text + '</#>');
		else DOpus.Output('ERROR   => ' + text, true);
	}
}

function GetItemsVector(set1, set2) {
	var set_items = DOpus.Create().Vector(set1);
	if (set2 !== undefined) {
		for (var i = 0; i < set2.count; i++) {
			set_items.push_back(set2(i));
		}
	}
	return set_items;
}

function ZeroPad(value, length) {
	// if (value === length) return value;
	var zero = '';
	for (var i = 0; i < length - String(value).length; i++)
		zero += '0';
	return zero + value;
}

function getCollPath(default_value, subvalue) {
	try {
		var collname = str_tools.MakeLegal(Script.config['collection name'].trim(), 'n');
		if (!collname) collname = default_value;
		collname = 'coll://' + collname;
		if (Script.config['create subcollections']) collname += '/' + (subvalue ? (str_tools.MakeLegal(subvalue, 'n') + '_') : '') + DOpus.Create().Date().Format('D#yyyy-MM-dd T#HHmmss');
	}
	catch (err) {
		return 'coll://' + default_value;
	}
	Log(1, '   collection name      : ' + collname);
	return collname;
}

function alert(parent, message, level) {
	var dlg = DOpus.Dlg();
	dlg.window = parent;
	dlg.disable_window = parent;
	dlg.message = message;
	dlg.buttons = "&OK";
	dlg.title = script_name + ' v' + script_version;
	if (level === 0) dlg.icon = 'info';
	else if (level === 1) dlg.icon = 'question';
	else if (level === 2) dlg.icon = 'warning';
	else dlg.icon = 'error';
	dlg.Show();
	dlg = null;
}

function OnDeleteScript(deleteScriptData) {
	Log(2, 'Uninstalling "' + script_name + '" : It was good while it lasted :(...');

	Script.vars.Delete('*');
}




==SCRIPT RESOURCES
<resources>
	<resource type="strings">
		<strings lang="english">
			<string id="add_column">Add the referenced column to the results tab after the search.</string>
			<string id="collection_name">Collection name for the search results.</string>
			<string id="create_subcollections">Create sub-collections instead of overwriting search results.</string>
			<string id="custom_columns_categories">Here you can define the type of data that a column returns. Used for Script, Evaluator, and Shell columns.
Format: &quot;&lt;name&gt;&quot;:&quot;&lt;type&gt;&quot;
&lt;name&gt; must be the same value as the column keyword. 
&lt;type&gt; can be string, date, duration, number, or size.</string>
			<string id="custom_columns_names">Define custom names to refer to a specific column, using the following syntax:
&quot;mycolname&quot;:&quot;&lt;col name&gt;&quot;
Where &lt;col name&gt; is the actual column raw name. (Press this command&apos;s quick key twice to show a list of current column&apos;s names.
Don&apos;t use any operator (&lt;,=,&gt; or !) for mycolname value.</string>
			<string id="debug">Logging level to be displayed. OFF to show only errors. 
DEBUG to show all messages. 
STANDARD to show only the most relevant information.
WARNING to show messages that needs your attention.</string>
			<string id="default_implicit_column">If a column is not explicitly specified, choose between searching by name, the current sorted or highlighted column, or user_defined_implicit_column value.</string>
			<string id="enable_wildcards">Set to True to enable compatibility with Opus standard pattern matching.
NOTE: This will force use of Textual Filters.</string>
			<string id="flag0">&amp;Case sensitive</string>
			<string id="flag1">&amp;Whole words</string>
			<string id="flag2">Regular e&amp;xpressions</string>
			<string id="flag3">Ignore &amp;diacritics</string>
			<string id="flag4">Ignore &amp;undefined values</string>
			<string id="flag5">Filter only what is &amp;visible</string>
			<string id="flag6">&amp;Search Submode</string>
			<string id="flag7">&amp;Multi Mode</string>
			<string id="flags_quickkey">When typing in the FAYT field, the entered character will activate an internal dialog for setting flags.</string>
			<string id="include_evalcols">List all Evaluator Columns in DIALOG.</string>
			<string id="include_scpcols">List all Script Columns in DIALOG.</string>
			<string id="include_shellcols">List all Shell Columns in DIALOG.</string>
			<string id="labels_access">Choose whether filtering by labels should include all or only those visible in the Label Column.</string>
			<string id="max_history_size">Maximum number of entries to be remembered.</string>
			<string id="nested_in_textualfilters">Enable support for showing nested items in results when using Textual Filters.
NOTE: This may significantly increase operation time.</string>
			<string id="preferred_filter_mode">Choose the method to obtain data from a Script, Evaluator, or Shell column.
Textual Filters: Faster, no temporary files.
Rename Preset: Slightly slower, with temporary files. May provide more consistent results. Allows nested item matches.</string>
			<string id="user_defined_implicit_column">In implicit mode, filter by this column, if it exists. If not, the Name column will be used.</string>
		</strings>
		<strings lang="esm">
			<string id="add_column">Tras realizar la búsqueda, añadir la columna referenciada a la pestaña con los resultados.</string>
			<string id="collection_name">Nombre de la colección usada para los resultados.</string>
			<string id="create_subcollections">Crear subcolecciones en lugar de sobreescribir los resultados de la búsqueda.</string>
			<string id="custom_columns_categories">Acá puede definir el tipo de dato que devuelve una columna. Usado para columnas Script, Evaluator y Shell.
Formato: &quot;&lt;nombre&gt;&quot;:&quot;&lt;tipo&gt;&quot;
&lt;nombre&gt; debe ser el mismo valor que el nombre (keyword) de la columna. 
&lt;tipo&gt; puede ser string, date, duration, number o size.</string>
			<string id="custom_columns_names">Defina nombres personalizados para referirse a una columna específica, utilizando la siguiente sintaxis:
mycolname:&lt;nombre col&gt;
Donde &lt;nombre col&gt; es el nombre interno de la columna. (Pulse dos veces la tecla rápida de este comando para mostrar una lista de los nombres de columna actuales.
No utilice ningún operador (&gt;,&lt;,= o !) para el valor de mycolname.</string>
			<string id="debug">El nivel de registro a mostrar. OFF para mostrar solo errores. 
DEBUG para mostrar todos los mensajes. 
STANDARD para mostrar solo la información más relevante. 
WARNING para mostrar mensajes que necesitan tu atención.</string>
			<string id="default_implicit_column">Si no se especifica una columna de manera explícita, elegir entre buscar por nombre, según la columna activa/seleccionada o el valor de user_defined_implicit_column.</string>
			<string id="enable_wildcards">Habilitar el soporte para comodines estándar de Opus.</string>
			<string id="flag0">Distinguir &amp;mayúsculas</string>
			<string id="flag1">&amp;Palabras completas</string>
			<string id="flag2">E&amp;xpresiones regulares</string>
			<string id="flag3">Ignorar &amp;diacriticos</string>
			<string id="flag4">I&amp;gnorar valores no definidos</string>
			<string id="flag5">Filtrar solamente &amp;visibles</string>
			<string id="flag6">Submodo &amp;Búsqueda</string>
			<string id="flag7">Modo M&amp;ulti</string>
			<string id="flags_quickkey">Al escribir en el filtro FAYT el caracter introducido, se activará un diálogo interno para la configuración de flags.</string>
			<string id="include_evalcols">Listar en el diálogo todas las columnas de Evaluator.</string>
			<string id="include_scpcols">Listar en el diálogo todas las columnas de Script.</string>
			<string id="include_shellcols">Listar en el diálogo todas las columnas de Shell.</string>
			<string id="labels_access">Elija si el filtrado por etiquetas debería incluir todas o solo aquellas visibles en la Columna Etiqueta.</string>
			<string id="max_history_size">Número máximo de entradas a recordar.</string>
			<string id="nested_in_textualfilters">Habilitar el soporte para mostrar items anidados en los resultados cuando se utiliza Filtros Textuales.
NOTA: Esto hace que la operación tarde mucho más tiempo en completarse.</string>
			<string id="preferred_filter_mode">Establecer la forma a emplear para obtener los datos de columnas de Script, Evaluator y Shell.
Textual Filters : Más rápido, no hay archivos temporales.
Rename Preset : Ligeramente más lento, con archivos temporales. Puede dar resultados más consistentes.</string>
			<string id="user_defined_implicit_column">En modo implícito, filtrar por esta columna, si existiese. De lo contrario, se utilizará la columna Nombre.</string>
		</strings>
	</resource>
	<resource name="filter_ui" type="dialog">
		<dialog height="222" lang="english" resize="yes" width="154">
			<languages>
				<language height="220" lang="esm" width="154" />
			</languages>
			<control halign="center" height="8" name="message" resize="w" type="static" valign="center" width="120" x="15" y="134" />
			<control halign="left" height="8" name="ignore_btn" resize="x" title="&lt;a id=&quot;ign_empty&quot;&gt;&lt;%ddbi:133&gt;&lt;/a&gt;" type="markuptext" width="8" x="130" y="56" />
			<control edit="yes" height="40" name="name_combo" resize="w" sort="yes" type="combo" width="135" x="4" y="2" />
			<control changelinkcolor="no" halign="left" height="8" name="tocoll_btn" resize="x" title="&lt;a id=&quot;add_coll&quot;&gt;&lt;%ddbi:26&gt;&lt;/a&gt;" type="markuptext" width="10" x="142" y="4" />
			<control height="40" name="category_combo" resize="w" type="combo" width="90" x="36" y="16" />
			<control changelinkcolor="no" halign="left" height="8" name="edit_cat_btn" resize="x" title="&lt;a id=&quot;edit_cat&quot;&gt;&lt;%ddbi:105&gt;&lt;/a&gt;" type="markuptext" width="8" x="142" y="18" />
			<control halign="left" height="12" name="query_edit" resize="w" type="edit" width="120" x="18" y="30" />
			<control height="12" name="clear_btn" resize="x" title="❌" type="button" width="12" x="138" y="30" />
			<control height="8" name="use_and_checkbox" title="&amp;Use AND" type="check" width="48" x="8" y="44">
				<languages>
					<language height="10" lang="esm" title="&amp;Usar AND" width="42" x="4" y="44" />
				</languages>
			</control>
			<control changelinkcolor="no" halign="left" height="8" name="regex_btn" resize="x" title="&lt;a id=&quot;regex&quot;&gt;•✱&lt;/a&gt;" type="markuptext" width="10" x="84" y="44" />
			<control changelinkcolor="no" halign="left" height="8" name="case_btn" resize="x" title="&lt;a id=&quot;case&quot;&gt;Aa&lt;/a&gt;" type="markuptext" width="10" x="98" y="44" />
			<control changelinkcolor="no" halign="left" height="8" name="diac_btn" resize="x" title="&lt;a id=&quot;link&quot;&gt;ůü&lt;/a&gt;" type="markuptext" width="10" x="112" y="44" />
			<control halign="left" height="8" name="more_options" resize="x" title="&lt;b&gt;&lt;a id=&quot;more_options&quot;&gt;&lt;%ddbi:117&gt;&lt;/a&gt;&lt;/b&gt;" type="markuptext" width="8" x="140" y="44" />
			<control checkboxes="auto" fullrow="yes" height="154" multisel="yes" name="listview" resize="wh" type="listview" viewmode="details" width="146" x="4" y="54">
				<columns>
					<item text="Value" />
					<item text="#" />
				</columns>
			</control>
			<control changelinkcolor="no" halign="left" height="8" name="sel_btn" resize="xy" title="&lt;a id=&quot;sel_all&quot;&gt;&lt;#!BTNFACE&gt; &lt;%checkbox&gt; &lt;/#&gt;&lt;/a&gt;&lt;%sp:2&gt;&lt;a id=&quot;sel_invert&quot;&gt;&lt;#!BTNFACE&gt; &lt;%checkbox:2&gt; &lt;/#&gt;&lt;/a&gt;&lt;%sp:2&gt;&lt;a id=&quot;sel_none&quot;&gt;&lt;#!BTNFACE&gt; &lt;%checkbox:0&gt; &lt;/#&gt;&lt;/a&gt;&lt;%sp:2&gt;" type="markuptext" width="44" x="90" y="210" />
			<control changelinkcolor="no" halign="left" height="8" name="refresh_btn" resize="xy" title="&lt;a id=&quot;refresh&quot;&gt;&lt;%oned:4&gt;&lt;/a&gt;" type="markuptext" width="12" x="136" y="210" />
			<control changelinkcolor="no" halign="left" height="8" name="total_title" resize="yw" type="markuptext" width="84" x="4" y="210" />
			<control changelinkcolor="no" halign="left" height="8" name="search_btn" title="&lt;%ddbi:25&gt;&lt;a id=&quot;search_opt&quot;&gt;&lt;%ddbi:6&gt;&lt;/a&gt;" type="markuptext" width="14" x="4" y="32" />
			<control changelinkcolor="no" halign="left" height="8" name="ww_btn" resize="x" title="&lt;a id=&quot;link&quot;&gt;ww&lt;/a&gt;" type="markuptext" width="10" x="126" y="44" />
			<control changelinkcolor="no" halign="left" height="10" name="category_title" type="markuptext" width="30" x="6" y="18" />
			<control changelinkcolor="no" halign="left" height="8" name="save_cat_btn" resize="x" title="&lt;a id=&quot;save_cat&quot;&gt;&lt;%ddbi:69&gt;&lt;/a&gt;" type="markuptext" width="8" x="130" y="18" />
		</dialog>
	</resource>
	<resource name="configflags" type="dialog">
		<dialog height="122" lang="english" width="180">
			<languages>
				<language height="122" lang="esm" width="180" />
			</languages>
			<control height="10" name="flag0" type="check" width="170" x="4" y="4" />
			<control height="10" name="flag1" type="check" width="170" x="4" y="16" />
			<control height="10" name="flag2" type="check" width="170" x="4" y="28" />
			<control height="10" name="flag3" type="check" width="170" x="4" y="40" />
			<control height="10" name="flag4" type="check" width="170" x="4" y="52" />
			<control height="10" name="flag5" type="check" width="170" x="4" y="64" />
			<control height="10" name="flag6" type="check" width="170" x="4" y="76" />
			<control height="10" name="flag7" type="check" width="170" x="4" y="88" />
			<control default="yes" height="14" name="ok_btn" title="Save" type="button" width="50" x="126" y="100">
				<languages>
					<language height="14" lang="esm" title="Guardar" width="50" x="126" y="100" />
				</languages>
			</control>
		</dialog>
	</resource>
	<resource name="category_editor" type="dialog">
		<dialog height="156" lang="english" width="266">
			<control fullrow="yes" height="134" name="cat_listview" nosortheader="yes" type="listview" viewmode="details" width="258" x="4" y="4">
				<columns>
					<item text="Column" />
					<item text="Category" />
				</columns>
			</control>
			<control changelinkcolor="no" halign="left" height="8" name="cat_colname" type="markuptext" width="168" x="4" y="142" />
			<control height="40" name="cat_combo" type="combo" width="64" x="174" y="140">
				<contents>
					<item text="string" />
					<item text="date" />
					<item text="number" />
					<item text="size" />
					<item text="duration" />
				</contents>
			</control>
			<control halign="left" height="8" name="ok_btn" title="&lt;a id=&quot;link&quot;&gt;&lt;%ddbi:28&gt;&lt;/a&gt;" type="markuptext" width="8" x="240" y="142" />
			<control halign="left" height="8" name="clear_btn" title="&lt;a id=&quot;link&quot;&gt;&lt;%ddbi:83&gt;&lt;/a&gt;" type="markuptext" width="8" x="252" y="142" />
		</dialog>
	</resource>
</resources>
