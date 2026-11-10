// ==WindhawkMod==
// @id ali-floating-taskbar
// @name Floating Taskbar For Windows 10
// @description Converts the Windows 10 taskbar into a centered, rounded floating island.
// @version 1.0
// @author Ali
// @github https://github.com/raystarling-tech
// @include explorer.exe
// ==/WindhawkMod==

// ==WindhawkModSettings==
/*...*/
// ==/WindhawkModSettings==
#include <windows.h>
#pragma comment(lib, "gdi32.lib")
typedef HRGN (WINAPI* CreateRoundRectRgn_t)(
    int, int, int, int, int, int
);

typedef BOOL (WINAPI* DeleteObject_t)(HGDIOBJ);

static CreateRoundRectRgn_t pCreateRoundRectRgn = nullptr;
static DeleteObject_t pDeleteObject = nullptr;
static HWND g_taskbar = nullptr;
// ===== CUSTOM TASKBAR POSITIONS =====

int StartX = 21;
int StartY = 0;
int StartZ = 0;

int SearchX = 57;
int SearchY = 0;
int SearchZ = 0;

int TaskViewX = 93;
int TaskViewY = 0;
int TaskViewZ = 0;

int AppsX = 0;
int AppsY = 0;
int AppsZ = 0;

int TrayX = 826;
int TrayY = 0;
int TrayZ = 0;
static void LoadSettings()
{
    StartX = Wh_GetIntSetting(L"StartX");
    StartY = Wh_GetIntSetting(L"StartY");
    StartZ = Wh_GetIntSetting(L"StartZ");

    SearchX = Wh_GetIntSetting(L"SearchX");
    SearchY = Wh_GetIntSetting(L"SearchY");
    SearchZ = Wh_GetIntSetting(L"SearchZ");

    TaskViewX = Wh_GetIntSetting(L"TaskViewX");
    TaskViewY = Wh_GetIntSetting(L"TaskViewY");
    TaskViewZ = Wh_GetIntSetting(L"TaskViewZ");

    TrayX = Wh_GetIntSetting(L"TrayX");
    TrayY = Wh_GetIntSetting(L"TrayY");
    TrayZ = Wh_GetIntSetting(L"TrayZ");

    AppsX = Wh_GetIntSetting(L"AppsX");
    AppsY = Wh_GetIntSetting(L"AppsY");
    AppsZ = Wh_GetIntSetting(L"AppsZ");
}
static HWND g_start = nullptr;
static HWND g_search = nullptr;
static HWND g_taskView = nullptr;
static HWND g_tray = nullptr;
static HWND g_taskbarList = nullptr;

static RECT g_originalStart = {};
static RECT g_originalSearch = {};
static RECT g_originalTaskView = {};
static RECT g_originalTray = {};
static RECT g_originalTaskbarList = {};

static bool g_savedStart = false;
static bool g_savedSearch = false;
static bool g_savedTaskView = false;
static bool g_savedTray = false;
static bool g_savedTaskbarList = false;

static const int ISLAND_WIDTH = 1150;
static const int TASKLIST_WIDTH = 555;

// Save a window's original screen coordinates.
static bool SaveWindowRect(HWND hwnd, RECT* rect)
{
    if (!hwnd || !IsWindow(hwnd))
        return false;

    return GetWindowRect(hwnd, rect) != FALSE;
}

// Move a child window using screen coordinates.

static bool MoveChildToScreen(
    HWND hwnd,
    int x,
    int y,
    int width,
    int height,
    int zOrder = 0
)
{
    if (!IsWindow(hwnd))
        return false;

    HWND parent = GetParent(hwnd);

    POINT points[2] = {
        { x, y },
        { x + width, y + height }
    };

    if (parent)
        MapWindowPoints(HWND_DESKTOP, parent, points, 2);

    HWND insertAfter = nullptr;
    UINT flags = SWP_NOACTIVATE | SWP_NOSIZE;

    if (zOrder == 1)
        insertAfter = HWND_TOP;
    else if (zOrder == -1)
        insertAfter = HWND_BOTTOM;
    else
        flags |= SWP_NOZORDER;

    BOOL result = SetWindowPos(
        hwnd,
        insertAfter,
        points[0].x,
        points[0].y,
        width,
        height,
        flags
    );

    Wh_Log(
        L"MOVE: HWND=%p Result=%d X=%d Y=%d Z=%d",
        hwnd, result, x, y, zOrder
    );

    return result != FALSE;
}
// Restore a child window to its original screen coordinates.
static void RestoreWindow(HWND hwnd, const RECT& rect)
{
    if (!hwnd || !IsWindow(hwnd))
        return;

    MoveChildToScreen(
        hwnd,
        rect.left,
        rect.top,
        rect.right - rect.left,
        rect.bottom - rect.top
    );
}

// Find the taskbar controls.
static bool FindTaskbarWindows()
{
    g_taskbar = FindWindowW(L"Shell_TrayWnd", nullptr);

    if (!g_taskbar)
        return false;

    g_start = FindWindowExW(
        g_taskbar,
        nullptr,
        L"Start",
        nullptr
    );

    if (!g_start)
        g_start = FindWindowExW(
            g_taskbar,
            nullptr,
            L"Button",
            nullptr
        );

    g_search = FindWindowExW(
        g_taskbar,
        g_start,
        L"TrayButton",
        nullptr
    );

    if (g_search)
    {
        g_taskView = FindWindowExW(
            g_taskbar,
            g_search,
            L"TrayButton",
            nullptr
        );
    }

    g_tray = FindWindowExW(
        g_taskbar,
        nullptr,
        L"TrayNotifyWnd",
        nullptr
    );

    HWND rebar = FindWindowExW(
        g_taskbar,
        nullptr,
        L"ReBarWindow32",
        nullptr
    );

    if (rebar)
    {
        HWND switcher = FindWindowExW(
            rebar,
            nullptr,
            L"MSTaskSwWClass",
            nullptr
        );

        if (switcher)
        {
            g_taskbarList = FindWindowExW(
                switcher,
                nullptr,
                L"MSTaskListWClass",
                nullptr
            );
        }
    }

    return g_start && g_search &&
           g_taskView && g_tray &&
           g_taskbarList;
}
static void LogTaskbarChildren(HWND parent)
{
    HWND child = nullptr;

    while ((child = FindWindowExW(
        parent, child, nullptr, nullptr)) != nullptr)
    {
        wchar_t className[256] = {};
        wchar_t title[256] = {};

        GetClassNameW(child, className, 256);
        GetWindowTextW(child, title, 256);

        RECT r = {};
        GetWindowRect(child, &r);

        Wh_Log(
            L"CHILD: HWND=%p CLASS=%s TITLE=%s X=%d Y=%d W=%d H=%d VISIBLE=%d",
            child,
            className,
            title,
            r.left,
            r.top,
            r.right - r.left,
            r.bottom - r.top,
            IsWindowVisible(child)
        );

        LogTaskbarChildren(child);
    }
}
// Center the application icons.
static bool CenterTaskbarIcons()
{
    if (!g_taskbarList)
        return false;

    HWND parent = GetParent(g_taskbarList);

    if (!parent)
        return false;

    RECT parentRect;
    if (!GetClientRect(parent, &parentRect))
        return false;

    int parentWidth = parentRect.right - parentRect.left;

    int width = TASKLIST_WIDTH;

    if (width > parentWidth)
        width = parentWidth;

    int x = (parentWidth - width) / 2;

    RECT screenRect;
    if (!GetWindowRect(parent, &screenRect))
        return false;

    POINT point = { x, 0 };

    MapWindowPoints(
        parent,
        HWND_DESKTOP,
        &point,
        1
    );

    return MoveChildToScreen(
        g_taskbarList,
        point.x,
        screenRect.top,
        width,
        screenRect.bottom - screenRect.top
    );
}

// Apply the rounded floating island region.
static bool ApplyFloatingRegion()
{
    if (!g_taskbar || !IsWindow(g_taskbar))
        return false;

    HMODULE gdi = GetModuleHandleW(L"gdi32.dll");

    if (!gdi)
        gdi = LoadLibraryW(L"gdi32.dll");

    if (!gdi)
        return false;

    pCreateRoundRectRgn =
        (CreateRoundRectRgn_t)GetProcAddress(
            gdi, "CreateRoundRectRgn"
        );

    pDeleteObject =
        (DeleteObject_t)GetProcAddress(
            gdi, "DeleteObject"
        );

    if (!pCreateRoundRectRgn || !pDeleteObject)
        return false;

    RECT clientRect;

    if (!GetClientRect(g_taskbar, &clientRect))
        return false;

    int taskbarWidth = clientRect.right;
    int islandWidth = ISLAND_WIDTH;

    if (islandWidth > taskbarWidth)
        islandWidth = taskbarWidth;

    int islandLeft = (taskbarWidth - islandWidth) / 2;

    HRGN region = pCreateRoundRectRgn(
        islandLeft,
        0,
        islandLeft + islandWidth + 1,
        clientRect.bottom + 1,
        30,
        30
    );

    if (!region)
        return false;

    if (!SetWindowRgn(g_taskbar, region, TRUE))
    {
        pDeleteObject(region);
        return false;
    }

    return true;
}
// Position all taskbar elements.

static bool PositionTaskbarElements()
{
    RECT taskbarRect;

    if (!GetWindowRect(g_taskbar, &taskbarRect))
        return false;

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);

    int islandWidth = ISLAND_WIDTH;

    if (islandWidth > screenWidth)
        islandWidth = screenWidth;

    int islandLeft = (screenWidth - islandWidth) / 2;

    int taskbarHeight =
        taskbarRect.bottom - taskbarRect.top;

    // Save original positions.
    g_savedStart = SaveWindowRect(g_start, &g_originalStart);
    g_savedSearch = SaveWindowRect(g_search, &g_originalSearch);
    g_savedTaskView = SaveWindowRect(g_taskView, &g_originalTaskView);
    g_savedTray = SaveWindowRect(g_tray, &g_originalTray);
    g_savedTaskbarList = SaveWindowRect(
        g_taskbarList,
        &g_originalTaskbarList
    );

    if (!g_savedStart || !g_savedSearch ||
        !g_savedTaskView || !g_savedTray ||
        !g_savedTaskbarList)
        return false;

    // START BUTTON
    MoveChildToScreen(
        g_start,
        islandLeft + StartX,
        taskbarRect.top + StartY,
        36,
        taskbarHeight,
        StartZ
    );

    // SEARCH BUTTON
    MoveChildToScreen(
        g_search,
        islandLeft + SearchX,
        taskbarRect.top + SearchY,
        g_originalSearch.right - g_originalSearch.left,
        g_originalSearch.bottom - g_originalSearch.top,
        SearchZ
    );

    // TASK VIEW BUTTON
    MoveChildToScreen(
        g_taskView,
        islandLeft + TaskViewX,
        taskbarRect.top + TaskViewY,
        g_originalTaskView.right - g_originalTaskView.left,
        g_originalTaskView.bottom - g_originalTaskView.top,
        TaskViewZ
    );

    // CLOCK AND NOTIFICATIONS
    MoveChildToScreen(
        g_tray,
        islandLeft + TrayX,
        taskbarRect.top + TrayY,
        305,
        taskbarHeight,
        TrayZ
    );

    CenterTaskbarIcons();

    return true;
}

static void MoveSearchAndTaskViewLast()
{
    RECT taskbarRect;

    if (!GetWindowRect(g_taskbar, &taskbarRect))
        return;

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);

    int islandWidth = ISLAND_WIDTH;

    if (islandWidth > screenWidth)
        islandWidth = screenWidth;

    int islandLeft = (screenWidth - islandWidth) / 2;

    int height = taskbarRect.bottom - taskbarRect.top;

    // SEARCH BUTTON
    MoveChildToScreen(
        g_search,
        islandLeft + SearchX,
        taskbarRect.top + SearchY,
        g_originalSearch.right - g_originalSearch.left,
        g_originalSearch.bottom - g_originalSearch.top,
        SearchZ
    );

    // TASK VIEW BUTTON
    MoveChildToScreen(
        g_taskView,
        islandLeft + TaskViewX,
        taskbarRect.top + TaskViewY,
        g_originalTaskView.right - g_originalTaskView.left,
        g_originalTaskView.bottom - g_originalTaskView.top,
        TaskViewZ
    );

    // Check final positions.
    RECT s, t;

    GetWindowRect(g_search, &s);
    GetWindowRect(g_taskView, &t);

    Wh_Log(L"FINAL SEARCH X=%d Y=%d", s.left, s.top);
    Wh_Log(L"FINAL TASKVIEW X=%d Y=%d", t.left, t.top);
}
// Restore original taskbar layout.
static void RestoreTaskbar()
{
    if (g_taskbar && IsWindow(g_taskbar))
        SetWindowRgn(g_taskbar, nullptr, TRUE);

    if (g_savedStart)
        RestoreWindow(g_start, g_originalStart);

    if (g_savedSearch)
        RestoreWindow(g_search, g_originalSearch);

    if (g_savedTaskView)
        RestoreWindow(g_taskView, g_originalTaskView);

    if (g_savedTray)
        RestoreWindow(g_tray, g_originalTray);

    if (g_savedTaskbarList)
        RestoreWindow(
            g_taskbarList,
            g_originalTaskbarList
        );
}
BOOL Wh_ModInit()
{
    LoadSettings();
    if (!FindTaskbarWindows())
    {
        Wh_Log(L"Could not find all required taskbar windows.");
        return FALSE;
    }
    LogTaskbarChildren(g_taskbar);

    if (!PositionTaskbarElements())
    {
        Wh_Log(L"Could not position taskbar elements.");
        RestoreTaskbar();
        return FALSE;
    }

    if (!ApplyFloatingRegion())
    {
        RECT taskbarRect;
    GetWindowRect(g_taskbar, &taskbarRect);

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int islandLeft = (screenWidth - ISLAND_WIDTH) / 2;
    int taskbarHeight = taskbarRect.bottom - taskbarRect.top;

    // Search
    MoveChildToScreen(
    g_search,
    islandLeft + 127,
    taskbarRect.top,
    36,
    taskbarHeight
    );

    // Task View
    MoveChildToScreen(
    g_taskView,
    islandLeft + 163,
    taskbarRect.top,
    36,
    taskbarHeight
    );
        Wh_Log(L"Could not apply floating taskbar region.");
        RestoreTaskbar();
        return FALSE;
    }

    RECT r1, r2;

GetWindowRect(g_search, &r1);
GetWindowRect(g_taskView, &r2);

Wh_Log(
    L"AFTER MOVE: Search X=%d Y=%d W=%d H=%d",
    r1.left, r1.top,
    r1.right - r1.left,
    r1.bottom - r1.top
);

Wh_Log(
    L"AFTER MOVE: TaskView X=%d Y=%d W=%d H=%d",
    r2.left, r2.top,
    r2.right - r2.left,
    r2.bottom - r2.top
);
HWND searchParent = GetParent(g_search);
HWND taskViewParent = GetParent(g_taskView);

RECT pr1 = {};
RECT pr2 = {};

GetWindowRect(searchParent, &pr1);
GetWindowRect(taskViewParent, &pr2);

Wh_Log(
    L"SEARCH PARENT: HWND=%p CLASS=%s X=%d Y=%d W=%d H=%d",
    searchParent,
    L"Parent",
    pr1.left,
    pr1.top,
    pr1.right - pr1.left,
    pr1.bottom - pr1.top
);

Wh_Log(
    L"TASKVIEW PARENT: HWND=%p CLASS=%s X=%d Y=%d W=%d H=%d",
    taskViewParent,
    L"Parent",
    pr2.left,
    pr2.top,
    pr2.right - pr2.left,
    pr2.bottom - pr2.top
);
    MoveSearchAndTaskViewLast();
    Wh_Log(L"Ali Floating Taskbar initialized.");

    return TRUE;
}

// Windhawk cleanup.
void Wh_ModUninit()
{
    RestoreTaskbar();

    Wh_Log(L"Ali Floating Taskbar unloaded.");
}

BOOL Wh_ModSettingsChanged(BOOL* bReload)
{
    *bReload = TRUE;
    return TRUE;
}
