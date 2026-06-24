#include <Windows.h>
#include <string>
#include <d2d1.h>
#include <dwrite.h>
#include <dwmapi.h>
#include <fstream>
#include <comdef.h>
#include <iostream>
#include <ctime>
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "dwrite.lib")

ID2D1Factory* factory;
ID2D1HwndRenderTarget* target;
ID2D1SolidColorBrush* solid_brush;
IDWriteFactory* w_factory;
IDWriteTextFormat* w_format;
IDWriteTextLayout* w_layout;
HWND overlayWindow;
HINSTANCE appInstance;
HWND targetWindow;
HWND enumWindow = NULL;

bool o_Foreground = false;
bool o_DrawFPS = false;
bool o_VSync = false;
std::wstring fontname = L"Courier";

// Link the static library (make sure that file is in the same directory as this file)
//#pragma comment(lib, "D2DOverlay.lib")

// Requires the targetted window to be active and the foreground window to draw.
#define D2DOV_REQUIRE_FOREGROUND	(1 << 0)

// Draws the FPS of the overlay in the top-right corner
#define D2DOV_DRAW_FPS				(1 << 1)

// Attempts to limit the frametimes so you don't render at 500fps
#define D2DOV_VSYNC					(1 << 2)

// Sets the text font to Calibri
#define D2DOV_FONT_CALIBRI			(1 << 3)

// Sets the text font to Arial
#define D2DOV_FONT_ARIAL			(1 << 4)

// Sets the text font to Courier
#define D2DOV_FONT_COURIER			(1 << 5)

// Sets the text font to Gabriola
#define D2DOV_FONT_GABRIOLA			(1 << 6)

// Sets the text font to Impact
#define D2DOV_FONT_IMPACT			(1 << 7)

// The function you call to set up the above options.  Make sure its called before the DirectOverlaySetup function
void DirectOverlaySetOption(DWORD option);

// typedef for the callback function, where you'll do the drawing.
typedef void(*DirectOverlayCallback)(int width, int height);

// Initializes a the overlay window, and the thread to run it.  Input your callback function.
// Uses the first window in the current process to target.  If you're external, use the next function
void DirectOverlaySetup(DirectOverlayCallback callbackFunction);

// Used to specify the window manually, to be used with externals.
void DirectOverlaySetup(DirectOverlayCallback callbackFunction, HWND targetWindow);

// Draws a line from (x1, y1) to (x2, y2), with a specified thickness.
// Specify the color, and optionally an alpha for the line.
void DrawLine(float x1, float y1, float x2, float y2, float thickness, float r, float g, float b, float a = 1);

// Draws a rectangle on the screen.  Width and height are relative to the coordinates of the box.  
// Use the "filled" bool to make it a solid rectangle; ignore the thickness.
// To just draw the border around the rectangle, specify a thickness and pass "filled" as false.
void DrawBox(float x, float y, float width, float height, float thickness, float r, float g, float b, float a, bool filled);

// Draws a circle.  As with the DrawBox, the "filled" bool will make it a solid circle, and thickness is only used when filled=false.
void DrawCircle(float x, float y, float radius, float thickness, float r, float g, float b, float a, bool filled);

// Allows you to draw an elipse.  Same as a circle, except you have two different radii, for width and height.
void DrawEllipse(float x, float y, float width, float height, float thickness, float r, float g, float b, float a, bool filled);

// Draw a string on the screen.  Input is in the form of an std::string.
void DrawString(std::string str, float fontSize, float x, float y, float r, float g, float b, float a = 1);

DirectOverlayCallback drawLoopCallback = NULL;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

class Rect {
public:
	float x;
	float y;
	float width;
	float height;

	Rect() {
		this->x = 0;
		this->y = 0;
		this->width = 0;
		this->height = 0;
	}

	Rect(float x, float y, float width, float height) {
		this->x = x;
		this->y = y;
		this->width = width;
		this->height = height;
	}

	bool operator==(const Rect& src) const {
		return (src.x == this->x && src.y == this->y && src.height == this->height &&
			src.width == this->width);
	}

	bool operator!=(const Rect& src) const {
		return (src.x != this->x && src.y != this->y && src.height != this->height &&
			src.width != this->width);
	}
};

void DrawString(std::string str, float fontSize, float x, float y, float r, float g, float b, float a)
{
	RECT re;
	GetClientRect(overlayWindow, &re);
	FLOAT dpix, dpiy;
	dpix = static_cast<float>(re.right - re.left);
	dpiy = static_cast<float>(re.bottom - re.top);
	HRESULT res = w_factory->CreateTextLayout(std::wstring(str.begin(), str.end()).c_str(), str.length() + 1, w_format, dpix, dpiy, &w_layout);
	if (SUCCEEDED(res))
	{
		DWRITE_TEXT_RANGE range = { 0, str.length() };
		w_layout->SetFontSize(fontSize, range);
		solid_brush->SetColor(D2D1::ColorF(r, g, b, a));
		target->DrawTextLayout(D2D1::Point2F(x, y), w_layout, solid_brush);
		w_layout->Release();
		w_layout = NULL;
	}
}

void DrawName(const char* text, float fontSize, Vector2 pos, float r, float g, float b, float a = 1)
{
	RECT re;
	GetClientRect(overlayWindow, &re);
	FLOAT dpix, dpiy;
	dpix = static_cast<float>(re.right - re.left);
	dpiy = static_cast<float>(re.bottom - re.top);

	int size_needed = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
	wchar_t* wtext = new wchar_t[size_needed];
	MultiByteToWideChar(CP_UTF8, 0, text, -1, wtext, size_needed);

	IDWriteTextFormat* w_format;
	HRESULT res = w_factory->CreateTextFormat(
		AY_OBFUSCATE(L"Segoe UI Emoji"),
		NULL,
		DWRITE_FONT_WEIGHT_NORMAL,
		DWRITE_FONT_STYLE_NORMAL,
		DWRITE_FONT_STRETCH_NORMAL,
		fontSize,
		AY_OBFUSCATE(L"en-US"),
		&w_format
	);

	if (SUCCEEDED(res))
	{
		IDWriteTextLayout* w_layout;
		res = w_factory->CreateTextLayout(
			wtext,
			wcslen(wtext),
			w_format,
			dpix,
			dpiy,
			&w_layout
		);

		if (SUCCEEDED(res))
		{
			DWRITE_TEXT_METRICS textMetrics;
			w_layout->GetMetrics(&textMetrics);

			ID2D1SolidColorBrush* textBrush;
			target->CreateSolidColorBrush(D2D1::ColorF(r, g, b, a), &textBrush);

			target->DrawTextLayout(
				D2D1::Point2F(pos.X - (textMetrics.width / 2), pos.Y - textMetrics.height),
				w_layout,
				textBrush
			);

			textBrush->Release();

			w_layout->Release();
			w_format->Release();
		}

		delete[] wtext;
	}
}

void DrawLine(float x1, float y1, float x2, float y2, float thickness, float r, float g, float b, float a) {
	solid_brush->SetColor(D2D1::ColorF(r, g, b, a));
	target->DrawLine(D2D1::Point2F(x1, y1), D2D1::Point2F(x2, y2), solid_brush, thickness);
}

void DrawBox(const Rect& rect, float thickness, float r, float g, float b, float a = 1) {
	Vector3 v1 = Vector3(rect.x, rect.y);
	Vector3 v2 = Vector3(rect.x + rect.width, rect.y);
	Vector3 v3 = Vector3(rect.x + rect.width, rect.y + rect.height);
	Vector3 v4 = Vector3(rect.x, rect.y + rect.height);

	DrawLine(v1.X, v1.Y, v2.X, v2.Y, thickness, r, g, b, a);
	DrawLine(v2.X, v2.Y, v3.X, v3.Y, thickness, r, g, b, a);
	DrawLine(v3.X, v3.Y, v4.X, v4.Y, thickness, r, g, b, a);
	DrawLine(v4.X, v4.Y, v1.X, v1.Y, thickness, r, g, b, a);
}

void DrawDistance(const wchar_t* text, float fontSize, Vector2 pos, float r, float g, float b, float a = 1)
{
	RECT re;
	GetClientRect(overlayWindow, &re);
	FLOAT dpix, dpiy;
	dpix = static_cast<float>(re.right - re.left);
	dpiy = static_cast<float>(re.bottom - re.top);

	IDWriteTextFormat* w_format;
	HRESULT res = w_factory->CreateTextFormat(
		AY_OBFUSCATE(L"Arial"),
		NULL,
		DWRITE_FONT_WEIGHT_NORMAL,
		DWRITE_FONT_STYLE_NORMAL,
		DWRITE_FONT_STRETCH_NORMAL,
		fontSize,
		AY_OBFUSCATE(L"en-US"),
		&w_format
	);

	if (SUCCEEDED(res))
	{
		IDWriteTextLayout* w_layout;
		res = w_factory->CreateTextLayout(
			text,
			wcslen(text),
			w_format,
			dpix,
			dpiy,
			&w_layout
		);

		if (SUCCEEDED(res))
		{
			DWRITE_TEXT_METRICS textMetrics;
			w_layout->GetMetrics(&textMetrics);

			ID2D1SolidColorBrush* textBrush;
			target->CreateSolidColorBrush(D2D1::ColorF(r, g, b, a), &textBrush);

			target->DrawTextLayout(
				D2D1::Point2F(
					pos.X - (textMetrics.width / 2), // centraliza horizontalmente
					pos.Y // cresce pra baixo a partir daqui
				),
				w_layout,
				textBrush
			);

			textBrush->Release();

			w_layout->Release();
			w_format->Release();
		}
	}
}

void DrawRect(float x, float y, float width, float height, int color, float strokeWidth, bool filled)
{
	solid_brush->SetColor(D2D1::ColorF(color));
	target->DrawRectangle(D2D1::RectF(x, y, x + width, y + height), solid_brush, strokeWidth);
	if (filled) {
		target->FillRectangle(D2D1::RectF(x, y, x + width, y + height), solid_brush);
	}
}

void DrawRoundHealthBar(const Rect& rect, float maxHealth, float currentHealth) {
	int healthColor = -16711936;
	float currentHealthWidth = (currentHealth * rect.height) / maxHealth;
	float maxHealthWidth = (maxHealth * rect.height) / maxHealth;; // Largura total da vida m�xima

	if (currentHealth <= (maxHealth * 1.0)) {
		healthColor = -16711936;
	}
	if (currentHealth <= (maxHealth * 0.66)) {
		healthColor = -256;
	}
	if (currentHealth <= (maxHealth * 0.33)) {
		healthColor = -65536;
	}

	DrawRect(rect.x - rect.width / 4, rect.y, rect.width / 10, maxHealthWidth, -16777216, 1, false);
	DrawRect(rect.x - rect.width / 4, rect.y + rect.height - currentHealthWidth, rect.width / 10, currentHealthWidth, healthColor, 1, true);
}

void DrawCircle(float x, float y, float radius, float thickness, float r, float g, float b, float a, bool filled)
{
	solid_brush->SetColor(D2D1::ColorF(r, g, b, a));
	if (filled) target->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x, y), radius, radius), solid_brush);
	else target->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(x, y), radius, radius), solid_brush, thickness);
}

void DrawEllipse(float x, float y, float width, float height, float thickness, float r, float g, float b, float a, bool filled)
{
	solid_brush->SetColor(D2D1::ColorF(r, g, b, a));
	if (filled) target->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x, y), width, height), solid_brush);
	else target->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(x, y), width, height), solid_brush, thickness);
}

void d2oSetup(HWND tWindow) {
	targetWindow = tWindow;
	WNDCLASS wc = { };
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = GetModuleHandle(0);
	wc.lpszClassName = AY_OBFUSCATE("d2do");
	RegisterClass(&wc);
	overlayWindow = CreateWindowEx(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST,
		wc.lpszClassName, AY_OBFUSCATE("Overlay"), WS_POPUP, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		NULL, NULL, wc.hInstance, NULL);

	MARGINS mar = { -1 };
	DwmExtendFrameIntoClientArea(overlayWindow, &mar);
	D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, &factory);
	factory->CreateHwndRenderTarget(
		D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
			D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED)),
		D2D1::HwndRenderTargetProperties(overlayWindow, D2D1::SizeU(200, 200),
			D2D1_PRESENT_OPTIONS_IMMEDIATELY), &target);
	target->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f), &solid_brush);
	target->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
	DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&w_factory));
	w_factory->CreateTextFormat(fontname.c_str(), NULL, DWRITE_FONT_WEIGHT_NORMAL,
		DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 10.0f, AY_OBFUSCATE(L"en-us"), &w_format);
}

void mainLoop() {
	ShowWindow(overlayWindow, 1);
	UpdateWindow(overlayWindow);
	SetLayeredWindowAttributes(overlayWindow, RGB(0, 0, 0), 255, LWA_ALPHA);

	MSG message;
	while (true) {
		while (PeekMessage(&message, overlayWindow, NULL, NULL, PM_REMOVE)) {
			TranslateMessage(&message);
			DispatchMessage(&message);
			if (message.message == WM_QUIT) return;
		}

		WINDOWINFO info;
		ZeroMemory(&info, sizeof(info));
		info.cbSize = sizeof(info);
		GetWindowInfo(targetWindow, &info);
		D2D1_SIZE_U siz;
		siz.height = ((info.rcClient.bottom) - (info.rcClient.top));
		siz.width = ((info.rcClient.right) - (info.rcClient.left));
		if (!IsIconic(overlayWindow)) {
			SetWindowPos(overlayWindow, NULL, info.rcClient.left, info.rcClient.top, siz.width, siz.height, SWP_SHOWWINDOW | SWP_NOCOPYBITS | SWP_ASYNCWINDOWPOS);
			target->Resize(&siz);
		}
		target->BeginDraw();
		target->Clear(D2D1::ColorF(0, 0, 0, 0));
		if (drawLoopCallback != NULL) {
			if (o_Foreground) {
				if (GetForegroundWindow() == targetWindow)
					goto toDraw;
				else goto noDraw;
			}

		toDraw:
			drawLoopCallback(siz.width, siz.height);
		}
	noDraw:
		target->EndDraw();
		Sleep(1);
	}
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uiMessage, WPARAM wParam, LPARAM lParam)
{
	switch (uiMessage)
	{
	case WM_CLOSE:
		DestroyWindow(hWnd);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, uiMessage, wParam, lParam);
	}
	return 0;
}

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
	DWORD lpdwProcessId;
	GetWindowThreadProcessId(hwnd, &lpdwProcessId);
	if (lpdwProcessId == GetCurrentProcessId())
	{
		enumWindow = hwnd;
		return FALSE;
	}
	return TRUE;
}

DWORD WINAPI OverlayThread(LPVOID lpParam)
{
	if (lpParam == NULL) {
		EnumWindows(EnumWindowsProc, NULL);
	}
	else {
		enumWindow = (HWND)lpParam;
	}
	d2oSetup(enumWindow);
	for (;;) {
		mainLoop();
	}
}

void DirectOverlaySetup(DirectOverlayCallback callback) {
	drawLoopCallback = callback;
	CreateThread(0, 0, OverlayThread, NULL, 0, NULL);
}

void DirectOverlaySetup(DirectOverlayCallback callback, HWND _targetWindow) {
	drawLoopCallback = callback;
	CreateDetachedThread([=]() { OverlayThread((LPVOID)_targetWindow); });
	// CreateThread(0, 0, OverlayThread, _targetWindow, 0, NULL);
}

void DirectOverlaySetOption(DWORD option) {
	if (option & D2DOV_REQUIRE_FOREGROUND) o_Foreground = true;
	if (option & D2DOV_DRAW_FPS) o_DrawFPS = true;
	if (option & D2DOV_VSYNC) o_VSync = true;
	if (option & D2DOV_FONT_ARIAL) fontname = AY_OBFUSCATE(L"arial");
	if (option & D2DOV_FONT_COURIER) fontname = AY_OBFUSCATE(L"Courier");
	if (option & D2DOV_FONT_CALIBRI) fontname = AY_OBFUSCATE(L"Calibri");
	if (option & D2DOV_FONT_GABRIOLA) fontname = AY_OBFUSCATE(L"Gabriola");
	if (option & D2DOV_FONT_IMPACT) fontname = AY_OBFUSCATE(L"Impact");
}
