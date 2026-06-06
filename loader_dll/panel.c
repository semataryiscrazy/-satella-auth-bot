#include <windows.h>
#include <stdio.h>
#include <time.h>
#include "panel_obf.h"

static HWND g_hwndMain = NULL;
static HWND g_hwndUsername = NULL;
static HWND g_hwndPassword = NULL;
static HWND g_hwndLoginBtn = NULL;
static HWND g_hwndRegisterBtn = NULL;
static HWND g_hwndLoadBtn = NULL;
static HWND g_hwndBypassBtn = NULL;
static HWND g_hwndStatusText = NULL;
static HFONT g_hFontTitle = NULL;
static HFONT g_hFontLabel = NULL;
static HFONT g_hFontBtn = NULL;

static BOOL g_isLoggedIn = FALSE;
static char g_username[64] = "";
static char g_password[64] = "";
static HINSTANCE g_hInstance = NULL;
static int g_hoverBtn = 0;

void WriteDebugLog(const char* msg) {
    (void)msg;
}

static void CreateFonts() {
    g_hFontTitle = CreateFontW(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    g_hFontLabel = CreateFontW(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    g_hFontBtn = CreateFontW(13, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

static void PaintBackground(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc;
    GetClientRect(hwnd, &rc);

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
    SelectObject(memDC, memBmp);

    GRADIENT_RECT gr = {0, 1};
    TRIVERTEX tvTop[2] = {
        {rc.left, rc.top, 14000, 8000, 24000, 10000},
        {rc.right, rc.bottom/2, 6000, 3000, 12000, 3000}
    };
    GradientFill(memDC, tvTop, 2, &gr, 1, GRADIENT_FILL_RECT_V);

    TRIVERTEX tvBot[2] = {
        {rc.left, rc.bottom/2, 6000, 3000, 12000, 3000},
        {rc.right, rc.bottom, 3000, 2000, 6000, 4000}
    };
    GradientFill(memDC, tvBot, 2, &gr, 1, GRADIENT_FILL_RECT_V);

    DrawTextCenter(memDC, 0, 20, rc.right, 50, L"Satella Loader", RGB(255, 255, 255), g_hFontTitle);

    BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);
    DeleteDC(memDC);
    DeleteObject(memBmp);
    EndPaint(hwnd, &ps);
}

static void DrawRoundRect(HDC hdc, int x, int y, int w, int h, int r, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    HRGN region = CreateRoundRectRgn(x, y, x + w, y + h, r, r);
    FillRgn(hdc, region, brush);
    DeleteObject(region);
    DeleteObject(brush);
}

static void DrawTextCenter(HDC hdc, int x, int y, int w, int h, const wchar_t* text, COLORREF color, HFONT font) {
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    SelectObject(hdc, font);
    RECT r = {x, y, x + w, y + h};
    DrawTextW(hdc, text, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void DrawCustomButton(HDC hdc, int x, int y, int w, int h, const wchar_t* text, COLORREF bg, COLORREF textCol, bool hover) {
    DrawRoundRect(hdc, x, y, w, h, 6, hover ? RGB(
        min(255, GetRValue(bg) + 25),
        min(255, GetGValue(bg) + 25),
        min(255, GetBValue(bg) + 25)
    ) : bg);
    HRGN brgn = CreateRoundRectRgn(x, y, x + w, y + h, 6, 6);
    HBRUSH border = CreateSolidBrush(RGB(70, 72, 90));
    FrameRgn(hdc, brgn, border, 1, 1);
    DeleteObject(border);
    DeleteObject(brgn);
    DrawTextCenter(hdc, x, y, w, h, text, textCol, g_hFontBtn);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT:
        PaintBackground(hwnd);
        break;
    case WM_CTLCOLORSTATIC:
    {
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(180, 185, 210));
        return (LRESULT)GetStockObject(NULL_BRUSH);
    }
    case WM_CTLCOLOREDIT:
    {
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(230, 230, 245));
        SetDCBrushColor(hdc, RGB(18, 18, 30));
        return (LRESULT)GetStockObject(DC_BRUSH);
    }
    case WM_DRAWITEM:
    {
        LPDRAWITEMSTRUCT di = (LPDRAWITEMSTRUCT)lParam;
        HDC hdc = di->hDC;
        int w = di->rcItem.right - di->rcItem.left;
        int h = di->rcItem.bottom - di->rcItem.top;
        BOOL hover = di->itemState & ODS_HOTLIGHT;
        BOOL pressed = di->itemState & ODS_SELECTED;
        BOOL isAccent = (di->hwndItem == g_hwndLoginBtn || di->hwndItem == g_hwndLoadBtn);

        COLORREF bg;
        if (isAccent) {
            bg = pressed ? RGB(71, 74, 200) : hover ? RGB(110, 113, 255) : RGB(99, 102, 241);
        } else {
            bg = pressed ? RGB(55, 55, 68) : hover ? RGB(60, 60, 75) : RGB(45, 45, 55);
        }

        DrawRoundRect(hdc, 0, 0, w, h, 6, bg);

        wchar_t txt[64];
        GetWindowTextW(di->hwndItem, txt, 64);
        DrawTextCenter(hdc, 0, 0, w, h, txt, RGB(255, 255, 255), g_hFontBtn);
        return TRUE;
    }
    case WM_COMMAND:
        if ((HWND)lParam == g_hwndLoginBtn) {
            GetWindowTextA(g_hwndUsername, g_username, sizeof(g_username));
            GetWindowTextA(g_hwndPassword, g_password, sizeof(g_password));

            if (strlen(g_username) > 0 && strlen(g_password) > 0) {
                g_isLoggedIn = TRUE;
                SetWindowTextW(g_hwndStatusText, cobw_WLOGIN_OK());
                ShowWindow(g_hwndLoadBtn, SW_SHOW);
                ShowWindow(g_hwndBypassBtn, SW_SHOW);
                ShowWindow(g_hwndUsername, SW_HIDE);
                ShowWindow(g_hwndPassword, SW_HIDE);
                ShowWindow(g_hwndLoginBtn, SW_HIDE);
                ShowWindow(g_hwndRegisterBtn, SW_HIDE);
                InvalidateRect(hwnd, NULL, TRUE);
            }
        }
        else if ((HWND)lParam == g_hwndRegisterBtn) {
            GetWindowTextA(g_hwndUsername, g_username, sizeof(g_username));
            GetWindowTextA(g_hwndPassword, g_password, sizeof(g_password));

            if (strlen(g_username) > 0 && strlen(g_password) > 0) {
                g_isLoggedIn = TRUE;
                SetWindowTextW(g_hwndStatusText, cobw_WREG_OK());
                ShowWindow(g_hwndLoadBtn, SW_SHOW);
                ShowWindow(g_hwndBypassBtn, SW_SHOW);
                ShowWindow(g_hwndUsername, SW_HIDE);
                ShowWindow(g_hwndPassword, SW_HIDE);
                ShowWindow(g_hwndLoginBtn, SW_HIDE);
                ShowWindow(g_hwndRegisterBtn, SW_HIDE);
                InvalidateRect(hwnd, NULL, TRUE);
            }
        }
        else if ((HWND)lParam == g_hwndLoadBtn) {
            SetWindowTextW(g_hwndStatusText, cobw_WLOADING());
        }
        else if ((HWND)lParam == g_hwndBypassBtn) {
            SetWindowTextW(g_hwndStatusText, cobw_WBYPASS());
        }
        break;
    case WM_MOUSEMOVE:
    {
        POINT pt = {LOWORD(lParam), HIWORD(lParam)};
        int oldHover = g_hoverBtn;
        g_hoverBtn = 0;
        RECT rc;
        if (IsWindowVisible(g_hwndLoadBtn)) {
            GetWindowRect(g_hwndLoadBtn, &rc);
            MapWindowPoints(HWND_DESKTOP, hwnd, (POINT*)&rc, 2);
            if (PtInRect(&rc, pt)) g_hoverBtn = 1;
        }
        if (IsWindowVisible(g_hwndBypassBtn)) {
            GetWindowRect(g_hwndBypassBtn, &rc);
            MapWindowPoints(HWND_DESKTOP, hwnd, (POINT*)&rc, 2);
            if (PtInRect(&rc, pt)) g_hoverBtn = 2;
        }
        if (IsWindowVisible(g_hwndLoginBtn)) {
            GetWindowRect(g_hwndLoginBtn, &rc);
            MapWindowPoints(HWND_DESKTOP, hwnd, (POINT*)&rc, 2);
            if (PtInRect(&rc, pt)) g_hoverBtn = 3;
        }
        if (IsWindowVisible(g_hwndRegisterBtn)) {
            GetWindowRect(g_hwndRegisterBtn, &rc);
            MapWindowPoints(HWND_DESKTOP, hwnd, (POINT*)&rc, 2);
            if (PtInRect(&rc, pt)) g_hoverBtn = 4;
        }
        if (oldHover != g_hoverBtn) {
            InvalidateRect(hwnd, NULL, TRUE);
        }
        break;
    }
    case WM_SETCURSOR:
        if (g_hoverBtn > 0) {
            SetCursor(LoadCursor(NULL, IDC_HAND));
            return TRUE;
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    case WM_CLOSE:
        ShowWindow(g_hwndMain, SW_HIDE);
        return 0;
    case WM_DESTROY:
        if (g_hFontTitle) DeleteObject(g_hFontTitle);
        if (g_hFontLabel) DeleteObject(g_hFontLabel);
        if (g_hFontBtn) DeleteObject(g_hFontBtn);
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

void CreatePanelWindow(void) {
    WriteDebugLog("CreatePanelWindow: Iniciando...");

    CreateFonts();

    const wchar_t* CLASS_NAME = cobw_WCLASS();

    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = g_hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = NULL;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.style = CS_HREDRAW | CS_VREDRAW;

    if (!RegisterClassW(&wc)) {
        WriteDebugLog("CreatePanelWindow: RegisterClassW falhou");
        return;
    }

    WriteDebugLog("CreatePanelWindow: Classe registrada");

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int panelWidth = 420;
    int panelHeight = 380;
    int posX = (screenWidth - panelWidth) / 2;
    int posY = (screenHeight - panelHeight) / 2;

    g_hwndMain = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        CLASS_NAME,
        cobw_WTITLE(),
        WS_POPUP | WS_VISIBLE | WS_SYSMENU,
        posX, posY, panelWidth, panelHeight,
        NULL, NULL, g_hInstance, NULL
    );

    if (!g_hwndMain) {
        WriteDebugLog("CreatePanelWindow: CreateWindowExW falhou");
        return;
    }

    SetLayeredWindowAttributes(g_hwndMain, 0, 235, LWA_ALPHA);

    HRGN winRgn = CreateRoundRectRgn(0, 0, panelWidth, panelHeight, 10, 10);
    SetWindowRgn(g_hwndMain, winRgn, TRUE);
    DeleteObject(winRgn);

    int formX = 50;
    int formW = panelWidth - 100;
    int fieldH = 32;
    int yStart = 110;

    CreateWindowW(L"STATIC", cobw_WUSER(), WS_VISIBLE | WS_CHILD, formX, yStart - 22, formW, 18, g_hwndMain, NULL, g_hInstance, NULL);
    g_hwndUsername = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", cobw_WEMPTY(), WS_VISIBLE | WS_CHILD | ES_LEFT, formX, yStart, formW, fieldH, g_hwndMain, NULL, g_hInstance, NULL);

    CreateWindowW(L"STATIC", cobw_WPASS(), WS_VISIBLE | WS_CHILD, formX, yStart + fieldH + 20, formW, 18, g_hwndMain, NULL, g_hInstance, NULL);
    g_hwndPassword = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", cobw_WEMPTY(), WS_VISIBLE | WS_CHILD | ES_PASSWORD | ES_LEFT, formX, yStart + fieldH + 38, formW, fieldH, g_hwndMain, NULL, g_hInstance, NULL);

    int btnW = (formW - 10) / 2;
    int btnY = yStart + fieldH + 38 + fieldH + 18;
    g_hwndLoginBtn = CreateWindowW(L"BUTTON", cobw_WLOGIN(), WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, formX, btnY, btnW, 36, g_hwndMain, NULL, g_hInstance, NULL);
    g_hwndRegisterBtn = CreateWindowW(L"BUTTON", cobw_WREG(), WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, formX + btnW + 10, btnY, btnW, 36, g_hwndMain, NULL, g_hInstance, NULL);

    g_hwndLoadBtn = CreateWindowW(L"BUTTON", cobw_WLOAD(), WS_CHILD | BS_OWNERDRAW, formX, btnY, btnW, 36, g_hwndMain, NULL, g_hInstance, NULL);
    g_hwndBypassBtn = CreateWindowW(L"BUTTON", cobw_WBYPASS_BTN(), WS_CHILD | BS_OWNERDRAW, formX + btnW + 10, btnY, btnW, 36, g_hwndMain, NULL, g_hInstance, NULL);

    g_hwndStatusText = CreateWindowW(L"STATIC", cobw_WWAITING(), WS_VISIBLE | WS_CHILD | SS_CENTER, 20, btnY + 50, panelWidth - 40, 50, g_hwndMain, NULL, g_hInstance, NULL);

    SendMessageW(g_hwndUsername, WM_SETFONT, (WPARAM)g_hFontLabel, TRUE);
    SendMessageW(g_hwndPassword, WM_SETFONT, (WPARAM)g_hFontLabel, TRUE);
    SendMessageW(g_hwndStatusText, WM_SETFONT, (WPARAM)g_hFontLabel, TRUE);

    ShowWindow(g_hwndMain, SW_SHOW);
    SetForegroundWindow(g_hwndMain);
    SetActiveWindow(g_hwndMain);
    UpdateWindow(g_hwndMain);

    WriteDebugLog("CreatePanelWindow: Janela mostrada e atualizada");
}

DWORD WINAPI PanelMessageLoopThread(LPVOID lpParam) {
    WriteDebugLog("PanelMessageLoopThread: Iniciando message loop");

    MSG msg = {0};
    int msgCount = 0;

    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
        msgCount++;

        if (msgCount % 100 == 0) {
            char logMsg[128];
            sprintf_s(logMsg, sizeof(logMsg), "PanelMessageLoopThread: %d mensagens processadas", msgCount);
            WriteDebugLog(logMsg);
        }
    }

    WriteDebugLog("PanelMessageLoopThread: Message loop finalizado");
    return 0;
}

void ShowPanel(void) {
    WriteDebugLog("ShowPanel: Iniciando");

    if (!g_hInstance) {
        g_hInstance = GetModuleHandleW(NULL);
        char logMsg[128];
        sprintf_s(logMsg, sizeof(logMsg), "ShowPanel: g_hInstance obtido: %p", g_hInstance);
        WriteDebugLog(logMsg);
    }

    CreatePanelWindow();

    if (g_hwndMain) {
        WriteDebugLog("ShowPanel: Janela criada, iniciando thread de message loop");

        HANDLE hThread = CreateThread(NULL, 0, PanelMessageLoopThread, NULL, 0, NULL);
        if (hThread) {
            WriteDebugLog("ShowPanel: Thread criada com sucesso");
        } else {
            char logMsg[128];
            sprintf_s(logMsg, sizeof(logMsg), "ShowPanel: Falha ao criar thread, erro: %lu", GetLastError());
            WriteDebugLog(logMsg);
        }
    } else {
        WriteDebugLog("ShowPanel: Falha ao criar janela");
    }
}
