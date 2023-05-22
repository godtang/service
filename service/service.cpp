#define _CRT_SECURE_NO_WARNINGS

#pragma comment( lib, "kernel32.lib" )
#pragma comment( lib, "advapi32.lib" )
#pragma comment( lib, "shell32.lib" )

#define SZAPPNAME		"Simple"
#define SZSERVICENAME		"SimpleService"
#define SZSERVICEDISPLAYNAME	"Simple Service"
#define SZDEPENDENCIES		""

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>
#include "pcquicklib.h"
#include "config.h"

SERVICE_STATUS		ssStatus;
SERVICE_STATUS_HANDLE   sshStatusHandle;
DWORD			dwErr = 0;
BOOL			bDebug = FALSE;
TCHAR			szErr[256];
HANDLE			hServerStopEvent = NULL;
bool Init();
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

bool Init()
{
	OutputDebugString(L"service init");
	// ³õÊ¼»¯
	if (nullptr == CConfig::GetInstance())
	{
		return false;
	}

	auto config = CConfig::GetInstance();
	auto strLogName = config->Root.Get("/log/name", "a.log");
	auto strLogLevle = config->Root.Get("/log/level", "debug");
	auto iLogMaxSize = config->Root.Get("/log/maxSize", 5 * 1024 * 1024);
	std::map<std::string, CPCLog::ePCLogThreshold> loggerLevel;
	loggerLevel["trace"] = CPCLog::eTRACE;
	loggerLevel["debug"] = CPCLog::eDEBUG;
	loggerLevel["info"] = CPCLog::eINFO;
	loggerLevel["warn"] = CPCLog::eWARN;
	loggerLevel["error"] = CPCLog::eERROR;
	loggerLevel["off"] = CPCLog::eOFF;

	if (CPCFileUtil::PCPathIsAbsolute(strLogName))
	{
		CPCLog::Default()->SetLogAttr(strLogName, loggerLevel[strLogLevle], false, false, iLogMaxSize);
	}
	else
	{
		std::string strTempLogName = CPCFileUtil::PCGetSelfFileDir().Get().first + strLogName;
		CPCLog::Default()->SetLogAttr(strTempLogName.c_str(), loggerLevel[strLogLevle], false, false, iLogMaxSize);
	}
	PC_INFO("service init finish");
	return true;
}

bool run = false;
DWORD WINAPI ThreadFunc(LPVOID);
VOID ServiceStart(DWORD dwArgc, LPTSTR* lpszArgv)
{
	PC_INFO("ServiceStart");
	run = true;
	HANDLE hThread;
	DWORD  threadId;
	hThread = CreateThread(NULL, 0, ThreadFunc, 0, 0, &threadId);

	if (!ReportStatusToSCMgr(SERVICE_RUNNING, NO_ERROR, 0))
	{
		PC_ERROR("report service running error");
	}
	while (run)
	{
		//PC_INFO("main running, thread id = %d", GetCurrentThreadId());
		Sleep(1000);
	}
	PC_INFO("ServiceStart end");
}

DWORD WINAPI ThreadFunc(LPVOID p)
{
	PC_INFO("ThreadFunc");
	while (run)
	{
		//PC_INFO("child running, thread id = %d", GetCurrentThreadId());
		Sleep(1000);
	}
	PC_WARN("child thread quit");
	return 0;
}

VOID ServiceStop()
{
	PC_WARN("ServiceStop");
	run = false;
	if (!ReportStatusToSCMgr(SERVICE_STOPPED, NO_ERROR, 0))
	{
		PC_ERROR("report service running error");
	}
	Sleep(10000);
	PC_INFO("ServiceStop end");
}

int main(int argc, char** argv)
{
	TCHAR szName[] = TEXT(SZSERVICENAME);
	SERVICE_TABLE_ENTRY dispatchTable[] =
	{
		{ szName, service_main },
		{ NULL, NULL }
	};

	if ((argc > 1) && ((*argv[1] == '-') || (*argv[1] == '/')))
	{
		if (_stricmp("install", argv[1] + 1) == 0)
		{
			CmdInstallService();
		}
		else if (_stricmp("remove", argv[1] + 1) == 0)
		{
			CmdRemoveService();
		}
		else if (_stricmp("debug", argv[1] + 1) == 0)
		{
			bDebug = TRUE;
			CmdDebugService(argc, argv);
		}
		else
		{
			goto dispatch;
		}

		return 0;
	}

dispatch:
	// this is just to be friendly
	printf("%s -install          to install the service\n", SZAPPNAME);
	printf("%s -remove           to remove the service\n", SZAPPNAME);
	printf("%s -debug <params>   to run as a console app for debugging\n", SZAPPNAME);
	printf("\nStartServiceCtrlDispatcher being called.\n");
	printf("This may take several seconds.  Please wait.\n");

	if (!Init())
	{
		AddToMessageLog(TEXT("init fail"));
		return 1;
	}


	PC_ERROR("StartServiceCtrlDispatcher 1 %d", GetCurrentProcessId());
	if (!StartServiceCtrlDispatcher(dispatchTable))
	{
		PC_ERROR("StartServiceCtrlDispatcher 2 %d", GetCurrentProcessId());
		AddToMessageLog(TEXT("StartServiceCtrlDispatcher failed."));
	}

	PC_ERROR("StartServiceCtrlDispatcher 3 %d", GetCurrentProcessId());
	return 0;
}

void WINAPI service_main(DWORD dwArgc, LPTSTR* lpszArgv)
{
	PC_INFO("service_main");
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

	PC_INFO("service_main ServiceStart");
	ServiceStart(dwArgc, lpszArgv);
	PC_INFO("service_main ServiceStart end");

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
		_stprintf(lpszBuf, TEXT("%s (0x%x)"), lpszTemp, GetLastError());
	}

	if (lpszTemp)
	{
		LocalFree((HLOCAL)lpszTemp);
	}

	return lpszBuf;
}