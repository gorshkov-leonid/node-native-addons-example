#include <Windows.h>
#include <Windowsx.h>
#include <Winuser.h>
#include <iostream>
#include <thread>
#include <Psapi.h>
#include <assert.h>

#define NAPI_EXPERIMENTAL
#include <napi.h> //c++ node-addon-api that uses c-ish n-api headers inside
#include <node_api.h> //c implementation n-api

#include "pcQueue.h"

using namespace std;

HANDLE _hookMessageLoop;
HANDLE _readMessageLoop;
static HHOOK _hook;

typedef struct {
  napi_async_work work;
  napi_threadsafe_function tsfn;
  uint32_t processId;
} AddonData;

class CaptureInfo
{
  public:
    LONG clickX = 0;
    LONG clickY = 0;
    LONG zoneX = 0;
    LONG zoneY = 0;
    LONG zoneWidth = 0;
    LONG zoneHeight = 0;

    CaptureInfo(
        LONG _clickX,
        LONG _clickY,
        LONG _zoneX,
        LONG _zoneY,
        LONG _zoneWidth,
        LONG _zoneHeight
	){
	   clickX = _clickX;	
	   clickY = _clickY;
       zoneX = _zoneX;
       zoneY = _zoneY;
       zoneWidth = _zoneWidth;
	   zoneHeight = _zoneHeight;
	}
};

PCQueue<CaptureInfo> clicksQueue;

//todo make global data in Init function
AddonData* addon_data = (AddonData*)malloc(sizeof(*addon_data));

static RECT getRect(HWND hwnd)
{
    RECT rect;
    GetWindowRect(hwnd, &rect);
    return rect;
}

static DWORD getWindowProcessId(HWND handle)
{
    DWORD procId = 0;
    GetWindowThreadProcessId(handle, &procId);
    return procId;
}


//https://code.i-harness.com/en/q/1cd25f
HWND FindTopWindow(DWORD pid)
{
    std::pair<HWND, DWORD> params = { 0, pid };

    // Enumerate the windows using a lambda to process each window
    BOOL bResult = EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL 
    {
        auto pParams = (std::pair<HWND, DWORD>*)(lParam);
        DWORD processId;
        if (GetWindowThreadProcessId(hwnd, &processId) && processId == pParams->second)
        {
            // Stop enumerating
            SetLastError(-1);
            pParams->first = hwnd;
            return FALSE;
        }
        // Continue enumerating
        return TRUE;
    }, (LPARAM)&params);

    if (!bResult && GetLastError() == -1 && params.first)
    {
        return params.first;
    }
    return 0;
}

//static void printWindowText(HWND hwnd){
//   int cTxtLen; 
//   PSTR pszMem; 
//   cTxtLen = GetWindowTextLength(hwnd); 
//
//   pszMem = (PSTR) VirtualAlloc((LPVOID) NULL, 
//       (DWORD) (cTxtLen + 1), MEM_COMMIT, 
//       PAGE_READWRITE); 
//   GetWindowText(hwnd, pszMem, 
//       cTxtLen + 1); 
//	cout<< "!!!!!!!!!!! " << pszMem << std::endl;
//}


LRESULT MouseLLHookCallback(
		_In_ int    nCode,
		_In_ WPARAM wParam,
		_In_ LPARAM lParam
	)
{
	if (nCode >= 0)
	{
		
		MSLLHOOKSTRUCT * cp = (MSLLHOOKSTRUCT*)lParam;
		//http://vsokovikov.narod.ru/New_MSDN_API/Mouse_input/notify_wm_lbuttondown.htm
		//https://stackoverflow.com/questions/29915639/why-get-x-lparam-does-return-an-absolute-position-on-mouse-wheel
		if (wParam == WM_LBUTTONDOWN && cp) 
		{

			POINT pt = cp -> pt;
			HWND controlHandle = WindowFromPoint(pt);
			DWORD activeMainWindowProcessId = getWindowProcessId(controlHandle);
			DWORD currentProcessId = GetCurrentProcessId();
			if(activeMainWindowProcessId != currentProcessId 
			    && (!addon_data->processId || addon_data->processId && addon_data->processId == activeMainWindowProcessId)
			)
			{
				RECT controlRect = getRect(controlHandle);
				
				HWND rootWindowHandle = GetAncestor(controlHandle, GA_ROOTOWNER);
				RECT topWindowRect = getRect(rootWindowHandle);
				
				
				HWND activeMainWindowHandle =  GetAncestor(controlHandle, GA_ROOT);
				RECT mainWindowRect = getRect(activeMainWindowHandle);
            
				LONG clickX = (pt.x - mainWindowRect.left);
                LONG clickY = (pt.y - mainWindowRect.top);
                LONG zoneX = (controlRect.left - mainWindowRect.left);
                LONG zoneY = (controlRect.top - mainWindowRect.top);
                LONG zoneWidth = controlRect.right - controlRect.left;
                LONG zoneHeight = controlRect.bottom - controlRect.top;

				CaptureInfo capture (
				    clickX, 
				    clickY, 
				    zoneX, 
				    zoneY, 
				    zoneWidth, 
				    zoneHeight 
				);
		        clicksQueue.write(capture);  
			}
		}
	}
	return CallNextHookEx(_hook, nCode, wParam, lParam);
}

static DWORD WINAPI HooksMessageLoop( LPVOID lpParam)
{

	HINSTANCE appInstance = GetModuleHandle( NULL );
	//https://github.com/dapplo/Dapplo.Windows/blob/master/src/Dapplo.Windows/Desktop/WinEventHook.cs
	//https://github.com/dapplo/Dapplo.Windows/blob/master/src/Dapplo.Windows/Enums/WinEventHookFlags.cs
	_hook = SetWindowsHookEx( WH_MOUSE_LL, MouseLLHookCallback, appInstance, 0 );
	MSG msg;
	while( GetMessage( &msg, NULL, 0, 0 ) > 0 )
	{
		TranslateMessage( &msg );
		DispatchMessage ( &msg );
	}
	return 0;
}

static void CallJs(napi_env napiEnv, napi_value napi_js_cb, void* context, void* data) {
	CaptureInfo capture = *((CaptureInfo*)data);
	Napi::Env env = Napi::Env(napiEnv);
	
    Napi::Object jsCaptureInfo = Napi::Object::New(env);
	jsCaptureInfo.Set("clickX",   Napi::Number::New(env, capture.clickX));
	jsCaptureInfo.Set("clickY", Napi::Number::New(env, capture.clickY));
	jsCaptureInfo.Set("zoneX",   Napi::Number::New(env, capture.zoneX));
	jsCaptureInfo.Set("zoneY", Napi::Number::New(env, capture.zoneY));
	jsCaptureInfo.Set("zoneWidth",   Napi::Number::New(env, capture.zoneWidth));
	jsCaptureInfo.Set("zoneHeight", Napi::Number::New(env, capture.zoneHeight));
	
	Napi::Function js_cb = Napi::Value(env, napi_js_cb).As<Napi::Function>();
	js_cb.Call(env.Global(), { jsCaptureInfo });
}

static void ExecuteWork(napi_env env, void* data) {
	AddonData* addon_data = (AddonData*)data;
	 
	assert(napi_acquire_threadsafe_function(addon_data->tsfn) == napi_ok);
	
	CaptureInfo* captureInfo = NULL;
  	do
	{
	   captureInfo = clicksQueue.read();	
	   CaptureInfo out_capture = *captureInfo;
	   assert(napi_call_threadsafe_function(addon_data->tsfn, &out_capture, napi_tsfn_blocking) == napi_ok);
	}
	while(captureInfo != NULL);
		
	assert(napi_release_threadsafe_function(addon_data->tsfn, napi_tsfn_release) == napi_ok);
}

// This function runs on the main thread after `ExecuteWork` exits.
static void WorkComplete(napi_env env, napi_status status, void* data) {
  AddonData* addon_data = (AddonData*)data;
}

Napi::Value AddHook(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1 || !info[0].IsFunction()) {
        Napi::TypeError::New(env, "Function expected as argument[0]").ThrowAsJavaScriptException();
		return env.Undefined();;
    } 
	if (info.Length() >= 2) {
		 addon_data->processId = info[1].As<Napi::Number>().Uint32Value();
    }
	
	Napi::String work_name = Napi::String::New(env, "async-work-name");
	
	//Napi::Function js_cb = Napi::Function::New(env, ClickCallback);
	Napi::Function js_cb = info[0].As<Napi::Function>();
	
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
		return env.Undefined();
	}
	//https://docs.microsoft.com/en-us/windows/desktop/procthread/creating-threads
	_hookMessageLoop = CreateThread( NULL, 0, HooksMessageLoop, NULL, 0, NULL);
	return env.Undefined();
}

Napi::Value RemoveHook(const Napi::CallbackInfo& info) {
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