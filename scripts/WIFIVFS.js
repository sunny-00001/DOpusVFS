var g_detailCache = {};
var LOG_DEBUG = 1;
var LOG_INFO = 2;
var LOG_WARN = 3;
var LOG_ERROR = 4;

function OnInit(initData)
{
	initData.name = "WifiVFS";
	initData.version = "1.0.0";
	initData.copyright = "(c) 2026";
	initData.desc = "WiFi\u7F51\u7EDC\u7BA1\u7406 - \u547D\u4EE4";
	initData.default_enable = true;
	initData.min_version = "12.0";

	initData.config = DOpus.Create().OrderedMap();
	initData.config_desc = DOpus.Create().OrderedMap();

	var option_name = "log level";
	initData.config[option_name] = DOpus.Create().Vector(2, 'debug', 'standard', 'warning', 'off');
	initData.config_desc(option_name) = "\u65E5\u5FD7\u8F93\u51FA\u7EA7\u522B";

	var cmd;

	cmd = initData.AddCommand();
	cmd.name = "WifiViewDetail";
	cmd.method = "OnWifiViewDetail";
	cmd.desc = "\u67E5\u770B\u9009\u4E2AWiFi\u7684\u8BE6\u7EC6\u4FE1\u606F";
	cmd.label = "\u67E5\u770B\u8BE6\u60C5";
	cmd.template = "";
	cmd.context_args = "*";

	cmd = initData.AddCommand();
	cmd.name = "WifiEditPassword";
	cmd.method = "OnWifiEditPassword";
	cmd.desc = "\u7F16\u8F91\u9009\u4E2AWiFi\u7684\u5BC6\u7801";
	cmd.label = "\u7F16\u8F91\u5BC6\u7801";
	cmd.template = "";
	cmd.context_args = "*";

	cmd = initData.AddCommand();
	cmd.name = "WifiCopyPassword";
	cmd.method = "OnWifiCopyPassword";
	cmd.desc = "\u590D\u5236\u9009\u4E2AWiFi\u7684\u5BC6\u7801\u5230\u526A\u8D34\u677F";
	cmd.label = "\u590D\u5236\u5BC6\u7801";
	cmd.template = "";
	cmd.context_args = "*";

	cmd = initData.AddCommand();
	cmd.name = "WifiCopyName";
	cmd.method = "OnWifiCopyName";
	cmd.desc = "\u590D\u5236\u9009\u4E2AWiFi\u7684\u540D\u79F0\u5230\u526A\u8D34\u677F";
	cmd.label = "\u590D\u5236\u540D\u79F0";
	cmd.template = "";
	cmd.context_args = "*";

	cmd = initData.AddCommand();
	cmd.name = "WifiConnect";
	cmd.method = "OnWifiConnect";
	cmd.desc = "\u8FDE\u63A5\u5230\u9009\u4E2D\u7684WiFi\u7F51\u7EDC";
	cmd.label = "\u8FDE\u63A5\u7F51\u7EDC";
	cmd.template = "";
	cmd.context_args = "*";

	cmd = initData.AddCommand();
	cmd.name = "WifiForget";
	cmd.method = "OnWifiForget";
	cmd.desc = "\u5FD8\u8BB0(\u5220\u9664)\u9009\u4E2D\u7684WiFi\u7F51\u7EDC";
	cmd.label = "\u5FD8\u8BB0\u7F51\u7EDC";
	cmd.template = "";
	cmd.context_args = "*";

	cmd = initData.AddCommand();
	cmd.name = "WifiRefresh";
	cmd.method = "OnWifiRefresh";
	cmd.desc = "\u5237\u65B0WiFi\u5217\u8868";
	cmd.label = "\u5237\u65B0\u5217\u8868";
	cmd.template = "";
	cmd.context_args = "*";
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

function RunNetshCommand(args)
{
	var shell = new ActiveXObject("WScript.Shell");
	var fso = new ActiveXObject("Scripting.FileSystemObject");
	var tempFile = fso.GetSpecialFolder(2) + "\\wifivfs_" + new Date().getTime() + ".txt";

	try {
		var fullCmd = 'cmd.exe /c chcp 65001 >nul & netsh ' + args + ' > "' + tempFile + '" 2>&1';
		Log(LOG_DEBUG, "RunNetshCommand: " + fullCmd);

		var exitCode = shell.Run(fullCmd, 0, true);

		var output = "";
		if (fso.FileExists(tempFile)) {
			var ts = fso.OpenTextFile(tempFile, 1, false, 0);
			if (!ts.AtEndOfStream) output = ts.ReadAll();
			ts.Close();
			fso.DeleteFile(tempFile, true);
		}

		return { exitCode: exitCode, output: output };
	} catch (e) {
		if (fso.FileExists(tempFile)) {
			try { fso.DeleteFile(tempFile, true); } catch (de) {}
		}
		Log(LOG_ERROR, "RunNetshCommand error: " + e.message);
		return { exitCode: -1, output: "" };
	}
}

function LoadProfileDetail(ssid)
{
	if (g_detailCache[ssid]) return g_detailCache[ssid];

	var result = RunNetshCommand('wlan show profile name="' + ssid + '" key=clear');
	var output = result.output;
	if (!output || output.length == 0) return null;

	var info = {
		ssid: ssid,
		password: "",
		hasPassword: false,
		authType: "",
		cipher: "",
		connectionMode: "",
		connectionType: ""
	};

	var lines = output.split(/\r?\n/);
	var currentSection = "";

	for (var i = 0; i < lines.length; i++) {
		var line = lines[i];

		if (line.indexOf("\u5B89\u5168\u8BBE\u7F6E") >= 0 || line.indexOf("Security settings") >= 0) {
			currentSection = "security";
			continue;
		}
		if (line.indexOf("\u8FDE\u63A5\u8BBE\u7F6E") >= 0 || line.indexOf("Connection settings") >= 0) {
			currentSection = "connection";
			continue;
		}

		var colonPos = line.indexOf(" : ");
		if (colonPos < 0) continue;

		var key = line.substring(0, colonPos).replace(/^\s+|\s+$/g, "");
		var val = line.substring(colonPos + 3).replace(/^\s+|\s+$/g, "");

		if (currentSection == "security") {
			if (key.indexOf("\u8EAB\u4EFD\u9A8C\u8BC1") >= 0 || key.indexOf("Authentication") >= 0) {
				info.authType = val;
			} else if (key.indexOf("\u52A0\u5BC6") >= 0 || key.indexOf("Cipher") >= 0) {
				info.cipher = val;
			} else if (key.indexOf("\u5B89\u5168\u5BC6\u94A5") >= 0 || key.indexOf("Security key") >= 0) {
				info.hasPassword = (val == "\u5B58\u5728" || val == "Present");
			} else if (key.indexOf("\u5BC6\u94A5\u5185\u5BB9") >= 0 || key.indexOf("Key Content") >= 0) {
				info.password = val;
				info.hasPassword = true;
			}
		}

		if (currentSection == "connection") {
			if (key.indexOf("\u8FDE\u63A5\u6A21\u5F0F") >= 0 || key.indexOf("Connection mode") >= 0) {
				info.connectionMode = val;
			} else if (key.indexOf("\u7F51\u7EDC\u7C7B\u578B") >= 0 || key.indexOf("Network type") >= 0) {
				info.connectionType = val;
			}
		}
	}

	g_detailCache[ssid] = info;
	return info;
}

function OnButtonContext(buttonContextData)
{
	var lister = buttonContextData.lister;
	if (!lister) return;

	var cmdName = buttonContextData.method;
	var tab = lister.activetab;
	var hasSelection = (tab && tab.selected.count > 0);

	var disabled = false;

	if (cmdName == "OnWifiViewDetail") {
		disabled = !hasSelection;
	} else if (cmdName == "OnWifiEditPassword") {
		disabled = !hasSelection;
	} else if (cmdName == "OnWifiCopyPassword") {
		disabled = !hasSelection;
	} else if (cmdName == "OnWifiCopyName") {
		disabled = !hasSelection;
	} else if (cmdName == "OnWifiConnect") {
		disabled = !hasSelection;
	} else if (cmdName == "OnWifiForget") {
		disabled = !hasSelection;
	} else if (cmdName == "OnWifiRefresh") {
		disabled = false;
	}

	buttonContextData.ctx.disabled = disabled;
}

function OnWifiViewDetail(scriptCmdData)
{
	Log(LOG_INFO, "OnWifiViewDetail invoked");
	var func = scriptCmdData.func;
	var tab = func.sourcetab;

	if (tab.selected.count == 0) {
		func.Dlg.Request("\u8BF7\u5148\u9009\u62E9\u4E00\u4E2AWiFi\u7F51\u7EDC", "OK", "\u63D0\u793A");
		return;
	}

	var item = tab.selected(0);
	var ssid = String(item.name);

	var profileInfo = LoadProfileDetail(ssid);
	if (!profileInfo) {
		func.Dlg.Request("\u627E\u4E0D\u5230WiFi\u4FE1\u606F: " + ssid, "OK", "\u9519\u8BEF");
		return;
	}

	var info = "WiFi\u540D\u79F0 (SSID):\t" + profileInfo.ssid + "\n";
	info += "\u5BC6\u7801:\t\t\t" + (profileInfo.hasPassword ? profileInfo.password : "(\u65E0\u5BC6\u7801/\u5F00\u653E\u7F51\u7EDC)") + "\n";
	info += "\u8EAB\u4EFD\u9A8C\u8BC1:\t\t" + (profileInfo.authType || "\u672A\u77E5") + "\n";
	info += "\u52A0\u5BC6\u65B9\u5F0F:\t\t" + (profileInfo.cipher || "\u672A\u77E5") + "\n";
	info += "\u8FDE\u63A5\u6A21\u5F0F:\t\t" + (profileInfo.connectionMode || "\u672A\u77E5") + "\n";
	info += "\u7F51\u7EDC\u7C7B\u578B:\t\t" + (profileInfo.connectionType || "\u672A\u77E5");

	func.Dlg.Request(info, "OK", "WiFi\u8BE6\u60C5 - " + ssid);
}

function OnWifiEditPassword(scriptCmdData)
{
	Log(LOG_INFO, "OnWifiEditPassword invoked");
	var func = scriptCmdData.func;
	var tab = func.sourcetab;

	if (tab.selected.count == 0) {
		func.Dlg.Request("\u8BF7\u5148\u9009\u62E9\u4E00\u4E2AWiFi\u7F51\u7EDC", "OK", "\u63D0\u793A");
		return;
	}

	var item = tab.selected(0);
	var ssid = String(item.name);

	var profileInfo = LoadProfileDetail(ssid);
	if (!profileInfo) {
		func.Dlg.Request("\u627E\u4E0D\u5230WiFi\u4FE1\u606F: " + ssid, "OK", "\u9519\u8BEF");
		return;
	}

	var currentPwd = profileInfo.hasPassword ? profileInfo.password : "";
	var newPwd = func.Dlg.Input("\u8F93\u5165\u65B0\u5BC6\u7801:", "\u7F16\u8F91WiFi\u5BC6\u7801 - " + ssid, currentPwd);
	if (newPwd == null || newPwd == currentPwd) return;

	if (newPwd.length == 0) {
		func.Dlg.Request("\u5BC6\u7801\u4E0D\u80FD\u4E3A\u7A7A", "OK", "\u63D0\u793A");
		return;
	}

	var result = RunNetshCommand('wlan add profile name="' + ssid + '" ssid="' + ssid + '" key="' + newPwd + '" keyUsage=persistent');
	g_detailCache = {};

	if (result.exitCode == 0) {
		func.Dlg.Request("WiFi\u5BC6\u7801\u5DF2\u66F4\u65B0\u3002", "OK", "\u6210\u529F");
		RefreshTab(tab);
	} else {
		func.Dlg.Request("\u66F4\u65B0WiFi\u5BC6\u7801\u5931\u8D25\uFF0C\u8BF7\u786E\u8BA4\u60A8\u6709\u8DB3\u591F\u7684\u6743\u9650\u3002", "OK", "\u9519\u8BEF");
	}
}

function OnWifiCopyPassword(scriptCmdData)
{
	Log(LOG_INFO, "OnWifiCopyPassword invoked");
	var func = scriptCmdData.func;
	var tab = func.sourcetab;

	if (tab.selected.count == 0) {
		func.Dlg.Request("\u8BF7\u5148\u9009\u62E9\u4E00\u4E2AWiFi\u7F51\u7EDC", "OK", "\u63D0\u793A");
		return;
	}

	var item = tab.selected(0);
	var ssid = String(item.name);

	var profileInfo = LoadProfileDetail(ssid);
	if (!profileInfo) {
		func.Dlg.Request("\u627E\u4E0D\u5230WiFi\u4FE1\u606F: " + ssid, "OK", "\u9519\u8BEF");
		return;
	}

	var text = profileInfo.hasPassword ? profileInfo.password : "";
	DOpus.SetClip(text);
	Log(LOG_INFO, "OnWifiCopyPassword: copied password for '" + ssid + "'");
}

function OnWifiCopyName(scriptCmdData)
{
	Log(LOG_INFO, "OnWifiCopyName invoked");
	var func = scriptCmdData.func;
	var tab = func.sourcetab;

	if (tab.selected.count == 0) {
		func.Dlg.Request("\u8BF7\u5148\u9009\u62E9\u4E00\u4E2AWiFi\u7F51\u7EDC", "OK", "\u63D0\u793A");
		return;
	}

	var item = tab.selected(0);
	var ssid = String(item.name);

	DOpus.SetClip(ssid);
	Log(LOG_INFO, "OnWifiCopyName: copied name '" + ssid + "'");
}

function OnWifiConnect(scriptCmdData)
{
	Log(LOG_INFO, "OnWifiConnect invoked");
	var func = scriptCmdData.func;
	var tab = func.sourcetab;

	if (tab.selected.count == 0) {
		func.Dlg.Request("\u8BF7\u5148\u9009\u62E9\u4E00\u4E2AWiFi\u7F51\u7EDC", "OK", "\u63D0\u793A");
		return;
	}

	var item = tab.selected(0);
	var ssid = String(item.name);

	var result = RunNetshCommand('wlan connect name="' + ssid + '"');

	if (result.exitCode == 0) {
		func.Dlg.Request("\u6B63\u5728\u8FDE\u63A5\u5230 \"" + ssid + "\"...", "OK", "\u8FDE\u63A5WiFi");
	} else {
		func.Dlg.Request("\u8FDE\u63A5\u5230 \"" + ssid + "\" \u5931\u8D25\u3002", "OK", "\u8FDE\u63A5\u5931\u8D25");
	}
}

function OnWifiForget(scriptCmdData)
{
	Log(LOG_INFO, "OnWifiForget invoked");
	var func = scriptCmdData.func;
	var tab = func.sourcetab;

	if (tab.selected.count == 0) {
		func.Dlg.Request("\u8BF7\u5148\u9009\u62E9\u4E00\u4E2AWiFi\u7F51\u7EDC", "OK", "\u63D0\u793A");
		return;
	}

	var item = tab.selected(0);
	var ssid = String(item.name);

	var confirmResult = func.Dlg.Request(
		"\u786E\u5B9A\u8981\u5FD8\u8BB0WiFi\u7F51\u7EDC \"" + ssid + "\" \u5417\uFF1F\n\n\u6B64\u64CD\u4F5C\u5C06\u4ECE\u7CFB\u7EDF\u4E2D\u5220\u9664\u8BE5WiFi\u914D\u7F6E\u6587\u4EF6\u3002",
		"\u786E\u5B9A|\u53D6\u6D88",
		"\u786E\u8BA4\u5FD8\u8BB0\u7F51\u7EDC"
	);

	if (confirmResult == 0) {
		Log(LOG_INFO, "OnWifiForget: user cancelled");
		return;
	}

	var result = RunNetshCommand('wlan delete profile name="' + ssid + '"');
	g_detailCache = {};

	if (result.exitCode == 0) {
		RefreshTab(tab);
	} else {
		func.Dlg.Request("\u5220\u9664WiFi\u914D\u7F6E\u6587\u4EF6\u5931\u8D25\uFF0C\u8BF7\u786E\u8BA4\u60A8\u6709\u8DB3\u591F\u7684\u6743\u9650\u3002", "OK", "\u9519\u8BEF");
	}
}

function OnWifiRefresh(scriptCmdData)
{
	Log(LOG_INFO, "OnWifiRefresh invoked");
	var func = scriptCmdData.func;
	var tab = func.sourcetab;

	g_detailCache = {};
	RefreshTab(tab);
}

function RefreshTab(tab)
{
	var cmd = DOpus.Create.Command();
	cmd.SetSourceTab(tab);
	cmd.RunCommand("Go REFRESH");
}
