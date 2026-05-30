function OnInit(initData)
{
	initData.name = "ServicesVFSTest";
	initData.version = "1.0.0";
	initData.copyright = "(c) 2026";
	initData.desc = "Tests for ServicesVFS script";
	initData.default_enable = false;
	initData.min_version = "12.0";

	var cmd = initData.AddCommand();
	cmd.name = "TestServicesVFS";
	cmd.method = "OnTestServicesVFS";
	cmd.desc = "Run tests for ServicesVFS";
	cmd.label = "Test ServicesVFS";
	cmd.template = "";
}

function OnTestServicesVFS(scriptCmdData)
{
	var func = scriptCmdData.func;
	var results = [];
	var passed = 0;
	var failed = 0;

	results.push("=== ServicesVFS 测试套件 ===");
	results.push("");

	results.push("1. 测试 GetServiceStatusText 函数:");
	if (testGetServiceStatusText()) {
		results.push("   ✓ 所有状态转换测试通过");
		passed++;
	} else {
		results.push("   ✗ 状态转换测试失败");
		failed++;
	}

	results.push("");
	results.push("2. 测试 GetServiceStartTypeText 函数:");
	if (testGetServiceStartTypeText()) {
		results.push("   ✓ 所有启动类型转换测试通过");
		passed++;
	} else {
		results.push("   ✗ 启动类型转换测试失败");
		failed++;
	}

	results.push("");
	results.push("3. 测试 GetServiceTypeText 函数:");
	if (testGetServiceTypeText()) {
		results.push("   ✓ 所有服务类型转换测试通过");
		passed++;
	} else {
		results.push("   ✗ 服务类型转换测试失败");
		failed++;
	}

	results.push("");
	results.push("4. 测试 EscapeWMI 函数:");
	if (testEscapeWMI()) {
		results.push("   ✓ WMI转义测试通过");
		passed++;
	} else {
		results.push("   ✗ WMI转义测试失败");
		failed++;
	}

	results.push("");
	results.push("5. 测试 GetServiceInfo 函数:");
	if (testGetServiceInfo()) {
		results.push("   ✓ 服务信息获取测试通过");
		passed++;
	} else {
		results.push("   ✗ 服务信息获取测试失败");
		failed++;
	}

	results.push("");
	results.push("=== 测试结果汇总 ===");
	results.push("通过: " + passed + " / " + (passed + failed));
	results.push("失败: " + failed + " / " + (passed + failed));
	results.push("覆盖率: " + Math.round((passed / (passed + failed)) * 100) + "%");

	if (failed > 0) {
		func.Dlg.Request(results.join("\n"), "OK", "测试失败");
	} else {
		func.Dlg.Request(results.join("\n"), "OK", "所有测试通过");
	}
}

function testGetServiceStatusText()
{
	var tests = [
		{ input: 4, expected: "运行中" },
		{ input: 1, expected: "已停止" },
		{ input: 7, expected: "已暂停" },
		{ input: 2, expected: "正在启动" },
		{ input: 3, expected: "正在停止" },
		{ input: 6, expected: "正在暂停" },
		{ input: 5, expected: "正在恢复" },
		{ input: 999, expected: "未知" }
	];

	for (var i = 0; i < tests.length; i++) {
		var result = GetServiceStatusText(tests[i].input);
		if (result !== tests[i].expected) {
			DOpus.Output("StatusText test failed: input=" + tests[i].input + ", expected='" + tests[i].expected + "', got='" + result + "'");
			return false;
		}
	}
	return true;
}

function testGetServiceStartTypeText()
{
	var tests = [
		{ input: 2, expected: "自动" },
		{ input: 3, expected: "手动" },
		{ input: 4, expected: "已禁用" },
		{ input: 0, expected: "启动" },
		{ input: 1, expected: "系统" },
		{ input: 999, expected: "未知" }
	];

	for (var i = 0; i < tests.length; i++) {
		var result = GetServiceStartTypeText(tests[i].input);
		if (result !== tests[i].expected) {
			DOpus.Output("StartTypeText test failed: input=" + tests[i].input + ", expected='" + tests[i].expected + "', got='" + result + "'");
			return false;
		}
	}
	return true;
}

function testGetServiceTypeText()
{
	var tests = [
		{ input: 0x10, expected: "独立进程" },
		{ input: 0x20, expected: "共享进程" },
		{ input: 0x1, expected: "内核驱动" },
		{ input: 0x2, expected: "文件系统驱动" },
		{ input: 0x110, expected: "交互式独立进程" },
		{ input: 0x120, expected: "交互式共享进程" },
		{ input: 0x11, expected: "其他" }
	];

	for (var i = 0; i < tests.length; i++) {
		var result = GetServiceTypeText(tests[i].input);
		if (result !== tests[i].expected) {
			DOpus.Output("TypeText test failed: input=0x" + tests[i].input.toString(16) + ", expected='" + tests[i].expected + "', got='" + result + "'");
			return false;
		}
	}
	return true;
}

function testEscapeWMI()
{
	var tests = [
		{ input: "Test", expected: "Test" },
		{ input: "Test\\Path", expected: "Test\\\\Path" },
		{ input: "Test'Value", expected: "Test\\'Value" },
		{ input: "Test\\'Value", expected: "Test\\\\\\'Value" },
		{ input: "", expected: "" }
	];

	for (var i = 0; i < tests.length; i++) {
		var result = EscapeWMI(tests[i].input);
		if (result !== tests[i].expected) {
			DOpus.Output("EscapeWMI test failed: input='" + tests[i].input + "', expected='" + tests[i].expected + "', got='" + result + "'");
			return false;
		}
	}
	return true;
}

function testGetServiceInfo()
{
	var serviceInfo = GetServiceInfo("wuauserv");
	if (!serviceInfo) {
		DOpus.Output("GetServiceInfo test failed: could not get wuauserv info");
		return false;
	}
	
	if (serviceInfo.name !== "wuauserv") {
		DOpus.Output("GetServiceInfo test failed: name mismatch");
		return false;
	}
	
	if (serviceInfo.displayName === undefined) {
		DOpus.Output("GetServiceInfo test failed: displayName undefined");
		return false;
	}
	
	if (serviceInfo.status !== 1 && serviceInfo.status !== 4) {
		DOpus.Output("GetServiceInfo test failed: invalid status");
		return false;
	}
	
	return true;
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
	if ((type & 0x10) != 0) type = type & ~0x10;
	switch (type) {
		case 0x10: return "独立进程";
		case 0x20: return "共享进程";
		case 0x1: return "内核驱动";
		case 0x2: return "文件系统驱动";
		case 0x110: return "交互式独立进程";
		case 0x120: return "交互式共享进程";
		default: return "其他";
	}
}

function EscapeWMI(str)
{
	return str.replace(/\\/g, "\\\\").replace(/'/g, "\\'");
}

function GetServiceInfo(serviceName)
{
	try {
		var objWMIService = GetObject("winmgmts:{impersonationLevel=impersonate}!\\\\.\\root\\cimv2");
		var colServices = objWMIService.ExecQuery("SELECT * FROM Win32_Service WHERE Name = '" + EscapeWMI(serviceName) + "'");
		
		var enumServices = new Enumerator(colServices);
		if (enumServices.atEnd()) {
			DOpus.Output("Service not found: " + serviceName);
			return null;
		}
		
		var service = enumServices.item();
		return {
			name: service.Name,
			displayName: service.DisplayName,
			status: service.State == "Running" ? 4 : (service.State == "Stopped" ? 1 : (service.State == "Paused" ? 7 : 1)),
			startType: service.StartMode == "Auto" ? 2 : (service.StartMode == "Manual" ? 3 : (service.StartMode == "Disabled" ? 4 : 3)),
			type: service.ServiceType,
			description: service.Description,
			processId: parseInt(service.ProcessId) || 0,
			exitCode: parseInt(service.ExitCode) || 0
		};
	} catch (e) {
		DOpus.Output("GetServiceInfo error: " + e.message);
		return null;
	}
}