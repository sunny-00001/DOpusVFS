var g_services = null;
var g_lastLoadTime = 0;
var g_cacheTimeout = 30000;
var LOG_DEBUG = 1;
var LOG_INFO = 2;
var LOG_WARN = 3;
var LOG_ERROR = 4;

function OnInit(initData)
{
	initData.name = "ServicesVFS";
	initData.version = "1.1.0";
	initData.copyright = "(c) 2026";
	initData.desc = "Windows服务管理 - 自定义列和命令";
	initData.default_enable = true;
	initData.min_version = "12.0";

	initData.config = DOpus.Create().OrderedMap();
	initData.config_desc = DOpus.Create().OrderedMap();
	initData.config_groups = DOpus.Create().OrderedMap();
	initData.config_group_order = DOpus.NewVector('日志', '常规');

	var option_name = "";
	var option_group = "";

	option_group = '日志';
	option_name = "log level";
	initData.config[option_name] = DOpus.Create().Vector(2, 'debug', 'standard', 'warning', 'off');
	initData.config_desc(option_name) = "日志输出级别。debug=显示所有, standard=常规, warning=仅警告, off=仅错误";
	initData.config_groups(option_name) = option_group;

	option_group = '常规';
	option_name = "cache timeout";
	initData.config[option_name] = 30;
	initData.config_desc(option_name) = "服务数据缓存超时时间（秒）";
	initData.config_groups(option_name) = option_group;

	var cmd;

	cmd = initData.AddCommand();
	cmd.name = "ServiceStart";
	cmd.method = "OnServiceStart";
	cmd.desc = "启动选中的服务";
	cmd.label = "启动服务";
	cmd.template = "";
	cmd.context_args = "*";

	cmd = initData.AddCommand();
	cmd.name = "ServiceStop";
	cmd.method = "OnServiceStop";
	cmd.desc = "停止选中的服务";
	cmd.label = "停止服务";
	cmd.template = "";
	cmd.context_args = "*";

	cmd = initData.AddCommand();
	cmd.name = "ServicePause";
	cmd.method = "OnServicePause";
	cmd.desc = "暂停选中的服务";
	cmd.label = "暂停服务";
	cmd.template = "";
	cmd.context_args = "*";

	cmd = initData.AddCommand();
	cmd.name = "ServiceResume";
	cmd.method = "OnServiceResume";
	cmd.desc = "继续选中的服务";
	cmd.label = "继续服务";
	cmd.template = "";
	cmd.context_args = "*";

	cmd = initData.AddCommand();
	cmd.name = "ServiceRestart";
	cmd.method = "OnServiceRestart";
	cmd.desc = "重启选中的服务";
	cmd.label = "重启服务";
	cmd.template = "";
	cmd.context_args = "*";

	cmd = initData.AddCommand();
	cmd.name = "ServiceRefresh";
	cmd.method = "OnServiceRefresh";
	cmd.desc = "刷新服务状态";
	cmd.label = "刷新服务";
	cmd.template = "";
	cmd.context_args = "*";

	cmd = initData.AddCommand();
	cmd.name = "ServiceProperties";
	cmd.method = "OnServiceProperties";
	cmd.desc = "查看服务属性";
	cmd.label = "服务属性";
	cmd.template = "";
	cmd.context_args = "*";

	var col;

	col = initData.AddColumn();
	col.name = "SvcDisplayName";
	col.method = "OnServiceColumn";
	col.label = "显示名称";
	col.header = "显示名称";
	col.justify = "left";
	col.autogroup = true;
	col.multicol = true;

	col = initData.AddColumn();
	col.name = "SvcStatus";
	col.method = "OnServiceColumn";
	col.label = "状态";
	col.header = "状态";
	col.justify = "left";
	col.autogroup = true;
	col.multicol = true;

	col = initData.AddColumn();
	col.name = "SvcStartType";
	col.method = "OnServiceColumn";
	col.label = "启动类型";
	col.header = "启动类型";
	col.justify = "left";
	col.autogroup = true;
	col.multicol = true;

	col = initData.AddColumn();
	col.name = "SvcType";
	col.method = "OnServiceColumn";
	col.label = "服务类型";
	col.header = "服务类型";
	col.justify = "left";
	col.autogroup = true;
	col.multicol = true;

	col = initData.AddColumn();
	col.name = "SvcDescription";
	col.method = "OnServiceColumn";
	col.label = "描述";
	col.header = "描述";
	col.justify = "left";
	col.autogroup = true;
	col.multicol = true;

	col = initData.AddColumn();
	col.name = "SvcProcessId";
	col.method = "OnServiceColumn";
	col.label = "进程ID";
	col.header = "进程ID";
	col.justify = "right";
	col.autogroup = true;
	col.multicol = true;

	col = initData.AddColumn();
	col.name = "SvcExitCode";
	col.method = "OnServiceColumn";
	col.label = "退出代码";
	col.header = "退出代码";
	col.justify = "right";
	col.autogroup = true;
	col.multicol = true;
}

function Log(level, text)
{
	if (level === LOG_ERROR || Script.config["log level"] < level) {
		if (level == LOG_DEBUG) DOpus.Output('<#%vs_dragdrop_normal_action>DEBUG   => ' + text + '</#>');
		else if (level == LOG_INFO) DOpus.Output('INFO    => ' + text);
		else if (level == LOG_WARN) DOpus.Output('<#%vs_dragdrop_warning_action>WARNING => ' + text + '</#>');
		else DOpus.Output('ERROR   => ' + text, true);
	}
}

function LoadAllServices()
{
	var now = new Date().getTime();
	var timeout = (Script.config["cache timeout"] || 30) * 1000;

	if (g_services && (now - g_lastLoadTime) < timeout) {
		Log(LOG_DEBUG, "LoadAllServices: using cache");
		return g_services;
	}

	Log(LOG_INFO, "LoadAllServices: refreshing service data...");
	var services = {};

	try {
		var wmi = GetObject("winmgmts:");
		var colItems = wmi.InstancesOf("Win32_Service");

		var enumItems = new Enumerator(colItems);
		for (; !enumItems.atEnd(); enumItems.moveNext()) {
			var objItem = enumItems.item();

			var service = {
				name: String(objItem.Name || ""),
				displayName: String(objItem.DisplayName || ""),
				status: parseInt(objItem.State == "Running" ? 4 : objItem.State == "Paused" ? 7 : objItem.State == "Stopped" ? 1 : 1),
				state: String(objItem.State || ""),
				startType: GetStartTypeNum(objItem.StartMode),
				type: parseInt(objItem.ServiceType || 0x10),
				description: String(objItem.Description || ""),
				processId: parseInt(objItem.ProcessId || 0),
				exitCode: parseInt(objItem.ExitCode || 0),
				acceptPause: !!objItem.AcceptPause,
				acceptStop: !!objItem.AcceptStop
			};

			if (service.name) {
				services[service.name] = service;
			}
		}
		var svcCount = 0;
		for (var k in services) { if (services.hasOwnProperty(k)) svcCount++; }
		Log(LOG_INFO, "LoadAllServices: loaded " + svcCount + " services");
	} catch (e) {
		Log(LOG_ERROR, "LoadAllServices WMI query error: " + e.message);
	}

	g_services = services;
	g_lastLoadTime = now;

	return services;
}

function GetStartTypeNum(startMode)
{
	startMode = String(startMode || "");
	if (startMode == "Auto") return 2;
	if (startMode == "Manual") return 3;
	if (startMode == "Disabled") return 4;
	return 3;
}

function OnServiceColumn(colData)
{
	var item = colData.item;
	if (!item) return;

	var fileName = String(item.name);
	if (!fileName) return;

	var serviceName = ResolveServiceName(fileName);

	var allServices = LoadAllServices();
	var serviceInfo = allServices[serviceName];
	if (!serviceInfo) return;

	if (colData.columns.exists("SvcDisplayName")) {
		var displayName = String(serviceInfo.displayName || serviceName);
		colData.columns("SvcDisplayName").value = displayName;
		colData.columns("SvcDisplayName").sort = displayName;
		colData.columns("SvcDisplayName").group = serviceInfo.displayName ? "已命名" : "未命名";
	}

	if (colData.columns.exists("SvcStatus")) {
		var statusText = GetServiceStatusText(serviceInfo.status);
		colData.columns("SvcStatus").value = statusText;
		colData.columns("SvcStatus").sort = serviceInfo.status.toString();
		colData.columns("SvcStatus").group = statusText;
	}

	if (colData.columns.exists("SvcStartType")) {
		var startTypeText = GetServiceStartTypeText(serviceInfo.startType);
		colData.columns("SvcStartType").value = startTypeText;
		colData.columns("SvcStartType").sort = serviceInfo.startType.toString();
		colData.columns("SvcStartType").group = startTypeText;
	}

	if (colData.columns.exists("SvcType")) {
		var typeText = GetServiceTypeText(serviceInfo.type);
		colData.columns("SvcType").value = typeText;
		colData.columns("SvcType").sort = serviceInfo.type.toString();
		colData.columns("SvcType").group = typeText;
	}

	if (colData.columns.exists("SvcDescription")) {
		var desc = String(serviceInfo.description || "-");
		if (desc.length > 100) desc = desc.substring(0, 100) + "...";
		colData.columns("SvcDescription").value = desc;
		colData.columns("SvcDescription").sort = String(serviceInfo.description || "");
		colData.columns("SvcDescription").group = serviceInfo.description ? "有描述" : "无描述";
	}

	if (colData.columns.exists("SvcProcessId")) {
		colData.columns("SvcProcessId").value = serviceInfo.processId.toString();
		colData.columns("SvcProcessId").sort = serviceInfo.processId;
		colData.columns("SvcProcessId").group = serviceInfo.processId > 0 ? "运行中" : "已停止";
	}

	if (colData.columns.exists("SvcExitCode")) {
		colData.columns("SvcExitCode").value = serviceInfo.exitCode.toString();
		colData.columns("SvcExitCode").sort = serviceInfo.exitCode;
		colData.columns("SvcExitCode").group = serviceInfo.exitCode == 0 ? "成功" : "错误";
	}
}

function GetServiceStatusText(status)
{
	switch (status) {
		case 4: return "运行中";
		case 1: return "已停止";
		case 7: return "已暂停";
		case 2: return "正在启动";
		case 3: return "正在停止";
		case 6: return "正在暂停";
		case 5: return "正在恢复";
		default: return "未知";
	}
}

function GetServiceStartTypeText(startType)
{
	switch (startType) {
		case 2: return "自动";
		case 3: return "手动";
		case 4: return "已禁用";
		case 0: return "启动";
		case 1: return "系统";
		default: return "未知";
	}
}

function GetServiceTypeText(type)
{
	var isInteractive = (type & 0x100) != 0;
	var baseType = type & 0xFF;

	switch (baseType) {
		case 0x10:
			return isInteractive ? "交互式独立进程" : "独立进程";
		case 0x20:
			return isInteractive ? "交互式共享进程" : "共享进程";
		case 0x1: return "内核驱动";
		case 0x2: return "文件系统驱动";
		default: return "其他";
	}
}

function GetServiceInfo(serviceName)
{
	var allServices = LoadAllServices();
	return allServices[serviceName] || null;
}

function RunSCCommand(cmd)
{
	var shell = new ActiveXObject("WScript.Shell");
	var fso = new ActiveXObject("Scripting.FileSystemObject");
	var tempFile = fso.GetSpecialFolder(2) + "\\svfs_" + new Date().getTime() + ".txt";

	try {
		var fullCmd = 'cmd.exe /c ' + cmd + ' > "' + tempFile + '" 2>&1';
		Log(LOG_DEBUG, "RunSCCommand: " + fullCmd);

		var exitCode = shell.Run(fullCmd, 0, true);

		var output = "";
		if (fso.FileExists(tempFile)) {
			var ts = fso.OpenTextFile(tempFile, 1, false, 0);
			if (!ts.AtEndOfStream) output = ts.ReadAll();
			ts.Close();
			fso.DeleteFile(tempFile, true);
		}

		Log(LOG_DEBUG, "RunSCCommand: ExitCode=" + exitCode + ", Output=" + output.substring(0, 200));

		return { exitCode: exitCode, output: output };
	} catch (e) {
		if (fso.FileExists(tempFile)) {
			try { fso.DeleteFile(tempFile, true); } catch (de) {}
		}
		Log(LOG_ERROR, "RunSCCommand error: " + e.message);
		return { exitCode: -1, output: "" };
	}
}

function GetSCErrorMessage(exitCode, output)
{
	if (exitCode == 0) return "";
	var base = "";
	if (exitCode == 5) base = "拒绝访问，请以管理员身份运行 DOpus";
	else if (exitCode == 1052) base = "该服务不支持此操作";
	else if (exitCode == 1056) base = "服务已在运行中";
	else if (exitCode == 1060) base = "指定的服务不存在";
	else if (exitCode == 1062) base = "服务未启动";
	else if (exitCode == 1061) base = "服务无法接受暂停或继续控制";
	else base = "退出码: " + exitCode;
	if (output && output.length > 0) base += "\n" + output;
	return base;
}

function ResolveServiceName(rawName)
{
	var serviceName = rawName;
	if (serviceName.length > 7 && serviceName.substring(serviceName.length - 7).toLowerCase() == ".service") {
		serviceName = serviceName.substring(0, serviceName.length - 7);
	}

	var allServices = LoadAllServices();
	if (allServices[serviceName]) return serviceName;

	for (var name in allServices) {
		if (allServices.hasOwnProperty(name)) {
			if (String(allServices[name].displayName).toLowerCase() == serviceName.toLowerCase()) {
				return name;
			}
		}
	}

	return serviceName;
}

function ControlService(serviceName, action)
{
	var success = false;

	Log(LOG_INFO, "ControlService: " + action + " '" + serviceName + "'");

	try {
		var cmd = "";

		if (action == "start") {
			cmd = 'sc start "' + serviceName + '"';
		} else if (action == "stop") {
			cmd = 'sc stop "' + serviceName + '"';
		} else if (action == "pause") {
			cmd = 'sc pause "' + serviceName + '"';
		} else if (action == "resume") {
			cmd = 'sc continue "' + serviceName + '"';
		} else if (action == "restart") {
			var stopResult = RunSCCommand('sc stop "' + serviceName + '"');
			if (stopResult.exitCode == 0 || stopResult.exitCode == 1) {
				DOpus.Delay(2000);
			}
			var startResult = RunSCCommand('sc start "' + serviceName + '"');
			InvalidateCache();

			var msg = "";
			if (startResult.exitCode == 0) {
				msg = "服务重启成功";
			} else {
				msg = "重启失败: " + GetSCErrorMessage(startResult.exitCode, startResult.output);
			}
			return { success: startResult.exitCode == 0, message: msg };
		}

		var result = RunSCCommand(cmd);
		success = result.exitCode == 0;

		if (!success) {
			Log(LOG_WARN, "ControlService failed: ExitCode=" + result.exitCode);
		}

	} catch (e) {
		Log(LOG_ERROR, "ControlService error: " + e.message);
	}

	InvalidateCache();

	var msg = "";
	if (success) {
		if (action == "start") msg = "服务启动成功";
		else if (action == "stop") msg = "服务停止成功";
		else if (action == "pause") msg = "服务暂停成功";
		else if (action == "resume") msg = "服务继续成功";
	} else {
		msg = "操作失败: " + GetSCErrorMessage(result.exitCode, result.output);
	}

	return { success: success, message: msg };
}

function InvalidateCache()
{
	g_services = null;
	g_lastLoadTime = 0;
}

function OnButtonContext(buttonContextData)
{
	var lister = buttonContextData.lister;
	if (!lister) return;

	var cmdName = buttonContextData.method;
	var tab = lister.activetab;
	var hasSelection = (tab && tab.selected.count > 0);

	var state = "";
	var acceptPause = false;
	var acceptStop = false;

	if (hasSelection) {
		var item = tab.selected(0);
		var fileName = String(item.name);
		var serviceName = ResolveServiceName(fileName);
		var allServices = LoadAllServices();
		var svc = allServices[serviceName];
		if (svc) {
			state = svc.state;
			acceptPause = svc.acceptPause;
			acceptStop = svc.acceptStop;
		}
	}

	var disabled = false;

	if (cmdName == "OnServiceStart") {
		disabled = !hasSelection || state == "Running" || state == "Paused";
	} else if (cmdName == "OnServiceStop") {
		disabled = !hasSelection || state == "Stopped" || !acceptStop;
	} else if (cmdName == "OnServicePause") {
		disabled = !hasSelection || state != "Running" || !acceptPause;
	} else if (cmdName == "OnServiceResume") {
		disabled = !hasSelection || state != "Paused";
	} else if (cmdName == "OnServiceRestart") {
		disabled = !hasSelection || state == "Stopped";
	} else if (cmdName == "OnServiceRefresh") {
		disabled = false;
	} else if (cmdName == "OnServiceProperties") {
		disabled = !hasSelection;
	}

	buttonContextData.ctx.disabled = disabled;
}

function OnServiceStart(scriptCmdData)
{
	Log(LOG_INFO, "OnServiceStart invoked");
	ExecuteServiceAction(scriptCmdData, "start", "启动服务");
}

function OnServiceStop(scriptCmdData)
{
	Log(LOG_INFO, "OnServiceStop invoked");
	ExecuteServiceAction(scriptCmdData, "stop", "停止服务");
}

function OnServicePause(scriptCmdData)
{
	Log(LOG_INFO, "OnServicePause invoked");
	ExecuteServiceAction(scriptCmdData, "pause", "暂停服务");
}

function OnServiceResume(scriptCmdData)
{
	Log(LOG_INFO, "OnServiceResume invoked");
	ExecuteServiceAction(scriptCmdData, "resume", "继续服务");
}

function OnServiceRestart(scriptCmdData)
{
	Log(LOG_INFO, "OnServiceRestart invoked");
	ExecuteServiceAction(scriptCmdData, "restart", "重启服务");
}

function OnServiceRefresh(scriptCmdData)
{
	Log(LOG_INFO, "OnServiceRefresh invoked");
	var func = scriptCmdData.func;
	var tab = func.sourcetab;

	InvalidateCache();
	RefreshTab(tab);
	func.Dlg.Request("服务状态已刷新", "OK", "成功");
}

function ExecuteServiceAction(scriptCmdData, action, actionName)
{
	var func = scriptCmdData.func;
	var tab = func.sourcetab;

	Log(LOG_INFO, "ExecuteServiceAction: " + action + ", selected=" + tab.selected.count);

	if (tab.selected.count == 0) {
		func.Dlg.Request("请先选择一个服务", "OK", "提示");
		return;
	}

	var item = tab.selected(0);
	var fileName = String(item.name);
	var serviceName = ResolveServiceName(fileName);

	Log(LOG_INFO, "ExecuteServiceAction: fileName='" + fileName + "', resolved='" + serviceName + "'");

	var allServices = LoadAllServices();
	var svc = allServices[serviceName];
	if (!svc) {
		func.Dlg.Request("找不到服务: " + serviceName, "OK", "错误");
		return;
	}

	var state = svc.state;
	Log(LOG_INFO, "ExecuteServiceAction: state='" + state + "', acceptPause=" + svc.acceptPause + ", acceptStop=" + svc.acceptStop);

	if (action == "start" && state == "Running") {
		func.Dlg.Request("服务已在运行中，无需启动", "OK", "提示");
		return;
	}
	if (action == "start" && state == "Paused") {
		func.Dlg.Request("服务已暂停，请使用'继续'而非'启动'", "OK", "提示");
		return;
	}
	if (action == "stop" && state == "Stopped") {
		func.Dlg.Request("服务已停止，无需再次停止", "OK", "提示");
		return;
	}
	if (action == "stop" && !svc.acceptStop) {
		func.Dlg.Request("该服务不支持停止操作", "OK", "提示");
		return;
	}
	if (action == "pause" && state != "Running") {
		func.Dlg.Request("只有运行中的服务才能暂停\n当前状态: " + GetServiceStatusText(svc.status), "OK", "提示");
		return;
	}
	if (action == "pause" && !svc.acceptPause) {
		func.Dlg.Request("该服务不支持暂停操作", "OK", "提示");
		return;
	}
	if (action == "resume" && state != "Paused") {
		func.Dlg.Request("只有暂停中的服务才能继续\n当前状态: " + GetServiceStatusText(svc.status), "OK", "提示");
		return;
	}
	if (action == "restart" && state == "Stopped") {
		func.Dlg.Request("服务已停止，请使用'启动'而非'重启'", "OK", "提示");
		return;
	}

	if (action == "stop" || action == "restart") {
		Log(LOG_DEBUG, "ExecuteServiceAction: showing confirm dialog");
		var confirmResult = func.Dlg.Request("确定要" + actionName + "吗？\n服务名称: " + serviceName, "确定|取消", "确认");
		Log(LOG_DEBUG, "ExecuteServiceAction: confirmResult=" + confirmResult);
		if (confirmResult == 0) {
			Log(LOG_INFO, "ExecuteServiceAction: user cancelled");
			return;
		}
	}

	Log(LOG_INFO, "ExecuteServiceAction: calling ControlService");
	var result = ControlService(serviceName, action);
	Log(LOG_INFO, "ExecuteServiceAction: result success=" + result.success + ", message=" + result.message);

	if (result.success) {
		RefreshTab(tab);
	} else {
		func.Dlg.Request(result.message, "OK", "错误");
	}
}

function OnServiceProperties(scriptCmdData)
{
	Log(LOG_INFO, "OnServiceProperties invoked");
	var func = scriptCmdData.func;
	var tab = func.sourcetab;

	if (tab.selected.count == 0) {
		func.Dlg.Request("请先选择一个服务", "OK", "提示");
		return;
	}

	var item = tab.selected(0);
	var fileName = String(item.name);
	var serviceName = ResolveServiceName(fileName);

	var serviceInfo = GetServiceInfo(serviceName);
	if (!serviceInfo) {
		func.Dlg.Request("无法获取服务信息", "OK", "错误");
		return;
	}

	var info = "服务名称: " + serviceInfo.name + "\n";
	info += "显示名称: " + (serviceInfo.displayName || "-") + "\n";
	info += "状态: " + GetServiceStatusText(serviceInfo.status) + "\n";
	info += "启动类型: " + GetServiceStartTypeText(serviceInfo.startType) + "\n";
	info += "服务类型: " + GetServiceTypeText(serviceInfo.type) + "\n";
	info += "进程ID: " + serviceInfo.processId + "\n";
	info += "退出代码: " + serviceInfo.exitCode + "\n";
	info += "\n描述:\n" + (serviceInfo.description || "无");

	func.Dlg.Request(info, "OK", "服务属性 - " + serviceName);
}

function RefreshTab(tab)
{
	var cmd = DOpus.Create.Command();
	cmd.SetSourceTab(tab);
	cmd.RunCommand("Go REFRESH");
}
