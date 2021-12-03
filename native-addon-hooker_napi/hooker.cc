// todo resolution and multiple monitors
//   https://docs.microsoft.com/en-us/windows/win32/gdi/positioning-objects-on-multiple-display-monitors
//   https://stackoverflow.com/questions/39677569/high-dpi-scaling-mouse-hooks-and-windowfrompoint
//   https://github.com/microsoft/Windows-classic-samples/blob/main/Samples/DPIAwarenessPerWindow/client/DpiAwarenessContext.cpp !! see SetThreadDpiAwarenessContext
//   https://social.msdn.microsoft.com/Forums/windows/en-US/a1699afe-fdae-444c-a9e6-34b768be733b/get-hwnd-of-desktop-window-of-another-hmonitor?forum=windowssdk !!questions diff monitors
//   https://stackoverflow.com/questions/4324954/whats-the-difference-between-windowfromphysicalpoint-and-windowfrompoint !!! physical point LogicalToPhysicalPoint
//   !! https://forum.codejock.com/cannot-drag-panels-to-other-monitor-with-other-dpi_topic23662.html пример длинный
//
//   WindowFromPoint LogicalToPhysicalPoint

// todo https://github.com/nodejs/node-addon-api/blob/main/doc/async_worker_variants.md
#include <Windows.h>
#include <Windowsx.h>
#include <Winuser.h>
#include <iostream>
#include <thread>
#include <Psapi.h>
#include <assert.h>
#include <vector>

#include <napi.h> //c++ node-addon-api that uses c-ish n-api headers inside
#include <node_api.h> //c implementation n-api
#include <signal.h>

#include "pcQueue.h"


using namespace std;

HANDLE _hookMessageLoop;
static HHOOK _hook;

typedef struct {
    napi_async_work work;
    napi_threadsafe_function tsfn;
    uint32_t processId;
} AddonData;

class CaptureInfo {
public:
    LONG clickX = 0;
    LONG clickY = 0;
    std::vector<RECT> rectHierarchy = {};

    CaptureInfo(
            LONG _clickX,
            LONG _clickY
    ) {
        clickX = _clickX;
        clickY = _clickY;
    }
};


PCQueue<CaptureInfo *> clicksQueue;

AddonData *addon_data = (AddonData *) malloc(sizeof(*addon_data));

static RECT getRect(HWND hwnd) {
    RECT rect;
    GetWindowRect(hwnd, &rect);
    return rect;
}

static DWORD getWindowProcessId(HWND handle) {
    DWORD procId = 0;
    GetWindowThreadProcessId(handle, &procId);
    return procId;
}

LRESULT MouseLLHookCallback(
        _In_ int nCode,
        _In_ WPARAM wParam,
        _In_ LPARAM lParam
) {
    if (nCode >= 0) {

        MSLLHOOKSTRUCT *cp = (MSLLHOOKSTRUCT *) lParam;
        //http://vsokovikov.narod.ru/New_MSDN_API/Mouse_input/notify_wm_lbuttondown.htm
        //https://stackoverflow.com/questions/29915639/why-get-x-lparam-does-return-an-absolute-position-on-mouse-wheel
        if (wParam == WM_LBUTTONDOWN && cp) {
            SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE); //todo DPI_AWARENESS_CONTEXT_SYSTEM_AWARE, SetThreadDpiHostingBehavior
            POINT pt = cp->pt;
            HWND controlHandle = WindowFromPoint(pt);
            DWORD activeMainWindowProcessId = getWindowProcessId(controlHandle);
            DWORD currentProcessId = GetCurrentProcessId();
            std::cout << "click at (" << pt.x << ", " << pt.y << "): handle " << controlHandle << ", process " << activeMainWindowProcessId << std::endl;
            if (activeMainWindowProcessId != currentProcessId
                &&
                (!addon_data->processId || addon_data->processId && addon_data->processId == activeMainWindowProcessId)
                    ) {
                RECT controlRect = getRect(controlHandle);

                HWND rootWindowHandle = GetAncestor(controlHandle, GA_ROOTOWNER);
                RECT topWindowRect = getRect(rootWindowHandle);


                HWND activeMainWindowHandle = GetAncestor(controlHandle, GA_ROOT);
                RECT mainWindowRect = getRect(activeMainWindowHandle);

                LONG clickX = pt.x;
                LONG clickY = pt.y;
                CaptureInfo *capture = new CaptureInfo(clickX, clickY);

                capture->rectHierarchy.push_back(controlRect);
                capture->rectHierarchy.push_back(mainWindowRect);
                if (activeMainWindowHandle != rootWindowHandle) {
                    capture->rectHierarchy.push_back(topWindowRect);
                }

                clicksQueue.write(capture);
            }
        }
    }
    return CallNextHookEx(_hook, nCode, wParam, lParam);
}

static DWORD WINAPI HooksMessageLoop(LPVOID lpParam) {

    HINSTANCE appInstance = GetModuleHandle(NULL);
    //https://github.com/dapplo/Dapplo.Windows/blob/master/src/Dapplo.Windows/Desktop/WinEventHook.cs
    //https://github.com/dapplo/Dapplo.Windows/blob/master/src/Dapplo.Windows/Enums/WinEventHookFlags.cs
    _hook = SetWindowsHookEx(WH_MOUSE_LL, MouseLLHookCallback, appInstance, 0);
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}

static void CallJs(napi_env napiEnv, napi_value napi_js_cb, void *context, void *data) {
    CaptureInfo *capture = (CaptureInfo *) data;
    try {
        Napi::Env env = Napi::Env(napiEnv);

        Napi::Array rects = Napi::Array::New(env);

        int i = 0;
        for (std::vector<RECT>::iterator it = capture->rectHierarchy.begin();
             it != capture->rectHierarchy.end(); ++it) {
            RECT rect = *it;
            Napi::Object jsRect = Napi::Object::New(env);
            jsRect.Set("x", Napi::Number::New(env, rect.left));
            jsRect.Set("y", Napi::Number::New(env, rect.top));
            jsRect.Set("width", Napi::Number::New(env, rect.right - rect.left));
            jsRect.Set("height", Napi::Number::New(env, rect.bottom - rect.top));
            rects.Set(i, jsRect);
            i++;
        }

        Napi::Object jsCaptureInfo = Napi::Object::New(env);
        jsCaptureInfo.Set("clickX", Napi::Number::New(env, capture->clickX));
        jsCaptureInfo.Set("clickY", Napi::Number::New(env, capture->clickY));
        jsCaptureInfo.Set("rects", rects);

        Napi::Function js_cb = Napi::Value(env, napi_js_cb).As<Napi::Function>();
        js_cb.Call(env.Global(), {jsCaptureInfo});
        delete capture;
    }
    catch (...) {
        // todo RAII https://stackoverflow.com/questions/390615/finally-in-c
        delete capture;
    }

}

static void ExecuteWork(napi_env env, void *data) {
    AddonData *addon_data = (AddonData *) data;

    assert(napi_acquire_threadsafe_function(addon_data->tsfn) == napi_ok);

    CaptureInfo *captureInfo = NULL;
    do {
        captureInfo = clicksQueue.read();
        if (captureInfo != NULL) {
            assert(napi_call_threadsafe_function(addon_data->tsfn, captureInfo, napi_tsfn_blocking) == napi_ok);
        }
    } while (captureInfo != NULL);

    assert(napi_release_threadsafe_function(addon_data->tsfn, napi_tsfn_release) == napi_ok);
}

static void clearData() {
    if (addon_data != NULL) {
        delete addon_data;
    }
    clicksQueue.destroy();
}

// This function runs on the main thread after `ExecuteWork` exits.
static void WorkComplete(napi_env env, napi_status status, void *data) {
    clearData();
}

Napi::Value AddHook(const Napi::CallbackInfo &info) {
    std::cout << "Adding hook " << std::endl;
    Napi::Env env = info.Env();
    if (info.Length() < 1 || !info[0].IsFunction()) {
        Napi::TypeError::New(env, "Function expected as argument[0]").ThrowAsJavaScriptException();
        return env.Undefined();;
    }
    if (info.Length() >= 2) {
        addon_data->processId = info[1].As<Napi::Number>().Uint32Value();
    }

    Napi::String work_name = Napi::String::New(env, "async-work-name");

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

    if (_hookMessageLoop) {
        return env.Undefined();
    }
    //https://docs.microsoft.com/en-us/windows/desktop/procthread/creating-threads
    _hookMessageLoop = CreateThread(NULL, 0, HooksMessageLoop, NULL, 0, NULL);
    return env.Undefined();
}

Napi::Value RemoveHook(const Napi::CallbackInfo &info) {
    std::cout << "Removing hook " << std::endl;
    Napi::Env env = info.Env();
    if (!_hookMessageLoop && !_hook) {
        std::cout << "hook has not been added yet!" << std::endl;
        return env.Undefined();
    }
    if (_hook) {
        UnhookWindowsHookEx(_hook);
        _hook = 0;
    }
    if (_hookMessageLoop) {
        TerminateThread(_hookMessageLoop, 0);
        _hookMessageLoop = 0;
    }
    if (addon_data && addon_data->work) {
        napi_delete_async_work(env, addon_data->work);
    }
    clearData();
    return env.Undefined();
}

void signal_callback_handler(int signum) {
    clearData();
    std::cout << "Termination " << signum << std::endl;
    exit(signum);
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    signal(SIGINT, signal_callback_handler);

    exports.Set(Napi::String::New(env, "addHook"), Napi::Function::New(env, AddHook));
    exports.Set(Napi::String::New(env, "removeHook"), Napi::Function::New(env, RemoveHook));

    return exports;
}

NODE_API_MODULE(hooker, Init)