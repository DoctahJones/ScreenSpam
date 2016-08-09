#include <windows.h>
#include <tchar.h>

// The main window class name.
static TCHAR szWindowClass[] = _T("Window");
// The string that appears in the application's title bar.
static TCHAR windowTitle[] = _T("Title");

//Text being displayed on screen.
LPWSTR screenText;

int currentCharPos = -1;
int screenHorizontal = 0, screenVertical = 0;

//Methods declared
void GetDesktopResolution(int& horizontal, int& vertical);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	LPWSTR *args;
	int argCount;

	args = CommandLineToArgvW(GetCommandLine(), &argCount);
	if (argCount == 1)
	{
		MessageBox(NULL, L"Usage - ScreenSpam [message]", L"Error", MB_OK);
		return 10;
	}

	screenText = args[1];

	GetDesktopResolution(screenHorizontal, screenVertical);

	//Set up window info.
	WNDCLASSEX wcex;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SHIELD));
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = NULL;
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SHIELD));

	RegisterClassEx(&wcex);

	HWND hWnd = CreateWindow(szWindowClass, windowTitle, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
		screenHorizontal, screenVertical, NULL, NULL, hInstance, NULL);

	//Change window behaviour/style
	// WS_EX_LAYERED - enable layering to hide window parts with alpha. 
	// WS_EX_TOOLWINDOW - prevents taskbar appearing for application
	SetWindowLong(hWnd, GWL_EXSTYLE, GetWindowLong(hWnd, GWL_EXSTYLE) | WS_EX_LAYERED | WS_EX_TOOLWINDOW);

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	//Timer to update letter being shown.
	SetTimer(hWnd, NULL, 1000, NULL);

	// Main message loop:
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	LocalFree(args);
	return (int)msg.wParam;
}

void GetDesktopResolution(int& horizontal, int& vertical)
{
	RECT desktop;
	const HWND hDesktop = GetDesktopWindow();
	GetWindowRect(hDesktop, &desktop);
	horizontal = desktop.right;
	vertical = desktop.bottom;
}

BYTE PremultiplyAlpha(BYTE source, BYTE alpha) {
	return (BYTE)(float(source) * float(alpha) / (float)255 + 0.5f);
}

//COLORREF = 0x00BGR
HBITMAP CreateTextBitmap(LPCWSTR text, HFONT font, COLORREF colour, RECT &textRect)
{
	int textLength = (int)wcslen(text);
	if (textLength <= 0) return NULL;

	// Create DC and select font into it 
	HDC hTextDC = CreateCompatibleDC(NULL);
	HFONT hOldFont = (HFONT)SelectObject(hTextDC, font);
	HBITMAP bitmap = NULL;

	// Calculates the rectangle where the text will be drawn.
	RECT textArea = { 0, 0, 0, 0 };
	DrawText(hTextDC, text, textLength, &textArea, DT_CALCRECT);
	textRect = textArea;
	if ((textArea.right > textArea.left) && (textArea.bottom > textArea.top))
	{
		BITMAPINFOHEADER bitmapHeader;
		memset(&bitmapHeader, 0x0, sizeof(BITMAPINFOHEADER));
		//A pointer to the location of the DIB bit values, the actual rgba bytes.
		void *bitValues = NULL;

		// Specify DIB setup 
		bitmapHeader.biSize = sizeof(bitmapHeader);
		bitmapHeader.biWidth = textArea.right - textArea.left;
		bitmapHeader.biHeight = textArea.bottom - textArea.top;
		bitmapHeader.biPlanes = 1;
		bitmapHeader.biBitCount = 32;
		bitmapHeader.biCompression = BI_RGB;

		// Create and select DIB into DC 
		bitmap = CreateDIBSection(hTextDC, (LPBITMAPINFO)&bitmapHeader, 0, (LPVOID*)&bitValues, NULL, 0);
		HBITMAP hOldBitmap = (HBITMAP)SelectObject(hTextDC, bitmap);
		if (hOldBitmap != NULL)
		{
			// Set up DC properties 
			SetTextColor(hTextDC, 0x00FFFFFF);
			SetBkColor(hTextDC, 0x00000000);
			SetBkMode(hTextDC, OPAQUE);

			// Draw text to buffer 
			DrawText(hTextDC, text, textLength, &textArea, DT_NOCLIP);

			//color the text and add modify by alpha
			BYTE* currentData = (BYTE*)bitValues;
			BYTE colorR = GetRValue(colour);
			BYTE colorG = GetGValue(colour);
			BYTE colorB = GetBValue(colour);
			BYTE alphaOfThisPoint;
			for (int y = 0; y < bitmapHeader.biHeight; y++) {
				for (int x = 0; x < bitmapHeader.biWidth; x++) {
					alphaOfThisPoint = *currentData;

					*currentData++ = PremultiplyAlpha(colorB, alphaOfThisPoint);
					*currentData++ = PremultiplyAlpha(colorG, alphaOfThisPoint);
					*currentData++ = PremultiplyAlpha(colorR, alphaOfThisPoint);
					*currentData++ = alphaOfThisPoint;
				}
			}
			// De-select bitmap 
			SelectObject(hTextDC, hOldBitmap);
		}
	}

	// De-select font and destroy temp DC 
	SelectObject(hTextDC, hOldFont);
	DeleteDC(hTextDC);

	// Return DIBSection 
	return bitmap;
}


void DrawTextWithAlpha(HWND hWnd, HDC inDC)
{
	int textLength = (int)wcslen(screenText);
	currentCharPos = ++currentCharPos % textLength;
	TCHAR current[] = { screenText[currentCharPos], screenText[textLength] };

	RECT textArea = { 0, 0, 0, 0 };
	HFONT font = CreateFont(screenVertical, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ANTIALIASED_QUALITY, 0, _T("Arial\0"));
	HBITMAP textBitmap = CreateTextBitmap(current, font, 0x000000FF, textArea);
	DeleteObject(font);

	if (textBitmap)
	{
		//Create temporary DC and select new Bitmap into it 
		HDC hTempDC = CreateCompatibleDC(inDC);
		HBITMAP hOldBitmap = (HBITMAP)SelectObject(hTempDC, textBitmap);
		if (hOldBitmap)
		{
			//Get info about created bitmap
			BITMAP bitmapInfo;
			GetObject(textBitmap, sizeof(BITMAP), &bitmapInfo);

			//Create blend function to blend textBitmap with window.
			BLENDFUNCTION bf;
			bf.BlendOp = AC_SRC_OVER;
			bf.BlendFlags = 0;
			bf.SourceConstantAlpha = 0xFF;
			bf.AlphaFormat = AC_SRC_ALPHA;

			POINT origin;
			origin.x = screenHorizontal / 2 - (textArea.right / 2);
			origin.y = screenVertical / 2 - (textArea.bottom / 2);

			SIZE bitmapSize = { bitmapInfo.bmWidth, bitmapInfo.bmHeight };
			POINT zero = { 0 };

			UpdateLayeredWindow(hWnd, inDC, &origin, &bitmapSize, hTempDC, &zero, RGB(0, 0, 0), &bf, ULW_ALPHA);

			//De-select temp DC and delete objects we have finished with.
			SelectObject(hTempDC, hOldBitmap);
			DeleteObject(textBitmap);
			DeleteDC(hTempDC);
		}
	}
}

void DrawTextToScreen(HWND hWnd)
{
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(hWnd, &ps);
	DrawTextWithAlpha(hWnd, hdc);
	EndPaint(hWnd, &ps);
	::SetForegroundWindow(hWnd);
}

//  WM_PAINT    - Called to repaint window on resize etc.
//	WM_TIMER	- Called when timer ticks.
//  WM_DESTROY  - Called on exit.
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_PAINT:
		DrawTextToScreen(hWnd);
		break;
	case WM_TIMER:
		DrawTextToScreen(hWnd);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
		break;
	}
	return 0;
}