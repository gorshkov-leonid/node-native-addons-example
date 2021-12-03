#include <Windows.h>
#include <Windowsx.h>
#include <Winuser.h>
#include <iostream>
#include <thread>
#include <Psapi.h>
#include <assert.h>
#include <vector>

#define NAPI_EXPERIMENTAL

#include <napi.h> //c++ node-addon-api that uses c-ish n-api headers inside
#include <node_api.h> //c implementation n-api

#include <signal.h>

#include <chrono>
#include "pcQueue.h"


using namespace Napi;
using namespace std;

HANDLE _hookMessageLoop;

static HHOOK _hook;

typedef struct {
    uint32_t processId;
} AddonData;

class CaptureInfo {
public:
    LONG clickX = 0;
    LONG clickY = 0;
    std::vector<RECT> rectHierarchy = {};

    CaptureInfo() {}

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
            SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);
            POINT pt = cp->pt;
            HWND controlHandle = WindowFromPoint(pt);
            DWORD activeMainWindowProcessId = getWindowProcessId(controlHandle);
            DWORD currentProcessId = GetCurrentProcessId();
            std::cout << "click at (" << pt.x << ", " << pt.y << "): handle " << controlHandle << ", process "
                      << activeMainWindowProcessId << std::endl;
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

static void clearData() {
    if (addon_data != NULL) {
        delete addon_data;
    }
    clicksQueue.destroy();
}

class EchoWorker : public AsyncProgressQueueWorker<CaptureInfo> {
public:
    EchoWorker(Function &okCallback, Function &errorCallback, Function &progressCallback, uint32_t &processId)
            : AsyncProgressQueueWorker(okCallback), processId(processId) {
        // Set our function references to use them below
        this->errorCallback.Reset(errorCallback, 1);
        this->progressCallback.Reset(progressCallback, 1);
    }

    ~EchoWorker() {}

    void Execute(const ExecutionProgress &progress) {
        HINSTANCE appInstance = GetModuleHandle(NULL);

        CaptureInfo *captureInfo = NULL;
        do {
            captureInfo = clicksQueue.read();
            if (captureInfo != NULL) {
                progress.Send(captureInfo, 1);
            }
        } while (captureInfo != NULL);
    }

//    void OnOK() {
//        HandleScope scope(Env());
//        Callback().Call({Number::New(Env(), processId)}); //todo
//    }

//    void OnError(const Error &e) {
//        HandleScope scope(Env()); //todo
//
//        // We call our callback provided in the constructor with 2 parameters
//        if (!this->errorCallback.IsEmpty()) {
//            // Call our onErrorCallback in javascript with the error message
//            this->errorCallback.Call(Receiver().Value(), {String::New(Env(), e.Message())});
//        }
//    }

    void OnProgress(const CaptureInfo *capture, size_t /* count */) {

        HandleScope scope(Env());

        Napi::Env env = Env();

        if (!this->progressCallback.IsEmpty()) {
            Napi::Array rects = Napi::Array::New(env);

            int i = 0;
            for (std::vector<RECT>::const_iterator it = capture->rectHierarchy.begin();
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

            this->progressCallback.Call(Receiver().Value(), {jsCaptureInfo});
        }
    }

private:
    uint32_t processId;
    FunctionReference progressCallback;
    FunctionReference errorCallback;
};

EchoWorker *wk;

Napi::Value AddHook(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();
    if (info.Length() < 4 || !info[0].IsFunction()) {
        Napi::TypeError::New(env,
                             "Expected as arguments: errorCallback, onCallback, progressCallback, procId (optional)").ThrowAsJavaScriptException();
        return env.Undefined();;
    }
    uint32_t processId;
    if (info.Length() >= 4) {
        processId = info[3].As<Napi::Number>().Uint32Value();
    }

    Function errorCb = info[0].As<Function>();
    Function okCb = info[1].As<Function>();
    Function progressCb = info[2].As<Function>();

    addon_data->processId = processId;

    _hookMessageLoop = CreateThread(NULL, 0, HooksMessageLoop, NULL, 0, NULL);

    wk = new EchoWorker(okCb, errorCb, progressCb, processId);
    wk->Queue();

    return env.Undefined();
}

Napi::Value RemoveHook(const Napi::CallbackInfo &info) {
    clearData();
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
    if (wk != NULL) {
        delete wk;
    }
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
