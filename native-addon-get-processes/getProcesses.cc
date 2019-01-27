#include <Windows.h>
#include <stdio.h>
#include <tchar.h>
#include <Psapi.h>

#include <Winuser.h>
#include <iostream>
#include<set>
#include<map>
#include <tlhelp32.h>

#include <napi.h> //c++ node-addon-api that uses c-ish n-api headers inside

/*
https://toster.ru/q/55778
https://docs.microsoft.com/en-gb/windows/desktop/winmsg/window-functions
https://docs.microsoft.com/en-us/windows/desktop/psapi/enumerating-all-processes
https://thispointer.com/stdset-tutorial-part-1-set-usage-details-with-default-sorting-criteria/
https://ru.cppreference.com/w/cpp/container/set
https://thispointer.com/multimap-example-and-tutorial-in-c/
https://thispointer.com/finding-all-values-for-a-key-in-multimap-using-equals_range-example/  !!!
https://en.cppreference.com/w/cpp/container/multimap/equal_range !!!
https://github.com/Microsoft/vscode-windows-process-tree


!!!! https://stackoverflow.com/questions/6202547/win32-get-main-wnd-handle-of-application
*/

using namespace std;

typedef struct {
  std::set<DWORD> processIds;
  std::multimap<DWORD,HWND> processIdToHandles;
} EnumWndProcData;

typedef struct {
  TCHAR processName[MAX_PATH];
} ProcInfo;

struct ProcRelation {
  DWORD pid;
  DWORD ppid;
};

// http://stackoverflow.com/questions/7277366/why-does-enumwindows-return-more-windows-than-i-expected
BOOL IsAltTabWindow1(HWND hwnd)
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

std::map<DWORD,ProcRelation> GetProcessRelations() {
  std::map<DWORD,ProcRelation> processInfos;
	
  // Fetch the PID and PPIDs
  PROCESSENTRY32 process_entry = { 0 };
  DWORD parent_pid = 0;
  HANDLE snapshot_handle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  process_entry.dwSize = sizeof(PROCESSENTRY32);
  if (Process32First(snapshot_handle, &process_entry)) {
    do {
      if (process_entry.th32ProcessID != 0) {
		  
		  ProcRelation procInfo = { 
		      process_entry.th32ProcessID, 
		      process_entry.th32ParentProcessID 
          };
		  
		  processInfos.insert(std::pair<DWORD, ProcRelation>(process_entry.th32ProcessID, procInfo));
		  
      }
    } while (Process32Next(snapshot_handle, &process_entry));
  }
  
  CloseHandle(snapshot_handle);
  return processInfos;
}


ProcInfo* GetProcessInfo( DWORD processID )
{
    TCHAR szProcessName[MAX_PATH] = TEXT("<?>");

    HANDLE hProcess = OpenProcess( PROCESS_QUERY_INFORMATION |
                                   PROCESS_VM_READ,
                                   FALSE, processID );
    if (NULL != hProcess )
    {
        HMODULE hMod;
        DWORD cbNeeded;

		ProcInfo procInfo = {};
		
        if ( EnumProcessModules( hProcess, &hMod, sizeof(hMod), &cbNeeded) )
        {
            GetModuleBaseName( hProcess, hMod, procInfo.processName, sizeof(procInfo.processName)/sizeof(TCHAR) + 1 );
        }
		CloseHandle( hProcess );
		return &procInfo;
    }
    //std::cout << "hook has been already added!"<< std::endl;
    //_tprintf( TEXT("%s %s (PID: %u)\n"), szProcessName, szFileName, processID );

    return NULL;
}

BOOL CALLBACK enum_wnd_proc(HWND hwnd, LPARAM lParam)
{
  
  if (!IsAltTabWindow1(hwnd)) {
        return TRUE;
  }
  
  DWORD windowProcessId = 0;
  GetWindowThreadProcessId(hwnd, &windowProcessId);
  //std::cout << hwnd << "  " << windowProcessId << std::endl;
  if(windowProcessId)
  {
	EnumWndProcData* enumWndProcData = (EnumWndProcData*)lParam;
    std::multimap<DWORD, HWND>* processIdToHandles = &(enumWndProcData->processIdToHandles);
    std::set<DWORD>* processIds = &(enumWndProcData->processIds);
	
	processIdToHandles->insert(std::pair<DWORD, HWND>(windowProcessId, hwnd));
	
    if(processIds->find(windowProcessId) != processIds->end())
    {
      return TRUE;
    }
	
	processIds->insert(windowProcessId);
  }
 
  //EnumChildWindows(hwnd, enum_child_wnd_proc, (LPARAM)0);
  return TRUE;
}



void PrintProcessNameAndID( DWORD processID )
{
    TCHAR szProcessName[MAX_PATH] = TEXT("<unknown>");

    // Get a handle to the process.

    HANDLE hProcess = OpenProcess( PROCESS_QUERY_INFORMATION |
                                   PROCESS_VM_READ,
                                   FALSE, processID );

    // Get the process name.

    if (NULL != hProcess )
    {
        HMODULE hMod;
        DWORD cbNeeded;

        if ( EnumProcessModules( hProcess, &hMod, sizeof(hMod), 
             &cbNeeded) )
        {
            GetModuleBaseName( hProcess, hMod, szProcessName, 
                               sizeof(szProcessName)/sizeof(TCHAR) );
        }
    }

    // Print the process name and identifier.

    _tprintf( TEXT("%s  (PID: %u)\n"), szProcessName, processID );

    // Release the handle to the process.

    CloseHandle( hProcess );
}


Napi::Value GetProcessesList(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
	
	
	
	 DWORD aProcesses[1024], cbNeeded, cProcesses;
    unsigned int i;

    if ( !EnumProcesses( aProcesses, sizeof(aProcesses), &cbNeeded ) )
    {
        return  env.Undefined();
    }


    // Calculate how many process identifiers were returned.

    cProcesses = cbNeeded / sizeof(DWORD);

    // Print the name and process identifier for each process.

    for ( i = 0; i < cProcesses; i++ )
    {
        if( aProcesses[i] != 0 )
        {
            PrintProcessNameAndID( aProcesses[i] );
        }
    }

	
	
	
	
    EnumWndProcData enumWndProcData = {};
	EnumWindows(enum_wnd_proc, (LPARAM)(&enumWndProcData));
	std::multimap<DWORD,HWND> processIdToHandles = enumWndProcData.processIdToHandles;
	
	auto processRelations = GetProcessRelations();
	
	for(std::multimap<DWORD, HWND>::iterator it = processIdToHandles.begin(), end = processIdToHandles.end(); it != end; it = processIdToHandles.upper_bound(it->first))
    {
	  std::cout << "-----------------" << std::endl;  		  
	  DWORD windowProcessId = it->first;
	  ProcInfo* procInfo = GetProcessInfo(windowProcessId);
	  if(procInfo)
	  {
		  TCHAR* processName = procInfo -> processName;
		  
	      DWORD parentPid = 0; 
		  
	      auto it = processRelations.find(windowProcessId);
          if( it != processRelations.end() ) {
		     parentPid = (it->second).ppid;
          }
		  
		  std::cout << "pid: " << windowProcessId << ", parentPid: " << parentPid << ", name: " << processName << std::endl;
		  
		  //todo: walking by processes only and collect root but not explorer.exe. And on click call GetTopWindow for used process (?)
		  //      How does Task Manager categorize processes as App, Background Process, or Windows Process? https://blogs.msdn.microsoft.com/oldnewthing/20171219-00/?p=97606 
		  //      https://stackoverflow.com/questions/53509063/explorer-exe-as-the-parent-process-in-windows
		  //      http://blogs.microsoft.co.il/pavely/2011/06/18/getshellwindow-vs-getdesktopwindow/
		  
	      //temporary: 
		  auto handleRange = processIdToHandles.equal_range(windowProcessId);
		  
		  std::set<HWND> handles;
		  for (auto i = handleRange.first; i != handleRange.second; ++i)
		  {
			  HWND windowHandle = i->second;
			  //HWND parentHandle = GetAncestor(windowHandle, GA_PARENT);
			  //HWND rootHandle = GetAncestor(windowHandle, GA_ROOT);
			  HWND rootOwnerHandle = GetAncestor(windowHandle, GA_ROOTOWNER);

              if(handles.find(rootOwnerHandle) != handles.end())
              {
                continue;
              }
			  
			  
			  BOOL isWindow = IsWindow(rootOwnerHandle);
			  //handles.insert(windowHandle);
			  //handles.insert(parentHandle);
			  //handles.insert(rootHandle);
		      if(isWindow)
			  {
				handles.insert(rootOwnerHandle);  
			  }
		  }
		  
		  for(auto handle : handles) {
			  
			  TCHAR windowHandleText[MAX_PATH];
			  GetWindowText(handle, windowHandleText,  sizeof(windowHandleText) / sizeof(TCHAR) + 1 );			  
			  std::cout << "             " << handle << " " << windowHandleText << std::endl;		
		  }    
	  }
    }
	
	return env.Undefined();
}

Napi::Object InitGetProcesses(Napi::Env env, Napi::Object exports) {
    exports.Set(Napi::String::New(env, "getProcessesList"), Napi::Function::New(env, GetProcessesList));
    return exports;
}
NODE_API_MODULE(getProcesses, InitGetProcesses);









