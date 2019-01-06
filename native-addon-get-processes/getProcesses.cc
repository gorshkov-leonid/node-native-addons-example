#include <Windows.h>
#include <stdio.h>
#include <tchar.h>
#include <Psapi.h>

#include <Winuser.h>
#include <iostream>
#include<set>
#include<map>

#include <napi.h> //c++ node-addon-api that uses c-ish n-api headers inside


using namespace std;

typedef struct {
  std::set<DWORD> processIds;
  std::multimap<DWORD,HWND> processIdToHandles;
} EnumWndProcData;

typedef struct {
  TCHAR* processName;
  TCHAR* fileName;
} ProcInfo;

ProcInfo* GetProcessInfo( DWORD processID )
{
    TCHAR szProcessName[MAX_PATH] = TEXT("<?>");
	TCHAR szFileName[MAX_PATH + 1] = TEXT("<?>");

    HANDLE hProcess = OpenProcess( PROCESS_QUERY_INFORMATION |
                                   PROCESS_VM_READ,
                                   FALSE, processID );
    if (NULL != hProcess )
    {
        HMODULE hMod;
        DWORD cbNeeded;

        if ( EnumProcessModules( hProcess, &hMod, sizeof(hMod), &cbNeeded) )
        {
            GetModuleBaseName( hProcess, hMod, szProcessName, sizeof(szProcessName)/sizeof(TCHAR) );
			GetModuleFileName(hMod, szFileName, MAX_PATH + 1);
        }
		
		ProcInfo procInfo = {};
	    procInfo.processName = szProcessName;
	    procInfo.fileName = szFileName;
		CloseHandle( hProcess );
		return &procInfo;
    }
    return NULL;
    // Print the process name and identifier.
    //std::cout << "hook has been already added!"<< std::endl;
    //_tprintf( TEXT("%s %s (PID: %u)\n"), szProcessName, szFileName, processID );
}

BOOL CALLBACK enum_wnd_proc(HWND hwnd, LPARAM lParam)
{
  
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




/*
https://toster.ru/q/55778
https://docs.microsoft.com/en-gb/windows/desktop/winmsg/window-functions
https://docs.microsoft.com/en-us/windows/desktop/psapi/enumerating-all-processes
https://thispointer.com/stdset-tutorial-part-1-set-usage-details-with-default-sorting-criteria/
https://ru.cppreference.com/w/cpp/container/set
https://thispointer.com/multimap-example-and-tutorial-in-c/
https://thispointer.com/finding-all-values-for-a-key-in-multimap-using-equals_range-example/  !!!
https://en.cppreference.com/w/cpp/container/multimap/equal_range !!!
*/
Napi::Value GetProcessesList(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
	
    EnumWndProcData enumWndProcData = {};
	EnumWindows(enum_wnd_proc, (LPARAM)(&enumWndProcData));
	std::multimap<DWORD,HWND> processIdToHandles = enumWndProcData.processIdToHandles;
	
	for(std::multimap<DWORD, HWND>::iterator it = processIdToHandles.begin(), end = processIdToHandles.end(); it != end; it = processIdToHandles.upper_bound(it->first))
    {
	  DWORD windowProcessId = it->first;
	  ProcInfo* procInfo = GetProcessInfo(windowProcessId);
	  if(procInfo)
	  {
		  TCHAR* processName = procInfo -> processName;
		  TCHAR* fileName = procInfo -> fileName;
		  std::cout << windowProcessId << " " << processName << " " << fileName << std::endl;
	  
		  auto handleRange = processIdToHandles.equal_range(windowProcessId);
		  for (auto i = handleRange.first; i != handleRange.second; ++i)
		  {
			  HWND windowHandle = i->second;
			  std::cout << "             " << windowHandle << std::endl;		  
		  }
	  }
    }
  
    /*
    for (std::pair<DWORD, HWND>  processIdToHandle : processIdToHandles)
	{
		DWORD windowProcessId = processIdToHandle.first;
		DWORD windowHandle = processIdToHandle.second;
		
		PrintProcessNameAndID(windowProcessId);  
		std::cout << windowProcessId << " :: " << processIdToHandle << std::endl;
	}
	*/	
	
	return env.Undefined();
}




Napi::Object Init(Napi::Env env, Napi::Object exports) {
    exports.Set(Napi::String::New(env, "getProcessesList"), Napi::Function::New(env, GetProcessesList));
    return exports;
}
NODE_API_MODULE(getProcesses, Init)







