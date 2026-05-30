@include inc_commonSAL.js
@include inc_Logger.js
@include inc_MsgLoopHandler.js
// CommonLogger
// (c) 2024 Stephane

// This is a script for Directory Opus.
// See https://www.gpsoft.com.au/endpoints/redirect.php?page=scripts for development information.

loggerScriptVersion = "0.9.2";

// Called by Directory Opus to initialize the script
function OnInit(initData)
{
	initData.name = "CommonLogger";
	initData.version = loggerScriptVersion;
	initData.copyright = "(c) 2024 Stephane";
//	initData.url = "https://resource.dopus.com/c/buttons-scripts/16";
	initData.desc = "";
	initData.default_enable = true;
	initData.min_version = "13.19";
	initData.Group = "0 - Custom Scripts [SAL]";

	// if (!DOpus.Vars.Exists("CommonLoggerConfig")) {
	// 	DOpus.Output("No global config variable for CommonLogger found. Need to create one.");
	// 	globalConf = new LoggerNS.LoggerConfiguration();
	// 	DOpus.Vars.Set("CommonLoggerConfig", globalConf);
	// }
	// else {
	// 	globalConf = DOpus.Vars.Get("CommonLoggerConfig"); // usefull ??
	// }
	// DOpus.Vars("CommonLoggerConfig").persist = true;

	if (!DOpus.Vars.Exists("CLogger_Debug")) DOpus.Vars.Set("CLogger_Debug", false);
	DOpus.Vars("CLogger_Debug").persist = true;


	// Script configuration
	// settings & defaults
	initData.config = DOpus.Create().OrderedMap();
	initData.config_desc = DOpus.Create().OrderedMap();
	initData.config_groups = DOpus.Create().OrderedMap();
	initData.config_group_order = DOpus.NewVector('Old Logs Management', 'Icons');

	var option_name = "";
	var option_group = "";

	// ===
	option_group = 'Old Logs Management';

	option_name = "Log To Console";
	initData.Config[option_name] = true;
	initData.config_desc(option_name) = "Indicates wether the ManageOldLogFiles command will log to console or not.\nNote this command is automatically called when attempting to log for the first time in a script execution.";
	initData.config_groups(option_name) = option_group;

	option_name = "Log To File";
	initData.Config[option_name] = true;
	initData.config_desc(option_name) = "Indicates wether the ManageOldLogFiles command will log to a file or not.\nNote this command is automatically called when attempting to log for the first time in a script execution.";
	initData.config_groups(option_name) = option_group;

	// ===
	option_group = 'Icons';

	option_name = "Icon for 'Create New Config' button";
	initData.Config[option_name] = "#cLogger_add";
	initData.config_desc(option_name) = "Filename with path or internal icon name (e.g. #about for the internal about icon)";
	initData.config_groups(option_name) = option_group;

	option_name = "Icon for 'Delete Config' button";
	initData.Config[option_name] = "#cLogger_delete";
	initData.config_desc(option_name) = "Filename with path or internal icon name (e.g. #about for the internal about icon)";
	initData.config_groups(option_name) = option_group;

	option_name = "Icon for 'Duplicate Config' button";
	initData.Config[option_name] = "#cLogger_clone";
	initData.config_desc(option_name) = "Filename with path or internal icon name (e.g. #about for the internal about icon)";
	initData.config_groups(option_name) = option_group;

	option_name = "Icon for 'Export Config' button";
	initData.Config[option_name] = "#cLogger_export_arrow";
	initData.config_desc(option_name) = "Filename with path or internal icon name (e.g. #about for the internal about icon)";
	initData.config_groups(option_name) = option_group;

	option_name = "Icon for 'Import Config' button";
	initData.Config[option_name] = "#cLogger_import_arrow";
	initData.config_desc(option_name) = "Filename with path or internal icon name (e.g. #about for the internal about icon)";
	initData.config_groups(option_name) = option_group;

	option_name = "Icon for 'Close' button";
	initData.Config[option_name] = "#cLogger_close2";
	initData.config_desc(option_name) = "Filename with path or internal icon name (e.g. #about for the internal about icon)";
	initData.config_groups(option_name) = option_group;
}

// Called to add commands to Opus
function OnAddCommands(addCmdData)
{
	var cmd = addCmdData.AddCommand();
	cmd.name = "ConfigLogger";
	cmd.method = "OnConfigLogger";
	cmd.desc = "";
	cmd.label = "ConfigLogger";
	cmd.template = "";
	cmd.hide = false;
	cmd.icon = "script";

	var cmd = addCmdData.AddCommand();
	cmd.name = "ManageOldLogFiles";
	cmd.method = "OnManageOldLogFiles";
	cmd.desc = "";
	cmd.label = "ManageOldLogFiles";
	cmd.template = "SCRIPT_PATH/K,SCRIPT_NAME/K,ARCHIVE_DELAY/N,DELETE_DELAY/N";
	cmd.hide = true;
	cmd.icon = "script";

	var cmd = addCmdData.AddCommand();
	cmd.name = "CLogger_Private_DeleteGlobalConfig";
	cmd.method = "OnPrivate_DeleteGlobalConfig";
	cmd.desc = "";
	cmd.label = "Priavte CLogger command to reset the default configuration. Deletes the configuration that will be recreated on first logger creation.";
	cmd.template = "";
	cmd.hide = false;
	cmd.icon = "script";

	var cmd = addCmdData.AddCommand();
	cmd.name = "CLogger_Private_SetMode";
	cmd.method = "OnPrivate_SetMode";
	cmd.desc = "";
	cmd.label = "Private CLogger command to set the CommonLogger to debug mode. Forces script reload on GUI Configuration (in case inc_logger.js has been modifier).";
	cmd.template = "DEBUG/S";
	cmd.hide = false;
	cmd.icon = "script";
}

// Called to add pages to the system configuration dialog
function OnAddConfigPages(addConfigPagesData) {

	var dlg = DOpus.Dlg;
	dlg.template ="cfgConfigPage";
	dlg.detach = true;
	dlg.create();
	dlg.AddConfigPages(addConfigPagesData);

	dlg.control('cbGlobalMinLevel', 'dlgConfig').value = LoggerNS._GetGlobalConsoleMinLevelStatus();
	dlg.control('lcMinLevel', 'dlgConfig').value = LoggerNS._GetGlobalConsoleMinLevel();

	while (true) {
		var msg = dlg.GetMsg();
		dout("event = " + msg.event + " | Control = " + msg.Control + " | result = " + msg.result);
		if (!msg.result) break;

		if (msg.event == 'click' && msg.control == 'btOpenConfig') {
			DOpus.Create.Command().RunCommand('ConfigLogger');
		}

		if (msg.event == 'click' && msg.control == 'cbGlobalMinLevel') {
			dlg.control('lcMinLevel', 'dlgConfig').enabled = dlg.control('cbGlobalMinLevel', 'dlgConfig').value;
			LoggerNS._SetGlobalConsoleMinLevelStatus(dlg.control('cbGlobalMinLevel', 'dlgConfig').value);
			LoggerNS._SetGlobalConsoleMinLevel(dlg.control('lcMinLevel', 'dlgConfig').value);
		}

		if (msg.event == 'selchange' && msg.control == 'lcMinLevel') {
			LoggerNS._SetGlobalConsoleMinLevel(dlg.control('lcMinLevel', 'dlgConfig').value);
		}
	}
}


// Implement the ArchiveOldLogFile command
function OnManageOldLogFiles(scriptCmdData) {
	var loggerCfg = new LoggerNS.LoggerConfiguration();

	if (Script.config["Log To Console"]) {
		var consoleLog = new LoggerNS.LoggerTarget("ConfigLogger console", LoggerNS.LogLevel.Trace, LoggerNS.LogLevel.Fatal, LoggerNS.TargetType.Console);
		consoleLog.layout.IncludeDateTime = false;
		loggerCfg.AddTarget(consoleLog);
	}

	if (Script.config["Log To File"]) {
		var fileLog = new LoggerNS.LoggerTarget("ConfigLogger console", LoggerNS.LogLevel.Trace, LoggerNS.LogLevel.Fatal, LoggerNS.TargetType.File, undefined, "OldLogsManager");
		loggerCfg.AddTarget(fileLog);
	}

	clLogger = new LoggerNS.Logger(loggerCfg);
	if (!Script.config["Log To Console"] && !Script.config["Log To File"])
		clLogger.disabled = true;


	var cdf = scriptCmdData.func;
	if (!cdf.args.got_arg.script_path || !cdf.args.got_arg.script_name || !cdf.args.got_arg.archive_delay || !cdf.args.got_arg.delete_delay) {
		clLogger.Fatal("Can not manage old log files, at least one of the arguments is missing.", true, true);
		return;
	}
	// else
	var scriptPath 		= cdf.args.script_path;
	var scriptName 		= cdf.args.script_name;
	var archiveDelay	= cdf.args.archive_delay;
	var deleteDelay		= cdf.args.delete_delay;

	var varManageOldLogFilesInProgress = "ManageOldLogFilesInProgress";
	if (!Script.Vars.Exists(varManageOldLogFilesInProgress)) {
		Script.Vars(varManageOldLogFilesInProgress) = DOpus.Create.Map();
	}
	var wipManagers = Script.Vars.Get(varManageOldLogFilesInProgress);
	if (wipManagers.Exists(scriptName)) {
		// Return before trying to log anything, otherwise we'll enter an infinite loop
		// Trying to log will trigger this method, which will try do its job and to log which will trigger this method ... and so on
		// dout("DEBUG : EXITING : Work is already in progress for '" + scriptName + "' !");
		return;	// the same job is already in progress
	}
	else wipManagers(scriptName) = "job in progress";


	clLogger.info("Managing old log files for '" + scriptPath + "' / '" + scriptName + "' - Archive = " + archiveDelay + " - Delete = " + deleteDelay);

	var todayGDH = clLogger.LoggerGetDateHour();
	var todayAsNum = + (todayGDH.year + todayGDH.month + todayGDH.day);
	clLogger.debug("Today as number = " + todayAsNum + " / " + typeof todayAsNum);
	var archiveDateMax	= new Date(todayGDH.year, todayGDH.month - 1 , todayGDH.day);
	var deleteDateMax	= new Date(todayGDH.year, todayGDH.month - 1, todayGDH.day);
	archiveDateMax.setDate(archiveDateMax.getDate() - archiveDelay);
	deleteDateMax.setDate(deleteDateMax.getDate() - deleteDelay);

	var archiveMax 	= +( "" + archiveDateMax.getFullYear() + (""+(archiveDateMax.getMonth()+1)).padStart(2, '0') + (""+archiveDateMax.getDate()).padStart(2, '0'));
	var deleteMax 	= +( "" + deleteDateMax.getFullYear() + (""+(deleteDateMax.getMonth()+1)).padStart(2, '0') + (""+deleteDateMax.getDate()).padStart(2, '0'));

	clLogger.info("Archive Max = " + archiveMax + " / Delete Max = " + deleteMax);

	// Build regex to look for this script files in folder
	// target.fullLogfilePath = target.logPath + "\\" + target.logFilename + " - " + this.formatFileDate(timestamp) + " - " + this.Id + ".log";
	// var re = new RegExp( "^" + scriptName + " - (\d{4})-(\d{2})-(\d{2}) - \d+\.log$");
	var re = new RegExp( "^" + scriptName + " - ([0-9]{4})-([0-9]{2})-([0-9]{2}) - [0-9]+\.(log|zip)$");
	clLogger.debug("regexp = '" + re.toString() + "'");

	var itemsToArchive 	= DOpus.Create.Vector();
	var itemsToDelete 	= DOpus.Create.Vector();

	var dirIter = DOpus.FSUtil.ReadDir(DOpus.FSUtil.Resolve(scriptPath));
	while (!dirIter.complete) {
		var item = dirIter.next();
		//clLogger.trace("Found 1 file : '" + item.name);
		var resRE = item.name.match(re);
		if (resRE != null) {
			//clLogger.debug("Item matches : '" + resRE.join("~"));
			var itemDate = +(resRE[1]+resRE[2]+resRE[3]);
			if (itemDate <= deleteMax) {
				itemsToDelete.push_back(item);
				clLogger.info("Need to delete : " + item.name);
			}
			else if (itemDate <= archiveMax && resRE[4] != "zip") {
				itemsToArchive.push_back(item);
				clLogger.info("Need to archive : " + item.name);
			}
		}
		// else clLogger.trace("item does not match");
	}

	clLogger.info(itemsToDelete.length + " items to delete");
	clLogger.info(itemsToArchive.length + " items to archive");

	var cmdArchive = DOpus.Create.Command();
	cmdArchive.AddFiles(itemsToArchive);
	cmdArchive.AddLine('Copy ARCHIVE=single MOVE TO "' + DOpus.FSUtil.Resolve(scriptPath) + '"');
	cmdArchive.Run();
	clLogger.info("End of logs archiving");

	var cmdDelete = DOpus.Create.Command();
	cmdDelete.AddFiles(itemsToDelete);
	cmdDelete.AddLine("Delete NORECYCLE QUIET");
	cmdDelete.Run();
	clLogger.info("End of archive & logs deletion");

	wipManagers.erase(scriptName);	// Now we're ok to manage old log files for this script again
	clLogger.info("End of old logs management.");
}

// Implement the Private_DeleteGlobalConfig command
function OnPrivate_DeleteGlobalConfig(scriptCmdData) {
	DOpus.Output("Private_DeleteGlobalConfig called");
	if (!DOpus.Vars.Exists("CommonLoggerConfig"))
		DOpus.Output("No global config variable for CommonLogger found. No need to delete one.");
	else
		DOpus.Vars.Delete("CommonLoggerConfig");
}

function OnPrivate_SetMode(scriptCmdData) {
	DOpus.Output("Private_SetMode called. Debug = " + scriptCmdData.func.args.got_arg.debug);
	DOpus.Vars.Set("CLogger_Debug", scriptCmdData.func.args.got_arg.debug);
	DOpus.Vars("CLogger_Debug").persist = true;


	// TMP -- Used to clear a remaining ActivateOrCreateLister job in the list which was not cleared at then end of execution.(was still wip)
	// dout("Clearing Script ");
	// // ActivateOrCreateLister
	// var varManageOldLogFilesInProgress = "ManageOldLogFilesInProgress";
	// var wipManagers = Script.Vars.Get(varManageOldLogFilesInProgress);
	// if (wipManagers.Exists("ActivateOrCreateLister")) {
	// 	// Return before trying to log anything, otherwise we'll enter an infinite loop
	// 	// Trying to log will trigger this method, which will try do its job and to log which will trigger this method ... and so on
	// 	dout("DEBUG : CLEARING !");
	// 	wipManagers.erase("ActivateOrCreateLister");
	// 	return;	// the same job is already in progress
	// }

}


// Implement the ConfigLogger command
function OnConfigLogger(scriptCmdData) {
	if (DOpus.Vars.Exists("CLogger_Debug") && DOpus.Vars.Get("CLogger_Debug")) {
		DOpus.Output("CommonLogger in debug mode. Reloading.");
		DOpus.ReloadScript(Script.file);
	}

	var cdf = scriptCmdData.func;
	var fsu = DOpus.fsUtil;
	// /!\ Global variable to avoid multiple instanciation (1 for each preview rendering)
	previewLogger = new LoggerNS.Logger();


	var consoleLog = new LoggerNS.LoggerTarget("ConfigLogger console", LoggerNS.LogLevel.Trace, LoggerNS.LogLevel.Fatal, LoggerNS.TargetType.Console);
	consoleLog.layout.IncludeDateTime = false;
	try {
		var existOrFail = DOpus.aliases("cLoggerPath");	// exception if it is not defined ==> goes to catch
		var fileLog = new LoggerNS.LoggerTarget("ConfigLogger file", LoggerNS.LogLevel.Trace, LoggerNS.LogLevel.Fatal, LoggerNS.TargetType.File, "/cLoggerPath", "ConfigLogger");
		fileLog.archiveDelay = 10;
		fileLog.deleteDelay = 400;
		clLogger = new LoggerNS.Logger(consoleLog, fileLog);
	}
	catch(e) {
		clLogger = new LoggerNS.Logger(consoleLog);
	}
	clLogger.info("CommonLogger Template Configuration - Start");

	// Test if global var CommonLoggerConfig exists. If not create it. If yes, check if targets need updating
	var allConfigs = new LoggerNS.LoggerConfiguration();
	
	allConfigs.Load();
	clLogger.info("CommonLoggerConfig exists (#" + allConfigs.targets.length  + " configs).");

	var dlg = DOpus.Dlg;
	dlg.window = cdf.sourcetab;
	dlg.template ="cfgDlg";
	dlg.detach = true;
	dlg.create();
	//dlg.AddCustomMsg("reloadInProgress");

	var tabsMap = DOpus.Create.Map();	// translate colors tab names to target.colors names
	tabsMap("dlgDate") 		= "date";
	tabsMap("dlgId") 		= "id";
	tabsMap("dlgCallsite")	= "caller";
	tabsMap("dlgLevel")		= "level";
	tabsMap("dlgMessage")	= "msg";
	var dialogsMap = DOpus.Create.Map();
	for (var e = new Enumerator(tabsMap); !e.atEnd(); e.moveNext())
		dialogsMap(tabsMap(e.item())) = e.item();
	tabsNames = ["date", "id", "caller", "level", "msg" ];	// used to track selected tab association with target.colors names

	var levelsMap = DOpus.Create.Map();
	for (var l = LoggerNS.LogLevel.Trace, i = 0; l <= LoggerNS.LogLevel.Fatal; l++, i++)
		levelsMap(i) = l;


	LoadButtonImage(dlg, "btNewCfg", "Icon for 'Create New Config' button");
	LoadButtonImage(dlg, "btDeleteCfg", "Icon for 'Delete Config' button");
	LoadButtonImage(dlg, "btDuplicateCfg", "Icon for 'Duplicate Config' button");
	LoadButtonImage(dlg, "btExportCfg", "Icon for 'Export Config' button");
	LoadButtonImage(dlg, "btImportCfg", "Icon for 'Import Config' button");
	LoadButtonImage(dlg, "btClose", "Icon for 'Close' button");
	// ---- ---- ---- ----

	// Init UI
	allConfigs.targets.sort();
	// Build UIcontext object from : dlg, current target, current targetIndex, creationInProgress status, modified status
	//var uiCtxt = new UIcontext(dlg, allConfigs.targets(0).Clone(), 0, false, false);
	var uiCtxt = new UIcontext(dlg, allConfigs, tabsMap);
	uiCtxt.targetIndex = 0;
	uiCtxt.target = uiCtxt.allConfigs.targets(uiCtxt.targetIndex).Clone();
	uiCtxt.tabName = tabsNames[0];
	uiCtxt.tabsNamesArray = tabsNames;
	uiCtxt.dialogsMap = dialogsMap;

	RefreshTargetsList(dlg.control("lcTargets"), uiCtxt.allConfigs, uiCtxt.target.name);

	// Load first target
	LoadTarget(uiCtxt.target, uiCtxt.tabsMap, dlg);

	var msgLoopHandler = new MsgLoopHandler(dlg, uiCtxt, OnDialogClosed, null, /*debug*/false, /*listHandlersOnStart*/false);
	msgLoopHandler.AddEventHandler('btNewCfg',				"click",		cb_CreateNewTarget);
	msgLoopHandler.AddEventHandler("btDeleteCfg",			"click",		cb_DeleteTarget);
	msgLoopHandler.AddEventHandler("btDuplicateCfg",		"click",		cb_DuplicateTarget);
	msgLoopHandler.AddEventHandler("lcTargets",				"selchange",	cb_SelectedTargetChange);
	msgLoopHandler.AddEventHandler("btSaveCfg",				"click",		cb_SaveTarget);
	msgLoopHandler.AddEventHandler("btCancelCfg",			"click",		cb_CancelTargetModifications);
	msgLoopHandler.AddEventHandler("btExportCfg",			"click",		cb_ExportConfigs);
	msgLoopHandler.AddEventHandler("btImportCfg",			"click",		cb_ImportConfigs);
	msgLoopHandler.AddEventHandler("btClose",				"click",		cb_CloseWindow);
	msgLoopHandler.AddEventHandler("edName",				"focus",		cb_EditTargetName);
	msgLoopHandler.AddEventHandler("lcType",				"selchange",	cb_EditTargetType);
	msgLoopHandler.AddEventHandler("edLogPath",				"focus",		cb_EditTargetLogPath);
	msgLoopHandler.AddEventHandler("btLocate",				"click",		cb_LocateLogPath);
	msgLoopHandler.AddEventHandler("lcMinLevel",			"selchange",	cb_MinLevelChange);
	msgLoopHandler.AddEventHandler("lcMaxLevel",			"selchange",	cb_MaxLevelChange);
	msgLoopHandler.AddEventHandler("edSeparator",			"focus",		cb_EditSeparator);
	msgLoopHandler.AddEventHandler("eArchiveDelay",			"editchange",	cb_DelayChanged);
	msgLoopHandler.AddEventHandler("eDeletehDelay",			"editchange",	cb_DelayChanged);
	msgLoopHandler.AddEventHandler("cbDefaultCfg",			"click",		cb_SetDefaultTarget);
	msgLoopHandler.AddEventHandler("cbIncDateHour",			"click",		cb_SetLayoutIncludeProperty);
	msgLoopHandler.AddEventHandler("cbIncCallSite",			"click",		cb_SetLayoutIncludeProperty);
	msgLoopHandler.AddEventHandler("cbIncInstanceId",		"click",		cb_SetLayoutIncludeProperty);
	msgLoopHandler.AddEventHandler("cbIncLogLevel",			"click",		cb_SetLayoutIncludeProperty);
	msgLoopHandler.AddEventHandler("cbCallsiteFixedLength",	"click",		cb_SetLayoutIncludeProperty);
	msgLoopHandler.AddEventHandler("eCallSiteLength",		"editchange",	cb_CallsiteLengthChanged);
	msgLoopHandler.AddEventHandler("cbLevelFixedLength",	"click",		cb_SetLayoutIncludeProperty);
	msgLoopHandler.AddEventHandler("cbUseColors",			"click",		cb_SetUseColors);
	msgLoopHandler.AddEventHandler("tabColors",				"selchange",	cb_TabColorsPageChange);
	for (var i = LoggerNS.LogLevel.Trace; i <= LoggerNS.LogLevel.Fatal; i++) {
		var text = "palText" + LoggerNS.LogLevel[i];
		var bg = "palBg" + LoggerNS.LogLevel[i];
		clLogger.debug("Registering color event for controls '" + text + "' and '" + bg + "'");
		msgLoopHandler.AddEventHandler(text,			"color",		cb_ColorChange);
		msgLoopHandler.AddEventHandler(bg,				"color",		cb_ColorChange);
	}
	dlg.title = "Common Logger Configuration Manager";
	dlg.Show();
	msgLoopHandler.Loop();

	clLogger.info("********** END OF MSG LOOP *****************");
}

// ===========================================================================
// ---------------------------------------------------------------------------
// Event handlers
// ---------------------------------------------------------------------------
// ===========================================================================


	// OnDialogClosed
	// --------------------------------
function OnDialogClosed(dlg) {
	clLogger.info("The dialog '" + dlg.title + "' has been closed.");
}

		// ------------------------------------------------
		// Top most actions == Global actions on target
		// --
		// ------------------------------------------------

	// CreateNewTarget
	// --------------------------------
function cb_CreateNewTarget(dlg, msg, uiCtxt) {
	var saveStatus = ManageSaveIfRequired(uiCtxt);
	if (saveStatus == "cancelled") return;	// Nothing else to do. Resume editing. Everything else is in place
	// else
	uiCtxt.creationInProgress = true;
	// Get New Target Name
	var name = GetNewTargetName(uiCtxt.allConfigs, "New Target");
	var newCfg = new LoggerNS.LoggerTarget(name, LoggerNS.LogLevel.Trace, LoggerNS.LogLevel.Fatal, LoggerNS.TargetType.Console);
	uiCtxt.allConfigs.AddTarget(newCfg);
	uiCtxt.target = newCfg.Clone();
	uiCtxt.targetIndex = uiCtxt.allConfigs.targets.length - 1;
	clLogger.debug("New target index = " + uiCtxt.targetIndex);
	LoadTargetAndRefreshUI(uiCtxt, true);
}

	// Delete target
	// --------------------------------
function cb_DeleteTarget(dlg, msg, uiCtxt) {
	var newIndex = Math.max(0, uiCtxt.targetIndex-1);
	clLogger.debug("new index after delete = " + newIndex);
	uiCtxt.allConfigs.targets.erase(uiCtxt.targetIndex);
	uiCtxt.targetIndex = newIndex;
	uiCtxt.target = uiCtxt.allConfigs.targets[uiCtxt.targetIndex].Clone();
	LoadTargetAndRefreshUI(uiCtxt, false);
	uiCtxt.allConfigs.SavePersist();
}

	// Duplicate target
	// --------------------------------
function cb_DuplicateTarget(dlg, msg, uiCtxt) {
	uiCtxt.creationInProgress = true;
	// Get New Target Name
	var name = GetNewTargetName(uiCtxt.allConfigs, uiCtxt.target.name);
	var newCfg = uiCtxt.target.Clone();
	newCfg.name = name;
	uiCtxt.allConfigs.AddTarget(newCfg);
	uiCtxt.target = newCfg.Clone();
	uiCtxt.targetIndex = uiCtxt.allConfigs.targets.length - 1;
	clLogger.debug("New target index = " + uiCtxt.targetIndex);
	LoadTargetAndRefreshUI(uiCtxt, true);
}

	// Selected Target Changed
	// --------------------------------
function cb_SelectedTargetChange(dlg, msg, uiCtxt) {
	var saveStatus = ManageSaveIfRequired(uiCtxt);
	if (saveStatus == "cancelled") {	// We stay on the current 'in progress' editing
		dlg.control("lcTargets").value = uiCtxt.targetIndex;
		clLogger.debug("Reset lcTargets.value to " + uiCtxt.targetIndex);
		return;	// Nothing else to do. Resume editing. Everything else is in place
	}
	else if (["saved", "discard", "proceed"].contains(saveStatus)) {
		var newTargetName = uiCtxt.allConfigs.targets[msg.index].name;
		uiCtxt.allConfigs.targets.sort();
		var newIndex = uiCtxt.allConfigs.getFirstIndex(newTargetName);
		uiCtxt.targetIndex = newIndex;
		uiCtxt.target = uiCtxt.allConfigs.targets[uiCtxt.targetIndex].Clone();
		uiCtxt.creationInProgress = false;	// Reinit values
		LoadTargetAndRefreshUI(uiCtxt, false);
	}
}


		// ------------------------------------------------
		// Bottom actions == Global actions on target
		// --
		// ------------------------------------------------

	// Save the target
	// --------------------------------
function cb_SaveTarget(dlg, msg, uiCtxt) {
	clLogger.info("Saving current target");
	if (SaveTarget(uiCtxt) == "cancelled") return;
	// else
	var newTargetName = uiCtxt.target.name;
	uiCtxt.allConfigs.targets.sort();
	var newIndex = uiCtxt.allConfigs.getFirstIndex(newTargetName);
	uiCtxt.targetIndex = newIndex;
	LoadTargetAndRefreshUI(uiCtxt, false);
}

	// Discard modification on target
	// --------------------------------
function cb_CancelTargetModifications(dlg, msg, uiCtxt) {
	if (!uiCtxt.modified) return;
	clLogger.info("Discarding modifications to the current target");
	clLogger.debug("  Status : modified = " + uiCtxt.modified + " / creationInProgress = " + uiCtxt.creationInProgress);
	if (uiCtxt.creationInProgress) {	// We need to cleanup the configs (note: they are unsaved).
		uiCtxt.allConfigs.targets.erase(uiCtxt.targetIndex);
		uiCtxt.targetIndex = Math.max(0, uiCtxt.targetIndex-1);
		RefreshTargetsList(dlg.control("lcTargets"), uiCtxt.allConfigs, uiCtxt.allConfigs.targets[uiCtxt.targetIndex].name);
	}

	uiCtxt.target = uiCtxt.allConfigs.targets[uiCtxt.targetIndex].Clone();
	uiCtxt.creationInProgress = false;	// Reinit values
	LoadTargetAndRefreshUI(uiCtxt, false);
}

	// Export all configurations to JSON file
	// --------------------------------
function cb_ExportConfigs(dlg, msg, uiCtxt) {
	clLogger.info("Exporting cfg");
	var msg = "Do you want to save modifications before exporting ?";
	msg += "\nIf you choose to ignore, all modifications will be lost"
	var saveStatus = ManageSaveIfRequired(uiCtxt, msg);
	if (saveStatus == "cancelled") return;
	// else
	var js = uiCtxt.allConfigs.ExportToJSON();
	//dout("allConfigs : " + js);
	var saveDlg = DOpus.Dlg.Save("Export all targets to JSON", "CommonLoggerCfg.json", dlg, "#JSON Files!*.json");
	clLogger.debug("save result = " + saveDlg.result);
	if (!saveDlg.result) {
		clLogger.info("Save operation cancelled");
		return;
	}
	var file = DOpus.FSUtil.OpenFile(saveDlg, "wa");
	file.Write(js);
}

	// Import all configurations from JSON file
	// --------------------------------
function cb_ImportConfigs(dlg, msg, uiCtxt) {
	WarningMsgDlg("You are about to replace the whole configuration", "If you proceed on the next dialog, all targets will be replaced", uiCtxt.dlg);

	var openDlg = DOpus.Dlg.Open("Import all targets from JSON", "CommonLoggerCfg.json", dlg, "#JSON Files!*.json");
	clLogger.debug("open result = " + openDlg.result);
	if (!openDlg.result) {
		clLogger.info("Open operation cancelled");
		return;
	}

	var file = DOpus.FSUtil.OpenFile(openDlg, "r");
	var content = ReadFile(file);
	clLogger.trace("Content read : " + content);

	var newCfg = new LoggerNS.LoggerConfiguration();
	newCfg.ImportFromJSON(content);
	clLogger.trace("read config = " + newCfg.ExportToJSON());
	uiCtxt.allConfigs = newCfg;
	uiCtxt.allConfigs.SavePersist();

	// Reset UI with these new configs
	uiCtxt.allConfigs.targets.sort();
	// Build UIcontext object from : dlg, current target, current targetIndex, creationInProgress status, modified status
	uiCtxt.targetIndex = 0;
	uiCtxt.target = uiCtxt.allConfigs.targets[uiCtxt.targetIndex].Clone();
	uiCtxt.creationInProgress = false;	// Reinit values
	LoadTargetAndRefreshUI(uiCtxt, false);
}

	// Close configuration window (after checking no editing is in progress)
	// --------------------------------
function cb_CloseWindow(dlg, msg, uiCtxt) {
	var saveStatus = ManageSaveIfRequired(uiCtxt, "Do you want to save modifications before quitting ?");
	if (saveStatus == "cancelled") return;	// Nothing else to do. Resume editing. Everything else is in place
	// else // => if (["proceed", "discard", "saved"].contains(saveStatus))
	else uiCtxt.dlg.EndDlg();
}


		// ------------------------------------------------
		// Target modifications actions
		// --
		// ------------------------------------------------

	// Name editing
	// --------------------------------
function cb_EditTargetName(dlg, msg, uiCtxt) {
	clLogger.debug("Name changed to : " + dlg.control("edName").value);
	if (dlg.control("edName").value != uiCtxt.target.name) {
		SetUiModificationState(uiCtxt, true);
		uiCtxt.target.name = dlg.control("edName").value;
	}
}

	// Change Target Type
	// --------------------------------
function cb_EditTargetType(dlg, msg, uiCtxt) {
	// 0 = Console | 1 = File
	SetUiModificationState(uiCtxt, true);
	uiCtxt.target.targetType = (uiCtxt.dlg.control("lcType").value == 0)?LoggerNS.TargetType.Console:LoggerNS.TargetType.File;
	uiCtxt.dlg.control("cbUseColors").enabled = (uiCtxt.dlg.control("lcType").value == 0);
	RefreshTargetType(uiCtxt.target, dlg);
	RefreshColorSettings(uiCtxt.target, dlg, uiCtxt.tabsMap);
	RefreshLogsPreview(uiCtxt.target, dlg);
	//dout("New type value = " + uiCtxt.dlg.control("lcType").value);
}

	// Edit target log path
	// --------------------------------
function cb_EditTargetLogPath(dlg, msg, uiCtxt) {
	if (uiCtxt.target.logPath != dlg.control("edLogPath").value) {
		SetUiModificationState(uiCtxt, true);
		clLogger.debug("target path changed to : " + dlg.control("edLogPath").value);
		uiCtxt.target.logPath = dlg.control("edLogPath").value;
	}
}

	// Locate target log path
	// --------------------------------
function cb_LocateLogPath(dlg, msg, uiCtxt) {
	var folder = DOpus.Dlg.Folder("Choose target Log Folder", uiCtxt.target.logPath, false, uiCtxt.dlg);
	if (folder.result == false) return;
	// else
	uiCtxt.dlg.control("edLogPath").value = folder;
	if (uiCtxt.target.logPath != dlg.control("edLogPath").value) {
		SetUiModificationState(uiCtxt, true);
		clLogger.debug("target path changed to : " + dlg.control("edLogPath").value);
		uiCtxt.target.logPath = dlg.control("edLogPath").value;
	}			
}

	// Change Min Level
	// --------------------------------
function cb_MinLevelChange(dlg, msg, uiCtxt) {
	clLogger.debug("MinLevel changed to : " + uiCtxt.dlg.control("lcMinLevel").value);
	// Compare to max level, can not be bigger
	if (uiCtxt.dlg.control("lcMinLevel").value > uiCtxt.target.maxLevel) {
		WarningMsgDlg("Min level value too high", "You can not set a min level higher than max level", uiCtxt.dlg);
		uiCtxt.dlg.control("lcMinLevel").value = uiCtxt.target.maxLevel;
	}
	if (uiCtxt.target.minLevel != LoggerNS.GetLogLevelValue(LoggerNS.LogLevel[uiCtxt.dlg.control("lcMinLevel").value])) {
		uiCtxt.target.minLevel = LoggerNS.GetLogLevelValue(LoggerNS.LogLevel[uiCtxt.dlg.control("lcMinLevel").value]);
		SetUiModificationState(uiCtxt, true);
		RefreshLogsPreview(uiCtxt.target, dlg);
	}
	//uiCtxt.target.minLevel = levelsMap(uiCtxt.dlg.control("lcMinLevel").value);
}

	// Change Max Level
	// --------------------------------
function cb_MaxLevelChange(dlg, msg, uiCtxt) {
	clLogger.debug("MaxLevel changed to : " + uiCtxt.dlg.control("lcMaxLevel").value);
	// Compare to min level, can not be smalle
	if (uiCtxt.dlg.control("lcMaxLevel").value < uiCtxt.target.minLevel) {
		WarningMsgDlg("Max level value too low", "You can not set a max level smaller than min level", uiCtxt.dlg);
		uiCtxt.dlg.control("lcMaxLevel").value = uiCtxt.target.minLevel;
	}
	if (uiCtxt.target.maxLevel != LoggerNS.GetLogLevelValue(LoggerNS.LogLevel[uiCtxt.dlg.control("lcMaxLevel").value])) {
		uiCtxt.target.maxLevel = LoggerNS.GetLogLevelValue(LoggerNS.LogLevel[uiCtxt.dlg.control("lcMaxLevel").value]);
		SetUiModificationState(uiCtxt, true);
		//uiCtxt.target.maxLevel = levelsMap(uiCtxt.dlg.control("lcMaxLevel").value);
		RefreshLogsPreview(uiCtxt.target, dlg);				
	}
}

	// Edit separator
	// --------------------------------
function cb_EditSeparator(dlg, msg, uiCtxt) {
	if (uiCtxt.target.layout.Separator != dlg.control("edSeparator").value) {
		clLogger.debug("Layout separator changed to : " + dlg.control("edSeparator").value);
		uiCtxt.target.layout.Separator = dlg.control("edSeparator").value;
		SetUiModificationState(uiCtxt, true);
		RefreshLogsPreview(uiCtxt.target, dlg);
	}
}

	// Edit Delays (Archive & Delete)
	// --------------------------------
function cb_DelayChanged(dlg, msg, uiCtxt) {
	clLogger.debug("Delay changed for control : " + msg.control + " / Value = " + msg.value);
	SetUiModificationState(uiCtxt, true);
	// Need to ensure archive delay <= delete delay
	if (msg.control == "eArchiveDelay") {
		uiCtxt.target.archiveDelay = + msg.value;
		if (uiCtxt.target.archiveDelay > uiCtxt.target.deleteDelay) dlg.control("eDeletehDelay").value = msg.value;
	}
	if (msg.control == "eDeletehDelay") {
		uiCtxt.target.deleteDelay = + msg.value;
		if (uiCtxt.target.archiveDelay > uiCtxt.target.deleteDelay) dlg.control("eArchiveDelay").value = msg.value;
	}
}

	// Edit CallSite Length
	// --------------------------------
function cb_CallsiteLengthChanged(dlg, msg, uiCtxt) {
	clLogger.debug("Callsite Fixed length changed for control : " + msg.control + " / Value = " + msg.value);
	SetUiModificationState(uiCtxt, true);
	uiCtxt.target.layout.CallSiteLength = + msg.value;
	RefreshLogsPreview(uiCtxt.target, dlg);
}


	// Set DefaultCfg property
	// --------------------------------
function cb_SetDefaultTarget(dlg, msg, uiCtxt) {
	clLogger.debug("Default Cfg ? " + uiCtxt.dlg.control("cbDefaultCfg").value);
	uiCtxt.target.defaultTarget = uiCtxt.dlg.control("cbDefaultCfg").value;
	SetUiModificationState(uiCtxt, true);
}

	// Set Layout IncludeXXX property
	// --------------------------------
function cb_SetLayoutIncludeProperty(dlg, msg, uiCtxt) {
	clLogger.debug("Layout modified for " + msg.control);
	switch(msg.control) {
		case "cbIncDateHour":
			uiCtxt.target.layout.IncludeDateTime = uiCtxt.dlg.control(msg.control).value;
			break;
		case "cbIncCallSite":
			uiCtxt.target.layout.IncludeCallsite = uiCtxt.dlg.control(msg.control).value;
			break;
		case "cbIncInstanceId":
			uiCtxt.target.layout.IncludeInstanceId = uiCtxt.dlg.control(msg.control).value;
			break;
		case "cbIncLogLevel":
			uiCtxt.target.layout.IncludeLogLevel = uiCtxt.dlg.control(msg.control).value;
			break;
		case "cbCallsiteFixedLength":
			uiCtxt.target.layout.FixedLengthCallSite = uiCtxt.dlg.control(msg.control).value;
			uiCtxt.target.layout.CallSiteLength = dlg.control("eCallSiteLength").value;
			dlg.control("eCallSiteLength").enabled = dlg.control("sCallsiteLength").enabled = uiCtxt.target.layout.FixedLengthCallSite;
			break;
		case "cbLevelFixedLength":
			uiCtxt.target.layout.FixedLengthLogLevel = uiCtxt.dlg.control(msg.control).value;
			break;
		default:
			dout("Unable to deal with control '" + msg.control + "'")
			break;
	}
	SetUiModificationState(uiCtxt, true);
	RefreshLogsPreview(uiCtxt.target, dlg);
}

	// Set UseColors property
	// --------------------------------
function cb_SetUseColors(dlg, msg, uiCtxt) {
	clLogger.debug("Use color new value " + uiCtxt.dlg.control("cbUseColors").value);
	uiCtxt.target.layout.UseColor = uiCtxt.dlg.control("cbUseColors").value;
	SetUiModificationState(uiCtxt, true);
	RefreshColorSettings(uiCtxt.target, dlg, uiCtxt.tabsMap);
	RefreshLogsPreview(uiCtxt.target, dlg);
}

	// Detects page change in tabs Colors
	// --------------------------------
function cb_TabColorsPageChange(dlg, msg, uiCtxt) {
	uiCtxt.tabName = uiCtxt.tabsNamesArray[msg.index];
}

	// Color change
	// --------------------------------
function cb_ColorChange(dlg, msg, uiCtxt) {
	if (msg.data != 1) return;	// msg.data !=1 <=> not a final choice
		
	clLogger.trace("palette : " + [msg.buttons, msg.data].join(" / "));
	var ctlName = msg.control;	// ex. : palTextTrace
	var val = dlg.control(ctlName, uiCtxt.dialogsMap(uiCtxt.tabName)).value;	// Need to convert name (eg "date") to tab dlgName (eg "dlgDate")
	var res = ctlName.match(/^pal(Text|Bg)(.*)/);
	//dout("regex res = " + res);
	if (res != null) {
		SetUiModificationState(uiCtxt, true);
		clLogger.debug("Log Level = " + LoggerNS.GetLogLevelValue(res[2]));
		if (res[1] == "Text")
			uiCtxt.target.colors(LoggerNS.GetLogLevelValue(res[2]))(uiCtxt.tabName).fg = val;
		else if (res[1] == "Bg")
			uiCtxt.target.colors(LoggerNS.GetLogLevelValue(res[2]))(uiCtxt.tabName).bg = val;
		RefreshLogsPreview(uiCtxt.target, dlg);
	}
}	

// ===========================================================================
// ---------------------------------------------------------------------------
// Script (business) functions
// ---------------------------------------------------------------------------
// ===========================================================================
function ManageSaveIfRequired(uiCtxt, userDefinedMsg, userDefinedTitle) {
	if (!uiCtxt.modified) return "proceed";

	//if (uiCtxt.modified)
	var title = userDefinedTitle || "Target modified";
	var msg = userDefinedMsg || "Do you want to save modifications before changing ?";
	var choice = WarnBeforeChange(title, msg, uiCtxt.dlg);
	clLogger.debug("User choice = " + choice);	// 0 = Cancel, 1 = Save, 2 = Ignore
	switch(choice) {
		case 0:	// Cancel
			clLogger.info("User chose to cancel. Resume.");
			return "cancelled";
			break;

		case 1:	// Save
			clLogger.info("User chose to save.");
			return SaveTarget(uiCtxt);
			break;

		case 2:	// Ignore
			clLogger.info("User chose to ignore.");
			if (uiCtxt.creationInProgress) {	// We need to cleanup the configs (note: they are unsaved).
				uiCtxt.allConfigs.targets.erase(uiCtxt.targetIndex);
				uiCtxt.targetIndex = -1;
			}
			return "discard";
			break;
	}
}

function SaveTarget(uiCtxt) {
	if (CheckConflict(uiCtxt.allConfigs, uiCtxt.target.name, uiCtxt.targetIndex)) {
		WarningMsgDlg("Name conflict", "Unable to save : another configuration exists with the same name.\nChange name and try again.", uiCtxt.dlg);
		return "cancelled";
	}
	uiCtxt.allConfigs.targets[uiCtxt.targetIndex] = uiCtxt.target;	// We assign the clone to the config list
	uiCtxt.target = uiCtxt.target.Clone();	// To be ready to new modifications, we clone the target again.
	uiCtxt.creationInProgress = false;	// Reinit values
	SetUiModificationState(uiCtxt, false);
	uiCtxt.dlg.control("btNewCfg").enabled = true;
	uiCtxt.allConfigs.SavePersist();
	return "saved";
}

function LoadTargetAndRefreshUI(uiCtxt, modificationState) {
	RefreshTargetsList(uiCtxt.dlg.control("lcTargets"), uiCtxt.allConfigs, uiCtxt.target.name);
	LoadTarget(uiCtxt.target, uiCtxt.tabsMap, uiCtxt.dlg);
	SetUiModificationState(uiCtxt, modificationState);
}

function SetUiModificationState(uiCtxt, modificationState) {	// state = true when modification in progress
	clLogger.debug("** Settting mod state : " + modificationState + " **");
	uiCtxt.modified = modificationState;
	uiCtxt.dlg.control("btDuplicateCfg").enabled = !modificationState;
	uiCtxt.dlg.control("btDeleteCfg").enabled = !uiCtxt.creationInProgress;
	uiCtxt.dlg.control("gpTargetCfg").title = "Target Configuration" + ((modificationState)?" (*)":"") + ((uiCtxt.creationInProgress)?" [NEW]":"");
}

function CheckConflict(allConfigs, name, allowedIndex) {
	for (var i=0; i<allConfigs.targets.length; i++)
		if (allConfigs.targets[i].name == name && i != allowedIndex)
			return true;
	return false;
}

function RefreshTargetsList(lcTargets, allConfigs, name) {
	lcTargets.redraw = false;
	lcTargets.RemoveItem(-1);
	var iSelected = -1;
	for (var i=0; i<allConfigs.targets.size; i++) {
		lcTargets.AddItem(allConfigs.targets(i));
		if (allConfigs.targets(i).name == name) iSelected = i;
	}
	lcTargets.redraw = true;
	lcTargets.value = iSelected;
}

function LoadTarget(target, tabsMap, dlg) {
	dlg.control("edName").value 		= target.name;
	dlg.control("lcType").value			= target.targetType;
	RefreshTargetType(target, dlg);
	
	dlg.control("cbIncDateHour").value	= target.layout.IncludeDateTime;
	dlg.control("cbIncCallSite").value	= target.layout.IncludeCallsite;
	dlg.control("cbIncInstanceId").value = target.layout.IncludeInstanceId;
	dlg.control("cbIncLogLevel").value	= target.layout.IncludeLogLevel;
	dlg.control("cbUseColors").value	= target.layout.UseColor;
	dlg.control("cbUseColors").enabled	= (target.targetType == LoggerNS.TargetType.Console);
	dlg.control("lcMinLevel").value		= target.minLevel;;
	dlg.control("lcMaxLevel").value		= target.maxLevel;
	dlg.control("edSeparator").value	= target.layout.Separator;
	dlg.control("eArchiveDelay").value 	= target.archiveDelay;
	dlg.control("eArchiveDelay").enabled = (target.targetType == LoggerNS.TargetType.File);
	dlg.control("sArchiveDelay").enabled = (target.targetType == LoggerNS.TargetType.File);
	dlg.control("eDeletehDelay").value 	= target.deleteDelay;
	dlg.control("eDeletehDelay").enabled = (target.targetType == LoggerNS.TargetType.File);
	dlg.control("sDeleteDelay").enabled = (target.targetType == LoggerNS.TargetType.File);
	dlg.control("cbDefaultCfg").value	= target.defaultTarget;
	dlg.control("cbCallsiteFixedLength").value = target.layout.FixedLengthCallSite;
	dlg.control("eCallSiteLength").value = target.layout.CallSiteLength;
	dlg.control("eCallSiteLength").enabled = target.layout.FixedLengthCallSite;
	dlg.control("sCallsiteLength").enabled = target.layout.FixedLengthCallSite;
	dlg.control("cbLevelFixedLength").value = target.layout.FixedLengthLogLevel;

	RefreshColorSettings(target, dlg, tabsMap);
	dlg.FlushMsg(); // Need to flush, otherwise most of the modifications on the control will have triggered an event
	RefreshLogsPreview(target, dlg);
}

function RefreshTargetType(target, dlg) {
	dlg.control("edLogPath").enabled 	= (target.targetType == LoggerNS.TargetType.File);
	dlg.control("btLocate").enabled 	= (target.targetType == LoggerNS.TargetType.File);
	dlg.control("edLogPath").value		= (target.targetType == LoggerNS.TargetType.Console)?"":target.logPath;
}

function RefreshColorSettings(target, dlg, tabsMap) {
	for (var e = new Enumerator(tabsMap); !e.atEnd(); e.moveNext()) {
		var tabName = e.item();
		var tokenName = tabsMap(tabName);
		for (var i = LoggerNS.LogLevel.Trace; i <= LoggerNS.LogLevel.Fatal; i++) {
			dlg.control("s" + LoggerNS.LogLevel[i], tabName).enabled 			= target.layout.UseColor && (target.targetType == LoggerNS.TargetType.Console);
			dlg.control("s" + LoggerNS.LogLevel[i] + "Text", tabName).enabled	= target.layout.UseColor && (target.targetType == LoggerNS.TargetType.Console);
			dlg.control("s" + LoggerNS.LogLevel[i] + "Bg", tabName).enabled		= target.layout.UseColor && (target.targetType == LoggerNS.TargetType.Console);
			dlg.control("palText" + LoggerNS.LogLevel[i], tabName).enabled		= target.layout.UseColor && (target.targetType == LoggerNS.TargetType.Console);
			dlg.control("palText" + LoggerNS.LogLevel[i], tabName).value		= target.colors(i)(tokenName).fg;
			//dout("palText pour token=" + tokenName +" & level #" + i + " : " + target.colors(i)(tokenName).fg);
			dlg.control("palBg" + LoggerNS.LogLevel[i], tabName).enabled		= target.layout.UseColor && (target.targetType == LoggerNS.TargetType.Console);
			dlg.control("palBg" + LoggerNS.LogLevel[i], tabName).value			= target.colors(i)(tokenName).bg;
			//dout("palBg pour token=" + tokenName +" & level #" + i + " : " + target.colors(i)(tokenName).fg);
		}
	}
	//dlg.control("gpPreview").enabled 	= target.layout.UseColor && (target.targetType == LoggerNS.TargetType.Console);
	//dlg.control("mtPreview").enabled 	= target.layout.UseColor && (target.targetType == LoggerNS.TargetType.Console);
}

function RefreshLogsPreview(target, dlg) {
	var msg = "";
	for (var i = LoggerNS.LogLevel.Trace; i <= LoggerNS.LogLevel.Fatal; i++) {
		msg += previewLogger.formatMessage(target, "CallerFunction", i, previewLogger.LoggerGetDateHour(), "This is a message of level '" + LoggerNS.LogLevel[i] + "'");
		if (i<LoggerNS.LogLevel.Fatal) msg += "\n";
	}
	dlg.control("mtPreview", "dlgPreview").label = msg;
}

// Function to identify a "new target" name
function GetNewTargetName(allConfigs, rootName) {
	var newTargets = new Array();
	var maxIndex = -1;
	var re = new RegExp("^" + rootName + "[ ]*(\\d+)*$");
	for (var i = 0; i<allConfigs.targets.length; i++) {
		//var res = allConfigs.targets[i].name.match(/^New Target[ ]*(\d+)*$/);
		var res = allConfigs.targets[i].name.match(re);
		if (res != null) {
			clLogger.debug("Name matches : " + allConfigs.targets[i].name);
			clLogger.debug("res length=" + res.length + " / res[0] = " + res[0] + " / res[1] = '" + res[1] + "'");
			if (res[1] == "" && maxIndex == -1) maxIndex = 0;
			else if (res[1] != "") maxIndex = Math.max(maxIndex, parseInt(res[1]));
		}
		else clLogger.debug("Name '" + allConfigs.targets[i].name + "' not matching regexp");
	}
	if (maxIndex == -1) return rootName;
	else {
		//TODO deal with maxIndex
		return rootName + " " + (maxIndex+1);
	}
}

function LoadButtonImage(dlg, buttonName, configName) {
	var cfgValue = Script.config[configName];
	if (cfgValue.length > 0 && cfgValue[0] == "#")
		dlg.control(buttonName).image = cfgValue;
	else if (cfgValue.length > 0)
		dlg.control(buttonName).image = DOpus.LoadImage(cfgValue);
}


// UI Context object
//function UIcontext(dlg, target, targetIndex, creationInProgress, modified) {
function UIcontext(dlg, allConfigs, tabsMap) {	
	this.dlg				= dlg;
	this.allConfigs			= allConfigs;
	this.target 			= undefined;
	this.targetIndex		= 0;
	this.creationInProgress	= false;
	this.modified			= false;
	this.tabName			= "";
	this.tabsNamesArray		= [];
	this.tabsMap			= tabsMap;
	this.dialogsMap			= null;
}


function WarnBeforeChange(title, message, window) {
	var dlg = DOpus.Dlg;
	dlg.window = window;	// if window is a Dialog, this makes this new Dlg modal
	dlg.title = title;
	dlg.message = message;
	dlg.buttons = "Save|Ignore|Cancel";

	return dlg.Show();	// 0 = Cancel, 1 = Save, 2 = Ignore (alphabetical order ?)
}

// Helper OK only Dialog
function WarningMsgDlg(title, message, window) {
	var dlg = DOpus.Dlg;
	dlg.window = window;	// if window is a Dialog, this makes this new Dlg modal
	dlg.title = title;
	dlg.message = message;
	dlg.buttons = "OK";

	dlg.Show();
}


==SCRIPT RESOURCES
<resources>
	<resource name="cfgDlg" type="dialog">
		<dialog height="436" lang="francais" title="Common Logger Configuration" width="350">
			<control height="14" image="#cLogger_add" imagelabel="yes" name="btNewCfg" title="Create New Cfg" type="button" width="74" x="8" y="24" />
			<control height="14" image="#cLogger_clone" imagelabel="yes" name="btDuplicateCfg" title="Duplicate Cfg" type="button" width="74" x="268" y="24" />
			<control height="10" name="cbIncLogLevel" title=" Include LogLevel" type="check" width="82" x="250" y="128" />
			<control height="10" name="cbIncInstanceId" title=" Include Instance Id" type="check" width="82" x="250" y="104" />
			<control height="10" name="cbUseColors" title=" Use Colors" type="check" width="82" x="250" y="140" />
			<control halign="center" height="12" name="edSeparator" title="|" type="edit" width="18" x="62" y="126" />
			<control halign="left" height="8" name="sLogPath1" title="Separator:" type="static" valign="top" width="40" x="20" y="128" />
			<control header="yes" height="110" name="gpColors" title="Colors" type="group" width="318" x="18" y="202" />
			<control height="14" image="#cLogger_delete" imagelabel="yes" name="btDeleteCfg" title="Delete Cfg" type="button" width="74" x="138" y="24" />
			<control height="14" name="btCancelCfg" title="❌ Discard Changes" type="button" width="70" x="192" y="322" />
			<control height="14" name="btSaveCfg" title="💾 Save Cfg" type="button" width="70" x="268" y="322" />
			<control height="40" name="lcMaxLevel" type="combo" width="54" x="62" y="110">
				<contents>
					<item text="Trace" />
					<item text="Debug" />
					<item text="Info" />
					<item text="Warn" />
					<item text="Error" />
					<item text="Fatal" />
				</contents>
			</control>
			<control halign="left" height="8" name="static5" title="Max. Level:" type="static" valign="top" width="40" x="20" y="112" />
			<control height="40" name="lcMinLevel" type="combo" width="54" x="62" y="94">
				<contents>
					<item text="Trace" />
					<item text="Debug" />
					<item text="Info" />
					<item text="Warn" />
					<item text="Error" />
					<item text="Fatal" />
				</contents>
			</control>
			<control height="10" name="cbIncCallSite" title=" Include Callsite" type="check" width="82" x="250" y="116" />
			<control height="10" name="cbIncDateHour" title=" Include Date/Hour" type="check" width="82" x="250" y="92" />
			<control border="no" height="12" image="#folder" name="btLocate" title="Locate Log Folder" type="button" width="16" x="218" y="142" />
			<control halign="left" height="12" name="edLogPath" type="edit" width="154" x="62" y="142" />
			<control halign="left" height="8" name="sLogPath" title="Log Folder:" type="static" valign="top" width="40" x="20" y="144" />
			<control height="40" name="lcTargets" type="combo" width="252" x="90" y="6">
				<contents>
					<item text="TBD ..." />
					<item text="Default Console" />
					<item text="Default File" />
					<item text="Special Console" />
					<item text="... Add new target ..." />
				</contents>
			</control>
			<control halign="left" height="8" name="static4" title="Target Configuration:" type="static" valign="top" width="74" x="10" y="8" />
			<control height="300" name="gpTargetCfg" title="Target Configuration" type="group" width="336" x="8" y="40" />
			<control height="40" name="lcType" type="combo" width="54" x="62" y="78">
				<contents>
					<item text="Console" />
					<item text="File" />
				</contents>
			</control>
			<control height="10" name="cbDefaultCfg" title=" Default Configuration" type="check" width="82" x="250" y="78" />
			<control halign="left" height="12" name="edName" type="edit" width="270" x="62" y="62" />
			<control halign="left" height="8" name="static3" title="Min. Level:" type="static" valign="top" width="40" x="20" y="96" />
			<control halign="left" height="8" name="static2" title="Type:" type="static" valign="top" width="18" x="20" y="80" />
			<control halign="left" height="8" name="static1" title="Name:" type="static" valign="top" width="22" x="20" y="64" />
			<control height="94" name="tabColors" type="tab" width="306" x="28" y="214">
				<tabs>
					<tab dialog="dlgDate" />
					<tab dialog="dlgId" />
					<tab dialog="dlgCallsite" />
					<tab dialog="dlgLevel" />
					<tab dialog="dlgMessage" />
				</tabs>
			</control>
			<control height="14" image="#cLogger_close2" imagelabel="yes" name="btClose" title="Close" type="button" width="66" x="278" y="418" />
			<control height="14" image="#cLogger_export_arrow" imagelabel="yes" name="btExportCfg" title="Export Full Config" type="button" width="90" x="8" y="418" />
			<control height="14" image="#cLogger_import_arrow" imagelabel="yes" name="btImportCfg" title="Import Full Config" type="button" width="90" x="102" y="418" />
			<control header="yes" height="110" name="gpGeneral" title="Main" type="group" width="318" x="16" y="52" />
			<control header="yes" height="70" name="gpPreview1" type="group" width="318" x="18" y="312" />
			<control halign="left" height="8" name="sArchiveDelay" title="Archive Delay:" type="static" valign="top" width="48" x="128" y="80" />
			<control halign="left" height="8" name="sDeleteDelay" title="Delete Delay:" type="static" valign="top" width="48" x="128" y="96" />
			<control halign="right" height="12" max="4" name="eArchiveDelay" number="yes" title="Zone de texte" type="edit" updown="yes" val_min="1" width="54" x="182" y="78" />
			<control halign="right" height="12" max="4" name="eDeletehDelay" number="yes" title="Zone de texte" type="edit" updown="yes" val_min="1" width="54" x="182" y="94" />
			<control header="yes" height="110" name="gpSpacing" title="Spacing" type="group" width="318" x="18" y="160" />
			<control height="10" name="cbCallsiteFixedLength" title=" Fixed Length Callsite" type="check" width="98" x="20" y="172" />
			<control halign="left" height="8" name="sCallsiteLength" title="Callsite length:" type="static" valign="top" width="48" x="128" y="172" />
			<control halign="right" height="12" max="4" name="eCallSiteLength" number="yes" title="Zone de texte" type="edit" updown="yes" width="54" x="182" y="170" />
			<control height="10" name="cbLevelFixedLength" title=" Fixed Length LogLevel" type="check" width="98" x="20" y="184" />
			<control height="66" name="tabPreview" type="tab" width="336" x="8" y="346">
				<tabs>
					<tab dialog="dlgPreview" />
				</tabs>
			</control>
		</dialog>
	</resource>
	<resource name="dlgDate" type="dialog">
		<dialog height="80" lang="francais" title="Date/Hour" width="294">
			<control halign="left" height="8" name="sFatalBg" title="Back:" type="static" valign="top" width="24" x="178" y="68" />
			<control halign="left" height="8" name="sFatalText" title="Text:" type="static" valign="top" width="24" x="83" y="68" />
			<control height="10" name="palTextFatal" paltype="default" title="Palette" type="palette" width="42" x="111" y="66" />
			<control enable="no" height="10" name="palBgError" paltype="default" title="Palette" type="palette" width="42" x="206" y="54" />
			<control enable="no" halign="left" height="8" name="sErrorBg" title="Back:" type="static" valign="top" width="24" x="178" y="56" />
			<control enable="no" halign="left" height="8" name="sErrorText" title="Text:" type="static" valign="top" width="24" x="83" y="56" />
			<control enable="no" height="10" name="palTextError" paltype="default" title="Palette" type="palette" width="42" x="111" y="54" />
			<control height="10" name="palBgWarn" paltype="default" title="Palette" type="palette" width="42" x="206" y="42" />
			<control halign="left" height="8" name="sWarnBg" title="Back:" type="static" valign="top" width="24" x="178" y="44" />
			<control halign="left" height="8" name="sWarnText" title="Text:" type="static" valign="top" width="24" x="83" y="44" />
			<control height="10" name="palTextWarn" paltype="default" title="Palette" type="palette" width="42" x="111" y="42" />
			<control height="10" name="palBgInfo" paltype="default" title="Palette" type="palette" width="42" x="206" y="30" />
			<control halign="left" height="8" name="sInfoBg" title="Back:" type="static" valign="top" width="24" x="178" y="32" />
			<control halign="left" height="8" name="sInfoText" title="Text:" type="static" valign="top" width="24" x="83" y="32" />
			<control height="10" name="palTextInfo" paltype="default" title="Palette" type="palette" width="42" x="111" y="30" />
			<control height="10" name="palBgDebug" paltype="default" title="Palette" type="palette" width="42" x="206" y="18" />
			<control halign="left" height="8" name="sDebugBg" title="Back:" type="static" valign="top" width="24" x="178" y="20" />
			<control halign="left" height="8" name="sDebugText" title="Text:" type="static" valign="top" width="24" x="83" y="20" />
			<control height="10" name="palTextDebug" paltype="default" title="Palette" type="palette" width="42" x="111" y="18" />
			<control halign="left" height="8" name="sTraceBg" title="Back:" type="static" valign="top" width="24" x="178" y="8" />
			<control halign="left" height="8" name="sTraceText" title="Text:" type="static" valign="top" width="24" x="83" y="8" />
			<control height="10" name="palBgTrace" paltype="default" title="Palette" type="palette" width="42" x="206" y="6" />
			<control height="10" name="palTextTrace" paltype="default" title="Palette" type="palette" width="42" x="111" y="6" />
			<control halign="right" height="8" name="sFatal" title="Fatal ▶" type="static" valign="top" width="30" x="26" y="68" />
			<control enable="no" halign="right" height="8" name="sError" title="Error ▶" type="static" valign="top" width="30" x="26" y="56" />
			<control halign="right" height="8" name="sWarn" title="Warn ▶" type="static" valign="top" width="30" x="26" y="44" />
			<control halign="right" height="8" name="sInfo" title="Info ▶" type="static" valign="top" width="30" x="26" y="32" />
			<control halign="right" height="8" name="sDebug" title="Debug ▶" type="static" valign="top" width="30" x="26" y="20" />
			<control height="10" name="palBgFatal" paltype="default" title="Palette" type="palette" width="42" x="206" y="66" />
			<control halign="right" height="8" name="sTrace" title="Trace ▶" type="static" valign="top" width="30" x="26" y="8" />
		</dialog>
	</resource>
	<resource name="dlgId" type="dialog">
		<dialog height="80" lang="francais" title="Script Id" width="294">
			<control halign="left" height="8" name="sFatalBg" title="Back:" type="static" valign="top" width="24" x="178" y="68" />
			<control halign="left" height="8" name="sFatalText" title="Text:" type="static" valign="top" width="24" x="83" y="68" />
			<control height="10" name="palTextFatal" paltype="default" title="Palette" type="palette" width="42" x="111" y="66" />
			<control enable="no" height="10" name="palBgError" paltype="default" title="Palette" type="palette" width="42" x="206" y="54" />
			<control enable="no" halign="left" height="8" name="sErrorBg" title="Back:" type="static" valign="top" width="24" x="178" y="56" />
			<control enable="no" halign="left" height="8" name="sErrorText" title="Text:" type="static" valign="top" width="24" x="83" y="56" />
			<control enable="no" height="10" name="palTextError" paltype="default" title="Palette" type="palette" width="42" x="111" y="54" />
			<control height="10" name="palBgWarn" paltype="default" title="Palette" type="palette" width="42" x="206" y="42" />
			<control halign="left" height="8" name="sWarnBg" title="Back:" type="static" valign="top" width="24" x="178" y="44" />
			<control halign="left" height="8" name="sWarnText" title="Text:" type="static" valign="top" width="24" x="83" y="44" />
			<control height="10" name="palTextWarn" paltype="default" title="Palette" type="palette" width="42" x="111" y="42" />
			<control height="10" name="palBgInfo" paltype="default" title="Palette" type="palette" width="42" x="206" y="30" />
			<control halign="left" height="8" name="sInfoBg" title="Back:" type="static" valign="top" width="24" x="178" y="32" />
			<control halign="left" height="8" name="sInfoText" title="Text:" type="static" valign="top" width="24" x="83" y="32" />
			<control height="10" name="palTextInfo" paltype="default" title="Palette" type="palette" width="42" x="111" y="30" />
			<control height="10" name="palBgDebug" paltype="default" title="Palette" type="palette" width="42" x="206" y="18" />
			<control halign="left" height="8" name="sDebugBg" title="Back:" type="static" valign="top" width="24" x="178" y="20" />
			<control halign="left" height="8" name="sDebugText" title="Text:" type="static" valign="top" width="24" x="83" y="20" />
			<control height="10" name="palTextDebug" paltype="default" title="Palette" type="palette" width="42" x="111" y="18" />
			<control halign="left" height="8" name="sTraceBg" title="Back:" type="static" valign="top" width="24" x="178" y="8" />
			<control halign="left" height="8" name="sTraceText" title="Text:" type="static" valign="top" width="24" x="83" y="8" />
			<control height="10" name="palBgTrace" paltype="default" title="Palette" type="palette" width="42" x="206" y="6" />
			<control height="10" name="palTextTrace" paltype="default" title="Palette" type="palette" width="42" x="111" y="6" />
			<control halign="right" height="8" name="sFatal" title="Fatal ▶" type="static" valign="top" width="30" x="26" y="68" />
			<control enable="no" halign="right" height="8" name="sError" title="Error ▶" type="static" valign="top" width="30" x="26" y="56" />
			<control halign="right" height="8" name="sWarn" title="Warn ▶" type="static" valign="top" width="30" x="26" y="44" />
			<control halign="right" height="8" name="sInfo" title="Info ▶" type="static" valign="top" width="30" x="26" y="32" />
			<control halign="right" height="8" name="sDebug" title="Debug ▶" type="static" valign="top" width="30" x="26" y="20" />
			<control height="10" name="palBgFatal" paltype="default" title="Palette" type="palette" width="42" x="206" y="66" />
			<control halign="right" height="8" name="sTrace" title="Trace ▶" type="static" valign="top" width="30" x="26" y="8" />
		</dialog>
	</resource>
	<resource name="dlgCallsite" type="dialog">
		<dialog height="80" lang="francais" title="Callsite" width="294">
			<control halign="left" height="8" name="sFatalBg" title="Back:" type="static" valign="top" width="24" x="178" y="68" />
			<control halign="left" height="8" name="sFatalText" title="Text:" type="static" valign="top" width="24" x="83" y="68" />
			<control height="10" name="palTextFatal" paltype="default" title="Palette" type="palette" width="42" x="111" y="66" />
			<control enable="no" height="10" name="palBgError" paltype="default" title="Palette" type="palette" width="42" x="206" y="54" />
			<control enable="no" halign="left" height="8" name="sErrorBg" title="Back:" type="static" valign="top" width="24" x="178" y="56" />
			<control enable="no" halign="left" height="8" name="sErrorText" title="Text:" type="static" valign="top" width="24" x="83" y="56" />
			<control enable="no" height="10" name="palTextError" paltype="default" title="Palette" type="palette" width="42" x="111" y="54" />
			<control height="10" name="palBgWarn" paltype="default" title="Palette" type="palette" width="42" x="206" y="42" />
			<control halign="left" height="8" name="sWarnBg" title="Back:" type="static" valign="top" width="24" x="178" y="44" />
			<control halign="left" height="8" name="sWarnText" title="Text:" type="static" valign="top" width="24" x="83" y="44" />
			<control height="10" name="palTextWarn" paltype="default" title="Palette" type="palette" width="42" x="111" y="42" />
			<control height="10" name="palBgInfo" paltype="default" title="Palette" type="palette" width="42" x="206" y="30" />
			<control halign="left" height="8" name="sInfoBg" title="Back:" type="static" valign="top" width="24" x="178" y="32" />
			<control halign="left" height="8" name="sInfoText" title="Text:" type="static" valign="top" width="24" x="83" y="32" />
			<control height="10" name="palTextInfo" paltype="default" title="Palette" type="palette" width="42" x="111" y="30" />
			<control height="10" name="palBgDebug" paltype="default" title="Palette" type="palette" width="42" x="206" y="18" />
			<control halign="left" height="8" name="sDebugBg" title="Back:" type="static" valign="top" width="24" x="178" y="20" />
			<control halign="left" height="8" name="sDebugText" title="Text:" type="static" valign="top" width="24" x="83" y="20" />
			<control height="10" name="palTextDebug" paltype="default" title="Palette" type="palette" width="42" x="111" y="18" />
			<control halign="left" height="8" name="sTraceBg" title="Back:" type="static" valign="top" width="24" x="178" y="8" />
			<control halign="left" height="8" name="sTraceText" title="Text:" type="static" valign="top" width="24" x="83" y="8" />
			<control height="10" name="palBgTrace" paltype="default" title="Palette" type="palette" width="42" x="206" y="6" />
			<control height="10" name="palTextTrace" paltype="default" title="Palette" type="palette" width="42" x="111" y="6" />
			<control halign="right" height="8" name="sFatal" title="Fatal ▶" type="static" valign="top" width="30" x="26" y="68" />
			<control enable="no" halign="right" height="8" name="sError" title="Error ▶" type="static" valign="top" width="30" x="26" y="56" />
			<control halign="right" height="8" name="sWarn" title="Warn ▶" type="static" valign="top" width="30" x="26" y="44" />
			<control halign="right" height="8" name="sInfo" title="Info ▶" type="static" valign="top" width="30" x="26" y="32" />
			<control halign="right" height="8" name="sDebug" title="Debug ▶" type="static" valign="top" width="30" x="26" y="20" />
			<control height="10" name="palBgFatal" paltype="default" title="Palette" type="palette" width="42" x="206" y="66" />
			<control halign="right" height="8" name="sTrace" title="Trace ▶" type="static" valign="top" width="30" x="26" y="8" />
		</dialog>
	</resource>
	<resource name="dlgLevel" type="dialog">
		<dialog height="80" lang="francais" title="LogLevel" width="294">
			<control halign="left" height="8" name="sFatalBg" title="Back:" type="static" valign="top" width="24" x="178" y="68" />
			<control halign="left" height="8" name="sFatalText" title="Text:" type="static" valign="top" width="24" x="83" y="68" />
			<control height="10" name="palTextFatal" paltype="default" title="Palette" type="palette" width="42" x="111" y="66" />
			<control enable="no" height="10" name="palBgError" paltype="default" title="Palette" type="palette" width="42" x="206" y="54" />
			<control enable="no" halign="left" height="8" name="sErrorBg" title="Back:" type="static" valign="top" width="24" x="178" y="56" />
			<control enable="no" halign="left" height="8" name="sErrorText" title="Text:" type="static" valign="top" width="24" x="83" y="56" />
			<control enable="no" height="10" name="palTextError" paltype="default" title="Palette" type="palette" width="42" x="111" y="54" />
			<control height="10" name="palBgWarn" paltype="default" title="Palette" type="palette" width="42" x="206" y="42" />
			<control halign="left" height="8" name="sWarnBg" title="Back:" type="static" valign="top" width="24" x="178" y="44" />
			<control halign="left" height="8" name="sWarnText" title="Text:" type="static" valign="top" width="24" x="83" y="44" />
			<control height="10" name="palTextWarn" paltype="default" title="Palette" type="palette" width="42" x="111" y="42" />
			<control height="10" name="palBgInfo" paltype="default" title="Palette" type="palette" width="42" x="206" y="30" />
			<control halign="left" height="8" name="sInfoBg" title="Back:" type="static" valign="top" width="24" x="178" y="32" />
			<control halign="left" height="8" name="sInfoText" title="Text:" type="static" valign="top" width="24" x="83" y="32" />
			<control height="10" name="palTextInfo" paltype="default" title="Palette" type="palette" width="42" x="111" y="30" />
			<control height="10" name="palBgDebug" paltype="default" title="Palette" type="palette" width="42" x="206" y="18" />
			<control halign="left" height="8" name="sDebugBg" title="Back:" type="static" valign="top" width="24" x="178" y="20" />
			<control halign="left" height="8" name="sDebugText" title="Text:" type="static" valign="top" width="24" x="83" y="20" />
			<control height="10" name="palTextDebug" paltype="default" title="Palette" type="palette" width="42" x="111" y="18" />
			<control halign="left" height="8" name="sTraceBg" title="Back:" type="static" valign="top" width="24" x="178" y="8" />
			<control halign="left" height="8" name="sTraceText" title="Text:" type="static" valign="top" width="24" x="83" y="8" />
			<control height="10" name="palBgTrace" paltype="default" title="Palette" type="palette" width="42" x="206" y="6" />
			<control height="10" name="palTextTrace" paltype="default" title="Palette" type="palette" width="42" x="111" y="6" />
			<control halign="right" height="8" name="sFatal" title="Fatal ▶" type="static" valign="top" width="30" x="26" y="68" />
			<control enable="no" halign="right" height="8" name="sError" title="Error ▶" type="static" valign="top" width="30" x="26" y="56" />
			<control halign="right" height="8" name="sWarn" title="Warn ▶" type="static" valign="top" width="30" x="26" y="44" />
			<control halign="right" height="8" name="sInfo" title="Info ▶" type="static" valign="top" width="30" x="26" y="32" />
			<control halign="right" height="8" name="sDebug" title="Debug ▶" type="static" valign="top" width="30" x="26" y="20" />
			<control height="10" name="palBgFatal" paltype="default" title="Palette" type="palette" width="42" x="206" y="66" />
			<control halign="right" height="8" name="sTrace" title="Trace ▶" type="static" valign="top" width="30" x="26" y="8" />
		</dialog>
	</resource>
	<resource name="dlgMessage" type="dialog">
		<dialog height="80" lang="francais" title="Message" width="294">
			<control halign="left" height="8" name="sFatalBg" title="Back:" type="static" valign="top" width="24" x="178" y="68" />
			<control halign="left" height="8" name="sFatalText" title="Text:" type="static" valign="top" width="24" x="83" y="68" />
			<control height="10" name="palTextFatal" paltype="default" title="Palette" type="palette" width="42" x="111" y="66" />
			<control enable="no" height="10" name="palBgError" paltype="default" title="Palette" type="palette" width="42" x="206" y="54" />
			<control enable="no" halign="left" height="8" name="sErrorBg" title="Back:" type="static" valign="top" width="24" x="178" y="56" />
			<control enable="no" halign="left" height="8" name="sErrorText" title="Text:" type="static" valign="top" width="24" x="83" y="56" />
			<control enable="no" height="10" name="palTextError" paltype="default" title="Palette" type="palette" width="42" x="111" y="54" />
			<control height="10" name="palBgWarn" paltype="default" title="Palette" type="palette" width="42" x="206" y="42" />
			<control halign="left" height="8" name="sWarnBg" title="Back:" type="static" valign="top" width="24" x="178" y="44" />
			<control halign="left" height="8" name="sWarnText" title="Text:" type="static" valign="top" width="24" x="83" y="44" />
			<control height="10" name="palTextWarn" paltype="default" title="Palette" type="palette" width="42" x="111" y="42" />
			<control height="10" name="palBgInfo" paltype="default" title="Palette" type="palette" width="42" x="206" y="30" />
			<control halign="left" height="8" name="sInfoBg" title="Back:" type="static" valign="top" width="24" x="178" y="32" />
			<control halign="left" height="8" name="sInfoText" title="Text:" type="static" valign="top" width="24" x="83" y="32" />
			<control height="10" name="palTextInfo" paltype="default" title="Palette" type="palette" width="42" x="111" y="30" />
			<control height="10" name="palBgDebug" paltype="default" title="Palette" type="palette" width="42" x="206" y="18" />
			<control halign="left" height="8" name="sDebugBg" title="Back:" type="static" valign="top" width="24" x="178" y="20" />
			<control halign="left" height="8" name="sDebugText" title="Text:" type="static" valign="top" width="24" x="83" y="20" />
			<control height="10" name="palTextDebug" paltype="default" title="Palette" type="palette" width="42" x="111" y="18" />
			<control halign="left" height="8" name="sTraceBg" title="Back:" type="static" valign="top" width="24" x="178" y="8" />
			<control halign="left" height="8" name="sTraceText" title="Text:" type="static" valign="top" width="24" x="83" y="8" />
			<control height="10" name="palBgTrace" paltype="default" title="Palette" type="palette" width="42" x="206" y="6" />
			<control height="10" name="palTextTrace" paltype="default" title="Palette" type="palette" width="42" x="111" y="6" />
			<control halign="right" height="8" name="sFatal" title="Fatal ▶" type="static" valign="top" width="30" x="26" y="68" />
			<control enable="no" halign="right" height="8" name="sError" title="Error ▶" type="static" valign="top" width="30" x="26" y="56" />
			<control halign="right" height="8" name="sWarn" title="Warn ▶" type="static" valign="top" width="30" x="26" y="44" />
			<control halign="right" height="8" name="sInfo" title="Info ▶" type="static" valign="top" width="30" x="26" y="32" />
			<control halign="right" height="8" name="sDebug" title="Debug ▶" type="static" valign="top" width="30" x="26" y="20" />
			<control height="10" name="palBgFatal" paltype="default" title="Palette" type="palette" width="42" x="206" y="66" />
			<control halign="right" height="8" name="sTrace" title="Trace ▶" type="static" valign="top" width="30" x="26" y="8" />
		</dialog>
	</resource>
	<resource name="cfgDlgold" type="dialog">
		<dialog height="378" lang="francais" width="350">
			<control height="14" image="#newmenu" imagelabel="yes" name="btNewCfg" title="Create New Cfg" type="button" width="74" x="8" y="24" />
			<control height="14" image="#duplicate" imagelabel="yes" name="btDuplicateCfg" title="Duplicate Cfg" type="button" width="74" x="268" y="24" />
			<control height="10" name="cbIncLogLevel" title=" Include LogLevel" type="check" width="102" x="116" y="120" />
			<control height="10" name="cbIncInstanceId" title=" Include Instance Id" type="check" width="102" x="116" y="108" />
			<control height="10" name="cbUseColors" title=" Use Colors" type="check" width="82" x="18" y="132" />
			<control halign="center" height="12" name="edSeparator" title="|" type="edit" width="14" x="322" y="90" />
			<control halign="left" height="8" name="sLogPath1" title="Separator:" type="static" valign="top" width="36" x="280" y="92" />
			<control height="70" name="gpPreview" title="Preview" type="group" width="324" x="12" y="260" />
			<control height="110" name="gpColors" title="Colors" type="group" width="324" x="12" y="148" />
			<control height="14" image="#delete" imagelabel="yes" name="btDeleteCfg" title="Delete Cfg" type="button" width="74" x="138" y="24" />
			<control height="14" name="btCancelCfg" title="❌ Discard Changes" type="button" width="70" x="190" y="334" />
			<control height="14" name="btSaveCfg" title="💾 Save Cfg" type="button" width="70" x="266" y="334" />
			<control height="40" name="lcMaxLevel" type="combo" width="60" x="174" y="90">
				<contents>
					<item text="Trace" />
					<item text="Debug" />
					<item text="Info" />
					<item text="Warn" />
					<item text="Error" />
					<item text="Fatal" />
				</contents>
			</control>
			<control halign="left" height="8" name="static5" title="Max. Level:" type="static" valign="top" width="42" x="132" y="92" />
			<control height="40" name="lcMinLevel" type="combo" width="60" x="60" y="90">
				<contents>
					<item text="Trace" />
					<item text="Debug" />
					<item text="Info" />
					<item text="Warn" />
					<item text="Error" />
					<item text="Fatal" />
				</contents>
			</control>
			<control height="10" name="cbIncCallSite" title=" Include Callsite" type="check" width="86" x="18" y="120" />
			<control height="10" name="cbIncDateHour" title=" Include Date/Hour" type="check" width="82" x="18" y="108" />
			<control changelinkcolor="no" halign="left" height="54" name="mtPreview" title="&lt;font bgcol=#404040&gt;2024.07.05-19:33:47.267&lt;/#&gt;|&lt;font color=#919191&gt;6912&lt;/#&gt;|&lt;font color=#0061d8 bgcol=#ffffff&gt;&lt;b&gt;OnInitIncludeFile&lt;/b&gt;&lt;/#&gt;|&lt;font color=#000000 bgcol=#64ffff&gt;Trace&lt;/#&gt;|This is a Trace level log.\nDebug\nInfo\nWarn\nError\nFatal" type="markuptext" width="304" x="24" y="272" />
			<control height="12" image="#folder" name="btLocate" title="Locate Log Folder" type="button" width="14" x="322" y="72" />
			<control halign="left" height="12" name="edLogPath" type="edit" width="144" x="174" y="72" />
			<control halign="left" height="8" name="sLogPath" title="Log Folder:" type="static" valign="top" width="40" x="132" y="74" />
			<control height="40" name="lcTargets" type="combo" width="252" x="90" y="6">
				<contents>
					<item text="TBD ..." />
					<item text="Default Console" />
					<item text="Default File" />
					<item text="Special Console" />
					<item text="... Add new target ..." />
				</contents>
			</control>
			<control halign="left" height="8" name="static4" title="Target Configuration:" type="static" valign="top" width="74" x="10" y="8" />
			<control height="312" name="gpTargetCfg" title="Target Configuration" type="group" width="336" x="6" y="42" />
			<control height="40" name="lcType" type="combo" width="60" x="60" y="72">
				<contents>
					<item text="Console" />
					<item text="File" />
				</contents>
			</control>
			<control height="10" name="cbDefaultCfg" title=" Default Configuration" type="check" width="82" x="234" y="108" />
			<control halign="left" height="12" name="edName" type="edit" width="276" x="60" y="54" />
			<control halign="left" height="8" name="static3" title="Min. Level:" type="static" valign="top" width="42" x="18" y="92" />
			<control halign="left" height="8" name="static2" title="Type:" type="static" valign="top" width="18" x="18" y="74" />
			<control halign="left" height="8" name="static1" title="Name:" type="static" valign="top" width="22" x="18" y="56" />
			<control height="94" name="tabColors" type="tab" width="298" x="26" y="160">
				<tabs>
					<tab dialog="dlgDate" />
					<tab dialog="dlgId" />
					<tab dialog="dlgCallsite" />
					<tab dialog="dlgLevel" />
					<tab dialog="dlgMessage" />
				</tabs>
			</control>
			<control height="14" image="#closelister" imagelabel="yes" name="btClose" title="Close" type="button" width="66" x="276" y="358" />
			<control height="14" image="#importexport" imagelabel="yes" name="btExportCfg" title="Export Config" type="button" width="66" x="6" y="358" />
			<control height="14" image="#importexport" imagelabel="yes" name="btImportCfg" title="Import Config" type="button" width="66" x="78" y="358" />
		</dialog>
	</resource>
	<resource name="cfgConfigPage" type="dialog">
		<dialog height="76" lang="francais" width="330">
			<control height="70" name="tab1" type="tab" width="324" x="2" y="2">
				<tabs>
					<tab dialog="dlgConfig" />
				</tabs>
			</control>
		</dialog>
	</resource>
	<resource name="dlgConfig" type="dialog">
		<dialog height="66" lang="francais" resize="yes" title="Advanced Config" width="322">
			<control height="14" image="#properties" imagelabel="yes" name="btOpenConfig" resize="xy" title=" Open Configuration Tool" type="button" width="122" x="100" y="4" />
			<control height="10" name="cbGlobalMinLevel" title=" Enable Global Minimum Log Level:" type="check" width="124" x="4" y="32" />
			<control enable="no" height="40" name="lcMinLevel" type="combo" width="54" x="132" y="31">
				<contents>
					<item text="Trace" />
					<item text="Debug" />
					<item text="Info" />
					<item text="Warn" />
					<item text="Error" />
					<item text="Fatal" />
				</contents>
			</control>
			<control header="yes" height="70" name="gpPreview1" type="group" width="310" x="6" y="20" />
		</dialog>
	</resource>
	<resource name="dlgPreview" type="dialog">
		<dialog fontface="Consolas" height="64" lang="francais" title="Preview" width="330">
			<control changelinkcolor="no" halign="left" height="48" name="mtPreview" resize="wh" title="&lt;font bgcol=#404040&gt;2024.07.05-19:33:47.267&lt;/#&gt;|&lt;font color=#919191&gt;6912&lt;/#&gt;|&lt;font color=#0061d8 bgcol=#ffffff&gt;&lt;b&gt;OnInitIncludeFile&lt;/b&gt;&lt;/#&gt;|&lt;font color=#000000 bgcol=#64ffff&gt;Trace&lt;/#&gt;|This is a Trace level log.\nDebug\nInfo\nWarn\nError\nFatal" type="markuptext" width="308" x="10" y="8" />
			<control height="58" name="group1" type="group" width="324" x="2" y="0" />
		</dialog>
	</resource>
</resources>
