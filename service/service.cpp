#define _CRT_SECURE_NO_WARNINGS

#pragma comment( lib, "kernel32.lib" )
#pragma comment( lib, "advapi32.lib" )
#pragma comment( lib, "shell32.lib" )
#pragma comment( lib, "wtsapi32.lib" )
#pragma comment( lib, "userenv.lib" )
#pragma comment( lib, "shlwapi.lib" )

#define SZAPPNAME		"Simple"
#define SZSERVICENAME		"SimpleService"
#define SZSERVICEDISPLAYNAME	"Simple Service"
#define SZDEPENDENCIES		""

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>
#include <wtsapi32.h>
#include <userenv.h>
#include <tlhelp32.h>
#include <shlwapi.h>
#include <psapi.h>

#include <string>
#include <algorithm>
#include <time.h>

SERVICE_STATUS		ssStatus;
SERVICE_STATUS_HANDLE   sshStatusHandle;
DWORD			dwErr = 0;
BOOL			bDebug = FALSE;
TCHAR			szErr[256];
HANDLE			hServerStopEvent = NULL;

VOID ServiceStart(DWORD dwArgc, LPTSTR* lpszArgv);
VOID ServiceStop();
BOOL ReportStatusToSCMgr(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint);
void AddToMessageLog(LPCTSTR lpszMsg);
VOID WINAPI service_ctrl(DWORD dwCtrlCode);
VOID WINAPI service_main(DWORD dwArgc, LPTSTR* lpszArgv);
VOID CmdInstallService();
VOID CmdRemoveService();
VOID CmdDebugService(int argc, char** argv);
BOOL WINAPI ControlHandler(DWORD dwCtrlType);
LPTSTR GetLastErrorText(LPTSTR lpszBuf, DWORD dwSize);
int CreateProcessOnSession(int sessionId, LPWSTR app);
DWORD GetExplorerToken(PHANDLE phExplorerToken);
BOOL LaunchedSoftware(std::wstring strFileName, std::wstring csParam /* = EMPTY_STRING */);

CRITICAL_SECTION g_cs;
void DebugLog(LPCWSTR fmt, ...)
{
	EnterCriticalSection(&g_cs);
#define PAGE_SIZE 0x1000
	FILE* fp = nullptr;
	if (!fp)
		_wfopen_s(&fp, L"c:\\simpleservice.log", L"a+");
	if (fp)
	{
		va_list args;
		va_start(args, fmt);
		wchar_t fmtBuf[PAGE_SIZE];
		ZeroMemory(fmtBuf, sizeof(fmtBuf));
		_vsnwprintf_s(fmtBuf, PAGE_SIZE - 1, fmt, args);
		OutputDebugStringW(fmtBuf);
		time_t nowtime;
		nowtime = time(NULL);
		struct tm* local;
		local = localtime(&nowtime);
		fwprintf_s(fp, L"[%04d-%02d-%02d %02d:%02d:%02d] %s\n", local->tm_year, local->tm_mon, local->tm_mday,
			local->tm_hour, local->tm_min, local->tm_sec,
			fmtBuf);
		va_end(args);
		fclose(fp);
	}
	LeaveCriticalSection(&g_cs);
}

bool run = false;
DWORD WINAPI ThreadFunc(LPVOID);
VOID ServiceStart(DWORD dwArgc, LPTSTR* lpszArgv)
{
	InitializeCriticalSection(&g_cs);
	DebugLog(L"recv service start");
	run = true;
	HANDLE hThread;
	DWORD  threadId;
	hThread = CreateThread(NULL, 0, ThreadFunc, 0, 0, &threadId);

	if (!ReportStatusToSCMgr(SERVICE_RUNNING, NO_ERROR, 0))
	{
		DebugLog(L"report service running error");
	}
	while (run)
	{
		DebugLog(L"main running, thread id = %d", GetCurrentThreadId());
		Sleep(5000);
	}
	DeleteCriticalSection(&g_cs);
}

wchar_t* ANSIToUnicode(char* str)
{
	int  unicodeLen = ::MultiByteToWideChar(CP_ACP,
		0,
		str,
		-1,
		NULL,
		0);
	wchar_t* pUnicode;
	pUnicode = new  wchar_t[unicodeLen + 1];
	memset(pUnicode, 0, (unicodeLen + 1) * sizeof(wchar_t));
	::MultiByteToWideChar(CP_ACP,
		0,
		str,
		-1,
		(LPWSTR)pUnicode,
		unicodeLen);

	return pUnicode;
}

DWORD WINAPI ThreadFunc(LPVOID p)
{
	//LaunchedSoftware(L"notepad.exe", L"");
	FILE* fp = nullptr;
	wchar_t command[260] = { 0 };
	if (!fp)
		_wfopen_s(&fp, L"C:\\serviceCommand.txt", L"rb");
	if (fp)
	{
		fread(command, 2, 260, fp);
		fclose(fp);
		//DebugLog(L"command = %s", command);
	}

	//wchar_t* wCommand = ANSIToUnicode(command);
	//DebugLog(L"wCommand = %s", wCommand);

	wchar_t app[260] = { 0 };
	wcscpy(app, command);
	CreateProcessOnSession(1, app);
	while (run)
	{
		DebugLog(L"child running, thread id = %d", GetCurrentThreadId());
		Sleep(5000);
	}
	DebugLog(L"child thread quit");
	return 0;
}

VOID ServiceStop()
{
	DebugLog(L"recv service stop");
	run = false;
	if (!ReportStatusToSCMgr(SERVICE_STOPPED, NO_ERROR, 0))
	{
		DebugLog(L"report service running error");
	}
}


int wmain(int argc, wchar_t** argv)
{
	PROCESS_INFORMATION process = { 0 };
	SECURITY_ATTRIBUTES saProcess = { 0 };
	SECURITY_ATTRIBUTES saThread = { 0 };
	saProcess.nLength = sizeof(SECURITY_ATTRIBUTES);
	saThread.nLength = sizeof(SECURITY_ATTRIBUTES);

	STARTUPINFO si = { 0 };
	si.cb = sizeof(STARTUPINFO);
	si.lpDesktop = const_cast<wchar_t*>(L"WinSta0\\Default");
	si.dwFlags = STARTF_USESHOWWINDOW | STARTF_FORCEONFEEDBACK;
	si.wShowWindow = SW_SHOW;
	const wchar_t* command = L"C:\\??\\baretail.exe";
	wchar_t app[260] = { 0 };
	wcscpy(app, command);
	if (!CreateProcessW(app, nullptr, &saProcess, &saThread, true, CREATE_UNICODE_ENVIRONMENT, nullptr, nullptr, &si, &process))
	{
		int err = GetLastError();
		wprintf(TEXT("CreateProcessW failed - %s"), GetLastErrorText(szErr, 256));
		return 0;
	}
	return 0;
	//	TCHAR szName[] = TEXT(SZSERVICENAME);
	//	SERVICE_TABLE_ENTRY dispatchTable[] =
	//	{
	//		{ szName, service_main },
	//		{ NULL, NULL }
	//	};
	//
	//	if ((argc > 1) && ((*argv[1] == '-') || (*argv[1] == '/')))
	//	{
	//		if (_stricmp("install", argv[1] + 1) == 0)
	//		{
	//			CmdInstallService();
	//		}
	//		else if (_stricmp("remove", argv[1] + 1) == 0)
	//		{
	//			CmdRemoveService();
	//		}
	//		else if (_stricmp("debug", argv[1] + 1) == 0)
	//		{
	//			bDebug = TRUE;
	//			CmdDebugService(argc, argv);
	//		}
	//		else
	//		{
	//			goto dispatch;
	//		}
	//
	//		return 0;
	//	}
	//
	//dispatch:
	//	// this is just to be friendly
	//	printf("%s -install          to install the service\n", SZAPPNAME);
	//	printf("%s -remove           to remove the service\n", SZAPPNAME);
	//	printf("%s -debug <params>   to run as a console app for debugging\n", SZAPPNAME);
	//	printf("\nStartServiceCtrlDispatcher being called.\n");
	//	printf("This may take several seconds.  Please wait.\n");
	//
	//	if (!StartServiceCtrlDispatcher(dispatchTable))
	//	{
	//		AddToMessageLog(TEXT("StartServiceCtrlDispatcher failed."));
	//	}
	//
	//	return 0;
}

void WINAPI service_main(DWORD dwArgc, LPTSTR* lpszArgv)
{
	sshStatusHandle = RegisterServiceCtrlHandler(TEXT(SZSERVICENAME), service_ctrl);

	if (!sshStatusHandle)
	{
		goto cleanup;
	}

	ssStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	ssStatus.dwServiceSpecificExitCode = 0;

	if (!ReportStatusToSCMgr(SERVICE_START_PENDING, NO_ERROR, 3000))
	{
		goto cleanup;
	}

	ServiceStart(dwArgc, lpszArgv);

cleanup:
	if (sshStatusHandle)
	{
		(VOID)ReportStatusToSCMgr(SERVICE_STOPPED, dwErr, 0);
	}

	return;
}

VOID WINAPI service_ctrl(DWORD dwCtrlCode)
{
	switch (dwCtrlCode)
	{
	case SERVICE_CONTROL_STOP:
		ReportStatusToSCMgr(SERVICE_STOP_PENDING, NO_ERROR, 0);
		ServiceStop();
		return;

	case SERVICE_CONTROL_INTERROGATE:
		break;

	default:
		break;

	}

	ReportStatusToSCMgr(ssStatus.dwCurrentState, NO_ERROR, 0);
}

BOOL ReportStatusToSCMgr(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint)
{
	static DWORD dwCheckPoint = 1;
	BOOL fResult = TRUE;

	if (!bDebug)
	{
		if (dwCurrentState == SERVICE_START_PENDING)
		{
			ssStatus.dwControlsAccepted = 0;
		}
		else
		{
			ssStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
		}

		ssStatus.dwCurrentState = dwCurrentState;
		ssStatus.dwWin32ExitCode = dwWin32ExitCode;
		ssStatus.dwWaitHint = dwWaitHint;

		if ((dwCurrentState == SERVICE_RUNNING) || (dwCurrentState == SERVICE_STOPPED))
		{
			ssStatus.dwCheckPoint = 0;
		}
		else
		{
			ssStatus.dwCheckPoint = dwCheckPoint++;
		}

		if (!(fResult = SetServiceStatus(sshStatusHandle, &ssStatus)))
		{
			AddToMessageLog(TEXT("SetServiceStatus"));
		}
	}

	return fResult;
}


VOID AddToMessageLog(LPCTSTR lpszMsg)
{
	TCHAR   szMsg[256];
	HANDLE  hEventSource;
	LPCTSTR lpszStrings[2];


	if (!bDebug)
	{
		dwErr = GetLastError();

		hEventSource = RegisterEventSource(NULL, TEXT(SZSERVICENAME));

		_stprintf(szMsg, TEXT("%s error: %d"), TEXT(SZSERVICENAME), dwErr);
		lpszStrings[0] = szMsg;
		lpszStrings[1] = lpszMsg;

		if (hEventSource != NULL)
		{
			ReportEvent(
				hEventSource,
				EVENTLOG_ERROR_TYPE,
				0,
				0,
				NULL,
				2,
				0,
				lpszStrings,
				NULL);

			(VOID)DeregisterEventSource(hEventSource);
		}
	}
}

void CmdInstallService()
{
	SC_HANDLE schService;
	SC_HANDLE schSCManager;

	TCHAR szPath[512];

	if (GetModuleFileName(NULL, szPath, 512) == 0)
	{
		_tprintf(TEXT("Unable to install %s - %s\n"), TEXT(SZSERVICEDISPLAYNAME), GetLastErrorText(szErr, 256));
		return;
	}

	schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (schSCManager)
	{
		schService = CreateService(
			schSCManager,
			TEXT(SZSERVICENAME),
			TEXT(SZSERVICEDISPLAYNAME),
			SERVICE_ALL_ACCESS,
			SERVICE_WIN32_OWN_PROCESS,
			SERVICE_DEMAND_START,
			SERVICE_ERROR_NORMAL,
			szPath,
			NULL,
			NULL,
			TEXT(SZDEPENDENCIES),
			NULL,
			NULL);

		if (schService)
		{
			_tprintf(TEXT("%s installed.\n"), TEXT(SZSERVICEDISPLAYNAME));
			CloseServiceHandle(schService);
		}
		else
		{
			_tprintf(TEXT("CreateService failed - %s\n"), GetLastErrorText(szErr, 256));
		}

		CloseServiceHandle(schSCManager);
	}
	else
	{
		_tprintf(TEXT("OpenSCManager failed - %s\n"), GetLastErrorText(szErr, 256));
	}
}

void CmdRemoveService()
{
	SC_HANDLE schService;
	SC_HANDLE schSCManager;

	schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (schSCManager)
	{
		schService = OpenService(schSCManager, TEXT(SZSERVICENAME), SERVICE_ALL_ACCESS);

		if (schService)
		{
			if (ControlService(schService, SERVICE_CONTROL_STOP, &ssStatus))
			{
				_tprintf(TEXT("Stopping %s."), TEXT(SZSERVICEDISPLAYNAME));
				Sleep(1000);

				while (QueryServiceStatus(schService, &ssStatus))
				{
					if (ssStatus.dwCurrentState == SERVICE_STOP_PENDING)
					{
						_tprintf(TEXT("."));
						Sleep(1000);
					}
					else
					{
						break;
					}
				}

				if (ssStatus.dwCurrentState == SERVICE_STOPPED)
				{
					_tprintf(TEXT("\n%s stopped.\n"), TEXT(SZSERVICEDISPLAYNAME));
				}
				else
				{
					_tprintf(TEXT("\n%s failed to stop.\n"), TEXT(SZSERVICEDISPLAYNAME));
				}
			}

			if (DeleteService(schService))
			{
				_tprintf(TEXT("%s removed.\n"), TEXT(SZSERVICEDISPLAYNAME));
			}
			else
			{
				_tprintf(TEXT("DeleteService failed - %s\n"), GetLastErrorText(szErr, 256));
			}

			CloseServiceHandle(schService);
		}
		else
		{
			_tprintf(TEXT("OpenService failed - %s\n"), GetLastErrorText(szErr, 256));
		}

		CloseServiceHandle(schSCManager);
	}
	else
	{
		_tprintf(TEXT("OpenSCManager failed - %s\n"), GetLastErrorText(szErr, 256));
	}
}

void CmdDebugService(int argc, char** argv)
{
	int dwArgc;
	LPTSTR* lpszArgv;

#ifdef UNICODE
	lpszArgv = CommandLineToArgvW(GetCommandLineW(), &dwArgc);
#else
	dwArgc = (DWORD)argc;
	lpszArgv = argv;
#endif

	_tprintf(TEXT("Debugging %s.\n"), TEXT(SZSERVICEDISPLAYNAME));

	SetConsoleCtrlHandler(ControlHandler, TRUE);

	ServiceStart(dwArgc, lpszArgv);
}

BOOL WINAPI ControlHandler(DWORD dwCtrlType)
{
	switch (dwCtrlType)
	{
	case CTRL_BREAK_EVENT:
	case CTRL_C_EVENT:
		_tprintf(TEXT("Stopping %s.\n"), TEXT(SZSERVICEDISPLAYNAME));
		ServiceStop();
		return TRUE;
		break;

	}
	return FALSE;
}

LPTSTR GetLastErrorText(LPTSTR lpszBuf, DWORD dwSize)
{
	DWORD dwRet;
	LPTSTR lpszTemp = NULL;

	dwRet = FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ARGUMENT_ARRAY,
		NULL,
		GetLastError(),
		LANG_NEUTRAL,
		(LPTSTR)&lpszTemp,
		0,
		NULL);

	if (!dwRet || ((long)dwSize < (long)dwRet + 14))
	{
		lpszBuf[0] = TEXT('\0');
	}
	else
	{
		lpszTemp[lstrlen(lpszTemp) - 2] = TEXT('\0');
		_stprintf(lpszBuf, TEXT("%s (%d)"), lpszTemp, GetLastError());
	}

	if (lpszTemp)
	{
		LocalFree((HLOCAL)lpszTemp);
	}

	return lpszBuf;
}

int CreateProcessOnSession(int sessionId, LPWSTR app)
{
	DebugLog(L"CreateProcessOnSession %d %s", sessionId, app);
	HANDLE impersonationToken = nullptr;
	HANDLE userToken = nullptr;
	DebugLog(L"CreateProcessOnSession WTSQueryUserToken ...");
	if (0 == WTSQueryUserToken(sessionId, &impersonationToken))
	{
		int err = GetLastError();
		DebugLog(TEXT("WTSQueryUserToken failed - %s"), GetLastErrorText(szErr, 256));
		return 0;
	}

	SECURITY_ATTRIBUTES sa = { 0 };
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	DebugLog(L"CreateProcessOnSession DuplicateTokenEx ...");
	if (0 == DuplicateTokenEx(impersonationToken, TOKEN_ALL_ACCESS, nullptr, SecurityImpersonation, TokenPrimary, &userToken))
	{
		DebugLog(TEXT("DuplicateTokenEx failed - %s"), GetLastErrorText(szErr, 256));
		return 0;
	}
	if (impersonationToken)
	{
		CloseHandle(impersonationToken);
	}
	if (!userToken)
	{
		DebugLog(TEXT("DuplicateTokenEx ok but userToken null"));
		return 0;
	}

	LPVOID envBlock = nullptr;
	DebugLog(L"CreateProcessOnSession CreateEnvironmentBlock ...");
	if (!CreateEnvironmentBlock(&envBlock, userToken, false))
	{
		DebugLog(TEXT("CreateEnvironmentBlock failed - %s"), GetLastErrorText(szErr, 256));
		return 0;
	}
	if (!envBlock)
	{
		DebugLog(TEXT("CreateEnvironmentBlock ok but envBlock null"));
		return 0;
	}

	PROFILEINFO fileInfo = { 0 };
	LoadUserProfile(userToken, &fileInfo);

	PROCESS_INFORMATION process = { 0 };
	SECURITY_ATTRIBUTES saProcess = { 0 };
	SECURITY_ATTRIBUTES saThread = { 0 };
	saProcess.nLength = sizeof(SECURITY_ATTRIBUTES);
	saThread.nLength = sizeof(SECURITY_ATTRIBUTES);

	STARTUPINFO si = { 0 };
	si.cb = sizeof(STARTUPINFO);
	si.lpDesktop = const_cast<wchar_t*>(L"WinSta0\\Default");
	si.dwFlags = STARTF_USESHOWWINDOW | STARTF_FORCEONFEEDBACK;
	si.wShowWindow = SW_SHOW;

	DebugLog(L"CreateProcessOnSession CreateProcessAsUser ...");
	DebugLog(L"CreateProcessOnSession CreateProcessAsUser %s", app);
	if (!CreateProcessAsUser(userToken, nullptr, app, &saProcess, &saThread, true, CREATE_UNICODE_ENVIRONMENT, envBlock, nullptr, &si, &process))
	{
		DebugLog(TEXT("CreateProcessAsUser failed - %s"), GetLastErrorText(szErr, 256));
		return 0;
	}
	CloseHandle(process.hProcess);
	CloseHandle(process.hThread);

	DebugLog(TEXT("CreateProcessOnSession pid - %d"), process.dwProcessId);

	return process.dwProcessId;
}

DWORD GetExplorerToken(PHANDLE phExplorerToken)
{
	DWORD       dwStatus = ERROR_FILE_NOT_FOUND;
	BOOL        bRet = FALSE;
	HANDLE      hProcess = NULL;
	HANDLE      hProcessSnap = NULL;
	wchar_t        szExplorerPath[MAX_PATH] = { 0 };
	wchar_t        FileName[MAX_PATH] = { 0 };

	PROCESSENTRY32 pe32 = { 0 };

	GetWindowsDirectory(szExplorerPath, MAX_PATH);
	StrCatW(szExplorerPath, L"\\Explorer.EXE");
	hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hProcessSnap == INVALID_HANDLE_VALUE)
	{
		dwStatus = GetLastError();
		return 0;
	}
	pe32.dwSize = sizeof(PROCESSENTRY32);
	if (!Process32First(hProcessSnap, &pe32))
	{
		dwStatus = GetLastError();
		return 0;
	}

	do {
		hProcess = OpenProcess(
			PROCESS_ALL_ACCESS,
			FALSE,
			pe32.th32ProcessID);

		if (NULL != hProcess)
		{

			TCHAR szAppLocation[MAX_PATH] = _T("");
			DWORD dwRet = ::GetModuleFileNameEx(hProcess, NULL,
				szAppLocation, MAX_PATH);
			if (0 == dwRet)
			{
				::Process32Next(hProcessSnap, &pe32);
				CloseHandle(hProcess);
				continue;
			}

			TCHAR szProcessName[MAX_PATH] = _T("");
			dwRet = ::GetLongPathName(szAppLocation, szProcessName,
				MAX_PATH);
			if (0 == dwRet)
			{
				::Process32Next(hProcessSnap, &pe32);
				CloseHandle(hProcess);
				continue;
			}
			std::wstring csProcessName = szProcessName;
			for (auto& str : csProcessName)
			{
				str = ::towlower(str);
			}
			std::wstring csExplorerPath = szExplorerPath;
			for (auto& str : csExplorerPath)
			{
				str = ::towlower(str);
			}
			if (csProcessName == csExplorerPath)
			{
				HANDLE  hToken;

				if (OpenProcessToken(hProcess, TOKEN_DUPLICATE, &hToken))
				{
					HANDLE hNewToken = NULL;
					DuplicateTokenEx(hToken, TOKEN_ALL_ACCESS, NULL, SecurityImpersonation, TokenPrimary, &hNewToken);
					*phExplorerToken = hNewToken;
					dwStatus = 0;
					CloseHandle(hToken);
				}
				break;
			}

			CloseHandle(hProcess);
			hProcess = NULL;
		}

	} while (Process32Next(hProcessSnap, &pe32));

	return dwStatus;
}

BOOL LaunchedSoftware(std::wstring strFileName, std::wstring csParam /* = EMPTY_STRING */)
{
	STARTUPINFO info;
	memset(&info, 0, sizeof(info));
	info.cb = sizeof(info);
	//info.lpDesktop = _T("WinSta0\\Default");
	info.lpDesktop = const_cast<wchar_t*>(L"WinSta0\\Default");
	info.dwFlags |= STARTF_USESHOWWINDOW;
	info.wShowWindow = SW_SHOWNORMAL;
	std::wstring  strError;

	HANDLE hPtoken = NULL;

	GetExplorerToken(&hPtoken);
	if (NULL == hPtoken)
	{
		return FALSE;
	}

	static HMODULE advapi32_dll = LoadLibraryW(L"advapi32.dll");
	if (!advapi32_dll)
		return FALSE;

	DWORD dwSessionId = 0;
	dwSessionId = WTSGetActiveConsoleSessionId();

	if (!SetTokenInformation(hPtoken, TokenSessionId, (void*)(&dwSessionId), sizeof(DWORD)))
	{
		DWORD dwError = GetLastError();
		FreeLibrary(advapi32_dll);
		return FALSE;
	}

	TOKEN_PRIVILEGES tp; //新特权结构体   
	LUID Luid;
	BOOL retn = LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &Luid);

	if (retn != TRUE)
	{
		//cout<<"获取Luid失败"<<endl;   
		FreeLibrary(advapi32_dll);
		return FALSE;
	}
	//给TP和TP里的LUID结构体赋值   
	tp.PrivilegeCount = 1;
	tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	tp.Privileges[0].Luid = Luid;

	//TOKEN_PRIVILEGES tp;
	if (!AdjustTokenPrivileges(hPtoken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL)) //这个函数启用或禁止指定访问令牌的特权
	{
		int abc = GetLastError();
		printf("Adjust Privilege value Error: %u\n", GetLastError());
		FreeLibrary(advapi32_dll);
		return FALSE;
	}
	if (GetLastError() == ERROR_NOT_ALL_ASSIGNED)
	{
		printf("Token does not have the provilege\n");
	}

	DWORD dwCreationFlag = NORMAL_PRIORITY_CLASS | CREATE_NEW_CONSOLE | CREATE_UNICODE_ENVIRONMENT;
	LPVOID pEnv = NULL;
	if (!CreateEnvironmentBlock(&pEnv, hPtoken, FALSE))
	{
		//PrintfDbgStr(TEXT("CreateEnvironmentBlock error ！error code：%d\n"),GetLastError());  
		//bSuccess = FALSE;  
		//break;  
	}

	PROCESS_INFORMATION m_pinfo = { 0 };
	BOOL bResult = FALSE;
	if (0 == csParam.length())
	{
		bResult = CreateProcessAsUser(
			hPtoken, // 这个参数上面已经得到
			strFileName.c_str(),
			NULL, // command line
			NULL, // pointer to process SECURITY_ATTRIBUTES
			NULL, // pointer to thread SECURITY_ATTRIBUTES
			FALSE, // handles are not inheritable
			dwCreationFlag, // creation flags
			pEnv, // pointer to new environment block
			NULL, // name of current directory
			&info, // pointer to STARTUPINFO structure
			&m_pinfo // receives information about new process
		);
	}
	else
	{
		bResult = CreateProcessAsUser(
			hPtoken, // 这个参数上面已经得到
			strFileName.c_str(),
			NULL, // command line
			NULL, // pointer to process SECURITY_ATTRIBUTES
			NULL, // pointer to thread SECURITY_ATTRIBUTES
			FALSE, // handles are not inheritable
			dwCreationFlag, // creation flags
			NULL, // pointer to new environment block
			NULL, // name of current directory
			&info, // pointer to STARTUPINFO structure
			&m_pinfo // receives information about new process
		);
	}

	DWORD dwCode = GetLastError();

	FreeLibrary(advapi32_dll);
	return bResult;
}
