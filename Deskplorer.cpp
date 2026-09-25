// build command:
// cl /EHsc /O2 /W3 /DUNICODE /std:c++17 Deskplorer.cpp /link user32.lib shell32.lib gdi32.lib ole32.lib oleaut32.lib dwmapi.lib comctl32.lib shlwapi.lib /SUBSYSTEM:WINDOWS

#ifndef NOMINMAX
#define NOMINMAX
#endif

#define NTDDI_VERSION NTDDI_WIN10
#define _WIN32_WINNT _WIN32_WINNT_WIN10

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shobjidl_core.h>
#include <shlwapi.h>
#include <dwmapi.h>
#include <commctrl.h>
#include <vector>
#include <string>
#include <commoncontrols.h>
#include <map>
#include <KnownFolders.h>

#ifndef FWF_NOSB
#define FWF_NOSB 0x00000010
#endif

#ifndef FWF_NOHEADER
#define FWF_NOHEADER 0x00000800
#endif

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")

// ---------- Forward Declarations ----------
class CDummyShellBrowser;
// void NavigateCurrentTab(HWND hWndParent, const std::wstring& newPath);
void NavigateCurrentTab(HWND hWndParent, const std::wstring& newPath, bool recordHistory = true);

struct TabItem {
    std::wstring path;
    std::wstring title;
    IShellFolder* pFolder = nullptr;
    IShellView* pShellView = nullptr;
    HWND hViewWnd = NULL;
    CDummyShellBrowser* pBrowser = nullptr;
    std::vector<std::wstring> backHistory;
};

// ---------- Constants & Globals ----------
const int TAB_BAR_WIDTH = 28;     
const int ADD_BTN_HEIGHT = 28;    
const int CLOSE_BTN_SIZE = 16; 
int dockWidth = 160;              
APPBARDATA abd = { 0 };

#define TIMER_CLEANUP_SHELL_VIEW 1001
#define WM_APPBAR_CALLBACK (WM_USER + 100)

std::vector<TabItem> g_tabs;
int g_activeTabIndex = -1;
HWND g_hMainWnd = NULL;

// ---------- Helper Functions ----------
HBITMAP IconToBitmap(HICON hIcon, int size = 16)
{
    if (!hIcon) return NULL;

    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);

    BITMAPINFO bmi = { 0 };
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = size;
    bmi.bmiHeader.biHeight = -size; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    VOID* pBits = NULL;
    HBITMAP hBitmap = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);

    if (hBitmap)
    {
        HBITMAP hOldBmp = (HBITMAP)SelectObject(hdcMem, hBitmap);
        DrawIconEx(hdcMem, 0, 0, hIcon, size, size, 0, NULL, DI_NORMAL);
        SelectObject(hdcMem, hOldBmp);
    }

    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);
    return hBitmap;
}

HBITMAP GetPathBitmapIcon(const std::wstring& path)
{
    SHFILEINFOW sfi = { 0 };
    DWORD_PTR hr = SHGetFileInfoW(
        path.c_str(),
        0,
        &sfi,
        sizeof(sfi),
        SHGFI_ICON | SHGFI_SMALLICON
    );

    if (hr && sfi.hIcon)
    {
        HBITMAP hBmp = IconToBitmap(sfi.hIcon, 16);
        DestroyIcon(sfi.hIcon); 
        return hBmp;
    }
    return NULL;
}

void BuildFolderMenu(HMENU hParentMenu, const std::wstring& currentPath, int currentDepth, int maxDepth, std::map<int, std::wstring>& menuIdToPath, std::vector<HBITMAP>& bitmappedIcons, int& currentMenuId)
{
    if (currentDepth > maxDepth) return;

    std::wstring searchPath = currentPath + L"*";
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &fd);

    if (hFind == INVALID_HANDLE_VALUE) return;

    int folderCount = 0;
    do
    {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            if (wcscmp(fd.cFileName, L".") != 0 && wcscmp(fd.cFileName, L"..") != 0)
            {
                if (!(fd.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)))
                {
                    std::wstring folderPath = currentPath + fd.cFileName + L"\\";
                    
                    HMENU hSubMenu = CreatePopupMenu();

                    
                    menuIdToPath[currentMenuId] = folderPath;
                    MENUITEMINFOW miiOpen = { sizeof(MENUITEMINFOW) };
                    miiOpen.fMask = MIIM_STRING | MIIM_ID;
                    miiOpen.wID = currentMenuId++;
                    miiOpen.dwTypeData = (LPWSTR)L" Open Folder";
                    
                    
                    InsertMenuItemW(hSubMenu, 0, TRUE, &miiOpen);

                    
                    if (currentDepth < maxDepth)
                    {
                        AppendMenuW(hSubMenu, MF_SEPARATOR, 0, NULL);
                        BuildFolderMenu(hSubMenu, folderPath, currentDepth + 1, maxDepth, menuIdToPath, bitmappedIcons, currentMenuId);
                    }

                    
                    MENUITEMINFOW miiFolder = { sizeof(MENUITEMINFOW) };
                    miiFolder.fMask = MIIM_STRING | MIIM_SUBMENU;
                    miiFolder.hSubMenu = hSubMenu;
                    miiFolder.dwTypeData = fd.cFileName;

                    HBITMAP hFolderBmp = GetPathBitmapIcon(folderPath);
                                                        if (hFolderBmp) {
                                                            miiFolder.fMask |= MIIM_BITMAP;
                                                            miiFolder.hbmpItem = hFolderBmp;
                                                            bitmappedIcons.push_back(hFolderBmp);
                                                        }

                    InsertMenuItemW(hParentMenu, GetMenuItemCount(hParentMenu), TRUE, &miiFolder);

                    folderCount++;
                    if (folderCount >= 15)
                        break;
                }
            }
        }
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);
}

std::wstring GetParentFolderPath(const std::wstring& path)
{
    size_t lastSlash = path.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        if (lastSlash == 2 && path[1] == L':') {
            return path.substr(0, 3);
        }
        if (lastSlash > 0) {
            return path.substr(0, lastSlash);
        }
    }
    return L"";
}

std::wstring GetSelectedItemPath(HWND hList)
{
    if (g_activeTabIndex < 0 || g_activeTabIndex >= (int)g_tabs.size()) return L"";
    IShellFolder* pFolder = g_tabs[g_activeTabIndex].pFolder;
    if (!pFolder) return L"";

    int iSelected = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
    if (iSelected == -1) return L"";

    WCHAR szItemName[MAX_PATH] = { 0 };
    ListView_GetItemText(hList, iSelected, 0, szItemName, MAX_PATH);

    if (wcslen(szItemName) == 0) return L"";

    std::wstring currentPath = g_tabs[g_activeTabIndex].path;
    if (!currentPath.empty() && currentPath.back() != L'\\') {
        currentPath += L"\\";
    }
    std::wstring fullPath = currentPath + szItemName;

    DWORD dwAttr = GetFileAttributesW(fullPath.c_str());
    if (dwAttr != INVALID_FILE_ATTRIBUTES && (dwAttr & FILE_ATTRIBUTE_DIRECTORY)) {
        return fullPath;
    }

    return L"";
}


void GoBackActiveTab()
{
    if (g_activeTabIndex < 0 || g_activeTabIndex >= (int)g_tabs.size()) return;

    auto& hist = g_tabs[g_activeTabIndex].backHistory;
    while (!hist.empty())
    {
        std::wstring prev = hist.back();
        hist.pop_back();

        DWORD attr = GetFileAttributesW(prev.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
        {
            NavigateCurrentTab(g_hMainWnd, prev, false);   
            return;
        }
    }
}

IContextMenu2* g_pCtxMenu2 = nullptr;
IContextMenu3* g_pCtxMenu3 = nullptr;

void ShowBackgroundContextMenu(POINT pt)
{
    if (g_activeTabIndex < 0 || g_activeTabIndex >= (int)g_tabs.size()) return;
    IShellView* pView = g_tabs[g_activeTabIndex].pShellView;
    if (!pView) return;

    IContextMenu* pCM = nullptr;
    if (FAILED(pView->GetItemObject(SVGIO_BACKGROUND, IID_PPV_ARGS(&pCM))) || !pCM) return;

    const UINT ID_BACK = 1, ID_FIRST = 2, ID_LAST = 0x7FFF;

     bool canGoBack = !g_tabs[g_activeTabIndex].backHistory.empty();
    bool doBack = false;

    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING | (canGoBack ? MF_ENABLED : MF_GRAYED), ID_BACK, L"Back");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    if (SUCCEEDED(pCM->QueryContextMenu(hMenu, 2, ID_FIRST, ID_LAST, CMF_NORMAL)))
    {
        pCM->QueryInterface(IID_PPV_ARGS(&g_pCtxMenu3));
        if (!g_pCtxMenu3) pCM->QueryInterface(IID_PPV_ARGS(&g_pCtxMenu2));

        UINT cmd = (UINT)TrackPopupMenuEx(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, g_hMainWnd, NULL);

        if (cmd == ID_BACK)
        {
            doBack = true;
        }
        else if (cmd >= ID_FIRST)
        {
            CMINVOKECOMMANDINFOEX ici = { sizeof(ici) };
            ici.fMask = CMIC_MASK_UNICODE | CMIC_MASK_PTINVOKE;
            ici.hwnd = g_hMainWnd;
            ici.lpVerb = MAKEINTRESOURCEA(cmd - ID_FIRST);
            ici.lpVerbW = MAKEINTRESOURCEW(cmd - ID_FIRST);
            ici.nShow = SW_SHOWNORMAL;
            ici.ptInvoke = pt;
            pCM->InvokeCommand((CMINVOKECOMMANDINFO*)&ici);
        }
    }

    DestroyMenu(hMenu);
    if (g_pCtxMenu3) { g_pCtxMenu3->Release(); g_pCtxMenu3 = nullptr; }
    if (g_pCtxMenu2) { g_pCtxMenu2->Release(); g_pCtxMenu2 = nullptr; }
    pCM->Release();

    if (doBack) GoBackActiveTab();
}


LRESULT CALLBACK ListViewSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{

    if (uMsg == WM_GESTURE || uMsg == WM_VSCROLL || uMsg == WM_MOUSEWHEEL || uMsg == WM_MOUSEHWHEEL)
    OutputDebugStringW((L"LV msg=" + std::to_wstring(uMsg) + L"\n").c_str());
    if (uMsg == WM_SIZE || uMsg == WM_NCCALCSIZE || uMsg == WM_NCPAINT)
    {
        ShowScrollBar(hWnd, SB_BOTH, FALSE);
    }

    if (uMsg == WM_MOUSEWHEEL)
    {
        static int s_wheelRemainder = 0;
        int zDelta = (short)HIWORD(wParam);

        s_wheelRemainder += -zDelta * 60;              
        int pixels = s_wheelRemainder / 120;
        s_wheelRemainder -= pixels * 120;

        if (pixels) SendMessageW(hWnd, LVM_SCROLL, 0, pixels);
        return 0;
    }

    if (uMsg == WM_GESTURE)
    {
        static int s_lastPanY = 0;
        GESTUREINFO gi = { sizeof(gi) };
        if (GetGestureInfo((HGESTUREINFO)lParam, &gi) && gi.dwID == GID_PAN)
        {
            if (!(gi.dwFlags & GF_BEGIN))
                SendMessageW(hWnd, LVM_SCROLL, 0, s_lastPanY - gi.ptsLocation.y);
            s_lastPanY = gi.ptsLocation.y;

            CloseGestureInfoHandle((HGESTUREINFO)lParam);
            return 0;
        }
    }

    
    if (uMsg == WM_VSCROLL)
    {
        RECT rcList; GetClientRect(hWnd, &rcList);
        switch (LOWORD(wParam))
        {
        case SB_LINEUP:   SendMessageW(hWnd, LVM_SCROLL, 0, -40); return 0;
        case SB_LINEDOWN: SendMessageW(hWnd, LVM_SCROLL, 0,  40); return 0;
        case SB_PAGEUP:   SendMessageW(hWnd, LVM_SCROLL, 0, -rcList.bottom); return 0;
        case SB_PAGEDOWN: SendMessageW(hWnd, LVM_SCROLL, 0,  rcList.bottom); return 0;
        }
    }


        
    if (uMsg == WM_CONTEXTMENU && lParam != -1)
    {
        LVHITTESTINFO ht = { 0 };
        ht.pt.x = (short)LOWORD(lParam);
        ht.pt.y = (short)HIWORD(lParam);
        ScreenToClient(hWnd, &ht.pt);

        if (ListView_HitTest(hWnd, &ht) == -1)
        {
            POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
            ShowBackgroundContextMenu(pt);
            return 0;
        }
    }

    if (uMsg == WM_LBUTTONDBLCLK)
    {
        LVHITTESTINFO htInfo = { 0 };
        htInfo.pt.x = LOWORD(lParam);
        htInfo.pt.y = HIWORD(lParam);

        int index = ListView_HitTest(hWnd, &htInfo);
        
        if (index == -1 || (htInfo.flags & LVHT_NOWHERE))
        {
            if (g_activeTabIndex >= 0 && g_activeTabIndex < (int)g_tabs.size())
            {
                std::wstring parentPath = GetParentFolderPath(g_tabs[g_activeTabIndex].path);
                if (!parentPath.empty() && parentPath != g_tabs[g_activeTabIndex].path)
                {
                    NavigateCurrentTab(g_hMainWnd, parentPath);
                    return 0;
                }
            }
        }
        else
        {
            std::wstring folderPath = GetSelectedItemPath(hWnd);
            if (!folderPath.empty())
            {
                NavigateCurrentTab(g_hMainWnd, folderPath);
                return 0;
            }
        }
    }
    else if (uMsg == WM_KEYDOWN && wParam == VK_RETURN)
    {
        std::wstring folderPath = GetSelectedItemPath(hWnd);
        if (!folderPath.empty())
        {
            NavigateCurrentTab(g_hMainWnd, folderPath);
            return 0;
        }
    }

    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

// ---------- IShellBrowser Implementation ----------
class CDummyShellBrowser : public IShellBrowser
{
private:
    LONG m_cRef;
    HWND m_hwnd;

public:
    CDummyShellBrowser(HWND hwnd) : m_cRef(1), m_hwnd(hwnd) {}

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == IID_IOleWindow || riid == IID_IShellBrowser) {
            *ppv = static_cast<IShellBrowser*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = NULL;
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&m_cRef); }
    STDMETHODIMP_(ULONG) Release() override {
        ULONG res = InterlockedDecrement(&m_cRef);
        if (res == 0) delete this;
        return res;
    }

    STDMETHODIMP GetWindow(HWND* phwnd) override { *phwnd = m_hwnd; return S_OK; }
    STDMETHODIMP ContextSensitiveHelp(BOOL) override { return S_OK; }

    STDMETHODIMP InsertMenusSB(HMENU, LPOLEMENUGROUPWIDTHS) override { return S_OK; }
    STDMETHODIMP SetMenuSB(HMENU, HOLEMENU, HWND) override { return S_OK; }
    STDMETHODIMP RemoveMenusSB(HMENU) override { return S_OK; }
    STDMETHODIMP SetStatusTextSB(LPCWSTR) override { return S_OK; }
    STDMETHODIMP EnableModelessSB(BOOL) override { return S_OK; }
    
    STDMETHODIMP TranslateAcceleratorSB(MSG* pmsg, WORD) override {
        if (g_activeTabIndex >= 0 && g_activeTabIndex < (int)g_tabs.size()) {
            if (g_tabs[g_activeTabIndex].pShellView) {
                return g_tabs[g_activeTabIndex].pShellView->TranslateAccelerator(pmsg);
            }
        }
        return S_FALSE;
    }

    STDMETHODIMP BrowseObject(PCUIDLIST_RELATIVE pidl, UINT wFlags) override {
        if (g_activeTabIndex < 0 || g_activeTabIndex >= (int)g_tabs.size()) return E_FAIL;

        if (wFlags & SBSP_RELATIVE) {
            IShellFolder* pFolder = g_tabs[g_activeTabIndex].pFolder;
            if (pFolder) {
                STRRET str;
                if (SUCCEEDED(pFolder->GetDisplayNameOf(pidl, SHGDN_FORPARSING, &str))) {
                    WCHAR szPath[MAX_PATH];
                    StrRetToBufW(&str, pidl, szPath, MAX_PATH);
                    NavigateCurrentTab(m_hwnd, szPath);
                    return S_OK;
                }
            }
        } else {
            WCHAR szPath[MAX_PATH];
            if (SHGetPathFromIDListW((LPCITEMIDLIST)pidl, szPath)) {
                NavigateCurrentTab(m_hwnd, szPath);
                return S_OK;
            }
        }
        return S_OK;
    }

    STDMETHODIMP GetViewStateStream(DWORD, IStream**) override { return E_NOTIMPL; }
    STDMETHODIMP GetControlWindow(UINT, HWND* phwnd) override {
        if (phwnd) *phwnd = NULL;
        return S_FALSE;
    }
    STDMETHODIMP SendControlMsg(UINT, UINT, WPARAM, LPARAM, LRESULT* plr) override { if (plr) *plr = 0; return S_OK; }
    STDMETHODIMP QueryActiveShellView(IShellView** ppshv) override { 
        if (g_activeTabIndex >= 0 && g_activeTabIndex < (int)g_tabs.size()) {
            *ppshv = g_tabs[g_activeTabIndex].pShellView;
            if (*ppshv) {
                (*ppshv)->AddRef();
                return S_OK;
            }
        }
        *ppshv = NULL; 
        return E_FAIL; 
    }
    STDMETHODIMP OnViewWindowActive(IShellView*) override { return S_OK; }
    STDMETHODIMP SetToolbarItems(LPTBBUTTONSB, UINT, UINT) override { return S_OK; }
};

// ---------- Acrylic Blur API ----------
typedef enum _ACCENT_STATE {
    ACCENT_ENABLE_ACRYLICBLURBEHIND = 4
} ACCENT_STATE;

typedef struct _ACCENT_POLICY {
    ACCENT_STATE AccentState;
    DWORD AccentFlags;
    DWORD GradientColor;
    DWORD AnimationId;
} ACCENT_POLICY;

typedef enum _WINDOWCOMPOSITIONATTRIB {
    WCA_ACCENT_POLICY = 19
} WINDOWCOMPOSITIONATTRIB;

typedef struct _WINDOWCOMPOSITIONATTRIBDATA {
    WINDOWCOMPOSITIONATTRIB Attribute;
    PVOID pData;
    SIZE_T SizeOfData;
} WINDOWCOMPOSITIONATTRIBDATA;

typedef BOOL(WINAPI* pfnSetWindowCompositionAttribute)(HWND, WINDOWCOMPOSITIONATTRIBDATA*);

void EnableAcrylicBlur(HWND hwnd)
{
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    if (hUser32)
    {
        auto SetWindowCompositionAttribute = (pfnSetWindowCompositionAttribute)GetProcAddress(hUser32, "SetWindowCompositionAttribute");
        if (SetWindowCompositionAttribute)
        {
            ACCENT_POLICY accent = { ACCENT_ENABLE_ACRYLICBLURBEHIND, 0, 0x40121212, 0 };
            WINDOWCOMPOSITIONATTRIBDATA data;
            data.Attribute = WCA_ACCENT_POLICY;
            data.pData = &accent;
            data.SizeOfData = sizeof(accent);
            SetWindowCompositionAttribute(hwnd, &data);
        }
    }

    DWM_BLURBEHIND bb = { 0 };
    bb.dwFlags = DWM_BB_ENABLE;
    bb.fEnable = TRUE;
    DwmEnableBlurBehindWindow(hwnd, &bb);

    MARGINS margins = { -1, -1, -1, -1 };
    DwmExtendFrameIntoClientArea(hwnd, &margins);
}

void CleanUpListView(HWND hViewWnd, IShellView* pShellView)
{
    if (!hViewWnd) return;

    HWND hDefView = FindWindowExW(hViewWnd, NULL, L"SHELLDLL_DefView", NULL);
    if (!hDefView) hDefView = hViewWnd;

    HWND hList = FindWindowExW(hDefView, NULL, L"SysListView32", NULL);
    if (!hList) return;

    SetWindowSubclass(hList, ListViewSubclassProc, 1, 0);

    HWND hRebar = FindWindowExW(hViewWnd, NULL, L"ReBarWindow32", NULL);
    if (hRebar) ShowWindow(hRebar, SW_HIDE);

    
    HWND hHeader = ListView_GetHeader(hList);
    if (!hHeader) hHeader = FindWindowExW(hDefView, NULL, L"SysHeader32", NULL);
    if (hHeader) 
    {
        
        LONG_PTR hdrStyle = GetWindowLongPtrW(hHeader, GWL_STYLE);
        SetWindowLongPtrW(hHeader, GWL_STYLE, hdrStyle | HDS_HIDDEN);
        ShowWindow(hHeader, SW_HIDE);
    }

    ListView_EnableGroupView(hList, FALSE);

    
    LONG_PTR style = GetWindowLongPtrW(hList, GWL_STYLE);
    style &= ~(LVS_REPORT | LVS_LIST | LVS_SMALLICON | LVS_ALIGNLEFT); 
    style |= (LVS_ICON | LVS_AUTOARRANGE | LVS_ALIGNTOP | LVS_NOCOLUMNHEADER);
    SetWindowLongPtrW(hList, GWL_STYLE, style);

    
    ShowScrollBar(hList, SB_BOTH, FALSE);

    ListView_SetBkColor(hList, CLR_NONE);
    ListView_SetTextBkColor(hList, CLR_NONE);
    ListView_SetTextColor(hList, RGB(255, 255, 255));

    IImageList* pImgList = NULL;
    if (SUCCEEDED(SHGetImageList(SHIL_JUMBO, IID_IImageList, (void**)&pImgList)))
    {
        HIMAGELIST hImageList = reinterpret_cast<HIMAGELIST>(pImgList);
        ListView_SetImageList(hList, hImageList, LVSIL_NORMAL);
        pImgList->Release();
        ListView_SetIconSpacing(hList, 110, 125);
    }

    if (pShellView)
    {
        IFolderView2* pFolderView2 = NULL;
        if (SUCCEEDED(pShellView->QueryInterface(IID_PPV_ARGS(&pFolderView2))))
        {
            
            pFolderView2->SetViewModeAndIconSize(FVM_ICON, 96);
            pFolderView2->Release();
        }
    }

    
    SetWindowLongPtrW(hList, GWL_STYLE, GetWindowLongPtrW(hList, GWL_STYLE));
    ListView_Arrange(hList, LVA_ALIGNTOP);
    
    
    SetWindowPos(hList, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    InvalidateRect(hList, NULL, TRUE);
    UpdateWindow(hList);

    PostMessageW(hDefView, WM_COMMAND, 28713, 0); // Large Icons view command
}

IShellFolder* GetShellFolderForPath(const std::wstring& path)
{
    PIDLIST_ABSOLUTE pidlFull = NULL;
    HRESULT hr = SHParseDisplayName(path.c_str(), NULL, &pidlFull, 0, NULL);
    if (FAILED(hr) || !pidlFull) return nullptr;

    IShellFolder* pDesktop = nullptr;
    hr = SHGetDesktopFolder(&pDesktop);
    if (FAILED(hr) || !pDesktop) {
        CoTaskMemFree(pidlFull);
        return nullptr;
    }

    IShellFolder* pTargetFolder = nullptr;
    if (pidlFull->mkid.cb == 0) {
        pTargetFolder = pDesktop;
    } else {
        hr = pDesktop->BindToObject(pidlFull, NULL, IID_IShellFolder, (void**)&pTargetFolder);
        pDesktop->Release();
    }
    
    CoTaskMemFree(pidlFull);
    return pTargetFolder;
}

void NavigateCurrentTab(HWND hWndParent, const std::wstring& newPath, bool recordHistory)
{
    if (g_activeTabIndex < 0 || g_activeTabIndex >= (int)g_tabs.size()) return;

    TabItem& curTab = g_tabs[g_activeTabIndex];

    if (recordHistory && !curTab.path.empty() && curTab.path != newPath)
        curTab.backHistory.push_back(curTab.path); 

    if (curTab.hViewWnd && IsWindow(curTab.hViewWnd)) {
        HWND hDefView = FindWindowExW(curTab.hViewWnd, NULL, L"SHELLDLL_DefView", NULL);
        if (!hDefView) hDefView = curTab.hViewWnd;
        HWND hList = FindWindowExW(hDefView, NULL, L"SysListView32", NULL);
        if (hList) RemoveWindowSubclass(hList, ListViewSubclassProc, 1);

        DragAcceptFiles(curTab.hViewWnd, FALSE);
        ShowWindow(curTab.hViewWnd, SW_HIDE);
    }

    if (curTab.pShellView) {
        curTab.pShellView->UIActivate(SVUIA_DEACTIVATE);
        curTab.pShellView->DestroyViewWindow();
        curTab.pShellView->Release();
        curTab.pShellView = nullptr;
    }
    if (curTab.pFolder) {
        curTab.pFolder->Release();
        curTab.pFolder = nullptr;
    }

    curTab.path = newPath;
    size_t lastSlash = newPath.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos && lastSlash < newPath.length() - 1) {
        curTab.title = newPath.substr(lastSlash + 1);
    } else {
        curTab.title = newPath;
    }

    curTab.pFolder = GetShellFolderForPath(newPath);
    if (curTab.pFolder)
    {
        if (SUCCEEDED(curTab.pFolder->CreateViewObject(hWndParent, IID_PPV_ARGS(&curTab.pShellView))))
        {
            RECT rc;
            GetClientRect(hWndParent, &rc);
            rc.left += TAB_BAR_WIDTH;

            FOLDERSETTINGS folderSettings;
            folderSettings.ViewMode = FVM_ICON;
            folderSettings.fFlags = FWF_AUTOARRANGE | FWF_NOCLIENTEDGE | FWF_NOHEADER | FWF_NOSB;

            if (SUCCEEDED(curTab.pShellView->CreateViewWindow(NULL, &folderSettings, curTab.pBrowser, &rc, &curTab.hViewWnd)))
            {
                curTab.pShellView->UIActivate(SVUIA_ACTIVATE_FOCUS);
                DragAcceptFiles(curTab.hViewWnd, TRUE);
                ShowWindow(curTab.hViewWnd, SW_SHOW);
            }
        }
    }

    SetTimer(hWndParent, TIMER_CLEANUP_SHELL_VIEW + g_activeTabIndex, 100, NULL);
    InvalidateRect(hWndParent, NULL, TRUE);
}

void CreateNewTab(HWND hWndParent, const std::wstring& path)
{
    TabItem tab;
    // tab.path = path;

    // size_t lastSlash = path.find_last_of(L"\\/");
    // if (lastSlash != std::wstring::npos && lastSlash < path.length() - 1) {
    //     tab.title = path.substr(lastSlash + 1);
    // } else {
    //     tab.title = path;
    // }
    std::wstring cleanPath = path;
    if (cleanPath.length() > 3 && cleanPath.back() == L'\\') {
        cleanPath.pop_back();
    }
    
    SHFILEINFOW sfi = { 0 };
    if (SHGetFileInfoW(cleanPath.c_str(), 0, &sfi, sizeof(sfi), SHGFI_DISPLAYNAME)) {
         tab.title = sfi.szDisplayName;
    } else {
        tab.title = cleanPath;
    }

    
    tab.path = path; 
    

    tab.pFolder = GetShellFolderForPath(path);

    // tab.pFolder = GetShellFolderForPath(path);

    if (tab.pFolder)
    {
        if (SUCCEEDED(tab.pFolder->CreateViewObject(hWndParent, IID_PPV_ARGS(&tab.pShellView))))
        {
            RECT rc;
            GetClientRect(hWndParent, &rc);
            rc.left += TAB_BAR_WIDTH;

            FOLDERSETTINGS folderSettings;
            folderSettings.ViewMode = FVM_ICON;
            folderSettings.fFlags = FWF_AUTOARRANGE | FWF_NOCLIENTEDGE | FWF_NOHEADER | FWF_NOSB;

            tab.pBrowser = new CDummyShellBrowser(hWndParent);

            if (SUCCEEDED(tab.pShellView->CreateViewWindow(NULL, &folderSettings, tab.pBrowser, &rc, &tab.hViewWnd)))
            {
                tab.pShellView->UIActivate(SVUIA_ACTIVATE_FOCUS);
                DragAcceptFiles(tab.hViewWnd, TRUE);
            }
        }
    }

    g_tabs.push_back(tab);
    int newIndex = (int)g_tabs.size() - 1;

    SetTimer(hWndParent, TIMER_CLEANUP_SHELL_VIEW + newIndex, 100, NULL);

    for (int i = 0; i < (int)g_tabs.size(); ++i) {
        if (g_tabs[i].hViewWnd) {
            ShowWindow(g_tabs[i].hViewWnd, (i == newIndex) ? SW_SHOW : SW_HIDE);
        }
    }

    g_activeTabIndex = newIndex;
    InvalidateRect(hWndParent, NULL, TRUE);
}

void CloseTab(HWND hwnd, int index)
{
    if (index < 0 || index >= (int)g_tabs.size()) return;

    TabItem tab = g_tabs[index];

    if (tab.hViewWnd && IsWindow(tab.hViewWnd)) {
        HWND hDefView = FindWindowExW(tab.hViewWnd, NULL, L"SHELLDLL_DefView", NULL);
        if (!hDefView) hDefView = tab.hViewWnd;
        HWND hList = FindWindowExW(hDefView, NULL, L"SysListView32", NULL);
        if (hList) RemoveWindowSubclass(hList, ListViewSubclassProc, 1);

        DragAcceptFiles(tab.hViewWnd, FALSE);
        ShowWindow(tab.hViewWnd, SW_HIDE);
    }
    if (tab.pShellView) {
        tab.pShellView->UIActivate(SVUIA_DEACTIVATE);
        tab.pShellView->DestroyViewWindow();
        tab.pShellView->Release();
    }
    if (tab.hViewWnd && IsWindow(tab.hViewWnd)) {
        DestroyWindow(tab.hViewWnd);
    }
    if (tab.pFolder) {
        tab.pFolder->Release();
    }
    if (tab.pBrowser) {
        tab.pBrowser->Release();
    }

    g_tabs.erase(g_tabs.begin() + index);

    if (g_tabs.empty()) {
        g_activeTabIndex = -1;
    } else {
        if (g_activeTabIndex >= (int)g_tabs.size()) {
            g_activeTabIndex = (int)g_tabs.size() - 1;
        }
        for (int i = 0; i < (int)g_tabs.size(); ++i) {
            if (g_tabs[i].hViewWnd) {
                ShowWindow(g_tabs[i].hViewWnd, (i == g_activeTabIndex) ? SW_SHOW : SW_HIDE);
            }
        }
    }

    RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW | RDW_ALLCHILDREN);
}

void HandleDroppedFiles(HDROP hDrop)
{
    if (g_activeTabIndex < 0 || g_activeTabIndex >= (int)g_tabs.size()) {
        DragFinish(hDrop);
        return;
    }

    std::wstring destFolder = g_tabs[g_activeTabIndex].path;
    UINT fileCount = DragQueryFileW(hDrop, 0xFFFFFFFF, NULL, 0);

    std::vector<wchar_t> fromBuffer;

    for (UINT i = 0; i < fileCount; ++i) {
        WCHAR szPath[MAX_PATH];
        if (DragQueryFileW(hDrop, i, szPath, MAX_PATH)) {
            size_t len = wcslen(szPath);
            fromBuffer.insert(fromBuffer.end(), szPath, szPath + len + 1);
        }
    }
    fromBuffer.push_back(L'\0'); 

    std::vector<wchar_t> toBuffer(destFolder.begin(), destFolder.end());
    toBuffer.push_back(L'\0');
    toBuffer.push_back(L'\0');

    SHFILEOPSTRUCTW fileOp = { 0 };
    fileOp.hwnd = g_hMainWnd;
    fileOp.wFunc = FO_COPY;
    fileOp.pFrom = fromBuffer.data();
    fileOp.pTo = toBuffer.data();
    fileOp.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION;

    SHFileOperationW(&fileOp);
    DragFinish(hDrop);

    if (g_tabs[g_activeTabIndex].hViewWnd) {
        HWND hDefView = FindWindowExW(g_tabs[g_activeTabIndex].hViewWnd, NULL, L"SHELLDLL_DefView", NULL);
        if (hDefView) {
            PostMessageW(hDefView, WM_COMMAND, 28713, 0);
        }
    }
}

std::wstring GetDesktopPath()
{
    PWSTR path = NULL;
    std::wstring result = L"C:\\";
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Desktop, 0, NULL, &path))) {
        result = path;
        result += L"\\";
        CoTaskMemFree(path);
    }
    return result;
}

HICON GetFolderIcon(const std::wstring& path)
{
    SHFILEINFOW sfi = { 0 };
    DWORD_PTR hr = SHGetFileInfoW(
        path.c_str(),
        FILE_ATTRIBUTE_DIRECTORY,
        &sfi,
        sizeof(sfi),
        SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES
    );

    if (hr && sfi.hIcon)
        return sfi.hIcon;

    return NULL;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_DROPFILES:
    {
        HDROP hDrop = (HDROP)wParam;
        HandleDroppedFiles(hDrop);
        return 0;
    }
    case WM_TIMER:
    {
        if (wParam >= TIMER_CLEANUP_SHELL_VIEW)
        {
            int index = (int)(wParam - TIMER_CLEANUP_SHELL_VIEW);
            KillTimer(hwnd, (UINT_PTR)wParam);

            if (index >= 0 && index < (int)g_tabs.size())
            {
                TabItem& tab = g_tabs[index];
                if (tab.hViewWnd && IsWindow(tab.hViewWnd) && tab.pShellView)
                {
                    CleanUpListView(tab.hViewWnd, tab.pShellView);
                }
            }
            return 0;
        }
        break;
    }
    case WM_CREATE:
    {
        OleInitialize(NULL);
        g_hMainWnd = hwnd;

        DragAcceptFiles(hwnd, TRUE);

        RECT origWorkArea;
        SystemParametersInfo(SPI_GETWORKAREA, 0, &origWorkArea, 0);

        abd.cbSize = sizeof(APPBARDATA);
        abd.hWnd = hwnd;
        abd.uCallbackMessage = WM_APPBAR_CALLBACK;

        SHAppBarMessage(ABM_NEW, &abd);

        RECT rc;
        rc.left = origWorkArea.left;
        rc.top = origWorkArea.top;
        rc.right = origWorkArea.left + dockWidth;
        rc.bottom = origWorkArea.bottom;

        abd.rc = rc;
        abd.uEdge = ABE_LEFT;

        SHAppBarMessage(ABM_QUERYPOS, &abd);
        SHAppBarMessage(ABM_SETPOS, &abd);

        RECT wa = origWorkArea;
        wa.left += dockWidth;
        SystemParametersInfo(SPI_SETWORKAREA, 0, &wa, SPIF_SENDCHANGE);

        SetWindowPos(hwnd, HWND_TOPMOST, rc.left, rc.top, dockWidth, rc.bottom - rc.top, SWP_SHOWWINDOW);

        EnableAcrylicBlur(hwnd);

        CreateNewTab(hwnd, GetDesktopPath());

        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT clientRc;
        GetClientRect(hwnd, &clientRc);

        
        
        
        
        FillRect(hdc, &clientRc, (HBRUSH)GetStockObject(BLACK_BRUSH));

        RECT rectAddBtn = { 0, 0, TAB_BAR_WIDTH, ADD_BTN_HEIGHT };
        HBRUSH hAddBtnBrush = CreateSolidBrush(RGB(40, 40, 45));
        FillRect(hdc, &rectAddBtn, hAddBtnBrush);
        DeleteObject(hAddBtnBrush);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));

        HFONT hAddFont = CreateFontW(-18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Arial");
        HFONT hOldFont = (HFONT)SelectObject(hdc, hAddFont);
        DrawTextW(hdc, L"+", 1, &rectAddBtn, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, hOldFont);
        DeleteObject(hAddFont);

        LOGFONTW lf = { 0 };
        lf.lfHeight = -12;
        lf.lfWeight = FW_SEMIBOLD;
        lf.lfEscapement = 900; 
        lf.lfOrientation = 900;
        wcscpy_s(lf.lfFaceName, L"Segoe UI");
        HFONT hFont = CreateFontIndirectW(&lf);

        HFONT hCloseFont = CreateFontW(-12, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Segoe UI");

        int availableHeight = clientRc.bottom - ADD_BTN_HEIGHT;
        int totalTabs = (int)g_tabs.size();
        
        int minTabHeight = 50;
        int maxTabHeight = 130;
        int tabHeight = maxTabHeight;

        if (totalTabs > 0) {
            tabHeight = availableHeight / totalTabs;
            if (tabHeight > maxTabHeight) tabHeight = maxTabHeight;
            if (tabHeight < minTabHeight) tabHeight = minTabHeight;
        }

        for (int i = 0; i < totalTabs; ++i)
        {
            int topPos = ADD_BTN_HEIGHT + (i * tabHeight);
            RECT rTab = { 0, topPos, TAB_BAR_WIDTH, topPos + tabHeight };

            if (i == g_activeTabIndex) {
                HBRUSH hActive = CreateSolidBrush(RGB(55, 55, 62));
                FillRect(hdc, &rTab, hActive);
                DeleteObject(hActive);

                RECT rIndicator = { TAB_BAR_WIDTH - 3, topPos, TAB_BAR_WIDTH, topPos + tabHeight };
                HBRUSH hInd = CreateSolidBrush(RGB(0, 120, 215));
                FillRect(hdc, &rIndicator, hInd);
                DeleteObject(hInd);
            }

            if (tabHeight >= 50) {
                RECT rCloseBtn = { (TAB_BAR_WIDTH - CLOSE_BTN_SIZE) / 2, topPos + 4, (TAB_BAR_WIDTH + CLOSE_BTN_SIZE) / 2, topPos + 4 + CLOSE_BTN_SIZE };
                hOldFont = (HFONT)SelectObject(hdc, hCloseFont);
                SetTextColor(hdc, (i == g_activeTabIndex) ? RGB(240, 80, 80) : RGB(140, 140, 140));
                DrawTextW(hdc, L"✕", 1, &rCloseBtn, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }

            HICON hFolderIcon = GetFolderIcon(g_tabs[i].path);
            if (!hFolderIcon) {
                hFolderIcon = GetFolderIcon(L"C:\\Windows");
            }

            int iconSize = 16;
            int iconX = (TAB_BAR_WIDTH - iconSize) / 2;
            int iconY = topPos + tabHeight - 4 - iconSize;

            if (hFolderIcon) {
                DrawIconEx(hdc, iconX, iconY, hFolderIcon, iconSize, iconSize, 0, NULL, DI_NORMAL);
                DestroyIcon(hFolderIcon);
            }

            SelectObject(hdc, hFont);
            SetTextColor(hdc, (i == g_activeTabIndex) ? RGB(255, 255, 255) : RGB(170, 170, 170));

            std::wstring title = g_tabs[i].title;

            size_t maxChars = (tabHeight - 50) / 9; 
            if (maxChars < 3) maxChars = 3;
            if (title.length() > maxChars) {
                title = title.substr(0, maxChars - 1) + L"..";
            }

            int textX = 5; 
            int textY = topPos + tabHeight - 4 -iconSize -4;
            TextOutW(hdc, textX, textY, title.c_str(), (int)title.length());
        }

        SelectObject(hdc, hOldFont);
        DeleteObject(hFont);
        DeleteObject(hCloseFont);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_RBUTTONUP:
    {
        int x = LOWORD(lParam);
        int y = HIWORD(lParam);

        if (x < TAB_BAR_WIDTH && y >= ADD_BTN_HEIGHT)
        {
            RECT clientRc;
            GetClientRect(hwnd, &clientRc);
            
            int availableHeight = clientRc.bottom - ADD_BTN_HEIGHT;
            int totalTabs = (int)g_tabs.size();
            
            if (totalTabs > 0)
            {
                int minTabHeight = 50;
                int maxTabHeight = 130;
                int tabHeight = availableHeight / totalTabs;
                if (tabHeight > maxTabHeight) tabHeight = maxTabHeight;
                if (tabHeight < minTabHeight) tabHeight = minTabHeight;

                int clickedTab = (y - ADD_BTN_HEIGHT) / tabHeight;

                if (clickedTab >= 0 && clickedTab < totalTabs)
                {
                    HMENU hMenu = CreatePopupMenu();
                    if (hMenu)
                    {
                        DWORD drives = GetLogicalDrives();
                        
                        std::map<int, std::wstring> menuIdToPath;
                        std::vector<HBITMAP> bitmappedIcons;
                        int currentMenuId = 3000;

                        UINT oldErrorMode = SetErrorMode(SEM_FAILCRITICALERRORS); 

                        for (int i = 0; i < 26; ++i)
                        {
                            if (drives & (1 << i))
                            {
                                WCHAR drivePath[4] = { (WCHAR)(L'A' + i), L':', L'\\', L'\0' };
                                UINT driveType = GetDriveTypeW(drivePath);
                                WCHAR volumeName[MAX_PATH + 1] = { 0 };
                                
                                GetVolumeInformationW(drivePath, volumeName, MAX_PATH, NULL, NULL, NULL, NULL, 0);
                                
                                std::wstring menuText = std::wstring(drivePath);
                                if (wcslen(volumeName) > 0)
                                    menuText += L" (" + std::wstring(volumeName) + L")";

                                HMENU hSubMenu = CreatePopupMenu();

                                menuIdToPath[currentMenuId] = drivePath;
                                
                                MENUITEMINFOW miiDrive = { sizeof(MENUITEMINFOW) };
                                miiDrive.fMask = MIIM_STRING | MIIM_ID;
                                miiDrive.wID = currentMenuId++;
                                miiDrive.dwTypeData = (LPWSTR)L" Open Drive";
                                
                                HBITMAP hDriveBmp = GetPathBitmapIcon(drivePath);
                                if (hDriveBmp) {
                                    miiDrive.fMask |= MIIM_BITMAP;
                                    miiDrive.hbmpItem = hDriveBmp;
                                    bitmappedIcons.push_back(hDriveBmp);
                                }
                                InsertMenuItemW(hSubMenu, 0, TRUE, &miiDrive);

                                AppendMenuW(hSubMenu, MF_SEPARATOR, 0, NULL);

                                if (driveType == DRIVE_FIXED || driveType == DRIVE_REMOVABLE || driveType == DRIVE_REMOTE)
                                {
                                    BuildFolderMenu(hSubMenu, drivePath, 1, 5, menuIdToPath, bitmappedIcons, currentMenuId);
                                }

                                MENUITEMINFOW miiMainDrive = { sizeof(MENUITEMINFOW) };
                                miiMainDrive.fMask = MIIM_STRING | MIIM_SUBMENU;
                                miiMainDrive.hSubMenu = hSubMenu;
                                miiMainDrive.dwTypeData = (LPWSTR)menuText.c_str();

                                HBITMAP hMainDriveBmp = GetPathBitmapIcon(drivePath);
                                if (hMainDriveBmp) {
                                    miiMainDrive.fMask |= MIIM_BITMAP;
                                    miiMainDrive.hbmpItem = hMainDriveBmp;
                                    bitmappedIcons.push_back(hMainDriveBmp);
                                }

                                InsertMenuItemW(hMenu, GetMenuItemCount(hMenu), TRUE, &miiMainDrive);
                            }
                        }
                        SetErrorMode(oldErrorMode);

                        POINT pt = { x, y };
                        ClientToScreen(hwnd, &pt);

                        int selectedId = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
                        DestroyMenu(hMenu);

                        for (HBITMAP hBmp : bitmappedIcons) {
                            DeleteObject(hBmp);
                        }

                        if (selectedId > 0 && menuIdToPath.find(selectedId) != menuIdToPath.end())
                        {
                            if (g_activeTabIndex != clickedTab)
                            {
                                g_activeTabIndex = clickedTab;
                                for (int i = 0; i < totalTabs; ++i)
                                {
                                    if (g_tabs[i].hViewWnd)
                                        ShowWindow(g_tabs[i].hViewWnd, (i == g_activeTabIndex) ? SW_SHOW : SW_HIDE);
                                }
                                RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW | RDW_ALLCHILDREN);
                            }
                            
                            NavigateCurrentTab(hwnd, menuIdToPath[selectedId]);
                        }
                    }
                }
            }
        }
        return 0;
    }

    case WM_LBUTTONDOWN:
    {
        int x = LOWORD(lParam);
        int y = HIWORD(lParam);

        if (x < TAB_BAR_WIDTH)
        {
            if (y < ADD_BTN_HEIGHT)
            {
                CreateNewTab(hwnd, GetDesktopPath());
            }
            else
            {
                RECT clientRc;
                GetClientRect(hwnd, &clientRc);
                
                int availableHeight = clientRc.bottom - ADD_BTN_HEIGHT;
                int totalTabs = (int)g_tabs.size();
                
                if (totalTabs > 0)
                {
                    int minTabHeight = 50;
                    int maxTabHeight = 130;
                    int tabHeight = availableHeight / totalTabs;
                    if (tabHeight > maxTabHeight) tabHeight = maxTabHeight;
                    if (tabHeight < minTabHeight) tabHeight = minTabHeight;

                    int clickedTab = (y - ADD_BTN_HEIGHT) / tabHeight;

                    if (clickedTab >= 0 && clickedTab < totalTabs)
                    {
                        int topPos = ADD_BTN_HEIGHT + (clickedTab * tabHeight);
                        
                        int closeBtnLeft = (TAB_BAR_WIDTH - CLOSE_BTN_SIZE) / 2 - 2;
                        int closeBtnRight = closeBtnLeft + CLOSE_BTN_SIZE + 4;
                        int closeBtnTop = topPos + 2;
                        int closeBtnBottom = closeBtnTop + CLOSE_BTN_SIZE + 6;

                        if (tabHeight >= 50 && x >= closeBtnLeft && x <= closeBtnRight && y >= closeBtnTop && y <= closeBtnBottom)
                        {
                            CloseTab(hwnd, clickedTab);
                        }
                        else
                        {
                            g_activeTabIndex = clickedTab;
                            for (int i = 0; i < totalTabs; ++i)
                            {
                                if (g_tabs[i].hViewWnd)
                                    ShowWindow(g_tabs[i].hViewWnd, (i == g_activeTabIndex) ? SW_SHOW : SW_HIDE);
                            }
                            RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW | RDW_ALLCHILDREN);
                        }
                    }
                }
            }
        }
        return 0;
    }

    case WM_APPBAR_CALLBACK:
    {
        if (wParam == ABN_POSCHANGED)
        {
            MONITORINFO mi = { sizeof(mi) };
            GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi);

            APPBARDATA q = abd;
            q.uEdge = ABE_LEFT;
            q.rc = mi.rcMonitor;
            q.rc.right = q.rc.left + dockWidth;
            SHAppBarMessage(ABM_QUERYPOS, &q);

            RECT oldRc = abd.rc;
            RECT newRc = oldRc;

            if (q.rc.top < oldRc.top)       newRc.top = q.rc.top;        
            if (q.rc.bottom > oldRc.bottom) newRc.bottom = q.rc.bottom;  
            if (q.rc.left < oldRc.left)                                  
            {
                newRc.left = q.rc.left;
                newRc.right = newRc.left + dockWidth;
            }
            

            if (!EqualRect(&newRc, &oldRc))
            {
                abd.rc = newRc;
                SHAppBarMessage(ABM_SETPOS, &abd);
                SetWindowPos(hwnd, HWND_TOPMOST,
                    abd.rc.left, abd.rc.top,
                    dockWidth, abd.rc.bottom - abd.rc.top,
                    SWP_NOACTIVATE);
            }
        }
        return 0;
    }


        case WM_INITMENUPOPUP:
    case WM_DRAWITEM:
    case WM_MEASUREITEM:
    case WM_MENUCHAR:
    {
        LRESULT lr = 0;
        if (g_pCtxMenu3 && SUCCEEDED(g_pCtxMenu3->HandleMenuMsg2(msg, wParam, lParam, &lr)))
            return lr;
        if (g_pCtxMenu2 && SUCCEEDED(g_pCtxMenu2->HandleMenuMsg(msg, wParam, lParam)))
            return (msg == WM_INITMENUPOPUP) ? 0 : TRUE;
        break;
    }

    case WM_SIZE:
    {
        RECT rc;
        GetClientRect(hwnd, &rc);

        for (auto& tab : g_tabs)
        {
            if (tab.hViewWnd && IsWindow(tab.hViewWnd))
            {
                MoveWindow(
                    tab.hViewWnd,
                    TAB_BAR_WIDTH,
                    0,
                    rc.right - TAB_BAR_WIDTH,
                    rc.bottom,
                    TRUE
                );
            }
        }
        return 0;
    }

    case WM_DESTROY:
    {
        SHAppBarMessage(ABM_REMOVE, &abd);

        RECT wa;
        SystemParametersInfo(SPI_GETWORKAREA, 0, &wa, 0);
        wa.left -= dockWidth;
        SystemParametersInfo(SPI_SETWORKAREA, 0, &wa, SPIF_SENDCHANGE);

        for (auto& tab : g_tabs)
        {
            if (tab.pShellView) {
                tab.pShellView->UIActivate(SVUIA_DEACTIVATE);
                tab.pShellView->DestroyViewWindow();
                tab.pShellView->Release();
            }
            if (tab.pFolder) {
                tab.pFolder->Release();
            }
            if (tab.pBrowser) {
                tab.pBrowser->Release();
            }
        }
        g_tabs.clear();

        OleUninitialize();
        PostQuitMessage(0);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ---------- Dark menus (uxtheme ordinals: 135 = SetPreferredAppMode, 136 = FlushMenuThemes) ----------
typedef int  (WINAPI* fnSetPreferredAppMode)(int);
typedef void (WINAPI* fnFlushMenuThemes)();

void EnableDarkMenus()
{
    HMODULE hUx = LoadLibraryExW(L"uxtheme.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!hUx) return;

    auto SetPreferredAppMode = (fnSetPreferredAppMode)GetProcAddress(hUx, MAKEINTRESOURCEA(135));
    auto FlushMenuThemes     = (fnFlushMenuThemes)GetProcAddress(hUx, MAKEINTRESOURCEA(136));

    if (SetPreferredAppMode) SetPreferredAppMode(2); 
    if (FlushMenuThemes) FlushMenuThemes();
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int)
{
    EnableDarkMenus();

    const wchar_t CLASS_NAME[] = L"DeskplorerDockClass";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    RECT wa;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &wa, 0);

    HWND hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        CLASS_NAME,
        L"",
        WS_POPUP | WS_VISIBLE,
        wa.left, wa.top,
        dockWidth,
        wa.bottom - wa.top,
        NULL, NULL, hInst, NULL
    );

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        BOOL bHandled = FALSE;
        if (g_activeTabIndex >= 0 && g_activeTabIndex < (int)g_tabs.size()) {
            if (g_tabs[g_activeTabIndex].pShellView) {
                if (g_tabs[g_activeTabIndex].pShellView->TranslateAccelerator(&msg) == S_OK) {
                    bHandled = TRUE;
                }
            }
        }

        if (!bHandled) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return 0;
}