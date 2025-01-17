#include "injector.h"
#include "resource.h"
#include "ReleaseHelper.h"
#include "includes/SimpleIni.h"

#include <cstdio>
#include <string>
#include <iostream>
#include <sstream>
#include <format>
#include <string>

using namespace std;

static CSimpleIni ini;
int FindProcessId(const std::string& processName)
{
	int pid = -1;

	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	PROCESSENTRY32 process;
	ZeroMemory(&process, sizeof(process));
	process.dwSize = sizeof(process);

	if (Process32First(snapshot, &process))
	{
		do
		{
			if (std::string(w2c(process.szExeFile)) == processName)
			{
				pid = process.th32ProcessID;
				break;
			}
		} while (Process32Next(snapshot, &process));
	}

	CloseHandle(snapshot);

	return pid;
}
void WaitForCloseProcess(const std::string& processName)
{
	int pid = FindProcessId(processName);
	if (pid == -1)
		return;

	std::cout << "Found '" << processName << "' process. Waiting for closing..." << std::endl;

#ifdef _DEBUG
	std::stringstream stream;
	stream << "taskkill /F /T /IM " << processName;
	int retval = system(stream.str().c_str());

	std::cout << "Trying to kill process." << std::endl;
#endif

	HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
	DWORD exitCode = 0;
	while (hProc && (GetExitCodeProcess(hProc, &exitCode), exitCode == STILL_ACTIVE)) {
		Sleep(1000);
	}

	if (hProc != NULL)
		CloseHandle(hProc);
}

DWORD GetProcessIdByName(wchar_t *name) {
	PROCESSENTRY32 entry;
	entry.dwSize = sizeof(PROCESSENTRY32);

	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);

	if (Process32First(snapshot, &entry) == TRUE) {
		while (Process32Next(snapshot, &entry) == TRUE) {
			if (_wcsicmp(entry.szExeFile, name) == 0) {
				CloseHandle(snapshot); //thanks to Pvt Comfy
				return entry.th32ProcessID;
			}
		}
	}

	CloseHandle(snapshot);
	return 0;
}

HANDLE CreateYuanShenProc() {
	auto YSPath = ini.GetValue(c2w("Inject"), c2w("GenshinPath"));
	if (YSPath == nullptr) ini.GetValue(c2w("GenshinImpact"), c2w("Path"));
	if (YSPath == nullptr) {
		printf("Failed to found YuanShen path\n");
		system("pause");
		exit(-1);
	}

	printf("YuanShen Path: %s\n", w2c(YSPath));

	STARTUPINFO si;
	PROCESS_INFORMATION pi;

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));

	if (!CreateProcess(NULL, const_cast<wchar_t *>(YSPath), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
		printf("Failed to create YuanShen process");
		exit(-2);
	}
	return pi.hProcess;
}

int main(int argc, wchar_t *argv[]) {
	ini.SetUnicode();
	ini.LoadFile("cfg.ini");
	char *tempDir = getenv("TEMP");
	auto YSPath = ini.GetValue(c2w("Inject"), c2w("GenshinPath"));

	if (!ReleaseLibrary(IDR_DLL1, "Dll", format("{}\\DebuggerBypass.dll", tempDir).c_str())) {
		printf("Failed to release dll\n");
		system("pause");
		return -3;
	}
	if (YSPath == nullptr) ini.GetValue(c2w("GenshinImpact"), c2w("Path"));
	if (YSPath == nullptr) {
		printf("Failed to found YuanShen path\n");
		system("pause");
		exit(-1);
	}

	WaitForCloseProcess(string{ w2c(YSPath) });
	TOKEN_PRIVILEGES priv;
	ZeroMemory(&priv, sizeof(priv));
	HANDLE hToken = NULL;
	if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
		priv.PrivilegeCount = 1;
		priv.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

		if (LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &priv.Privileges[0].Luid))
			AdjustTokenPrivileges(hToken, FALSE, &priv, 0, NULL, NULL);

		CloseHandle(hToken);
	}

	HANDLE hProc = CreateYuanShenProc();
	if (!hProc) {
		DWORD Err = GetLastError();
		printf("Failed to create process: 0x%X\n", Err);
		system("pause");
		return -3;
	}

	printf("[INFO] YuanShen process handle: %p\n", hProc);
	//InjectDll(hProc, c2w(format("{}\\DebuggerBypass.dll", tempDir).c_str()));
	InjectDll(hProc, (wchar_t*)format(L"{}\\DebuggerBypass.dll", c2w(tempDir)).c_str());
	#ifdef _DEBUG
	printf("Wait 10 seconds for YuanShen init...\n");
	Sleep(10000);
	#else
	printf("Wait 30 seconds for YuanShen init...\n");
	Sleep(30000);
	#endif

	while (true) {
		system("cls");

		char dllPathC[256];
		wchar_t *dllPathW;

		printf("Input dlls path you want to inject below:\n");
		cin.getline(dllPathC, 256);

		dllPathW = c2w(dllPathC);
		#ifdef _DEBUG
		InjectDll(hProc, dllPathW);
		#else	
		if (InjectDll(hProc, dllPathW)) Sleep(5000);
		#endif	


		fflush(stdin);
	}

	return 0;
}
