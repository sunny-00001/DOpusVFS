/*
╔══════════════════════════════════════════════════════════════
║ EverythingOpus - A better dialog to integrate Everything with Directory Opus
║ (c) 2025 by Bytim
║ https://resource.dopus.com/t/everythingopus-a-better-dialog-to-integrate-everything-with-directory-opus/56212
║ This is a script interface that uses DOpus EverythingInterface object 
║ to retrieve files from Everything and add them to DOpus collection.
║ 
║ Based on EverythingDopus 2.0 (c) Zoc 2023
║ https://resource.dopus.com/t/everythingdopus-a-utility-to-integrate-everything-with-directory-opus/43844
║ https://github.com/TheZoc/EverythingDopus/
║ 
║ 要让 Everything 自动启动，需要在DOpus的【选项/杂项/高级】<行为> 设置 [everything_autolaunch] 为 [/Everything.exe -startup]。
║ "/Everything.exe" 是路径别名, 在【选项/常用路径/文件夹别名】添加一个别名 "Everything.exe" 目标为你的Everything.exe的完整路径。
║ v13.12.4 (2025.1.20) everything_autolaunch 开始支持别名路径。
║ 
║ To auto-launch Everything, see [Preferences/Miscellaneous/Advanced] <Behavior> set [everything_autolaunch] to [/Everything.exe -startup]
║ "/Everything.exe" is a folder alias. See [Preferences/Frequently Used Paths/Folder Aliases], 
║ add an alias named "Everything.exe" with target to fullpath of your Everything.exe.
║ v13.12.4 (2025.1.20) Aliases are now supported for everything_autolaunch.
║ 
║ https://www.voidtools.com/forum/viewtopic.php?p=36726#p36726
║ Everything v1.5a 需要在【工具--选项--高级】将alpha_instance改为false
║ 或编辑Everything-1.5a.ini 添加这个配置:alpha_instance=0
║ 或者在搜索框输入/alpha_instance=0 回车
║ 在Everything文件夹中新建一个空文件(无扩展名) 命名为: NO_ALPHA_INSTANCE(empty file without extension)
║ 这会使 Everything 创建的服务使用名称 "Everything" 而非 "Everything 1.5a", 且标题栏不带 "1.5a" 尾巴
║ The Everything Service will be named: Everything <and created without 1.5a>
║ 其配置文件也都不带 "-1.5a" 尾巴: Everything.ini Everything.db Filters.csv Plugins.ini
║ 
║ 建议使用 Everything v1.5, 功能更强大；v1.4 不支持多标签，不支持汉字拼音
║ 
║ Useage：
║ 使用方式：
║ ① In FAYT Command Bar at the bottom, input command:
║     >ed search string
║ ② In a toolbar button, use command:
║     EverythingOpusDialog 			(Press "Enter" to〈Search All drives〉by default)
║     EverythingOpusDialog CURRENT 	(Press "Enter" to〈Search current folder〉by default)
║     ("Ctrl+Enter" always responds to〈Search All drives〉)
║     (You can locate the icon of the button to "/Everything.exe".)
║ 
║ 要清除产生的文件收集，可用命令：
║ To clear the generated collections, use command:
║ dopusrt /col delete "EverythingOpus"
║ 
║ v1.0 - 2025.6.21 (the first version)
║ ● use DOpus.Create.EverythingInterface instead of "ed.exe" (in EverythingDopus).
║ ● make it easier to retrieve file infomation from Everything.
║ ● require DOpus v13.7.4 or above. v13.12.4 is required if you set [everything_autolaunch] to an alias.
║ 
║ v1.1 - 2025.6.29
║ ● If current folder is "/mycomputer" or "coll://*", always〈Search All drives〉.
║ ● Add a config property "Exclusion" for setting the path or string to exclude. Default value: '!?:$RECYCLE*\ !ext:cct;col;lnk' .
║ ● Add a hotkey "shift+enter" to the dialog.
║    when using command EverythingOpusDialog CURRENT, "Shift+Enter" responds to the button 〈Search All〉.
║    when using command EverythingOpusDialog, "Shift+Enter" responds to the button 〈Search Folder〉.
║    "Ctrl+Enter" always responds to〈Search All drives〉
║ 
║ v1.2 -2025.7.3
║ Improved IllegalReplace() for creating collection name: replace "\|/"" to "¦", and ":"" to ";".
║ 
║ v1.3 -2025.7.15
║ Add to Search Syntax: content: Search file content    utf8content: Can search .docx
║ 
║ v1.4 -2025.7.31
║ Increase the timeout seconds to 3s: evObj.Query(searchString, "", "f", evSortBy, evCount, 0, 3000);
║ Add a config option: prefixDate, you can choose whether to prefix the collection name with the current date and time.
║ 
║ v1.5 -2025.8.8
║ Support regular expression pattern wrapped in slashes, for example：/\d{4,}/ /[一-龟]{2,}/ png
║ Optimized the argment for "ed" command, now the argument name "s" is not need. Directly use: >ed search string
║ 
╚══════════════════════════════════════════════════════════════
*/

//https://www.voidtools.com/en-us/support/everything/command_line_interface/  The Help Document
//https://www.voidtools.com/zh-cn/support/everything/command_line_interface/  中文版帮助
 

//var objEverything = DOpus.Create.EverythingInterface;
//var evCommand = 'Find CLEAR QUERYENGINE=everythingglobal SHOWRESULTS=source,tab COLLNAME ';
//var evExclude = "!?:\\$RECYCLE*\\"; //排除字符串
var evSortNumber = DOpus.Create.StringSet("1","2","3","4","5","6","11","12","13","14","19","20");//ev的排序代码数值

// Called by Directory Opus to initialize the script
function OnInit(initData)
{
	initData.name = "EverythingOpus";
	initData.version = "1.5";
	initData.copyright = "(c) 2025 bytim";
	initData.url = "https://resource.dopus.com/t/everythingopus-a-better-dialog-to-integrate-everything-with-directory-opus/56212";
	initData.desc = "";
	initData.default_enable = true;
	initData.min_version = "13.7.4"; //v13.7.4 Added EverythingInterface script object.
	// v13.12.4 is required if you set [everything_autolaunch] to an alias.
	
	// variable for enabling debug, so as to enable debug via a button
	initData.config_desc = DOpus.Create.Map;//DOpus.NewMap()
	initData.config.DEBUG = false;
	initData.config_desc("DEBUG") = DOpus.strings.Get("DEBUG");

	initData.config.DebugEnableVar = "$glob:debug";
	initData.config_desc("DebugEnableVar") = "Global var for debug override. Used to enable and disable debug for script applications.";

	//DO选项/杂项/高级 <限制> everything_max_results 默认限制为10000
	initData.config.Limitation = 8000;//限制搜索结果最大值（0 =不限制） 避免过多文件导致DO卡久 ev参数 count:<int>
	initData.config_desc("Limitation") = DOpus.strings.Get("Limitation");

	initData.config.SortBy = 14;//默认排序 修改时间逆序
	initData.config_desc("SortBy") = DOpus.strings.Get("Sortby").replace(/\\n/g,"\n");
	
	initData.config.Exclusion = '!?:\\$RECYCLE*\\ !ext:cct;col;lnk';//要排除的路径或字符串
	initData.config_desc("Exclusion") = DOpus.strings.Get("Exclusion").replace(/\\n/g,"\n");

	initData.config.prefixDate = true; //收集名称以当前日期时间作前缀
	initData.config_desc("prefixDate") = DOpus.strings.Get("prefixDate");

	var searchCmd = initData.AddCommand();
	searchCmd.name     = "ed";
	searchCmd.method   = "OnEverythingOpusSearch";
	searchCmd.desc     = DOpus.strings.Get("cmdDesc_ed");
	searchCmd.label    = "Everything DOpus Search";
	//searchCmd.template = "S/R"; // "ed S SearchString" 当SearchString含空格时无法省略参数(虽然可以用引号"text1 text2") 只好简化为S 方便在命令栏输入
	//Use scriptCmdData.cmdline instead of searchCmd.template, not need argument name "S" any more.

	var searchDlgCmd = initData.AddCommand();
	searchDlgCmd.name     = "EverythingOpusDialog";
	searchDlgCmd.method   = "OnEverythingOpusDialog";
	searchDlgCmd.desc     = DOpus.strings.Get("cmdDesc_dialog");
	searchDlgCmd.label    = "Everything DOpus Dialog";
	searchDlgCmd.template = "CURRENT/S";
}

function OnEverythingOpusSearch(scriptCmdData)
{
	var evObj = DOpus.Create.EverythingInterface;
	var evRun = objEverythingRun(evObj, scriptCmdData);
	if(!evRun){
		DOpus.Output("OnEverythingOpusSearch() - " + DOpus.strings.Get("EverythingFailedRun"));
		return;
	}

	// Extract the search string
	//LogMessage("OnEverythingOpusSearch() - SearchString: " + scriptCmdData.func.args.S);
	//var searchString = scriptCmdData.func.args.got_arg.S ? scriptCmdData.func.args.S : "";
	LogMessage("Original Cmdline: " + scriptCmdData.cmdline)
	var cLine = scriptCmdData.cmdline.trim();
	//if(cLine.search(/^ed /) == -1) return;
	var searchString = cLine.substr(3);
	LogMessage("substr 3-end: " + searchString);
	searchString = searchString.replace(/\/(.+?)\//g, ' regex:$1 ').replace(/\s{2,}/g, ' ').trim(); //支持斜杠正则表达式 /regex/
	if(searchString == ""){
		LogMessage(DOpus.strings.Get("NoSearchString"));
		scriptCmdData.func.Dlg.Request(DOpus.strings.Get("NoSearchString") + "                                    ", DOpus.strings.Get("OK"));
		return;
	}
	ResultCollection(scriptCmdData, searchString, evObj);
}

function OnEverythingOpusDialog(scriptCmdData)
{
	var evObj = DOpus.Create.EverythingInterface;
	var evRun = objEverythingRun(evObj, scriptCmdData);
	if(!evRun){
		DOpus.Output("OnEverythingOpusSearch() - " + DOpus.strings.Get("EverythingFailedRun"));
		return;
	}
	DumpSearchHistory();

	var currentFolder = scriptCmdData.func.args.got_arg.CURRENT ? true : false;
	
	// Create the user dialog
	var dlg = scriptCmdData.func.Dlg;//DOpus.Dlg
	//dlg.title = "EverythingOpus";
	dlg.template = "dlgEverythingOpusSearchDialog";
	dlg.detach = true;
	//dlg.window = scriptCmdData.func.sourcetab;
	dlg.Create();
	dlg.Control("cmbFileType").value = "0";
	dlg.Control("cmbSortBy").value = "0";
	dlg.Control("cmbSymbol").value = "0";
	//dlg.Control("txtIcon").label = Script.LoadImage("EverythingOpusCLI-32x32.png");
	if(currentFolder)
		dlg.title = DOpus.strings.Get("DlgTitle");
	dlg.AddHotkey("enter", "enter");
	dlg.AddHotkey("shiftenter", "shift+enter");
	//dlg.AddHotkey("ctrlenter", "ctrl+enter");

	// Populate the search combo box
	for (var i = 0; i < 10; ++i) {
		var currentItem = "EverythingOpusHistory" + i.toString();
		if (!DOpus.vars.Exists(currentItem))
			break;
		dlg.Control("cmbSearch").AddItem(DOpus.Vars.Get(currentItem));
	}
	/*
	// Select the first item in the combobox, if available
	if (dlg.Control("cmbSearch").count > 0){
		dlg.Control("cmbSearch").SelectItem(0)
	}*/
	//var strName = GetFileName(scriptCmdData);
	//dlg.Control("cmbSearch").label = strName; //编辑框获取第一个选中文件的名称
	
	// Dialog message loop
	for (var msg = dlg.GetMsg(); msg.result; msg = dlg.GetMsg())
	{
		LogMessage("Msg Event: " + msg.event +"  qualifiers: "+ msg.qualifiers);
		if(msg.event == "selchange" && dlg.Control("cmbFileType").focus == true){
			switch (dlg.Control("cmbFileType").value.index) { //Number(dlg.Control("cmbFileType").value
				case 10: dlg.Control("cmbSearch").label += " ext:";break;
				case 11: dlg.Control("cmbSearch").label += " regex:[\u4e00-\u9fff] ";break;
				case 12: dlg.Control("cmbSearch").label += " startwith: endwith:";break;
				case 13: dlg.Control("cmbSearch").label += " size:400mb-4gb ";break;
				case 14: dlg.Control("cmbSearch").label += " dupe: sizedupe: file: size:>30mb ";break;
				case 15: dlg.Control("cmbSearch").label += " dm:last30mins ";break;
				case 16: dlg.Control("cmbSearch").label += " dm:last7days ";break;
				case 17: dlg.Control("cmbSearch").label += " dm:2023.1.6-2023.1.8 ";break;
                case 18: dlg.Control("cmbSearch").label += " dm:20230312t18:12-20230312t18:59 ";break;
                case 19: dlg.Control("cmbSearch").label = "wfn:" + dlg.Control("cmbSearch").label;break;
                case 20: dlg.Control("cmbSearch").label += " regex:";break; 
            }
		} else if (msg.event == "click") {
			if (dlg.Control("btnClearHistory").focus == true) {
				LogMessage("Clearing search history");
				ClearSearchHistory(dlg);
			} else if (dlg.Control("help").focus == true)
				ShowSearchSyntax(dlg);
		} else if (msg.event == "hotkey"){
			LogMessage("Msg Key: " + msg.control +"   qualifiers: "+ msg.qualifiers);
			if(msg.control == "enter"){
				if(currentFolder) dlg.EndDlg(2); //等于按下<当前路径>按钮
				else dlg.EndDlg(1);
			} else if(msg.control == "shiftenter"){
				if(currentFolder) dlg.EndDlg(1); //等于按下<全盘搜索>按钮
				else dlg.EndDlg(2);
			}
		}
	}

	var searchFileType = "", evSortBy = 14, searchString = "", cmbSearchT = "", currentPath = "", macro = "";
	var SourcePathDepth = 0, Symbol = "", pathDepthAdd = 0, pathDepth = "";
	
	if (dlg.result == 0) return;

	switch(dlg.Control("cmbFileType").value.index)
	{
		case 0: break;
		// 获取DO类型群组定义的扩展名，0-Archives,1-Documents,2-Images,3-Movies,4-Music,5-Programs
		case 1: searchFileType = GetTypeGroupExts(0); macro = "[zip] "; break;
		case 2: searchFileType = GetTypeGroupExts(2); macro = "[pic] "; break;
		case 3: searchFileType = GetTypeGroupExts(4); macro = "[audio] "; break;
		case 4: searchFileType = GetTypeGroupExts(3); macro = "[video] "; break;
		case 5: searchFileType = GetTypeGroupExts(1); macro = "[doc] "; break;
		case 6: searchFileType = "ext:bat;cmd;com;exe;msi;msp;scr;vbs"; macro = "[exe] "; break;
		case 7: searchFileType = "folder:"; macro ="[folder] "; break;
		case 8: searchFileType = "file:"; macro = "[file] "; break;
		case 9: searchFileType = "ext:ah2;ahk;bat;c;cct;cfg;col;cpp;css;csv;dcf;dft;dop;ejs;h;hpp;inf;ini;ion;js;jsee;json;log;lrc;lua;mac;md;nfo;off;ofv;oll;orp;osf;ouc;oxc;oxr;ps1;sql;txt;vbs;xml;xys;yaml;yml";
				macro = "[text] "; break;
	}
	if(searchFileType) 
		searchFileType = " " + searchFileType;

	switch(dlg.Control("cmbSortBy").value.index)
	{
		case 0: evSortBy = 14; break;
		case 1: evSortBy = 6; break;
		case 2: evSortBy = 12; break;
		case 3: evSortBy = 2; break;
		case 4: evSortBy = 4; break;
		case 5: evSortBy = 20; break;
		case 6: evSortBy = 13; break;
		case 7: evSortBy = 5; break;
		case 8: evSortBy = 11; break;
		case 9: evSortBy = 1; break;
		case 10: evSortBy = 3; break;
		case 11: evSortBy = 19; break;
	}

	cmbSearchT = dlg.Control("cmbSearch").label;
	cmbSearchT = cmbSearchT.trim();
	
	if(cmbSearchT) {
		AddSearchToHistory(cmbSearchT);
		LogMessage("Original string: " + cmbSearchT);
		/*
		//cmbSearchT = " " + cmbSearchT; //for command: "Find CLEAR QUERYENGINE=everythingglobal"

		//➤var regexMatch = cmbSearchT.match(/\/(.+?)\//); //仅允许一整个 /正则表达式/
		var regexMatch = cmbSearchT.match(/\/(.+?)\//g); //可匹配多个/正则/
		if(regexMatch){
			//➤cmbSearchT = "regex:" + regexMatch[1]; //non g
			cmbSearchT = cmbSearchT.replace(/\/(.+?)\//g, "");
			LogMessage("removed regex patterns：" + cmbSearchT)
			for(var i=0; i<regexMatch.length; i++){
				cmbSearchT += " regex:" + regexMatch[i].replace(/(^\/|\/$)/g, "") + " ";
			}
		}
		*/
		cmbSearchT = cmbSearchT.replace(/\/(.+?)\//g, ' regex:$1 ').replace(/\s{2,}/g, ' ').trim();
	}
	
	LogMessage("After add regex patterns: " + cmbSearchT)
	
	if(dlg.result == 2){ // Search local folder only
		currentPath = scriptCmdData.func.sourcetab.path+"";
		if(currentPath == "::{20D04FE0-3AEA-1069-A2D8-08002B30309D}" 
			|| DOpus.FSUtil.PathType(currentPath) == "coll") //当前路径是shell此电脑 或 文件收集，则全盘搜索
			currentPath = "";
		else currentPath = ' "' + currentPath.replace(/\\+$/, "") + '\\\"'; //在路径下搜索 要确保末尾是\
		if(dlg.Control("checkPathDepth").value)
			SourcePathDepth = scriptCmdData.func.sourcetab.path.components; //当前路径的深度
		LogMessage("Search Folder Pressed - searchString: " + cmbSearchT + currentPath);
	}
	
	if(dlg.Control("checkPathDepth").value){
		switch(dlg.Control("cmbSymbol").value.index)
		{
			case 0: Symbol = "<=";break;
			case 1: Symbol = "";break;
			case 2: Symbol = ">=";break;
		}
		pathDepthAdd = Number(dlg.Control("depth").value);
		SourcePathDepth += pathDepthAdd;
		if(SourcePathDepth>0){
			//Everything可以用 "parent+2:C:\Windows", 等价于 "C:\Windows\*\*\*"，等价于 "parents:4 C:\Windows\"
			//pathDepth = " parents:" + SourcePathDepth; //精确等于路径深度
			pathDepth = " parents:" + Symbol + SourcePathDepth; //第二种，可选择大于/小于/等于深度值
		}
		LogMessage("pathDepth:" + pathDepth + "   path.components:" + SourcePathDepth + " + " + "pathDepthAdd:" + pathDepthAdd);
	}

	var collString = macro + cmbSearchT + pathDepth + currentPath; //As a name used to create a new collection.
	searchString = cmbSearchT + pathDepth + currentPath + searchFileType;
	searchString = searchString.trim();
	if(searchString == "") {
		LogMessage(DOpus.strings.Get("NoSearchString"));
		return;
	}
	LogMessage("OnEverythingOpusSearch() - searchString: " + searchString);
	LogMessage("OnEverythingOpusSearch() - collString: " + collString);
	
	ResultCollection(scriptCmdData, searchString, evObj, evSortBy, collString);
}

function ResultCollection(scriptCmdData, searchString, evObj, evSortBy, collString){
	if(!collString) collString = searchString;
	var collName = 'EverythingOpus/' + NewCollName(collString);
	var evCount = Script.config.Limitation > 0 ? Script.config.Limitation : null; //for evObj.Query()  当 evCount=null 时，会使用DO选项/杂项/高级 <限制> everything_max_results 的值
	if(!evSortBy)
		var evSortBy = Script.config.SortBy; //for "ed"命令 读取脚本配置项的排序设定值
	if(!evSortNumber.exists(evSortBy))
		evSortBy = 14;
	var evExclude = Script.config.Exclusion;
	if(evExclude.trim()){
		DOpus.Output('<#%vs_dragdrop_normal_action>' + DOpus.strings.Get("ExclusionExist") + evExclude + '</#>');
		evExclude = " " + evExclude;
	}
	LogMessage("evSortBy="+evSortBy + "   evCount="+evCount + "   searchString="+searchString + "   evExclude="+evExclude);
	
	/*********************
	//Use command "Find QUERYENGINE=everythingglobal"
	var evCount = Script.config.Limitation > 0 ? " count:" + Script.config.Limitation : "";
	searchString = " " + searchString + evExclude;
	var commandLine = evCommand +'"'+ collName +'"' + ' QUERY' + evCount + searchString; //命令Find QUERYENGINE=everythingglobal 无法指定排序
	//LogMessage("ResultCollection() - Command line: " + commandLine);
	scriptCmdData.func.command.runcommand(commandLine);
	**********************/
	
	//ev退出进程需要耗时一两秒，在未彻底结束进程前DO仍会判断ev还在运行
	//测试问题：调用OnEverythingOpusDialog 打开对话框 输入内容，退出ev后立即执行全盘搜索，结果会是0
	//DO在启动时会检测ev的进程id 用以初始化EverythingInterface对象
	//只有在EverythingInterface对象检测到ev未运行(ev进程退出超过一两秒)后，evObj.start() 才能启动ev
	//evObj.start() 启动ev后需要等待1秒左右 对象的功能才可用
	//DOpus.Delay(1000)

	searchString += evExclude;
	// Query的最后一个参数是timeout，性能差的电脑上搜索可能耗时数秒(正则匹配时耗时更久)，默认值1秒不够可能会导致ev在达到超时即取消搜索，结果为0。可根据电脑性能适当增减超时秒数
	var searchResult = evObj.Query(searchString, "", "f", evSortBy, evCount, 0, 3000);
	if(searchResult.count == 0) {
		scriptCmdData.func.Dlg.Request(DOpus.strings.Get("NoFiles"), DOpus.strings.Get("OK"), DOpus.strings.Get("NoResult"));
		return;
	}
	var CountTip = "Count Limitation:" + evCount + "  |  Search Result:" + searchResult.count;
	if(searchResult.count == evCount){
		CountTip += "\n\t\t" + DOpus.strings.Get("CountTip");
		DOpus.Output(CountTip)
	}
	
	var cmd = DOpus.Create.Command;
	for (var i=0; i < searchResult.count; i++){	
		cmd.AddFile(searchResult(i).fullpath)
	}
	//cmd.AddLine('dopusrt /col create "' + collName + '"'); //先调用dopusrt先创建收集，这样可清空已存在的同名集合
	//cmd.AddLine('Copy COPYTOCOLL=member TO="coll://' + collName + '"');
	cmd.AddLine('Copy COPYTOCOLL=member WHENEXISTS=skip CREATEFOLDER="coll://' + collName + '"'); //比调用dopusrt更好，但无法清空已存在的同名集合
	cmd.AddLine('Go newtab=findexisting "coll://' + collName + '"');
	cmd.Run;
}
/*
function GetFileName(scriptCmdData){
	var sels = scriptCmdData.func.sourcetab.selected;
	if(sels.count == 0) return "";
	var names = sels(0).name;
	return names;
}*/

// Add an entry to the search history, stored in dopus variables
// 要删除搜索历史，可执行此命令GlobalVariables （来自 https://resource.dopus.com/t/global-variables-tool/35389）
// 或者编辑 /dopusdata\ConfigFiles\uservars.oxc  删除EverythingOpusHistory0等条目
function AddSearchToHistory(searchString){
	LogMessage("AddSearchToHistory() - searchString: " + searchString);

	// Keep the last 10 search entries
	var doVars = DOpus.vars;
	var foundIndex = -1;
	var lastValidIndex = -1;
	for (var i = 0; i < 10; ++i){
		// Check if we're at the end of the existing history
		var currentItem = "EverythingOpusHistory" + i.toString();
		lastValidIndex = i;
		if (!doVars.Exists(currentItem)){
			LogMessage("AddSearchToHistory() - last valid history index: " + lastValidIndex);
			break;
		} else if (doVars.Get(currentItem) == searchString){
			LogMessage("AddSearchToHistory() - doVars.Get(\"" + currentItem + "\"): " + doVars.Get(currentItem));
			foundIndex = i;
			break;
		}
	}

  // If we found it, put it on the top of the stack
	if (foundIndex != -1){
		LogMessage("AddSearchToHistory() - searchString found in index " + i + " - Moving to the top of the history");
		ShiftSearchHistoryDown(foundIndex);
	}

	// If we have valid indices, move everything down in the stack by 1, overwriting the last entry
	else if (lastValidIndex > -1){
		LogMessage("AddSearchToHistory() - Last valid index is " + lastValidIndex + ", shifting history down.");
		ShiftSearchHistoryDown(lastValidIndex);
	}

	LogMessage("AddSearchToHistory() - Setting EverythingOpusHistory0 to the search string");
	doVars.Set("EverythingOpusHistory0", searchString);
	doVars("EverythingOpusHistory0").persist = true;
}

// This function shifts everything in the search history down.
// It starts at the given index and move everything down until the top of the stack.
function ShiftSearchHistoryDown(startIndex){
	for (var i = startIndex; i > 0; --i){
		var varName = "EverythingOpusHistory" + i.toString();
		DOpus.vars.Set(varName, DOpus.vars.Get("EverythingOpusHistory" + (i-1).toString()));
		DOpus.Vars(varName).persist = true;
	}
}

// Clear the search history
function ClearSearchHistory(dlg){
	LogMessage("Clearing search history");
	for (var i = 0; i < 10; ++i){
		DOpus.vars.Delete("EverythingOpusHistory"+ i.toString());
	}
	dlg.Control("cmbSearch").RemoveItem(-1);
}

// Outputs all search history variables and their data to the console
function DumpSearchHistory(){
	if (!Script.config.DEBUG && !doLogCmd.IsSet(Script.config.DebugEnableVar))
		return;

	for (var i = 0; i < 10; ++i){
		var currentItem = "EverythingOpusHistory" + i.toString();
		if (!DOpus.vars.Exists(currentItem))
			DOpus.Output(currentItem + ": does not exist yet.");
		else
			DOpus.Output(currentItem + ": " + DOpus.Vars.Get(currentItem));
	}
}

//判断Everything是否已启动
function objEverythingRun(ev, scriptCmdData){
	//var ev = DOpus.Create.EverythingInterface;
	if(!ev.isrunning){
		var success = ev.start();
		if(!success){
			scriptCmdData.func.Dlg.Request(DOpus.strings.Get("LaunchFailed"), DOpus.strings.Get("OK"), DOpus.strings.Get("InvalidPath"));
			AliasCheck(scriptCmdData, "Everything.exe", "Everything.exe", "Everything64.exe");
			return false;
		}
	}
	
	if (ev.version && ev.version.slice(0,3) < 1.5){
		scriptCmdData.func.Dlg.Request(DOpus.strings.Get("VersionRqst"), DOpus.strings.Get("OK"));
		//return false;
	}
	return true;
}

//判断ev别名路径是否有效
//按住Ctrl 点击按钮，若ev启动失败则强制重新指定exe路径 (有时要检查两个文件名，例如 filename =Everything.exe 或 filename2 =Everything64.exe )
function AliasCheck(scriptCmdData, alias, filename, filename2){
	var doFsu = DOpus.FSUtil;
	var qualifiers = scriptCmdData.func.qualifiers;
	var fromKeyboard = scriptCmdData.func.fromkey;
	var forceRequestForExternalTool = (!fromKeyboard && (qualifiers.indexOf("ctrl") > -1));
	//LogMessage("fromKeyboard: " + fromKeyboard + " - forceRequestForExternalTool:" + forceRequestForExternalTool);
	var resourcePath ="";
	var targetPath = doFsu.Resolve("/" + alias) + "";
	
	if (forceRequestForExternalTool || !doFsu.Exists(targetPath) || (targetPath.search(filename) == -1 && targetPath.search(filename2) == -1)) {
		var dlgResult = scriptCmdData.func.dlg.Open(DOpus.strings.Get("LocateFile") + ' "' +filename + '"', "*.exe");
		if(dlgResult && dlgResult.result)
		{
			resourcePath = doFsu.Resolve(dlgResult);
			if(doFsu.Exists(resourcePath)) {
				DOpus.aliases.Add(alias, resourcePath);
				DOpus.Output("Added alias /" + alias + ": " + resourcePath, false, true)
			} else
				DOpus.Output(DOpus.strings.Get("InvalidPath"), true);
		}
	}
}
//生成新的文件收集名称
function NewCollName(str){
	var now = "";
	if(Script.config.prefixDate){
		now = DOpus.Create.Date;
		now = now.Format("D#yyyy-MM-dd T#HH-mm-ss") + " ";//"D#yyyy-MM-dd T#HH-mm-ss"
	}
	return IllegalReplace(now + str);
}
//文件名非法无效字符处理
//文件名中不允许的字符: \/|<>:*?"
//文件名最长为251个字符，但超过246个字符，某些软件不能正常操作文件名
function IllegalReplace(line){
	line = line.replace(/[\|\\\/]+/g, "¦").replace(/:/g, ";").replace(/</g, "[").replace(/>/g, "]");
	line = line.replace(/[\*\?]+/g, "·").replace(/"+/g, "'");
	line = line.trim();//删除前后空格
	line = line.slice(0,160);//避免文件名过长
	return line.trim();
}

function ShowSearchSyntax(dlg){
	dlgSyntax = DOpus.Dlg;
	dlgSyntax.window = dlg;
	//dlgSyntax.title = "Everything 搜索语法";
	dlgSyntax.template = "ShowEvSyntax";
	dlgSyntax.detach = true;
	dlgSyntax.position = "parent";
	dlgSyntax.x = 674;
	dlgSyntax.y = -120;
	dlgSyntax.Create();

	dlgSyntax.AddHotkey("esc", "escape");
	dlgSyntax.Show();
	
	while (true) {
		var msg = dlgSyntax.GetMsg();
		if (msg.event == "hotkey" && msg.Control === "esc") {
			LogMessage(msg.control);
			dlgSyntax.EndDlg();
		}
		if (!msg.result) break;
	}
}

//获取文件类型群组的扩展名
//0-Archives,1-Documents,2-Images,3-Movies,4-Music,5-Programs
function GetTypeGroupExts(x){
	var exts = "ext:";
	for (var eSel = new Enumerator(DOpus.filetypegroups(x)); !eSel.atEnd(); eSel.moveNext())	{
		exts += eSel.item() + ";";
	}
	return exts;
}

String.prototype.trim = function(){return this.replace(/^[\s\uFEFF\xA0\u3000]+|[\s\uFEFF\xA0\u3000]+$/g, '');}

var doLogCmd = DOpus.Create.Command;//DOpus.NewCommand;
function LogMessage(message){
  if (Script.config.DEBUG || doLogCmd.IsSet(Script.config.DebugEnableVar)) { DOpus.Output(message) };
}

==SCRIPT RESOURCES
<resources>
	<resource name="dlgEverythingOpusSearchDialog" type="dialog">
		<dialog fontsize="10" height="66" lang="english" title="Enter → Search all drives / Shift+Enter  → Search current folder    -  EverythingOpus" width="336">
			<languages>
				<language height="66" lang="chs" title="Enter → 全盘搜索 / Shift+Enter  → 搜当前路径    -  EverythingOpus" width="336" />
			</languages>
			<control halign="left" height="9" name="txtBody" title="space=and  |=or  !=not  regex:/.+?\d{2,}\s*/  ext:jpg;png  dm:modified" type="static" valign="top" width="217" x="6" y="8">
				<languages>
					<language height="9" lang="chs" title="空格=与  |=或  !=非  正则:/.+?\d{2,}\s*/  ext:jpg;png  dm:修改日期 size:大小" width="225" x="6" y="8" />
				</languages>
			</control>
			<control edit="yes" height="14" name="cmbSearch" type="combo" width="252" x="6" y="25" />
			<control height="12" name="cmbFileType" type="combo" width="66" x="264" y="24">
				<languages>
					<language height="12" lang="chs" width="66" x="264" y="24">
						<contents>
							<item text="0所有" />
							<item text="1压缩包" />
							<item text="2图片" />
							<item text="3音频" />
							<item text="4视频" />
							<item text="5文档" />
							<item text="6可执行" />
							<item text="7仅文件夹" />
							<item text="8仅文件" />
							<item text="9纯文本类" />
							<item text="e扩展名" />
							<item text="c包含汉字" />
							<item text="t开头/末尾字符" />
							<item text="s文件大小范围" />
							<item text="u重复文件" />
							<item text="30分钟内修改" />
							<item text="7天内修改" />
							<item text="d精确日期" />
							<item text="精确时间段" />
							<item text="w完整文件名" />
							<item text="r正则匹配" />
						</contents>
					</language>
				</languages>
				<contents>
					<item text="Everything" />
					<item text="Compressed" />
					<item text="Picture" />
					<item text="Audio" />
					<item text="Video" />
					<item text="Document" />
					<item text="Executable" />
					<item text="Folder only" />
					<item text="Files only" />
					<item text="Text files" />
					<item text="Extensions" />
					<item text="Chinese char" />
					<item text="startwith endwith" />
					<item text="Size range" />
					<item text="Size Dupe" />
					<item text="dmLast30mins" />
					<item text="dmLast7days" />
					<item text="Exact date" />
					<item text="Exact time" />
					<item text="Whole file name" />
					<item text="Regex match" />
				</contents>
			</control>
			<control close="1" default="yes" height="14" name="btnOK" title="Search &amp;All" type="button" width="48" x="6" y="42">
				<languages>
					<language height="14" lang="chs" title="全盘搜索(&amp;A)" width="48" x="6" y="42" />
				</languages>
			</control>
			<control close="2" height="14" name="btnLocalSearch" title="Search &amp;Folder" type="button" width="48" x="60" y="42">
				<languages>
					<language height="14" lang="chs" title="当前路径(&amp;F)" width="48" x="60" y="42" />
				</languages>
			</control>
			<control height="14" name="checkPathDepth" title="Path &amp;Depth" type="check" width="47" x="114" y="42">
				<languages>
					<language height="14" lang="chs" title="路径深度(&amp;D)" width="47" x="114" y="42" />
				</languages>
			</control>
			<control height="40" name="cmbSymbol" type="combo" width="24" x="162" y="43">
				<contents>
					<item text="&lt;=" />
					<item text="=" />
					<item text="&gt;=" />
				</contents>
			</control>
			<control halign="left" height="12" name="depth" number="yes" title="1" type="edit" updown="yes" val_max="999" width="24" x="192" y="43" />
			<control height="14" name="btnClearHistory" title="Clear &amp;History" type="button" width="48" x="222" y="42">
				<languages>
					<language height="14" lang="chs" title="清除历史(&amp;H)" width="48" x="222" y="42" />
				</languages>
			</control>
			<control close="0" height="14" name="btnCancel" title="&amp;Cancel" type="button" width="42" x="288" y="42">
				<languages>
					<language height="14" lang="chs" title="取消(&amp;C)" width="42" x="288" y="42" />
				</languages>
			</control>
			<control height="12" name="help" title="More Syntax" type="button" width="48" x="282" y="6">
				<languages>
					<language height="12" lang="chs" title="详细语法" width="48" x="282" y="6" />
				</languages>
			</control>
			<control height="40" name="cmbSortBy" type="combo" width="54" x="225" y="6">
				<languages>
					<language height="40" lang="chs" width="48" x="233" y="6">
						<contents>
							<item text="修改时间 ↓" />
							<item text="大小 ↓" />
							<item text="创建时间 ↓" />
							<item text="名称 ↓" />
							<item text="路径 ↓" />
							<item text="运行次数 ↓" />
							<item text="修改时间 ↑" />
							<item text="大小 ↑" />
							<item text="创建时间 ↑" />
							<item text="名称 ↑" />
							<item text="路径 ↑" />
							<item text="运行次数 ↑" />
						</contents>
					</language>
				</languages>
				<contents>
					<item text="Modified ↓" />
					<item text="Size ↓" />
					<item text="Created ↓" />
					<item text="Name ↓" />
					<item text="Path ↓" />
					<item text="RunCount ↓" />
					<item text="Modified ↑" />
					<item text="Size ↑" />
					<item text="Created ↑" />
					<item text="Name ↑" />
					<item text="Path ↑" />
					<item text="RunCount ↑" />
				</contents>
			</control>
		</dialog>
	</resource>
	<resource name="ShowEvSyntax" type="dialog">
		<dialog fontsize="11" height="233" lang="english" resize="yes" title="Everything Search Syntax" width="254">
			<languages>
				<language height="233" lang="chs" title="Everything 搜索语法" width="254" />
			</languages>
			<control halign="left" height="222" multiline="yes" name="edit1" readonly="yes" resize="wh" title="For full Syntax see Everything&apos;s menu [Help/Search Syntax]\n\nspace=and                |=or                !=not\nab*:	Start with &quot;ab&quot;\n*ab:	End with &quot;ab&quot;\next:jpg;png         Match extension(Separate with semicolons)\ndm:date_modified          dc:date_created          da:date_accessed\ncase:	Match case\nregex:	Enable regular expressions\nregex:\p{Han}  Match Chinese characters\n[一-龟] [一-龙] [一-龥]  [\u4e00-\u9fff] can also be used.\nUp to now, Unicode has included 100000 Chinese characters, the range is [\u3400-\u4dbf\u4e00-\u9fff\uf900-\ufaff\x{20000}-\x{3134a}].\n[\u2E80-\u9FFF]	includes Chinese, Japanese, and Korean characters\n[^x00-xff]		Double byte characters (including Chinese characters)\nWith EverythingOpus you can do a regex search using slashes: /^[a-z]{10,15}\d+.*?$/\n\nFilter Macros\nfile: folder: audio: video: pic: zip: doc: exe:\n\ncontent: Search file content    utf8content: Can search .docx\n\nWildcard matching:\n*:  Matches zero or more characters (except \)          **:  (include \)\n?:  Matches one character(except \)\n\nSpecify basename for folder or path: \nancestor:C:\Windows  ancestor-name:Windows  Items with an ancestor containing the specified name.\nparent: in-folder: parent+2:[fullpath]    Childs in the specified parent.\nparent-name:foldername    Parent name includes the string.\nparent-path:&quot;:\Program Files\&quot;   Item's path part includes the string.\nparent-size:>100mb    a parent folder with the specified size\nhttps://www.voidtools.com/forum/viewtopic.php?f=12&t=10176#ancestor-name\n\nwfn:	Match the whole filename\nww:	Match whole words only\nstartwith:	Match the start of the filename\nendwith:	Match the end of the filename\nname:	Match the name part\nstem:	Match the name part(without ext)\nlen:&gt;120	Match the length of the filename\npath:	Match path and filename\ndepth:	The number of parents matches the specified count(use parents: in ev1.4)\ndupe:	Items where the values of the specified property type are duplicated.(name for default)\nfile: dupe: dupe:size size:&gt;200mb	Files with the same name and size larger than 200MB\n(Compatible with ev1.4)sizedupe: dadupe: dcdupe: dmdupe: attribdupe:\ncontent:&lt;abc 123&gt;	Search for files where the content contains abc AND 123\ncount:	Limit the number of results\nfilelist:	Search for items where the filename is in the specified semicolon delimited list\nattrib:hr	The attributes contain hidden and read only flag(may slowly)\n\nFile size filter\nempty:		Folders where the number of subfolders and files is zero\nsize:empty	Empty file\nsize:&gt;=3gb\nsize:400mb-1gb\nsize:&lt;1kb\n\nDate filter\ndm:last7days\ndm:last30mins\ndm:&lt;202305	The date modified is before 2023.5\ndm:2023.1.6..2023.1.8	Between 2023.1.6 and 2023.1.8\ndm:20230312t18:12-20230312t18:59	Accurate to the minute\nFrom January to December: jan|feb|mar|apr|may|jun|jul|aug|sep|oct|nov|dec\ndm:jan|dm:feb	The date modified is in January or february\n\nFunction Syntax:\nfunction:n	Equal to n within the specified granularity\nfunction:&lt;=n	Less than or equal to n\nfunction:=n	Exactly equal to n\nfunction:&gt;=n	Greater than or equal to n\nfunction:!=n	Not equal to n\nfunction:!n	Not equal to value with specified granularity\nfunction:start..end	Range from start to end (inclusive)\nfunction:start-end	Same as above\n\nhttps://www.voidtools.com/en-us/support/everything/command_line_interface/\nhttps://www.voidtools.com/support/everything/command_line_options/\nhttps://github.com/TheZoc/EverythingDopus/\nIn cmd command line, you can search with: ed.exe regex:&quot;^[a-z]{10,15}\d+.*?$&quot; (need double quote)\nIn DOpus command bar, you can search with: ed text" type="edit" width="248" x="6" y="6">
				<languages>
					<language height="222" lang="chs" title="全部语法见Everything的菜单Help(帮助)/Search Syntax(搜索语法)\n\n空格=与                |=或                !=非\nab*:以ab开头	*ab:以ab结尾\next:jpg;png         限定扩展名, 分号间隔多个\ndm:修改日期          dc:创建日期          da:访问日期\ncase:	匹配大小写\nregex:	匹配正则表达式\nregex:\p{Han}  正则匹配汉字, 也可用[一-龟] [一-龙] [一-龥] [\u4e00-\u9fff]\n现在Unicode已收录10万汉字，应包括[\u3400-\u4dbf\u4e00-\u9fff\uf900-\ufaff\x{20000}-\x{3134a}]\n[\u2E80-\u9FFF]	包括中日韩字符\n[^x00-xff]		双字节字符(包括汉字在内)\nEverythingOpus支持斜杠内匹配正则 /^[a-z]{10,15}\d+.*?$/\n\n类型过滤宏：\nfile: folder: audio: video: pic: zip: doc: exe:\n\ncontent:搜索文本内容    utf8content:可搜索docx内容\n\n通配符:\n*:  匹配 0 个或多个字符(不含\)          **:  (含\)\n?:  匹配1个字符(不含\)\n\n指定根基文件夹: \nancestor:完整路径  ancestor-name:文件夹名   指定基文件夹下的所有层级项\nparent: parent+2:完整路径    指定文件夹内的一级子项/n级子项\nparent-name:文件夹名       父文件夹名包含指定字符\nparent-path:&quot;:\Program Files\&quot;     路径任意部分包含指定字符\nparent-size:>100mb     限定父文件夹大小\nhttps://www.voidtools.com/forum/viewtopic.php?f=12&t=10176#ancestor-name\n\nwfn:	匹配完整文件名\nww:	匹配单词\nstartwith:开头          endwith:结尾\nname:	限定文件名\nstem:	限定文件名(忽略扩展名)\nlen:&gt;120	限定文件名长度大于120个字符\npath:	匹配路径\ndepth:	路径深度 (ev1.4用parents:)\ndupe:	搜索指定属性有重复值的文件(默认是文件名)\nfile: dupe: dupe:size size:&gt;200mb   大于200mb且大小相同的同名文件\n兼容ev1.4的函数 sizedupe: dadupe: dcdupe: dmdupe: attribdupe:\ncontent:&lt;abc 123&gt;	文件内容中含有abc与123\ncount:	限定结果数量\nfilelist:	从列表文件搜索\nattrib:hr	属性为隐藏&amp;只读(有点慢)\n\n文件大小限定\nempty:		空文件夹\nsize:empty	空文件\nsize:&gt;=3gb\nsize:400mb-1gb\nsize:&lt;1kb\n\n日期限定\ndm:last7days	最近7天内\ndm:last30mins	最近30分钟内\ndm:&lt;202305	2023年5月之前\ndm:2023.1.6..2023.1.8	匹配日期段\ndm:20230312t18:12-20230312t18:59	精确到分钟\n1-12月份	jan|feb|mar|apr|may|jun|jul|aug|sep|oct|nov|dec\ndm:jan|dm:feb	限定修改时间在1月或2月份\n\n函数值的表达式:\nfunction:n	等于n(单位精度内)\nfunction:&lt;=n	小于等于n\nfunction:=n	精确等于n\nfunction:&gt;=n	大于等于n\nfunction:!=n	不等于n\nfunction:!n	不等于n(单位精度外)\nfunction:start..end	起止范围(闭区间)\nfunction:start-end	同上\n\nhttps://www.voidtools.com/en-us/support/everything/command_line_interface/\nhttps://www.voidtools.com/support/everything/command_line_options/\nhttps://github.com/TheZoc/EverythingDopus/\n直接用cmd命令行搜索正则式：\ned.exe regex:&quot;^[a-z]{10,15}\d+.*?$&quot; （必须双引号）\ned.exe /^[a-z]{10,15}\d+.*?$/  这样是有问题的\n也可在DOpus命令栏输入ed text来搜索" width="248" x="6" y="6" />
				</languages>
			</control>
		</dialog>
	</resource>
	<resource type="strings">
		<strings lang="english">
			<string id="cmdDesc_dialog">Display the search dialog for EverythingOpus</string>
			<string id="cmdDesc_ed">Search given string on Everything and add results to a Directory Opus collection</string>
			<string id="CountTip">Due to the count limitation setting, there may be more files that are not included in the new file collection.</string>
			<string id="DEBUG">Set DEBUG flag to true in order to enable logging messages to the Opus Output Window.</string>
			<string id="DlgTitle">Enter → Search current folder / Shift+Enter  → Search all drives    -  EverythingOpus</string>
			<string id="EverythingFailedRun">Everything is not running or The path of .exe is invalid. Please launch Everything first.</string>
			<string id="Exclusion">Set the path or string to exclude.\n(such as Recycle Bin：!?:\$RECYCLE*\, Collection: !ext:cct;col, Shortcut: !ext:lnk)</string>
			<string id="InvalidPath">No valid path specified for Everything exe.</string>
			<string id="LaunchFailed">Failed to start Everything. If the path of alias /Everything.exe is invalid, please add a valid path and restart DOpus.
See [Preferences/Miscellaneous/Advanced] &lt;Behavior&gt;, set a valid path for everything_autolaunch.
/Everything.exe -startup    (alias is supported here)</string>
			<string id="Limitation">Limit the number of count in the search result.  0 = unlimited.</string>
			<string id="LocateFile">Please locate the executable file</string>
			<string id="NoFiles">Search result: 0 files                                             </string>
			<string id="NoResult">No Files Found</string>
			<string id="NoSearchString">No Search String inputted</string>
			<string id="ExclusionExist">Exclusion filter string: </string>
			<string id="OK">&amp;OK</string>
			<string id="prefixDate">whether to prefix the collection name with the current date and time</string>
			<string id="Sortby">For ed command. It should be one of the EVERYTHING_IPC_SORT_xxx constants (the numeric value).
1=Name_Asc  2=Name_Desc;  3=Path_Asc  4=Path_Desc;  5=Size_Asc  6 = Size_Desc
11=Created_Asc  12=Created_Desc;  13=Modified_Asc  14=Modified_Desc;  19=Run_Count_Asc  20=Run_Count_Desc</string>
			<string id="VersionRqst">Some arguments doesn&apos;t support Everything v1.4-, please launch ev1.5+. DOpus may need to be restarted.</string>
		</strings>
		<strings lang="chs">
			<string id="cmdDesc_dialog">显示 EverythingOpus 搜索输入对话框</string>
			<string id="cmdDesc_ed">用 Everything 搜索给定文本，并将结果添加到 DOpus 文件收集</string>
			<string id="CountTip">由于设置了结果数量限制，可能还有一些文件未加入新建的文件收集中.</string>
			<string id="DEBUG">将DEBUG标志设置为true，以便将消息记录到Opus输出窗口</string>
			<string id="DlgTitle">Enter → 搜当前路径 / Shift+Enter  → 全盘搜索    -  EverythingOpus</string>
			<string id="EverythingFailedRun">Everything未启动 或 其exe路径无效. 请先启动Everything程序</string>
			<string id="Exclusion">设置要排除的路径或字符串\n(如 回收站：!?:\$RECYCLE*\, 文件收集：!ext:cct;col, 快捷方式：!ext:lnk)</string>
			<string id="InvalidPath">未指定有效的 Everything exe 路径.</string>
			<string id="LaunchFailed">未能启动Everything. 若别名/Everything.exe路径无效, 添加有效路径后需要重启DOpus. 
请在【选项/杂项/高级】&lt;行为&gt; everything_autolaunch 配置Ev程序的exe路径:
/Everything.exe -startup    (支持别名路径)</string>
			<string id="Limitation">限制搜索结果的数量. 0 = 不限制</string>
			<string id="LocateFile">请定位可执行文件</string>
			<string id="NoFiles">搜索结果：0 文件                                                 </string>
			<string id="NoResult">未搜索到文件</string>
			<string id="NoSearchString">未输入搜索内容</string>
			<string id="ExclusionExist">存在过滤排除：</string>
			<string id="OK">确定(&amp;O)</string>
			<string id="prefixDate">是否以当前日期时间作为收集名称的前缀</string>
			<string id="Sortby">用于 ed 命令对结果排序. 其值应为 EVERYTHING_IPC_SORT_xxx 常数值.\n1=名称_升序  2=名称_降序;  3=路径_升序  4=路径_降序;  5=大小_升序  6 = 大小_降序\n11=创建时间_升序  12=创建时间_降序;  13=修改时间_升序  14=修改时间_降序;  19=运行次数_升序  20=运行次数_降序</string>
			<string id="VersionRqst">部分参数不支持 Everything v1.4-, 请启动ev1.5+后 重启DOpus.</string>
		</strings>
	</resource>
</resources>
