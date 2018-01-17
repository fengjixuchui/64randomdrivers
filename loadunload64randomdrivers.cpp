#include <Windows.h>
#include <winternl.h>
#include <conio.h>


#include <filesystem>
#include <iostream>
#include <algorithm>
#include <string>
#include <cstdlib>
#include <limits>

#pragma comment(lib, "ntdll.lib")
#pragma comment(lib, "AdvAPI32.Lib")

using namespace std;


// Kiem tra va lay vai quyen neu can thiet 
// (C) kingofthedead
bool EnablePrivilege(	LPCWSTR lpPrivilegeName )
{
	TOKEN_PRIVILEGES Privilege;
	HANDLE hToken;
	DWORD dwErrorCode;

	Privilege.PrivilegeCount = 1;
	Privilege.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

	if (!LookupPrivilegeValueW(NULL, lpPrivilegeName,
		&Privilege.Privileges[0].Luid))
		return GetLastError();

	if (!OpenProcessToken(GetCurrentProcess(),
		TOKEN_ADJUST_PRIVILEGES, &hToken))
		return GetLastError();

	if (!AdjustTokenPrivileges(hToken, FALSE, &Privilege, sizeof(Privilege),
		NULL, NULL)) {
		dwErrorCode = GetLastError();
		CloseHandle(hToken);
		return dwErrorCode;
	}

	CloseHandle(hToken);
	return TRUE;
}


#pragma region Driver and Services processing
// (C) kingofthedead
bool create_driver_service(const std::wstring& service_name, const std::wstring& driver)
{
	std::wstring reg_key = L"System\\CurrentControlSet\\Services\\" + service_name;
	HKEY hKey;
	if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, reg_key.c_str(), NULL, nullptr, NULL, KEY_ALL_ACCESS, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {

		LSTATUS err = RegSetValueExW(hKey, L"ImagePath", 0, REG_MULTI_SZ, (const BYTE*)driver.c_str(), (driver.length() + 1) * sizeof(wchar_t));
		if (err != 0) {
			::RegCloseKey(hKey);
			return false;
		}
		DWORD dwType = 1;
		err = RegSetValueExW(hKey, L"Type", 0, REG_DWORD, (const BYTE*)&dwType, sizeof(DWORD));
		if (err != 0) {
			::RegCloseKey(hKey);
			::RegDeleteKeyEx(HKEY_LOCAL_MACHINE, reg_key.c_str(), DELETE, 0);
			return false;
		}
		DWORD dwErrorControl = 1;
		err = RegSetValueExW(hKey, L"ErrorControl", 0, REG_DWORD, (const BYTE*)&dwErrorControl, sizeof(DWORD));
		if (err != 0) {
			::RegCloseKey(hKey);
			return false;
		}
		DWORD dwStartType = 3;
		err = RegSetValueExW(hKey, L"Start", 0, REG_DWORD, (const BYTE*)&dwStartType, sizeof(DWORD));
		if (err != 0) {
			::RegCloseKey(hKey);
			return false;
		}
		::RegCloseKey(hKey);
		cout << "create_driver_service OK" << endl;
		return true;
	}
	else
		return false;
}

// (C) kingofthedead
bool load_driver(const std::wstring& service_name)
{
	std::wstring name = L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\" + service_name;
	NTSTATUS(NTAPI *NtLoadDriver)(IN PUNICODE_STRING DriverServiceName);
	*(FARPROC *)&NtLoadDriver = GetProcAddress(GetModuleHandle(L"ntdll.dll"), "NtLoadDriver");
	if (NtLoadDriver == nullptr)
		return false;
	UNICODE_STRING Path;
	RtlInitUnicodeString(&Path, name.c_str());
	cout << "load_driver OK" << endl;
	return NT_SUCCESS(NtLoadDriver(&Path));
}
// (C) kingofthedead
bool unload_driver(const std::wstring& service_name)
{
	std::wstring name = L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\" + service_name;
	NTSTATUS(NTAPI *NtUnloadDriver)(IN PUNICODE_STRING DriverServiceName);
	*(FARPROC *)&NtUnloadDriver = GetProcAddress(GetModuleHandle(L"NTDLL.DLL"), "NtUnloadDriver");
	if (NtUnloadDriver == nullptr)
		return false;
	UNICODE_STRING Path;
	RtlInitUnicodeString(&Path, name.c_str());
	cout << "unload_driver OK" << endl;
	return NT_SUCCESS(NtUnloadDriver(&Path));
}
// (C) kingofthedead
bool destroy_driver_service(const std::wstring& service_name)
{
	std::wstring reg_key = L"System\\CurrentControlSet\\Services\\" + service_name;
	HKEY hKey;
	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, reg_key.c_str(), NULL, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS) {
		RegDeleteValue(hKey, L"ImagePath");
		RegDeleteValue(hKey, L"Type");
		RegDeleteValue(hKey, L"Start");
		RegDeleteValue(hKey, L"ErrorControl");
		::RegCloseKey(hKey);
		::RegDeleteKeyEx(HKEY_LOCAL_MACHINE, reg_key.c_str(), DELETE, 0);

		cout << "destroy_driver_service OK" << endl;
		return true;
	}
	else
		return true; //check GetLastError = something  
    //TODO: Chu y
}

#pragma endregion


std::wstring s2ws(const std::string& str)
{
	int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
	std::wstring wstrTo(size_needed, 0);
	MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
	return wstrTo;
}

string remove(char* charToRemove, string &str) {
	//hxxps://stackoverflow.com/questions/31769162/how-to-delete-characters-in-a-string
	//string s = "Pineapple";
	//char chars[] = "p";
	for (unsigned int i = 0; i < strlen(charToRemove); ++i)
	{
		str.erase(remove(str.begin(), str.end(), charToRemove[i]), str.end());
	}

	cout << str << endl;
	return str;
}

//TODO: make it real "RANDOM"
void loadunload64randomdrivers()
{
	const std::string DRIVERS_PATH = "C://Windows//System32//drivers//";
	
	std::vector<string> myFileList;
	std::vector<string> myServiceNameList;
	int myFileListCount = 0;


	namespace fs = std::experimental::filesystem;

	for (auto & p : fs::directory_iterator(DRIVERS_PATH))
	{			
		//Get file item, filename
		myFileList.push_back(p.path().string());

		string fname = p.path().filename().string();		


		//Create service name with suffix sys : winhv.sys -> winhvsys
		char dot[] = ".";
		fname = remove(dot, fname);
		myServiceNameList.push_back(fname);

		//count to the last
		myFileListCount++;
	}
	
	for (int i = 0; i < myFileListCount; i++)
	{
		string servicename = myServiceNameList.at(i).c_str();
		string sysfilepath = myFileList.at(i).c_str();
		wstring wst_servicename = s2ws(servicename);

		//Print to test string
		std::cout << servicename << std::endl;
		std::cout << sysfilepath << std::endl;

		//Try to load 64 first items
		try 
		{
			std::cout << " Try # " << i << endl;
			load_driver(wst_servicename);
			create_driver_service(wst_servicename, s2ws(sysfilepath) );

			destroy_driver_service(wst_servicename);
			unload_driver(wst_servicename);

			//Try to load first item for testing
			if (i == 0) break;
		}
		catch (std::exception ex)
		{
			std::cout << "Exception: "<< ex.what() << std::endl;
		}
	}
	
}


int wmain(int argc, wchar_t* argv[])
{
	loadunload64randomdrivers();
	_getch();


	return 0;
}
