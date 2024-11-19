#include "exp.h"
 
#include "detours.h"
#include <fstream>
#include <vector>
#include <filesystem>
//#include "Common.h"
#include <commctrl.h>
#ifdef _WIN64
#pragma comment(lib, "detours64.lib")
#else
#pragma comment(lib, "detours86.lib")
#endif

void nlog(const char* pszFmt, ...)
{
	std::string str;
	va_list args;
	va_start(args, pszFmt);
	{
		int nLength = _vscprintf(pszFmt, args);
		nLength += 1; 
		std::vector<char> vectorChars(nLength);
		_vsnprintf_s(vectorChars.data(), nLength, 2048, pszFmt, args);
		str.assign(vectorChars.data());
	}
	va_end(args);
	if (IsWindow(GetConsoleWindow()))
	{
		printf("%s", str.c_str());
		OutputDebugStringA(str.c_str());
	}
	else {
		OutputDebugStringA(str.c_str());
	}
}


void nlog(const wchar_t* pszFmt, ...)
{
	std::wstring str;
	va_list args;
	va_start(args, pszFmt);
	{
		int nLength = _vscwprintf(pszFmt, args);
		nLength += 1;
		std::vector<wchar_t> vectorChars(nLength);
		_vsnwprintf_s(vectorChars.data(), nLength, 2048, pszFmt, args);
		str.assign(vectorChars.data());
	}
	va_end(args);
	if (IsWindow(GetConsoleWindow()))
	{
		wprintf(L"%s", str.c_str());
		OutputDebugStringW(str.c_str());
	}
	else {
		OutputDebugStringW(str.c_str());
	}
}
BOOL SetHook(__inout PVOID* ppvTarget, __in PVOID pvDetour) {
	if (DetourTransactionBegin() != NO_ERROR)
		return FALSE;

	if (DetourUpdateThread(GetCurrentThread()) == NO_ERROR)
		if (DetourAttach(ppvTarget, pvDetour) == NO_ERROR)
			if (DetourTransactionCommit() == NO_ERROR)
				return TRUE;

	DetourTransactionAbort();
	return FALSE;
}
inline std::wstring to_lower(const std::wstring& s) {
	std::wstring ret;
	for (std::wstring::const_iterator c = s.begin(); c != s.end(); ++c) {
		ret += static_cast<wchar_t>(::towlower(*c));
	}
	return ret;
}

 

WNDPROC g_OriginalFileListWndProc = NULL;
HWND fileList = NULL;
void RestoreProc()
{
	if (fileList)
	{
		if (g_OriginalFileListWndProc)
		{
			nlog("  RestoreProc();\r\n");
			SetWindowLongPtr(fileList, GWLP_WNDPROC, (LONG_PTR)g_OriginalFileListWndProc);
			fileList = NULL;
		}
	}
}


static HWND hButton;
static HWND hTextbox;
HWND tipsWindow = NULL;
void ShowErr(const char* str)
{
	MessageBeep(MB_ICONHAND);
	MessageBoxA(NULL, str, "错误", MB_ICONERROR + MB_OK + MB_SETFOREGROUND + MB_TOPMOST);
	
}
void SendToClipboard(const std::string& str) {
	if (!OpenClipboard(NULL)) {
		ShowErr("复制失败,无法打开剪切板");
		return;
	}

	EmptyClipboard();

	HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, str.size() + 1);
	if (!hGlobal) {
		ShowErr("复制失败,无法打开剪切板");
		CloseClipboard();
		return;
	}

	char* pGlobal = (char*)GlobalLock(hGlobal);
	if (pGlobal) {
		memcpy(pGlobal, str.c_str(), str.size() + 1);
		GlobalUnlock(hGlobal);
	}
	else {
		ShowErr("复制失败,无法打开剪切板");
		GlobalFree(hGlobal);
		CloseClipboard();
		return;
	}

	if (!SetClipboardData(CF_TEXT, hGlobal)) {
		ShowErr("复制失败");
		GlobalFree(hGlobal);
		CloseClipboard();
		return;
	}

	MessageBeep(MB_OK);
	CloseClipboard();
}
inline std::string utf16le_to_ansi(const std::wstring& utf16le_wstr) {

	int ansi_len = WideCharToMultiByte(CP_ACP, 0, utf16le_wstr.c_str(), -1, NULL, 0, NULL, NULL);//不传U16的长,让他自己算
	char* ansi_str = new char[ansi_len + 1];
	WideCharToMultiByte(CP_ACP, 0, utf16le_wstr.c_str(), -1, ansi_str, ansi_len, NULL, NULL);
	ansi_str[ansi_len] = '\0';	
	std::string ansi_sstr(ansi_str);	
	delete[] ansi_str;
	return ansi_sstr;
}

void ProcessED2K(HWND h,bool ball)
{
	HWND hHeader = ListView_GetHeader(h);// (HWND)SendMessageTimeout(h, LVM_GETHEADER, 0, 0, SMTO_NORMAL, 2000, NULL);
	if (hHeader == NULL) {
		ShowErr("错误,无法获取表头1.");
		return;
	}
	nlog("0x%x ->0x%x \r\n", h, hHeader);
	int columnCount = Header_GetItemCount(hHeader);// (int)SendMessageTimeout(hHeader, HDM_GETITEMCOUNT, 0, 0, SMTO_NORMAL, 2000, NULL);
 
	if (columnCount < 1)
	{
		ShowErr("错误,无法获取表头2.");
		return;
	}
 
	int ed2kcolumn = -1;
	for (int i = 0; i < columnCount; ++i) {
		wchar_t headerText[256];
		HDITEM headerItem = { 0 };
		headerItem.mask = HDI_TEXT;
		headerItem.pszText = headerText;
		headerItem.cchTextMax = sizeof(headerText) / sizeof(wchar_t);
		if (Header_GetItem(hHeader, i, &headerItem)) {
			std::wstring text = to_lower(std::wstring(headerText));
			if (text.find(L"ed2k") != std::string::npos)
			{
				ed2kcolumn = i;
				break;
				nlog(L"ed2k 位于第 %d列\r\n", i);
			}
		}				
	}
	if (ed2kcolumn == -1)
	{
		ShowErr("错误,表头中不存在ED2K");
		return;
	}
	else {
		if (ball)
		{
			int rowCount = ListView_GetItemCount(h);
			std::wstring str = L"";
			for (int i = 0; i < rowCount; ++i) {
				wchar_t itemText[2048];//可能超长
				LVITEM item = { 0 };
				item.iItem = i;
				item.iSubItem = ed2kcolumn;
				item.mask = LVIF_TEXT;
				item.pszText = itemText;
				item.cchTextMax = sizeof(itemText) / sizeof(wchar_t);
				if (ListView_GetItem(h, &item)) {
					//nlog("%d->%s\r\n", i, itemText);
					if (wcslen(itemText) > 14)//ed2k://|file|
					{
						str += std::wstring(itemText) + L"\n";
					}
				}
			}
			if (str.length() > 1)
			{
				wchar_t tempPath[MAX_PATH];
				if (GetTempPath(MAX_PATH, tempPath) == 0) {
					ShowErr("提取失败，无法创建临时文件");
					return;
				}

				wchar_t tempFilename[MAX_PATH];
				if (GetTempFileName(tempPath, L"tmp", 0, tempFilename) == 0) {
					ShowErr("提取失败，无法创建临时文件");
					return;
				}

				try {
					std::wofstream file(tempFilename);
					if (!file.is_open()) {
						ShowErr("提取失败，无法写入临时文件");
						return;
					}

					file << str <<std::endl;
					file.close();
					MessageBeep(MB_OK);
					ShellExecuteW(0, L"open", L"notepad.exe", tempFilename, NULL, SW_SHOW);
				}
				catch (const std::exception& e) {
					std::string err= "提取失败:" +std::string(e.what());
					ShowErr(err.c_str() );
				}


			}
			else {
				ShowErr("提取失败,找不到任何ED2K链接.");
			}
		 
		}
		else { //copy
			int itemIndex = -1;
			std::wstring str = L"";
			while ((itemIndex = ListView_GetNextItem(h, itemIndex, LVNI_SELECTED)) != -1) {
				 
				// 获取项的文本
				wchar_t itemText[2048];//可能超长
				LVITEM lvi = { 0 };
				lvi.iItem = itemIndex;
				lvi.iSubItem = ed2kcolumn;
				lvi.mask = LVIF_TEXT;
				lvi.pszText = itemText;
				lvi.cchTextMax = sizeof(itemText);
				ListView_GetItem(h, &lvi);
				if (wcslen(itemText) > 14)//ed2k://|file|
				{
					str += std::wstring(itemText) + L"\r\n";
				}
			}
			if (str.length() > 1)
			{
				std::string s = utf16le_to_ansi(str);
				SendToClipboard(s);
			}
			else {
				ShowErr("复制失败,找不到任何ED2K链接.");
			}
		}
		nlog("alldone\r\n");

	}
	
}
LRESULT CALLBACK NewWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	// 在这里处理消息
	switch (uMsg) {
	case WM_COMMAND:
	{
		
		int mid = LOWORD(wParam);
		if (mid == 0xdead)//copy
		{
			ProcessED2K(hwnd,false);
			nlog("复制.\r\n");
		}
		else if (mid == 0xdeae) //export all
		{
			ProcessED2K(hwnd,true);
			nlog("导出.\r\n");
		}
		 
		break;
	}
	case WM_CLOSE:
	{
		 
		RestoreProc();
		break;
	}
	case WM_DESTROY:
	{
		 
		RestoreProc();
		break;
	}
	}

	// 调用原始窗口过程
	return CallWindowProc(g_OriginalFileListWndProc, hwnd, uMsg, wParam, lParam);
}

void WorkWork()
{
	 
	static decltype(&TrackPopupMenuEx) _TrackPopupMenuEx = &TrackPopupMenuEx;
	decltype(&TrackPopupMenuEx) _TrackPopupMenuEx_Hook = [](_In_ HMENU hMenu, _In_ UINT uFlags, _In_ int x, _In_ int y, _In_ HWND hWnd, _In_opt_ LPTPMPARAMS lptpm) -> BOOL {
		//nlog("popupmenuex:hmenu:%p hwnd:%p\r\n", hMenu, hWnd);
		int menuItemCount = GetMenuItemCount(hMenu);
		if (menuItemCount > 0)
		{
			if (IsWindow(hWnd))
			{
				char cls[256];
				if (GetClassNameA(hWnd, cls, 256) != 0)
				{
					if (strcmp(cls, "SysListView32") == 0)//是 列表的菜单
					{
						int nPositon = -1;
						for (int i = menuItemCount - 1; i > 0; --i) {
							MENUITEMINFO mii = { 0 };
							mii.cbSize = sizeof(MENUITEMINFO);
							mii.fMask = MIIM_STRING | MIIM_ID;
							mii.dwTypeData = new wchar_t[256];
							mii.cch = 256;

							if (GetMenuItemInfoW(hMenu, i, TRUE, &mii)) {
								if (mii.wID == 0xdead)//应该没人用这个吧。。。
								{

									nlog("已添加");
									break;
								}
								else {
									std::wstring cn = to_lower(std::wstring(mii.dwTypeData));
									if (cn.find(L"ed2k") != std::string::npos)
									{
										nPositon = i + 1;
										if (nPositon >= menuItemCount)
										{
											menuItemCount = menuItemCount - 1;
										}
										nlog("找到ed2k相关选项，添加 pos:%d\r\n", nPositon);
										break;
									}
								}
								//nlog(L"->%d:%s/0x%x\r\n", i, mii.dwTypeData, mii.wID);
							}
							delete[] mii.dwTypeData;
						}
						if (nPositon > -1)
						{
							
							InsertMenuW(hMenu, nPositon, MF_BYPOSITION | MF_STRING | MF_ENABLED, 0xdeae, L"提取所有ED2K链接(&X)");
							InsertMenuW(hMenu, nPositon  , MF_BYPOSITION | MF_STRING | MF_ENABLED, 0xdead, L"复制选中的ED2K链接(&C)");
							InsertMenuW(hMenu, nPositon, MF_BYPOSITION | MF_SEPARATOR | MF_STRING | MF_ENABLED, 0, L"-");
							if (hWnd != fileList)
							{
								RestoreProc();//可能切换视图，先清除
								fileList = hWnd;
								g_OriginalFileListWndProc = (WNDPROC)GetWindowLongPtr(fileList, GWLP_WNDPROC);
								SetWindowLongPtr(fileList, GWLP_WNDPROC, (LONG_PTR)NewWndProc);
								nlog("子类化 窗口:0x%x\r\n", fileList);

							}
							else {
								nlog("子类化过了 窗口:0x%x\r\n", hWnd);
							}
							int rr = (int)_TrackPopupMenuEx(hMenu, uFlags, x, y, hWnd, lptpm);
							if (rr)
							{
								//nlog("选择了：0x%x\r\n", rr);
							}
							return (BOOL)rr;
						}
					}
				}
				else {
					nlog("获取窗口类名失败.");
				}


			}
		}
 

		return _TrackPopupMenuEx(hMenu, uFlags, x, y, hWnd, lptpm);
		};

	SetHook(reinterpret_cast<void**>(&_TrackPopupMenuEx), _TrackPopupMenuEx_Hook);
	/**
	static decltype(&InsertMenuItemW) _InsertMenuItemW = &InsertMenuItemW;
	decltype(&InsertMenuItemW) _InsertMenuItemW_Hook = [](_In_ HMENU hMenu, _In_ UINT uItem, _In_ BOOL fByPosition, _In_ const MENUITEMINFOW* lpmii) -> BOOL {
		nlog(L"InsertMenuItemW: hMenu:%p uItem:%u fByPosition:%d\r\n", hMenu, uItem, fByPosition);
		//if (uItem>5)
		//std::wstring cn = to_lower(std::wstring(lpmii->dwTypeData));

		return _InsertMenuItemW(hMenu, uItem, fByPosition, lpmii);
		};
	SetHook(reinterpret_cast<void**>(&_InsertMenuItemW), _InsertMenuItemW_Hook);**/
}
bool hook = true;
void Main(HINSTANCE hinstDLL)
{
	if (!hook)
	{
		return;
	}
	LPSTR a1 = new CHAR[MAX_PATH];
	GetModuleFileNameA(hinstDLL, a1, MAX_PATH);
	std::filesystem::path a2(a1);
	std::string filename = a2.filename().string();
	std::for_each(filename.begin(), filename.end(), [](char& character) { character = ::tolower(character); });
	char* system32path = new char[MAX_PATH];
	if (GetSystemDirectoryA(system32path, MAX_PATH) == NULL)
	{
		delete[] system32path;
		nlog("无法获取system32路径");
		return;
	}
	HMODULE originaldll = LoadLibraryA((std::string(system32path) + "\\" + filename).c_str());
	delete[] system32path;
	if (originaldll == NULL)
	{
		nlog("无法加载原始DLL?");
		return;
	}
	if (strstr(filename.c_str(), "version") != NULL)
		LoadExports_version(originaldll);
	else if (strstr(filename.c_str(), "winmm") != NULL)
		LoadExports_winmm(originaldll);
	else if (strstr(filename.c_str(), "winhttp") != NULL)
		LoadExports_winhttp(originaldll);
	else
	{
		MessageBoxA(NULL, "dll文件名只能是 version.dll/winmm.dll/winhttp.dll", "错误", MB_ICONERROR | MB_OK);
		return;
	}
	//xs::pe::OpenConsole();
	WorkWork();	

}
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	if (fdwReason != DLL_PROCESS_ATTACH)
		return TRUE;
	Main(hinstDLL);
	return TRUE;
}