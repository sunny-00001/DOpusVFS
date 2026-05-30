var g_shell = null;
var g_sysEnv = null;
var g_userEnv = null;
var g_procEnv = null;

function OnInit(initData)
{
	initData.name = "EnvColumn";
	initData.version = "1.4.1";
	initData.copyright = "(c) 2025";
	initData.desc = "Enhanced custom columns and commands for EnvVFS plugin";
	initData.default_enable = true;
	initData.min_version = "12.0";

	var cmd;

	cmd = initData.AddCommand();
	cmd.name = "EnvOpen";
	cmd.method = "OnEnvOpen";
	cmd.desc = "Open environment variable editor";
	cmd.label = "Open Env";
	cmd.template = "";

	cmd = initData.AddCommand();
	cmd.name = "EnvNew";
	cmd.method = "OnEnvNew";
	cmd.desc = "New user environment variable";
	cmd.label = "New Env";
	cmd.template = "";

	cmd = initData.AddCommand();
	cmd.name = "EnvDelete";
	cmd.method = "OnEnvDelete";
	cmd.desc = "Delete selected environment variable";
	cmd.label = "Delete Env";
	cmd.template = "";

	cmd = initData.AddCommand();
	cmd.name = "EnvNewEntry";
	cmd.method = "OnEnvNewEntry";
	cmd.desc = "Add new path entry to multi-value environment variable";
	cmd.label = "New Path Entry";
	cmd.template = "";

	var col;

	col = initData.AddColumn();
	col.name = "EnvValue";
	col.method = "OnEnvColumn";
	col.label = "Env Value";
	col.header = "Value";
	col.justify = "left";
	col.autogroup = true;
	col.multicol = true;

	col = initData.AddColumn();
	col.name = "EnvType";
	col.method = "OnEnvColumn";
	col.label = "Env Type";
	col.header = "Type";
	col.justify = "left";
	col.autogroup = true;
	col.multicol = true;

	col = initData.AddColumn();
	col.name = "EnvSource";
	col.method = "OnEnvColumn";
	col.label = "Env Source";
	col.header = "Source";
	col.justify = "left";
	col.autogroup = true;
	col.multicol = true;

	col = initData.AddColumn();
	col.name = "EnvIsSystem";
	col.method = "OnEnvColumn";
	col.label = "System Var";
	col.header = "System";
	col.justify = "center";
	col.autogroup = true;
	col.multicol = true;

	col = initData.AddColumn();
	col.name = "EnvValueLength";
	col.method = "OnEnvColumn";
	col.label = "Value Length";
	col.header = "Length";
	col.justify = "right";
	col.autogroup = true;
	col.multicol = true;
}

function OnEnvColumn(colData)
{
	var item = colData.item;
	if (!item) return;

	var fileName = item.name;
	if (!fileName) return;

	var envName = fileName;
	var isSubEntry = false;
	var isFolder = false;

	if (envName.length > 5 && envName.substring(envName.length - 5).toLowerCase() == ".path") {
		envName = envName.substring(0, envName.length - 5);
		isSubEntry = true;
	} else if (envName.length > 4 && envName.substring(envName.length - 4).toLowerCase() == ".env") {
		envName = envName.substring(0, envName.length - 4);
	}

	if (isSubEntry) {
		var path = item.path;
		var parentDir = path.substring(0, path.lastIndexOf("\\"));
		var parentName = parentDir.substring(parentDir.lastIndexOf("\\") + 1);
		
		var parentInfo = GetEnvVariableInfo(parentName);
		if (!parentInfo) return;

		var entries = parentInfo.value.split(";");
		var entryIdx = parseInt(envName);
		if (isNaN(entryIdx) || entryIdx < 1 || entryIdx > entries.length) return;
		
		var entryValue = entries[entryIdx - 1];

		if (colData.columns.exists("EnvValue")) {
			var displayValue = entryValue;
			if (displayValue.length > 100) displayValue = displayValue.substring(0, 100) + "...";
			colData.columns("EnvValue").value = displayValue;
			colData.columns("EnvValue").sort = entryValue;
		}

		if (colData.columns.exists("EnvType")) {
			colData.columns("EnvType").value = "Path Entry";
			colData.columns("EnvType").sort = "2";
			colData.columns("EnvType").group = "Path Entry";
		}

		if (colData.columns.exists("EnvSource")) {
			colData.columns("EnvSource").value = parentInfo.source;
			colData.columns("EnvSource").sort = parentInfo.source;
			colData.columns("EnvSource").group = parentInfo.source;
		}

		if (colData.columns.exists("EnvIsSystem")) {
			var isSystemText = parentInfo.isSystem ? "Yes" : "No";
			colData.columns("EnvIsSystem").value = isSystemText;
			colData.columns("EnvIsSystem").sort = parentInfo.isSystem ? "1" : "0";
			colData.columns("EnvIsSystem").group = isSystemText;
		}

		if (colData.columns.exists("EnvValueLength")) {
			var length = entryValue.length;
			colData.columns("EnvValueLength").value = length.toString();
			colData.columns("EnvValueLength").sort = length;
		}
		return;
	}

	var envInfo = GetEnvVariableInfo(envName);
	if (!envInfo) return;

	var isMultiValue = envInfo.value.indexOf(";") >= 0;

	if (isMultiValue && !fileName.match(/\.env$/i)) {
		isFolder = true;
	}

	if (colData.columns.exists("EnvValue")) {
		if (isFolder) {
			var entries = envInfo.value.split(";");
			var count = 0;
			for (var i = 0; i < entries.length; i++) {
				if (entries[i].length > 0) count++;
			}
			colData.columns("EnvValue").value = count + " entries";
			colData.columns("EnvValue").sort = envInfo.value;
			colData.columns("EnvValue").group = "Multi-Value";
		} else {
			var displayValue = envInfo.value;
			if (displayValue.length > 100) displayValue = displayValue.substring(0, 100) + "...";
			colData.columns("EnvValue").value = displayValue;
			colData.columns("EnvValue").sort = envInfo.value;
			colData.columns("EnvValue").group = envInfo.value.length > 50 ? "Long" : "Short";
		}
	}

	if (colData.columns.exists("EnvType")) {
		colData.columns("EnvType").value = envInfo.typeName;
		colData.columns("EnvType").sort = envInfo.type;
		colData.columns("EnvType").group = envInfo.typeName;
	}

	if (colData.columns.exists("EnvSource")) {
		colData.columns("EnvSource").value = envInfo.source;
		colData.columns("EnvSource").sort = envInfo.source;
		colData.columns("EnvSource").group = envInfo.source;
	}

	if (colData.columns.exists("EnvIsSystem")) {
		var isSystemText = envInfo.isSystem ? "Yes" : "No";
		colData.columns("EnvIsSystem").value = isSystemText;
		colData.columns("EnvIsSystem").sort = envInfo.isSystem ? "1" : "0";
		colData.columns("EnvIsSystem").group = isSystemText;
	}

	if (colData.columns.exists("EnvValueLength")) {
		var length = envInfo.value.length;
		colData.columns("EnvValueLength").value = length.toString();
		colData.columns("EnvValueLength").sort = length;
		if (length < 50) colData.columns("EnvValueLength").group = "Short(<50)";
		else if (length < 200) colData.columns("EnvValueLength").group = "Medium(50-200)";
		else if (length < 1000) colData.columns("EnvValueLength").group = "Long(200-1000)";
		else colData.columns("EnvValueLength").group = "VeryLong(>1000)";
	}
}

function EnsureShell()
{
	if (!g_shell) {
		g_shell = new ActiveXObject("WScript.Shell");
		g_sysEnv = g_shell.Environment("System");
		g_userEnv = g_shell.Environment("User");
		g_procEnv = g_shell.Environment("Process");
	}
}

function GetEnvVariableInfo(name)
{
	EnsureShell();

	var sysValue = "", userValue = "", procValue = "";
	try {
		sysValue  = g_sysEnv(name)  || "";
		userValue = g_userEnv(name) || "";
		procValue = g_procEnv(name) || "";
	} catch(e) {
		return null;
	}

	if (sysValue && String(sysValue) != "") {
		return { name: name, value: String(sysValue), type: 0, typeName: "String", source: "System", isSystem: true };
	}
	if (userValue && String(userValue) != "") {
		return { name: name, value: String(userValue), type: 0, typeName: "String", source: "User", isSystem: false };
	}
	if (procValue && String(procValue) != "") {
		return { name: name, value: String(procValue), type: 0, typeName: "String", source: "Process", isSystem: false };
	}
	return null;
}

function OnEnvOpen(scriptCmdData)
{
	var func = scriptCmdData.func;
	var tab = func.sourcetab;

	if (tab.selected.count == 0) {
		EnsureShell();
		g_shell.Run("rundll32.exe sysdm.cpl,EditEnvironmentVariables", 1, false);
		return;
	}

	var item = tab.selected(0);
	var fileName = item.name;
	var envName = fileName;
	var isSubEntry = false;

	if (envName.length > 5 && envName.substring(envName.length - 5).toLowerCase() == ".path") {
		envName = envName.substring(0, envName.length - 5);
		isSubEntry = true;
	} else if (envName.length > 4 && envName.substring(envName.length - 4).toLowerCase() == ".env") {
		envName = envName.substring(0, envName.length - 4);
	}

	if (isSubEntry) {
		var path = item.path;
		var parentDir = path.substring(0, path.lastIndexOf("\\"));
		var parentName = parentDir.substring(parentDir.lastIndexOf("\\") + 1);

		var parentInfo = GetEnvVariableInfo(parentName);
		if (!parentInfo) {
			func.Dlg.Request("Parent environment variable not found:\n" + parentName, "OK", "Error");
			return;
		}

		var entries = parentInfo.value.split(";");
		var entryIdx = parseInt(envName);
		if (isNaN(entryIdx) || entryIdx < 1 || entryIdx > entries.length) {
			func.Dlg.Request("Invalid path entry index: " + envName, "OK", "Error");
			return;
		}

		var entryValue = entries[entryIdx - 1];

		var dlg = func.Dlg;
		dlg.template = "EnvEditDialog";
		dlg.title = "Edit " + parentName + " [" + entryIdx + "/" + entries.length + "] (" + parentInfo.source + ")";
		dlg.window = tab.lister;
		dlg.detach = true;
		dlg.Create();

		dlg.Control("env_name").value = parentName + " [" + entryIdx + "]";
		dlg.Control("env_value").value = entryValue;

		dlg.AddHotkey("enter", "enter");
		dlg.AddHotkey("esc", "escape");
		dlg.Show();

		var result = 0;
		var savedValue = "";
		for (var msg = dlg.GetMsg(); msg.result; msg = dlg.GetMsg()) {
			if (msg.event == "click") {
				if (msg.control == "btn_ok")     { result = 1; savedValue = dlg.Control("env_value").value; dlg.EndDlg(1); }
				if (msg.control == "btn_cancel") { result = 0; dlg.EndDlg(0); }
			} else if (msg.event == "hotkey") {
				if (msg.control == "enter")  { result = 1; savedValue = dlg.Control("env_value").value; dlg.EndDlg(1); }
				if (msg.control == "escape") { result = 0; dlg.EndDlg(0); }
			}
		}

		if (result == 1) {
			var newValue = savedValue;
			if (newValue !== entryValue) {
				entries[entryIdx - 1] = newValue;
				var combined = entries.join(";");
				try {
					EnsureShell();
				var envObj = g_shell.Environment(parentInfo.isSystem ? "System" : "User");
				envObj(parentName) = combined;
					var cmd = DOpus.Create.Command();
					cmd.SetSourceTab(tab);
					cmd.RunCommand("Go REFRESH");
				} catch (e) {
					func.Dlg.Request("Failed to update!\nError: " + e.description, "OK", "Error");
				}
			}
		}
		return;
	}

	var envInfo = GetEnvVariableInfo(envName);
	if (!envInfo) {
		func.Dlg.Request("Environment variable not found:\n" + envName, "OK", "Error");
		return;
	}

	if (envInfo.value.indexOf(";") >= 0 && !fileName.match(/\.env$/i)) {
		func.Dlg.Request("This is a multi-value variable folder.\nDouble-click to enter the folder, then edit individual entries.", "OK", "Info");
		return;
	}

	var dlg = func.Dlg;
	dlg.template = "EnvEditDialog";
	dlg.title = "Edit " + envName + " (" + envInfo.source + ")";
	dlg.window = tab.lister;
	dlg.detach = true;
	dlg.Create();

	dlg.Control("env_name").value = envName;
	dlg.Control("env_value").value = envInfo.value;

	dlg.AddHotkey("enter", "enter");
	dlg.AddHotkey("esc", "escape");
	dlg.Show();

	var result = 0;
	var savedValue = "";
	for (var msg = dlg.GetMsg(); msg.result; msg = dlg.GetMsg()) {
		if (msg.event == "click") {
			if (msg.control == "btn_ok")     { result = 1; savedValue = dlg.Control("env_value").value; dlg.EndDlg(1); }
			if (msg.control == "btn_cancel") { result = 0; dlg.EndDlg(0); }
		} else if (msg.event == "hotkey") {
			if (msg.control == "enter")  { result = 1; savedValue = dlg.Control("env_value").value; dlg.EndDlg(1); }
			if (msg.control == "escape") { result = 0; dlg.EndDlg(0); }
		}
	}

	if (result == 1) {
		var newValue = savedValue;
		if (newValue !== envInfo.value) {
			try {
				EnsureShell();
				var envObj = g_shell.Environment(envInfo.isSystem ? "System" : "User");
				if (newValue === "") {
					if (func.Dlg.Request("Value is empty. This will DELETE the variable.\nContinue?", "Yes|No", "Confirm") != 1) return;
					envObj.Remove(envName);
				} else {
					envObj(envName) = newValue;
				}
				var cmd = DOpus.Create.Command();
				cmd.SetSourceTab(tab);
				cmd.RunCommand("Go REFRESH");
			} catch (e) {
				func.Dlg.Request("Failed to update!\nError: " + e.description, "OK", "Error");
			}
		}
	}
}

function OnEnvNew(scriptCmdData)
{
	var func = scriptCmdData.func;
	var tab = func.sourcetab;

	var dlg = func.Dlg;
	dlg.template = "EnvEditDialog";
	dlg.title = "New Environment Variable (User)";
	dlg.window = tab.lister;
	dlg.detach = true;
	dlg.Create();

	dlg.Control("env_name").value = "";
	dlg.Control("env_value").value = "";

	dlg.AddHotkey("enter", "enter");
	dlg.AddHotkey("esc", "escape");
	dlg.Show();

	var result = 0;
	var savedName = "";
	var savedValue = "";
	for (var msg = dlg.GetMsg(); msg.result; msg = dlg.GetMsg()) {
		if (msg.event == "click") {
			if (msg.control == "btn_ok")     { result = 1; savedName = dlg.Control("env_name").value; savedValue = dlg.Control("env_value").value; dlg.EndDlg(1); }
			if (msg.control == "btn_cancel") { result = 0; dlg.EndDlg(0); }
		} else if (msg.event == "hotkey") {
			if (msg.control == "enter")  { result = 1; savedName = dlg.Control("env_name").value; savedValue = dlg.Control("env_value").value; dlg.EndDlg(1); }
			if (msg.control == "escape") { result = 0; dlg.EndDlg(0); }
		}
	}

	if (result == 1) {
		var envName = savedName;
		var envValue = savedValue;
		if (!envName || envName == "") {
			func.Dlg.Request("Variable name cannot be empty!", "OK", "Error");
			return;
		}
		try {
			EnsureShell();
			g_shell.Environment("User")(envName) = envValue;
			var cmd = DOpus.Create.Command();
			cmd.SetSourceTab(tab);
			cmd.RunCommand("Go REFRESH");
		} catch (e) {
			func.Dlg.Request("Failed to create!\nError: " + e.description, "OK", "Error");
		}
	}
}

function OnEnvDelete(scriptCmdData)
{
	var func = scriptCmdData.func;
	var tab = func.sourcetab;
	if (tab.selected.count == 0) return;

	var item = tab.selected(0);
	var fileName = item.name;
	var envName = fileName;
	var isSubEntry = false;

	if (envName.length > 5 && envName.substring(envName.length - 5).toLowerCase() == ".path") {
		envName = envName.substring(0, envName.length - 5);
		isSubEntry = true;
	} else if (envName.length > 4 && envName.substring(envName.length - 4).toLowerCase() == ".env") {
		envName = envName.substring(0, envName.length - 4);
	}

	if (isSubEntry) {
		var path = item.path;
		var parentDir = path.substring(0, path.lastIndexOf("\\"));
		var parentName = parentDir.substring(parentDir.lastIndexOf("\\") + 1);

		var parentInfo = GetEnvVariableInfo(parentName);
		if (!parentInfo) return;

		var entries = parentInfo.value.split(";");
		var entryIdx = parseInt(envName);
		if (isNaN(entryIdx) || entryIdx < 1 || entryIdx > entries.length) return;

		var entryValue = entries[entryIdx - 1];

		if (func.Dlg.Request("Delete path entry from " + parentName + "?\nEntry #" + entryIdx + ": " + entryValue, "Yes|No", "Confirm") != 1) return;

		entries.splice(entryIdx - 1, 1);
		var combined = entries.join(";");

		try {
			EnsureShell();
			var envObj = g_shell.Environment(parentInfo.isSystem ? "System" : "User");
			envObj(parentName) = combined;
			var cmd = DOpus.Create.Command();
			cmd.SetSourceTab(tab);
			cmd.RunCommand("Go REFRESH");
		} catch (e) {
			func.Dlg.Request("Delete failed!\nError: " + e.description, "OK", "Error");
		}
		return;
	}

	var envInfo = GetEnvVariableInfo(envName);
	if (!envInfo) return;

	if (envInfo.value.indexOf(";") >= 0 && !fileName.match(/\.env$/i)) {
		func.Dlg.Request("Cannot delete a multi-value variable folder.\nEnter the folder and delete individual entries.", "OK", "Info");
		return;
	}

	if (func.Dlg.Request("Delete env?\nName: " + envName + "\nSource: " + envInfo.source, "Yes|No", "Confirm") != 1) return;

	try {
		EnsureShell();
		var envObj = g_shell.Environment(envInfo.isSystem ? "System" : "User");
		envObj.Remove(envName);
		var cmd = DOpus.Create.Command();
		cmd.SetSourceTab(tab);
		cmd.RunCommand("Go REFRESH");
	} catch (e) {
		func.Dlg.Request("Delete failed!\nError: " + e.description, "OK", "Error");
	}
}

function OnEnvNewEntry(scriptCmdData)
{
	var func = scriptCmdData.func;
	var tab = func.sourcetab;

	var currentPath = tab.path;
	var pathStr = currentPath + "";
	var envPrefix = "::envvfs::";
	var varName = "";

	if (pathStr.toLowerCase().indexOf(envPrefix) >= 0) {
		var afterPrefix = pathStr.substring(pathStr.toLowerCase().indexOf(envPrefix) + envPrefix.length);
		while (afterPrefix.length > 0 && afterPrefix.charAt(0) == "\\") afterPrefix = afterPrefix.substring(1);
		var slashPos = afterPrefix.indexOf("\\");
		if (slashPos >= 0) {
			varName = afterPrefix.substring(0, slashPos);
		} else {
			varName = afterPrefix;
		}
	}

	if (varName == "") {
		if (tab.selected.count > 0) {
			var item = tab.selected(0);
			var fileName = item.name;
			if (fileName.length > 4 && fileName.substring(fileName.length - 4).toLowerCase() == ".env") {
				fileName = fileName.substring(0, fileName.length - 4);
			}
			var envInfo = GetEnvVariableInfo(fileName);
			if (envInfo && envInfo.value.indexOf(";") >= 0) {
				varName = fileName;
			}
		}
	}

	if (varName == "") {
		func.Dlg.Request("Please navigate into a multi-value variable folder first,\nor select a multi-value variable.", "OK", "Info");
		return;
	}

	var envInfo = GetEnvVariableInfo(varName);
	if (!envInfo) {
		func.Dlg.Request("Variable not found: " + varName, "OK", "Error");
		return;
	}

	if (envInfo.value.indexOf(";") < 0) {
		func.Dlg.Request("This is not a multi-value variable.\nUse EnvNew to create a new variable instead.", "OK", "Info");
		return;
	}

	var dlg = func.Dlg;
	dlg.window = tab.lister;
	dlg.title = "Add Path Entry to " + varName;
	dlg.message = "Enter new path entry:";
	dlg.max = 1024;
	dlg.buttons = "OK|Cancel";
	var ret = dlg.Show;
	if (ret != 1) return;

	var newEntry = dlg.input;
	if (!newEntry || newEntry == "") return;

	var entries = envInfo.value.split(";");
	entries.push(newEntry);
	var combined = entries.join(";");

	try {
		EnsureShell();
		var envObj = g_shell.Environment(envInfo.isSystem ? "System" : "User");
		envObj(varName) = combined;
		var cmd = DOpus.Create.Command();
		cmd.SetSourceTab(tab);
		cmd.RunCommand("Go REFRESH");
	} catch (e) {
		func.Dlg.Request("Add entry failed!\nError: " + e.description, "OK", "Error");
	}
}

==SCRIPT RESOURCES
<resources>
	<resource name="EnvEditDialog" type="dialog">
		<dialog fontsize="0" height="116" lang="english" title="Edit Environment Variable" width="292">
			<languages>
				<language height="96" lang="chs" title="编辑环境变量" width="292" />
			</languages>
			<control title="Variable Name:" type="static" x="4" y="4" width="51" height="12" />
			<control name="env_name" type="edit" x="60" y="4" width="228" height="12" />
			<control title="Variable Value:" type="static" x="4" y="20" width="51" height="12" />
			<control name="env_value" type="edit" multiline="yes" x="60" y="20" width="228" height="56" />
			<control default="yes" name="btn_ok" title="&OK" type="button" x="96" y="80" width="60" height="12" />
			<control name="btn_cancel" title="&Cancel" type="button" x="166" y="80" width="60" height="12" />
		</dialog>
	</resource>
</resources>
