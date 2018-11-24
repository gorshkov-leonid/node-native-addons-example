//#define NAPI_VERSION 2
#include <napi.h>
#include <Windows.h>
#include <Winuser.h>
#include <iostream>
#include <thread>
#include <Psapi.h>

using namespace std;


HANDLE _hookMessageLoop;
static HHOOK _hook;


LRESULT MouseLLHookCallback(
		_In_ int    nCode,
		_In_ WPARAM wParam,
		_In_ LPARAM lParam
	)
{
	if (nCode >= 0)
	{
		
		MSLLHOOKSTRUCT * cp = (MSLLHOOKSTRUCT*)lParam;
		if (wParam == WM_LBUTTONDOWN) 
		{
			//MessageBox(NULL, TEXT("mouse click"), TEXT("mouse click"), MB_OK); 
			std::thread::id this_id = std::this_thread::get_id();
			//HANDLE handle = std::this_thread::native_handle();
			HANDLE handle = GetCurrentThread();
			DWORD pId = GetProcessIdOfThread(handle);
  		    HANDLE pHandle = OpenProcess(
				PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
				FALSE,
				pId /* This is the PID, you can find one from windows task manager */
			);
		    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS | PROCESS_QUERY_INFORMATION |
			  PROCESS_VM_READ,
			  FALSE, pId);
			if (NULL != hProcess) {
			  TCHAR nameProc[1024];
			  if (GetProcessImageFileNameA(hProcess, nameProc, sizeof(nameProc) / sizeof( * nameProc)) == 0) {
				std::cout << "GetProcessImageFileName Error"<< std::endl;
			  } else {
				std::wcout << "thread " << this_id << ", " << "pId " << pId << ", pName " << nameProc << std::endl;
			  }

			} else {
			  printf("OpenProcess(%i) failed, error: %i\n",
				pId, (int) GetLastError());
			}
			std::cout << "mouse click"<< std::endl;
		}
	}
	return CallNextHookEx(_hook, nCode, wParam, lParam);
}

static DWORD WINAPI MessageLoop( LPVOID )
{
	MSG msg;
	HINSTANCE appInstance = GetModuleHandle( NULL );
	_hook = SetWindowsHookEx( WH_MOUSE_LL, MouseLLHookCallback, appInstance, 0 );
	while( GetMessage( &msg, NULL, 0, 0 ) > 0 )
	{
		TranslateMessage( &msg );
		DispatchMessage ( &msg );
	}
	if(_hook)
	{
		UnhookWindowsHookEx(_hook);
		_hook = 0;
	}
	return 0;
}


Napi::Value AddHook(const Napi::CallbackInfo& info) {
	Napi::Env env = info.Env();
	if(_hookMessageLoop)
	{
		std::cout << "hook has been already added!"<< std::endl;
		return env.Undefined();
	}
	_hookMessageLoop = CreateThread( NULL, NULL, MessageLoop, NULL, NULL, NULL );
	return env.Undefined();
}

Napi::Value RemoveHook(const Napi::CallbackInfo& info) {
	Napi::Env env = info.Env();
	if(!_hookMessageLoop && !_hook)
	{
		std::cout << "hook has not been added yet!"<< std::endl;
		return env.Undefined();
	}
	if(_hookMessageLoop)
	{
		TerminateThread( _hookMessageLoop, 0 );
		_hookMessageLoop = 0;
	}
    return env.Undefined();
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    exports.Set(Napi::String::New(env, "addHook"), Napi::Function::New(env, AddHook));
    exports.Set(Napi::String::New(env, "removeHook"), Napi::Function::New(env, RemoveHook));
    return exports;
}
NODE_API_MODULE(hooker, Init)










/*
https://stackoverflow.com/questions/6801644/problem-on-hooking-keyboard-messages-using-setwindowshookex

https://social.msdn.microsoft.com/Forums/windowsdesktop/en-US/56093d14-c1bc-4d0a-a915-57fef0695191/windows-7-setwindowshookex-callback-proc-stops-getting-called-after-exceeding-max-time-allowed?forum=windowsgeneraldevelopmentissues

https://www.unknowncheats.me/forum/c-and-c-/83707-setwindowshookex-example.html


while( true ) {
    cout << "choose from the above menu: ";
    string s;
    getline( cin, s );
 
    if( !!isdigit( s.c_str() ) ) {
        menuChoice = stoi( s );
 
        if( menuChoice >= 1 && menuChoice <= 6 ) {
            break;
        }
    }
}
*/
//https://nodejs.org/en/docs/guides/dont-block-the-event-loop/



			//RECT rr;
			//GetWindowRect(activeWinHandle, &rr);
			//result message
			//std::wstringstream strstream;
			//strstream << "mouse LEFT ";
			//std::basic_string<TCHAR> msg2 = TEXT("mouse key pressed");
			//cout << strstream.str().c_str();