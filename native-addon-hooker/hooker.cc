#include <Windows.h>
#include <Winuser.h>
#include <iostream>
#include <thread>
#include <Psapi.h>
#include <assert.h>

#define NAPI_EXPERIMENTAL
#include <napi.h> //c++ node-addon-api that uses c-ish n-api headers inside
#include <node_api.h> //c implementation n-api


#include "a.h"


using namespace std;


HANDLE _hookMessageLoop;
HANDLE _readMessageLoop;
static HHOOK _hook;


typedef struct {
  napi_async_work work;
  napi_threadsafe_function tsfn;
} AddonData;

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
			/*
			//MessageBox(NULL, TEXT("mouse click"), TEXT("mouse click"), MB_OK); 
			std::thread::id this_id = std::this_thread::get_id();
			//HANDLE handle = std::this_thread::native_handle();
			HANDLE handle = GetCurrentThread();
			DWORD pId = GetProcessIdOfThread(handle);
  		    HANDLE pHandle = OpenProcess(
				PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
				FALSE,
				pId// This is the PID, you can find one from windows task manager/
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
			*/
			//std::cout << "mouse click"<< std::endl;
			//std::to_string(factor)
			Message message("click");
		    clicksQueue.write(message);
		}
	}
	return CallNextHookEx(_hook, nCode, wParam, lParam);
}

static DWORD WINAPI HooksMessageLoop( LPVOID )
{
	
	HINSTANCE appInstance = GetModuleHandle( NULL );
	_hook = SetWindowsHookEx( WH_MOUSE_LL, MouseLLHookCallback, appInstance, 0 );
	MSG msg;
	while( GetMessage( &msg, NULL, 0, 0 ) > 0 )
	{
		TranslateMessage( &msg );
		DispatchMessage ( &msg );
	}
	return 0;
}

static void ClickCallback( const Napi::CallbackInfo& info )
{
	 Napi::String arg = info[0].As<Napi::String>();
	
	std::cout << "########### ClickCallback " << arg.Utf8Value()  << std::endl;
}

static void CallJs(napi_env napiEnv, napi_value napi_js_cb, void* context, void* data) {
	Message m = *((Message*)data);
	Napi::Env env = Napi::Env(napiEnv);
	Napi::String jsCallbackParameter = Napi::String::New(env, m.data);
	Napi::Function js_cb = Napi::Value(env, napi_js_cb).As<Napi::Function>();
	js_cb.Call(env.Global(), { jsCallbackParameter });
}

static void ExecuteWork(napi_env env, void* data) {
	AddonData* addon_data = (AddonData*)data;
	 
    std::cout << "########### ExecuteWork" << std::endl;
	
	assert(napi_acquire_threadsafe_function(addon_data->tsfn) == napi_ok);
	
	Message* msg = NULL;
  	do
	{
	   msg = clicksQueue.read();	
	   Message out_msg = *msg;
	   assert(napi_call_threadsafe_function(addon_data->tsfn, &out_msg, napi_tsfn_blocking) == napi_ok);
	}
	while(msg != NULL);
		
	assert(napi_release_threadsafe_function(addon_data->tsfn, napi_tsfn_release) == napi_ok);
}


//todo make global data in Init function
AddonData* addon_data = (AddonData*)malloc(sizeof(*addon_data));


// This function runs on the main thread after `ExecuteWork` exits.
static void WorkComplete(napi_env env, napi_status status, void* data) {
  AddonData* addon_data = (AddonData*)data;
	 
  std::cout << "########### WorkComplete" << std::endl;
}

Napi::Value AddHook(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
	
	Napi::String work_name = Napi::String::New(env, "async-work-name");
	Napi::Function js_cb = Napi::Function::New(env, ClickCallback);
	
	addon_data->work = NULL;
	 
	assert(napi_create_threadsafe_function(env,
                                         js_cb,
                                         NULL,
                                         work_name,
                                         0,
                                         1,
                                         NULL,
                                         NULL,
                                         NULL,
                                         CallJs,
                                         &(addon_data->tsfn)) == napi_ok);
										 
  assert(napi_create_async_work(env,
                                NULL,
                                work_name,
                                ExecuteWork,
                                WorkComplete,
                                addon_data,
                                &(addon_data->work)) == napi_ok);

  // Queue the work item for execution.
  assert(napi_queue_async_work(env, addon_data->work) == napi_ok);										 
										 
										 
										 
										 
	
	if(_hookMessageLoop)
	{
		std::cout << "hook has been already added!"<< std::endl;
		return env.Undefined();
	}
	_hookMessageLoop = CreateThread( NULL, NULL, HooksMessageLoop, NULL, NULL, NULL );
	return env.Undefined();
}

Napi::Value RemoveHook(const Napi::CallbackInfo& info) {
	std::cout << "########### remove hook  " << std::endl;
	clicksQueue.destroy();
	Napi::Env env = info.Env();
	if(!_hookMessageLoop && !_hook)
	{
		std::cout << "hook has not been added yet!"<< std::endl;
		return env.Undefined();
	}
	if(_hook)
	{
		UnhookWindowsHookEx(_hook);
		_hook = 0;
	}
	if(_hookMessageLoop)
	{
		TerminateThread( _hookMessageLoop, 0 );
		_hookMessageLoop = 0;
	}
	if(addon_data &&  addon_data->work)
	{
		napi_delete_async_work(env, addon_data->work);
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