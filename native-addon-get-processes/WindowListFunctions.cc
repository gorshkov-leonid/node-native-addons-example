#include<napi.h>
#include <Windows.h>
#include <Psapi.h>
#include <iostream>

#include <thread>
#include <list>
#include <inttypes.h> //PRIx64???

using namespace Napi;

class CaptureEntity
{

  public:
    std::string id;
    std::string label;
    std::string type;
    std::string windowClassName;
    std::string windowName;
    std::string exeFullName;
    bool antiCheat = 0;
    bool once = 0;
    uint64_t hwnd = 0;

    CaptureEntity(){};

    ~CaptureEntity()
    {
    }
};

BOOL CALLBACK EnumerationCallbackWrapper(HWND hWnd, LPARAM lParam);

// http://stackoverflow.com/questions/7277366/why-does-enumwindows-return-more-windows-than-i-expected
BOOL IsAltTabWindow(HWND hwnd)
{
  TITLEBARINFO ti;
  HWND hwndTry, hwndWalk = NULL;

  if (!IsWindowVisible(hwnd))
    return FALSE;
  hwndTry = GetAncestor(hwnd, GA_ROOTOWNER);
  while (hwndTry != hwndWalk)
  {
    hwndWalk = hwndTry;
    hwndTry = GetLastActivePopup(hwndWalk);
    if (IsWindowVisible(hwndTry))
      break;
  }
  if (hwndWalk != hwnd)
    return FALSE;

  // the following removes some task tray programs and "Program Manager"
  ti.cbSize = sizeof(ti);
  GetTitleBarInfo(hwnd, &ti);
  if (ti.rgstate[0] & STATE_SYSTEM_INVISIBLE && !(ti.rgstate[0] & STATE_SYSTEM_FOCUSABLE))
    return FALSE;

  // Tool windows should not be displayed either, these do not appear in the
  // task bar.
  if (GetWindowLong(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW)
    return FALSE;

  return TRUE;
}

BOOL IsWindowBlacklisted(CaptureEntity* capture) {
  if (capture->windowClassName.empty()) {
    return TRUE;
  }

  if (capture->windowName.empty()) {
    return TRUE;
  }

  //if (capture->windowClassName.compare("Progman") == 0 ||
  //    capture->windowClassName.compare("Button") == 0) {
  //  return TRUE;
  //}
  // Windows 8 introduced a "Modern App" identified by their class name being
  // either ApplicationFrameWindow or windows.UI.Core.coreWindow. The
  // associated windows cannot be captured, so we skip them.
  // http://crbug.com/526883.
  //if (capture->windowClassName.compare("ApplicationFrameWindow") == 0 ||
  //    capture->windowClassName.compare("Windows.UI.Core.CoreWindow") == 0) {
  //  return TRUE;
  //}
  //if (capture->windowName.compare("Bebo All-In-One Streaming") == 0 &&
  //    capture->windowClassName.compare("Chrome_WidgetWin_1") == 0) {
  //  return TRUE;
  //}
  return FALSE;
}


bool WindowListComparator(CaptureEntity* left, CaptureEntity* right) {
  return left->windowName.compare(right->windowName) < 0;
}

class GetWindowListWorker : public AsyncWorker {
    public:
        GetWindowListWorker(Function& callback)
        : AsyncWorker(callback) {}

        ~GetWindowListWorker() {}
    // Executed inside the worker-thread.
    // It is not safe to access V8, or V8 data structures
    // here, so everything we need for input and output
    // should go on `this`.
    void Execute() {
        EnumWindows(EnumerationCallbackWrapper, (LPARAM) this);
        windowList.sort(WindowListComparator);
    }

    // Executed when the async work is complete
    // this function will be run inside the main event loop
    // so it is safe to use V8 again    
	void OnOK() {
	  Napi::Env env = Env();
	  
      int i = 0;
      Napi::Array result = Napi::Array::New(env, windowList.size());
      for (std::list<CaptureEntity *>::iterator it = windowList.begin(); it != windowList.end(); ++it) {
        CaptureEntity *capture = *it;
        Napi::Object obj = Napi::Object::New(env);
		obj.Set("windowName",   Napi::String::New(env, capture->windowName));
		obj.Set("windowClassName", Napi::String::New(env, capture->windowClassName));
        
        char windowHandle[20] = {0};
		std::sprintf(windowHandle, "0x%016" PRIx64, capture->hwnd);
		obj.Set("windowHandle",  Napi::String::New(env, windowHandle));
		obj.Set("exeFullName",  Napi::String::New(env, capture->exeFullName));

		result.Set(i, obj);
        i++;
      }
      Callback().MakeCallback(env.Global(), { result });
   }

    BOOL EnumerationCallback(HWND hWnd) {
      WCHAR title[1024] = { 0 };
      WCHAR class_name[1024] = { 0 };
      DWORD pid = 0;

      if (!IsAltTabWindow(hWnd)) {
        return TRUE;
      }

      CaptureEntity *capture = new CaptureEntity();

      GetWindowTextW(hWnd, title, 1024);
      GetClassNameW(hWnd, class_name, 1024);
      GetWindowThreadProcessId(hWnd, &pid);

      char title_utf8[1024] = { 0 };
      int title_utf8_size = 1024;
      WideCharToMultiByte(CP_UTF8, 0, title, sizeof(title), title_utf8, title_utf8_size, NULL, NULL);
      capture->windowName.append(title_utf8);

      char class_name_utf8[1024] = { 0 };
      int class_name_utf8_size = 1024;
      WideCharToMultiByte(CP_UTF8, 0, class_name, sizeof(class_name), class_name_utf8, class_name_utf8_size, NULL, NULL);
      capture->windowClassName.append(class_name_utf8);
      capture->hwnd = (uint64_t) hWnd;

      HANDLE handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, false, pid);
      if (handle != NULL) {
        WCHAR exe_name[1024] = { 0 };
        char exe_name_utf8[1024] = { 0 };
        int exe_name_utf8_size = 1024;
        GetModuleFileNameExW(handle, NULL, exe_name, 1024);
        CloseHandle(handle);
        WideCharToMultiByte(CP_UTF8, 0, exe_name, sizeof(exe_name), exe_name_utf8, exe_name_utf8_size, NULL, NULL);
        capture->exeFullName.append(exe_name_utf8);
      }

      if (!IsWindowBlacklisted(capture))
        windowList.push_back(capture);

      return TRUE;
    }
	
	protected:
       std::list<CaptureEntity *> windowList;
};

BOOL CALLBACK EnumerationCallbackWrapper(HWND hWnd, LPARAM lParam) {
  GetWindowListWorker * worker = (GetWindowListWorker *) lParam;
  return worker->EnumerationCallback(hWnd);
}


Napi::Value GetWindowList(const Napi::CallbackInfo& info) {
    Napi::Function cb = info[0].As<Napi::Function>();
    GetWindowListWorker* wk = new GetWindowListWorker(cb);
    wk->Queue();
    return info.Env().Undefined();
}

Napi::Object InitWindowListFunctions(Napi::Env env, Napi::Object exports) {
    exports.Set(Napi::String::New(env, "getWindowList"), Napi::Function::New(env, GetWindowList));
    return exports;
}
NODE_API_MODULE(WindowListFunctions, InitWindowListFunctions)
