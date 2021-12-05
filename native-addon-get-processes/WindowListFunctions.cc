#include<napi.h>
#include <Windows.h>
#include <Psapi.h>
#include <iostream>

#include <thread>
#include <list>
#include <inttypes.h> //PRIx64???
#include <locale>
#include <codecvt>
#include <strsafe.h>
#include <winver.h>

#pragma comment(lib, "version")

using namespace Napi;
using namespace std;

class CaptureEntity {
// todo A vs W
//      https://docs.microsoft.com/en-us/windows/win32/intl/conventions-for-function-prototypes
//
// todo friendly name https://stackoverflow.com/questions/18436407/is-there-a-way-to-find-a-friendly-name-for-an-executable-given-its-path
//      https://stackoverflow.com/questions/64321036/c-win32-getting-app-name-using-pid-and-executable-path
//      https://stackoverflow.com/questions/4570174/how-to-get-the-process-name-in-c
// todo utf8 lpstr lpwstr
//      https://stackoverflow.com/questions/8532855/how-to-convert-from-wchar-t-to-lpstr
//      https://stackoverflow.com/questions/21456926/how-do-i-convert-a-string-in-utf-16-to-utf-8-in-c WideCharToMultiByte
//      https://stackoverflow.com/questions/25550013/string-types-in-c-how-to-convert-from-lpwstr-to-lpstr WideCharToMultiByte
//         https://www.cyberforum.ru/win-api/thread52991.html
//      my question:
//      https://social.msdn.microsoft.com/Forums/sqlserver/en-US/d1d6bec9-04fe-4bb4-84e5-cbfdae4b051f/how-to-use-windowsh-in-lpstr-instead-of-lpwstr?forum=vcgeneral
// todo std:string std:wstring
//      all ways are deprecated
//      http://stack.imagetube.xyz/791/how-convert-std-string-to-std-wstring-in-c
// todo version.lib error LNK2019: ссылка на неразрешенный внешний символ GetFileVersionInfoSizeA
//      https://stackoverflow.com/questions/7028304/error-lnk2019-when-using-getfileversioninfosize
//

public:
    DWORD id;
    std::string label;
    std::string type;
    std::string windowClassName;
    std::string windowName;
    std::string friendlyName;
    std::string exeFullName;
    bool antiCheat = 0;
    bool once = 0;
    uint64_t hwnd = 0;

    CaptureEntity() {};

    ~CaptureEntity() {
    }
};

BOOL CALLBACK EnumerationCallbackWrapper(HWND hWnd, LPARAM lParam);

// http://stackoverflow.com/questions/7277366/why-does-enumwindows-return-more-windows-than-i-expected
BOOL IsAltTabWindow(HWND hwnd) {
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

BOOL IsWindowBlacklisted(CaptureEntity *capture) {
    if (capture->windowClassName.empty()) {
        return TRUE;
    }
    if (capture->windowName.empty()) {
        return TRUE;
    }
    // fixme ???
//    if (capture->friendlyName.empty()) {
//        return TRUE;
//    }
    // Windows 8 introduced a "Modern App" identified by their class name being
    // either ApplicationFrameWindow or windows.UI.Core.coreWindow. The
    // associated windows cannot be captured, so we skip them.
    // http://crbug.com/526883.
    if (capture->windowClassName.compare("ApplicationFrameWindow") ==
        0/* || capture->windowClassName.compare("Windows.UI.Core.CoreWindow") == 0*/) {
        return TRUE;
    }
    //if (capture->windowClassName.compare("Progman") == 0 ||
    //    capture->windowClassName.compare("Button") == 0) {
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
    GetWindowListWorker(Function &callback)
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
            obj.Set("Id", Napi::Number::New(env, capture->id));
            obj.Set("windowName", Napi::String::New(env, capture->windowName));
            obj.Set("friendlyName", Napi::String::New(env, capture->friendlyName));
            obj.Set("windowClassName", Napi::String::New(env, capture->windowClassName));

            char windowHandle[20] = {0};
            std::sprintf(windowHandle, "0x%016"
            PRIx64, capture->hwnd);
            obj.Set("windowHandle", Napi::String::New(env, windowHandle));
            obj.Set("exeFullName", Napi::String::New(env, capture->exeFullName));

            result.Set(i, obj);
            delete capture;
            i++;
        }
        Callback().MakeCallback(env.Global(), {result});
    }

    struct LANGANDCODEPAGE {
        WORD wLanguage;
        WORD wCodePage;
    } *lpTranslate;

    // https://stackoverflow.com/questions/64321036/c-win32-getting-app-name-using-pid-and-executable-path
    std::string GetApplicationName(DWORD pid) {
        cout << "AAA 1\n";
        HANDLE handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        cout << "AAA 2\n";
        if (!handle) return ""s;
        cout << "AAA 3\n";
        wchar_t pszFile[MAX_PATH] = L"";
        cout << "AAA 4\n";
        DWORD len = MAX_PATH;
        cout << "AAA 5\n";
        QueryFullProcessImageNameW(handle, 0, pszFile, &len);
        cout << "AAA 6\n";
        UINT dwBytes, cbTranslate;
        cout << "AAA 7\n";
        DWORD dwSize = GetFileVersionInfoSizeW(pszFile, (DWORD * ) & dwBytes);
        cout << "AAA 8\n";
        if (dwSize == 0) return ""s;
        cout << "AAA 9\n";
        LPVOID lpData = (LPVOID) malloc(dwSize);
        cout << "AAA 10\n";
        std::string result;
        cout << "AAA 11\n";
        ZeroMemory(lpData, dwSize);
        cout << "AAA 12\n";
        if (GetFileVersionInfoW(pszFile, 0, dwSize, lpData)) {
            cout << "AAA 13\n";
            VerQueryValueW(lpData,
                           L"\\VarFileInfo\\Translation",
                           (LPVOID * ) & lpTranslate,
                           &cbTranslate);
            cout << "AAA 14\n";
            wchar_t strSubBlock[MAX_PATH] = {0};
            cout << "AAA 15\n";
            wchar_t *lpBuffer;
            cout << "AAA 16\n";
            for (int i = 0; i < (cbTranslate / sizeof(struct LANGANDCODEPAGE)); i++) {
                cout << "AAA 17\n";
                StringCchPrintfW(strSubBlock, 50,
                                 L"\\StringFileInfo\\%04x%04x\\FileDescription",
                                 lpTranslate[i].wLanguage,
                                 lpTranslate[i].wCodePage);
                cout << "AAA 18\n";
                VerQueryValueW(lpData,
                               strSubBlock,
                               (void **) &lpBuffer,
                               &dwBytes);
                cout << "AAA 19\n";
                //fixme deprecated
                std::wstring_convert <std::codecvt_utf8_utf16<wchar_t>> converter;
                cout << "AAA 20\n";
                result = converter.to_bytes(lpBuffer);
            }
        }
        cout << "AAA 21\n";
        if (lpData) free(lpData);
        cout << "AAA 22\n";
        if (handle) CloseHandle(handle);
        cout << "AAA 23\n";
        return result;
    }

    BOOL EnumerationCallback(HWND hWnd) {
        WCHAR title[1024] = {0};
        WCHAR class_name[1024] = {0};
        DWORD pid = 0;

        if (!IsAltTabWindow(hWnd)) {
            return TRUE;
        }

        CaptureEntity *capture = new CaptureEntity();

        GetWindowTextW(hWnd, title, 1024);
        GetClassNameW(hWnd, class_name, 1024);
        GetWindowThreadProcessId(hWnd, &pid);
        capture->id = pid;
        char title_utf8[1024] = {0};
        int title_utf8_size = 1024;
        WideCharToMultiByte(CP_UTF8, 0, title, sizeof(title), title_utf8, title_utf8_size, NULL, NULL);
        capture->windowName.append(title_utf8);

        char class_name_utf8[1024] = {0};
        int class_name_utf8_size = 1024;
        WideCharToMultiByte(CP_UTF8, 0, class_name, sizeof(class_name), class_name_utf8, class_name_utf8_size, NULL,
                            NULL);
        capture->windowClassName.append(class_name_utf8);
        capture->hwnd = (uint64_t) hWnd;

        std::string friendlyName = GetApplicationName(capture->id);
        capture->friendlyName.append(friendlyName);


        HANDLE handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, false, pid);
        if (handle != NULL) {
            WCHAR exe_name[1024] = {0};
            char exe_name_utf8[1024] = {0};
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
