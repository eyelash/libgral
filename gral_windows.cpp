/*

Copyright (c) 2016-2024 Elias Aebi

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#include "gral.h"
#define UNICODE
#define _UNICODE
#include <Windows.h>
#include <windowsx.h>
#include <strsafe.h>
#include <d2d1.h>
#include <wincodec.h>
#include <dwrite.h>
#include <stdlib.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <mfapi.h>
#include <mferror.h>
#include <mftransform.h>
#include <wmcodecdsp.h>

template <class T> class ComPointer {
	T *pointer;
public:
	ComPointer(): pointer(NULL) {}
	ComPointer(ComPointer const &com_pointer): pointer(com_pointer.pointer) {
		if (pointer) {
			pointer->AddRef();
		}
	}
	~ComPointer() {
		if (pointer) {
			pointer->Release();
		}
	}
	ComPointer &operator =(ComPointer const &com_pointer) {
		if (pointer) {
			pointer->Release();
		}
		pointer = com_pointer.pointer;
		if (pointer) {
			pointer->AddRef();
		}
		return *this;
	}
	T **operator &() {
		return &pointer;
	}
	operator T *() const {
		return pointer;
	}
	T *operator ->() const {
		return pointer;
	}
};

template <class T> class Buffer {
	size_t length;
	T *data;
public:
	Buffer(size_t length): length(length), data(new T[length]) {}
	Buffer(Buffer const &buffer): length(buffer.length), data(new T[length]) {
		for (size_t i = 0; i < length; i++) {
			data[i] = buffer.data[i];
		}
	}
	~Buffer() {
		delete[] data;
	}
	Buffer &operator =(Buffer const &buffer) {
		if (buffer.length != length) {
			delete[] data;
			length = buffer.length;
			data = new T[length];
		}
		for (size_t i = 0; i < length; i++) {
			data[i] = buffer.data[i];
		}
		return *this;
	}
	size_t get_length() const {
		return length;
	}
	operator T *() const {
		return data;
	}
};

static Buffer<wchar_t> utf8_to_utf16(char const *utf8) {
	int length = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
	Buffer<wchar_t> utf16(length);
	MultiByteToWideChar(CP_UTF8, 0, utf8, -1, utf16, length);
	return utf16;
}

static Buffer<char> utf16_to_utf8(wchar_t const *utf16) {
	int length = WideCharToMultiByte(CP_UTF8, 0, utf16, -1, NULL, 0, NULL, NULL);
	Buffer<char> utf8(length);
	WideCharToMultiByte(CP_UTF8, 0, utf16, -1, utf8, length, NULL, NULL);
	return utf8;
}

static UINT32 get_next_code_point(wchar_t const *utf16, UINT32 i, UINT32 *code_point) {
	if ((utf16[i] & 0xFC00) == 0xD800) {
		*code_point = (((utf16[i] & 0x03FF) << 10) | (utf16[i+1] & 0x03FF)) + 0x10000;
		return 2;
	}
	else {
		*code_point = utf16[i];
		return 1;
	}
}

static UINT32 utf8_index_to_utf16(wchar_t const *utf16, int index) {
	int i8 = 0;
	UINT32 i16 = 0;
	while (i8 < index) {
		UINT32 c; // UTF-32 code point
		i16 += get_next_code_point(utf16, i16, &c);
		if (c <= 0x7F) i8 += 1;
		else if (c <= 0x7FF) i8 += 2;
		else if (c <= 0xFFFF) i8 += 3;
		else i8 += 4;
	}
	return i16;
}

static int utf16_index_to_utf8(wchar_t const *utf16, UINT32 index) {
	int i8 = 0;
	UINT32 i16 = 0;
	while (i16 < index) {
		UINT32 c; // UTF-32 code point
		i16 += get_next_code_point(utf16, i16, &c);
		if (c <= 0x7F) i8 += 1;
		else if (c <= 0x7FF) i8 += 2;
		else if (c <= 0xFFFF) i8 += 3;
		else i8 += 4;
	}
	return i8;
}

static HINSTANCE hInstance;
static ID2D1Factory *factory;
static ID2D1StrokeStyle *stroke_style;
static IWICImagingFactory *imaging_factory;
static IDWriteFactory *dwrite_factory;
static DWORD main_thread_id;
static int window_count = 0;

struct gral_draw_context {
	ID2D1HwndRenderTarget *target;
	ID2D1PathGeometry *path;
	ID2D1GeometrySink *sink;
	bool open;
	gral_draw_context(): open(false) {}
};

struct gral_image {
	int width;
	int height;
	void *data;
	gral_image(int width, int height, void *data): width(width), height(height), data(data) {}
};

struct gral_text {
	ComPointer<IDWriteTextLayout> layout;
	Buffer<wchar_t> utf16;
	gral_text(Buffer<wchar_t> const &utf16): utf16(utf16) {}
};

static void adjust_window_size(int &width, int &height) {
	RECT rect;
	rect.left = 0;
	rect.top = 0;
	rect.right = width;
	rect.bottom = height;
	AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
	width = rect.right - rect.left;
	height = rect.bottom - rect.top;
}

struct WindowData {
	gral_window_interface const *iface;
	void *user_data;
	ID2D1HwndRenderTarget *target;
	bool mouse_inside;
	int minimum_width, minimum_height;
	HCURSOR cursor;
	bool is_pointer_locked;
	POINT locked_pointer;
	WindowData(): mouse_inside(false), minimum_width(0), minimum_height(0), is_pointer_locked(false) {}
};

struct gral_timer {
	void (*callback)(void *user_data);
	void *user_data;
	HANDLE timer;
};


/*================
    APPLICATION
 ================*/

struct gral_application {
	gral_application_interface const *iface;
	void *user_data;
};

static int get_key(UINT virtual_key, UINT scan_code) {
	switch (virtual_key) {
	case VK_RETURN:
		return GRAL_KEY_ENTER;
	case VK_TAB:
		return GRAL_KEY_TAB;
	case VK_BACK:
		return GRAL_KEY_BACKSPACE;
	case VK_DELETE:
		return GRAL_KEY_DELETE;
	case VK_LEFT:
		return GRAL_KEY_ARROW_LEFT;
	case VK_UP:
		return GRAL_KEY_ARROW_UP;
	case VK_RIGHT:
		return GRAL_KEY_ARROW_RIGHT;
	case VK_DOWN:
		return GRAL_KEY_ARROW_DOWN;
	case VK_PRIOR:
		return GRAL_KEY_PAGE_UP;
	case VK_NEXT:
		return GRAL_KEY_PAGE_DOWN;
	case VK_HOME:
		return GRAL_KEY_HOME;
	case VK_END:
		return GRAL_KEY_END;
	case VK_ESCAPE:
		return GRAL_KEY_ESCAPE;
	default:
		{
			BYTE keyboard_state[256];
			GetKeyboardState(keyboard_state);
			keyboard_state[VK_SHIFT] = 0;
			keyboard_state[VK_CONTROL] = 0;
			keyboard_state[VK_MENU] = 0;
			keyboard_state[VK_LSHIFT] = 0;
			keyboard_state[VK_RSHIFT] = 0;
			keyboard_state[VK_LCONTROL] = 0;
			keyboard_state[VK_RCONTROL] = 0;
			keyboard_state[VK_LMENU] = 0;
			keyboard_state[VK_RMENU] = 0;
			WCHAR utf16[5];
			if (ToUnicode(virtual_key, scan_code, keyboard_state, utf16, 5, 0) > 0) {
				UINT32 code_point;
				get_next_code_point(utf16, 0, &code_point);
				return code_point;
			}
			return 0;
		}
	}
}
int get_modifiers() {
	int modifiers = 0;
	if (GetKeyState(VK_CONTROL) < 0) modifiers |= GRAL_MODIFIER_CONTROL;
	if (GetKeyState(VK_MENU) < 0) modifiers |= GRAL_MODIFIER_ALT;
	if (GetKeyState(VK_SHIFT) < 0) modifiers |= GRAL_MODIFIER_SHIFT;
	return modifiers;
}

static LRESULT CALLBACK window_procedure(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	WindowData *window_data = (WindowData *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
	switch (uMsg) {
	case WM_PAINT:
		{
			if (window_data->target == NULL) {
				RECT rc;
				GetClientRect(hwnd, &rc);
				D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
				factory->CreateHwndRenderTarget(D2D1::RenderTargetProperties(), D2D1::HwndRenderTargetProperties(hwnd, size, D2D1_PRESENT_OPTIONS_RETAIN_CONTENTS), &window_data->target);
			}
			RECT update_rect;
			GetUpdateRect(hwnd, &update_rect, FALSE);
			gral_draw_context draw_context;
			draw_context.target = window_data->target;
			factory->CreatePathGeometry(&draw_context.path);
			draw_context.path->Open(&draw_context.sink);
			draw_context.sink->SetFillMode(D2D1_FILL_MODE_WINDING);
			draw_context.target->BeginDraw();
			draw_context.target->PushAxisAlignedClip(D2D1::RectF((FLOAT)update_rect.left, (FLOAT)update_rect.top, (FLOAT)update_rect.right, (FLOAT)update_rect.bottom), D2D1_ANTIALIAS_MODE_ALIASED);
			draw_context.target->Clear(D2D1::ColorF(D2D1::ColorF::White));
			window_data->iface->draw(&draw_context, update_rect.left, update_rect.top, update_rect.right - update_rect.left, update_rect.bottom - update_rect.top, window_data->user_data);
			draw_context.target->PopAxisAlignedClip();
			if (draw_context.target->EndDraw() == D2DERR_RECREATE_TARGET) {
				draw_context.target->Release();
				window_data->target = NULL;
			}
			draw_context.sink->Release();
			draw_context.path->Release();
			ValidateRect(hwnd, NULL);
			return 0;
		}
	case WM_MOUSEMOVE:
		{
			if (window_data->is_pointer_locked) {
				POINT point;
				GetCursorPos(&point);
				if (point.x != window_data->locked_pointer.x || point.y != window_data->locked_pointer.y) {
					window_data->iface->mouse_move_relative((float)(point.x - window_data->locked_pointer.x), (float)(point.y - window_data->locked_pointer.y), window_data->user_data);
					SetCursorPos(window_data->locked_pointer.x, window_data->locked_pointer.y);
				}
			}
			else {
				if (!window_data->mouse_inside) {
					SetCursor(window_data->cursor);
					window_data->mouse_inside = true;
					window_data->iface->mouse_enter(window_data->user_data);
					TRACKMOUSEEVENT track_mouse_event;
					track_mouse_event.cbSize = sizeof(TRACKMOUSEEVENT);
					track_mouse_event.dwFlags = TME_LEAVE;
					track_mouse_event.hwndTrack = hwnd;
					TrackMouseEvent(&track_mouse_event);
				}
				window_data->iface->mouse_move((float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam), window_data->user_data);
			}
			return 0;
		}
	case WM_MOUSELEAVE:
		{
			window_data->iface->mouse_leave(window_data->user_data);
			window_data->mouse_inside = false;
			return 0;
		}
	case WM_LBUTTONDOWN:
		{
			SetCapture(hwnd);
			window_data->iface->mouse_button_press((float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam), GRAL_PRIMARY_MOUSE_BUTTON, get_modifiers(), window_data->user_data);
			return 0;
		}
	case WM_MBUTTONDOWN:
		{
			SetCapture(hwnd);
			window_data->iface->mouse_button_press((float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam), GRAL_MIDDLE_MOUSE_BUTTON, get_modifiers(), window_data->user_data);
			return 0;
		}
	case WM_RBUTTONDOWN:
		{
			SetCapture(hwnd);
			window_data->iface->mouse_button_press((float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam), GRAL_SECONDARY_MOUSE_BUTTON, get_modifiers(), window_data->user_data);
			return 0;
		}
	case WM_LBUTTONUP:
		{
			if ((wParam & (MK_LBUTTON | MK_MBUTTON | MK_RBUTTON)) == 0) {
				ReleaseCapture();
			}
			window_data->iface->mouse_button_release((float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam), GRAL_PRIMARY_MOUSE_BUTTON, window_data->user_data);
			return 0;
		}
	case WM_MBUTTONUP:
		{
			if ((wParam & (MK_LBUTTON | MK_MBUTTON | MK_RBUTTON)) == 0) {
				ReleaseCapture();
			}
			window_data->iface->mouse_button_release((float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam), GRAL_MIDDLE_MOUSE_BUTTON, window_data->user_data);
			return 0;
		}
	case WM_RBUTTONUP:
		{
			if ((wParam & (MK_LBUTTON | MK_MBUTTON | MK_RBUTTON)) == 0) {
				ReleaseCapture();
			}
			window_data->iface->mouse_button_release((float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam), GRAL_SECONDARY_MOUSE_BUTTON, window_data->user_data);
			return 0;
		}
	case WM_LBUTTONDBLCLK:
		{
			SetCapture(hwnd);
			int modifiers = get_modifiers();
			window_data->iface->mouse_button_press((float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam), GRAL_PRIMARY_MOUSE_BUTTON, modifiers, window_data->user_data);
			window_data->iface->double_click((float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam), GRAL_PRIMARY_MOUSE_BUTTON, modifiers, window_data->user_data);
			return 0;
		}
	case WM_MBUTTONDBLCLK:
		{
			SetCapture(hwnd);
			int modifiers = get_modifiers();
			window_data->iface->mouse_button_press((float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam), GRAL_MIDDLE_MOUSE_BUTTON, modifiers, window_data->user_data);
			window_data->iface->double_click((float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam), GRAL_MIDDLE_MOUSE_BUTTON, modifiers, window_data->user_data);
			return 0;
		}
	case WM_RBUTTONDBLCLK:
		{
			SetCapture(hwnd);
			int modifiers = get_modifiers();
			window_data->iface->mouse_button_press((float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam), GRAL_SECONDARY_MOUSE_BUTTON, modifiers, window_data->user_data);
			window_data->iface->double_click((float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam), GRAL_SECONDARY_MOUSE_BUTTON, modifiers, window_data->user_data);
			return 0;
		}
	case WM_MOUSEWHEEL:
		window_data->iface->scroll(0.0f, (float)GET_WHEEL_DELTA_WPARAM(wParam)/(float)WHEEL_DELTA, window_data->user_data);
		return 0;
	case WM_MOUSEHWHEEL:
		window_data->iface->scroll(-(float)GET_WHEEL_DELTA_WPARAM(wParam)/(float)WHEEL_DELTA, 0.0f, window_data->user_data);
		return 0;
	case WM_KEYDOWN:
		{
			UINT scan_code = (lParam >> 16) & 0xFF;
			int key = get_key((UINT)wParam, scan_code);
			if (key) {
				window_data->iface->key_press(key, scan_code, get_modifiers(), window_data->user_data);
			}
			return 0;
		}
	case WM_KEYUP:
		{
			UINT scan_code = (lParam >> 16) & 0xFF;
			int key = get_key((UINT)wParam, scan_code);
			if (key) {
				window_data->iface->key_release(key, scan_code, window_data->user_data);
			}
			return 0;
		}
	case WM_CHAR:
		{
			static WCHAR utf16[3];
			if ((wParam & 0xFC00) == 0xD800) {
				// high surrogate
				utf16[0] = (WCHAR)wParam;
			}
			else {
				if ((wParam & 0xFC00) == 0xDC00) {
					// low surrogate
					utf16[1] = (WCHAR)wParam;
					utf16[2] = 0;
				}
				else {
					utf16[0] = (WCHAR)wParam;
					utf16[1] = 0;
				}
				CHAR utf8[5];
				WideCharToMultiByte(CP_UTF8, 0, utf16, -1, utf8, 5, NULL, NULL);
				if ((UCHAR)utf8[0] > 0x1F) {
					window_data->iface->text(utf8, window_data->user_data);
				}
			}
			return 0;
		}
	case WM_SETFOCUS:
		window_data->iface->focus_enter(window_data->user_data);
		return 0;
	case WM_KILLFOCUS:
		window_data->iface->focus_leave(window_data->user_data);
		return 0;
	case WM_SIZE:
		{
			WORD width = LOWORD(lParam);
			WORD height = HIWORD(lParam);
			window_data->iface->resize(width, height, window_data->user_data);
			if (window_data->target) {
				window_data->target->Resize(D2D1::SizeU(width, height));
			}
			RedrawWindow(hwnd, NULL, NULL, RDW_ERASE|RDW_INVALIDATE);
			return DefWindowProc(hwnd, uMsg, wParam, lParam);
		}
	case WM_GETMINMAXINFO:
		{
			if (window_data) {
				MINMAXINFO *mmi = (MINMAXINFO *)lParam;
				mmi->ptMinTrackSize.x = window_data->minimum_width;
				mmi->ptMinTrackSize.y = window_data->minimum_height;
			}
			return 0;
		}
	case WM_CLOSE:
		{
			if (window_data->iface->close(window_data->user_data)) {
				DestroyWindow(hwnd);
			}
			return 0;
		}
	case WM_DESTROY:
		{
			window_data->iface->destroy(window_data->user_data);
			if (window_data->target) {
				window_data->target->Release();
			}
			delete window_data;
			window_count--;
			if (window_count == 0) {
				PostQuitMessage(0);
			}
			return 0;
		}
	default:
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
}

gral_application *gral_application_create(char const *id, gral_application_interface const *iface, void *user_data) {
	hInstance = GetModuleHandle(NULL);
	CoInitialize(NULL);
	D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &factory);
	factory->CreateStrokeStyle(D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_LINE_JOIN_ROUND), NULL, 0, &stroke_style);
	CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&imaging_factory));
	DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown **)&dwrite_factory);
	WNDCLASS window_class;
	window_class.style = CS_DBLCLKS;
	window_class.lpfnWndProc = &window_procedure;
	window_class.cbClsExtra = 0;
	window_class.cbWndExtra = 0;
	window_class.hInstance = hInstance;
	window_class.hIcon = NULL;
	window_class.hCursor = NULL;
	window_class.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	window_class.lpszMenuName = NULL;
	window_class.lpszClassName = L"gral_window";
	RegisterClass(&window_class);
	static gral_application application;
	application.iface = iface;
	application.user_data = user_data;
	return &application;
}

void gral_application_delete(gral_application *application) {
	factory->Release();
	stroke_style->Release();
	imaging_factory->Release();
	dwrite_factory->Release();
	CoUninitialize();
}

int gral_application_run(gral_application *application, int argc_, char **argv_) {
	main_thread_id = GetCurrentThreadId();
	application->iface->start(application->user_data);
	int argc;
	LPWSTR *argv = CommandLineToArgvW(GetCommandLine(), &argc);
	if (argc > 1) {
		for (int i = 1; i < argc; i++) {
			application->iface->open_file(utf16_to_utf8(argv[i]), application->user_data);
		}
	}
	else {
		application->iface->open_empty(application->user_data);
	}
	LocalFree(argv);
	MSG message;
	while (TRUE) {
		while (PeekMessage(&message, NULL, 0, 0, PM_REMOVE)) {
			if (message.message == WM_QUIT) {
				application->iface->quit(application->user_data);
				return 0;
			}
			TranslateMessage(&message);
			DispatchMessage(&message);
		}
		MsgWaitForMultipleObjectsEx(0, NULL, INFINITE, QS_ALLINPUT, MWMO_ALERTABLE);
	}
}


/*============
    DRAWING
 ============*/

class ColorDrawingEffect: public IUnknown {
	ULONG reference_count;
	D2D1_COLOR_F color;
public:
	ColorDrawingEffect(D2D1_COLOR_F const &color): reference_count(1), color(color) {}
	D2D1_COLOR_F const &get_color() const {
		return color;
	}
	IFACEMETHOD(QueryInterface)(REFIID riid, void **ppvObject) {
		if (riid == __uuidof(IUnknown)) {
			*ppvObject = this;
			AddRef();
			return S_OK;
		}
		else {
			*ppvObject = NULL;
			return E_NOINTERFACE;
		}
	}
	IFACEMETHOD_(ULONG, AddRef)() {
		return InterlockedIncrement(&reference_count);
	}
    IFACEMETHOD_(ULONG, Release)() {
		ULONG new_reference_count = InterlockedDecrement(&reference_count);
		if (new_reference_count == 0) {
			delete this;
		}
		return new_reference_count;
	}
};

class GralTextRenderer: public IDWriteTextRenderer {
	ULONG reference_count;
	ComPointer<ID2D1SolidColorBrush> brush;
public:
	GralTextRenderer(ComPointer<ID2D1SolidColorBrush> const &brush): reference_count(1), brush(brush) {}
	IFACEMETHOD(DrawGlyphRun)(void *clientDrawingContext, FLOAT baselineOriginX, FLOAT baselineOriginY, DWRITE_MEASURING_MODE measuringMode, DWRITE_GLYPH_RUN const *glyphRun, DWRITE_GLYPH_RUN_DESCRIPTION const *glyphRunDescription, IUnknown *clientDrawingEffect) {
		gral_draw_context *draw_context = (gral_draw_context *)clientDrawingContext;
		if (brush) {
			if (clientDrawingEffect) {
				ColorDrawingEffect *color_drawing_effect = (ColorDrawingEffect *)clientDrawingEffect;
				ComPointer<ID2D1SolidColorBrush> effect_brush;
				draw_context->target->CreateSolidColorBrush(color_drawing_effect->get_color(), &effect_brush);
				draw_context->target->DrawGlyphRun(D2D1::Point2F(baselineOriginX, baselineOriginY), glyphRun, effect_brush, measuringMode);
			}
			else {
				draw_context->target->DrawGlyphRun(D2D1::Point2F(baselineOriginX, baselineOriginY), glyphRun, brush, measuringMode);
			}
		}
		else {
			ComPointer<ID2D1PathGeometry> path;
			factory->CreatePathGeometry(&path);
			ComPointer<ID2D1GeometrySink> sink;
			path->Open(&sink);
			glyphRun->fontFace->GetGlyphRunOutline(glyphRun->fontEmSize, glyphRun->glyphIndices, glyphRun->glyphAdvances, glyphRun->glyphOffsets, glyphRun->glyphCount, glyphRun->isSideways, glyphRun->bidiLevel % 2, sink);
			sink->Close();
			if (draw_context->open) {
				draw_context->sink->EndFigure(D2D1_FIGURE_END_OPEN);
				draw_context->open = false;
			}
			path->Outline(D2D1::Matrix3x2F(1.0f, 0.0f, 0.0f, 1.0f, baselineOriginX, baselineOriginY), draw_context->sink);
			draw_context->sink->SetFillMode(D2D1_FILL_MODE_WINDING);
		}
		return S_OK;
	}
	IFACEMETHOD(DrawInlineObject)(void *clientDrawingContext, FLOAT originX, FLOAT originY, IDWriteInlineObject *inlineObject, BOOL isSideways, BOOL isRightToLeft, IUnknown *clientDrawingEffect) {
		// TODO: implement
		return S_OK;
	}
	IFACEMETHOD(DrawStrikethrough)(void *clientDrawingContext, FLOAT baselineOriginX, FLOAT baselineOriginY, DWRITE_STRIKETHROUGH const *strikethrough, IUnknown *clientDrawingEffect) {
		// TODO: implement
		return S_OK;
	}
	IFACEMETHOD(DrawUnderline)(void *clientDrawingContext, FLOAT baselineOriginX, FLOAT baselineOriginY, DWRITE_UNDERLINE const *underline, IUnknown *clientDrawingEffect) {
		// TODO: implement
		return S_OK;
	}
	IFACEMETHOD(IsPixelSnappingDisabled)(void *clientDrawingContext, BOOL *isDisabled) {
		*isDisabled = FALSE;
		return S_OK;
	}
	IFACEMETHOD(GetCurrentTransform)(void *clientDrawingContext, DWRITE_MATRIX *transform) {
		gral_draw_context *draw_context = (gral_draw_context *)clientDrawingContext;
		draw_context->target->GetTransform((D2D1_MATRIX_3X2_F *)transform);
		return S_OK;
	}
	IFACEMETHOD(GetPixelsPerDip)(void *clientDrawingContext, FLOAT *pixelsPerDip) {
		*pixelsPerDip = 1.0f;
		return S_OK;
	}
	IFACEMETHOD(QueryInterface)(REFIID riid, void **ppvObject) {
		if (riid == __uuidof(IDWriteTextRenderer) || riid == __uuidof(IDWritePixelSnapping) || riid == __uuidof(IUnknown)) {
			*ppvObject = this;
			AddRef();
			return S_OK;
		}
		else {
			*ppvObject = NULL;
			return E_NOINTERFACE;
		}
	}
	IFACEMETHOD_(ULONG, AddRef)() {
		return InterlockedIncrement(&reference_count);
	}
    IFACEMETHOD_(ULONG, Release)() {
		ULONG new_reference_count = InterlockedDecrement(&reference_count);
		if (new_reference_count == 0) {
			delete this;
		}
		return new_reference_count;
	}
};

gral_image *gral_image_create(int width, int height, void *data) {
	return new gral_image(width, height, data);
}

void gral_image_delete(gral_image *image) {
	free(image->data);
	delete image;
}

static gral_font *create_font(WCHAR const *name, float size) {
	WCHAR locale_name[LOCALE_NAME_MAX_LENGTH];
	GetUserDefaultLocaleName(locale_name, LOCALE_NAME_MAX_LENGTH);
	IDWriteTextFormat *format;
	dwrite_factory->CreateTextFormat(name, NULL, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, locale_name, &format);
	return (gral_font *)format;
}

gral_font *gral_font_create(gral_window *window, char const *name, float size) {
	return create_font(utf8_to_utf16(name), size);
}

gral_font *gral_font_create_default(gral_window *window, float size) {
	NONCLIENTMETRICS ncm;
	ncm.cbSize = sizeof(NONCLIENTMETRICS);
	SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncm, 0);
	return create_font(ncm.lfMessageFont.lfFaceName, size);
}

gral_font *gral_font_create_monospace(gral_window *window, float size) {
	return create_font(L"Consolas", size);
}

void gral_font_delete(gral_font *font) {
	IDWriteTextFormat *format = (IDWriteTextFormat *)font;
	format->Release();
}

void gral_font_get_metrics(gral_window *window, gral_font *font, float *ascent, float *descent) {
	IDWriteTextFormat *format = (IDWriteTextFormat *)font;
	UINT32 name_length = format->GetFontFamilyNameLength();
	Buffer<WCHAR> name(name_length + 1);
	format->GetFontFamilyName(name, name_length + 1);
	ComPointer<IDWriteFontCollection> font_collection;
	format->GetFontCollection(&font_collection);
	UINT32 index;
	BOOL exists;
	font_collection->FindFamilyName(name, &index, &exists);
	ComPointer<IDWriteFontFamily> font_family;
	font_collection->GetFontFamily(index, &font_family);
	ComPointer<IDWriteFont> dwrite_font;
	font_family->GetFirstMatchingFont(DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &dwrite_font);
	DWRITE_FONT_METRICS metrics;
	dwrite_font->GetMetrics(&metrics);
	if (ascent) *ascent = (float)metrics.ascent / (float)metrics.designUnitsPerEm * format->GetFontSize();
	if (descent) *descent = (float)metrics.descent / (float)metrics.designUnitsPerEm * format->GetFontSize();
}

gral_text *gral_text_create(gral_window *window, char const *utf8, gral_font *font) {
	IDWriteTextFormat *format = (IDWriteTextFormat *)font;
	gral_text *text = new gral_text(utf8_to_utf16(utf8));
	dwrite_factory->CreateTextLayout(text->utf16, (UINT32)text->utf16.get_length(), format, 1.0e30f, 1.0e30f, &text->layout);
	return text;
}

void gral_text_delete(gral_text *text) {
	delete text;
}

void gral_text_set_bold(gral_text *text, int start_index, int end_index) {
	DWRITE_TEXT_RANGE range;
	range.startPosition = utf8_index_to_utf16(text->utf16, start_index);
	range.length = utf8_index_to_utf16(text->utf16, end_index) - range.startPosition;
	text->layout->SetFontWeight(DWRITE_FONT_WEIGHT_BOLD, range);
}

void gral_text_set_italic(gral_text *text, int start_index, int end_index) {
	DWRITE_TEXT_RANGE range;
	range.startPosition = utf8_index_to_utf16(text->utf16, start_index);
	range.length = utf8_index_to_utf16(text->utf16, end_index) - range.startPosition;
	text->layout->SetFontStyle(DWRITE_FONT_STYLE_ITALIC, range);
}

void gral_text_set_color(gral_text *text, int start_index, int end_index, float red, float green, float blue, float alpha) {
	DWRITE_TEXT_RANGE range;
	range.startPosition = utf8_index_to_utf16(text->utf16, start_index);
	range.length = utf8_index_to_utf16(text->utf16, end_index) - range.startPosition;
	ComPointer<ColorDrawingEffect> drawing_effect;
	*&drawing_effect = new ColorDrawingEffect(D2D1::ColorF(red, green, blue, alpha));
	text->layout->SetDrawingEffect(drawing_effect, range);
}

float gral_text_get_width(gral_text *text, gral_draw_context *draw_context) {
	DWRITE_TEXT_METRICS metrics;
	text->layout->GetMetrics(&metrics);
	return metrics.width;
}

float gral_text_index_to_x(gral_text *text, int index) {
	float x, y;
	DWRITE_HIT_TEST_METRICS metrics;
	text->layout->HitTestTextPosition(utf8_index_to_utf16(text->utf16, index), false, &x, &y, &metrics);
	return x;
}

int gral_text_x_to_index(gral_text *text, float x) {
	BOOL trailing, inside;
	DWRITE_HIT_TEST_METRICS metrics;
	text->layout->HitTestPoint(x, 0.0f, &trailing, &inside, &metrics);
	return utf16_index_to_utf8(text->utf16, inside && trailing ? metrics.textPosition + metrics.length : metrics.textPosition);
}

void gral_draw_context_draw_image(gral_draw_context *draw_context, gral_image *image, float x, float y) {
	ComPointer<IWICBitmap> source_bitmap;
	imaging_factory->CreateBitmapFromMemory(image->width, image->height, GUID_WICPixelFormat32bppRGBA, image->width * 4, image->width * image->height * 4, (PBYTE)image->data, &source_bitmap);
	ComPointer<IWICFormatConverter> format_converter;
	imaging_factory->CreateFormatConverter(&format_converter);
	format_converter->Initialize(source_bitmap, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom);
	ComPointer<ID2D1Bitmap> bitmap;
	draw_context->target->CreateBitmapFromWicBitmap(format_converter, &bitmap);
	draw_context->target->DrawBitmap(bitmap, D2D1::RectF(x, y, x + image->width, y + image->height), 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR, D2D1::RectF(0.0f, 0.0f, (FLOAT)image->width, (FLOAT)image->height));
}

void gral_draw_context_draw_text(gral_draw_context *draw_context, gral_text *text, float x, float y, float red, float green, float blue, float alpha) {
	DWRITE_LINE_METRICS line_metrics;
	UINT32 count = 1;
	text->layout->GetLineMetrics(&line_metrics, count, &count);
	ComPointer<ID2D1SolidColorBrush> brush;
	draw_context->target->CreateSolidColorBrush(D2D1::ColorF(red, green, blue, alpha), &brush);
	ComPointer<GralTextRenderer> renderer;
	*&renderer = new GralTextRenderer(brush);
	text->layout->Draw(draw_context, renderer, x, y-line_metrics.baseline);
}

void gral_draw_context_add_text(gral_draw_context *draw_context, gral_text *text, float x, float y) {
	DWRITE_LINE_METRICS line_metrics;
	UINT32 count = 1;
	text->layout->GetLineMetrics(&line_metrics, count, &count);
	ComPointer<ID2D1SolidColorBrush> brush;
	ComPointer<GralTextRenderer> renderer;
	*&renderer = new GralTextRenderer(brush);
	text->layout->Draw(draw_context, renderer, x, y-line_metrics.baseline);
}

void gral_draw_context_close_path(gral_draw_context *draw_context) {
	draw_context->sink->EndFigure(D2D1_FIGURE_END_CLOSED);
	draw_context->open = false;
}

void gral_draw_context_move_to(gral_draw_context *draw_context, float x, float y) {
	D2D1_POINT_2F point = D2D1::Point2F(x, y);
	if (draw_context->open) {
		draw_context->sink->EndFigure(D2D1_FIGURE_END_OPEN);
	}
	draw_context->sink->BeginFigure(point, D2D1_FIGURE_BEGIN_FILLED);
	draw_context->open = true;
}

void gral_draw_context_line_to(gral_draw_context *draw_context, float x, float y) {
	D2D1_POINT_2F point = D2D1::Point2F(x, y);
	draw_context->sink->AddLine(point);
}

void gral_draw_context_curve_to(gral_draw_context *draw_context, float x1, float y1, float x2, float y2, float x, float y) {
	D2D1_POINT_2F point = D2D1::Point2F(x, y);
	draw_context->sink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(x1, y1), D2D1::Point2F(x2, y2), point));
}

static ComPointer<ID2D1LinearGradientBrush> create_linear_gradient_brush(gral_draw_context *draw_context, float start_x, float start_y, float end_x, float end_y, gral_gradient_stop const *stops, int count) {
	Buffer<D2D1_GRADIENT_STOP> gradient_stops(count);
	for (int i = 0; i < count; i++) {
		gradient_stops[i].position = stops[i].position;
		gradient_stops[i].color = D2D1::ColorF(stops[i].red, stops[i].green, stops[i].blue, stops[i].alpha);
	}
	ComPointer<ID2D1GradientStopCollection> gradient_stop_collection;
	draw_context->target->CreateGradientStopCollection(gradient_stops, count, &gradient_stop_collection);
	ComPointer<ID2D1LinearGradientBrush> brush;
	draw_context->target->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(D2D1::Point2F(start_x, start_y), D2D1::Point2F(end_x, end_y)), gradient_stop_collection, &brush);
	return brush;
}

void gral_draw_context_fill(gral_draw_context *draw_context, float red, float green, float blue, float alpha) {
	if (draw_context->open) {
		draw_context->sink->EndFigure(D2D1_FIGURE_END_OPEN);
		draw_context->open = false;
	}
	draw_context->sink->Close();

	ComPointer<ID2D1SolidColorBrush> brush;
	draw_context->target->CreateSolidColorBrush(D2D1::ColorF(red, green, blue, alpha), &brush);
	draw_context->target->FillGeometry(draw_context->path, brush);

	draw_context->sink->Release();
	draw_context->path->Release();
	factory->CreatePathGeometry(&draw_context->path);
	draw_context->path->Open(&draw_context->sink);
	draw_context->sink->SetFillMode(D2D1_FILL_MODE_WINDING);
}

void gral_draw_context_fill_linear_gradient(gral_draw_context *draw_context, float start_x, float start_y, float end_x, float end_y, gral_gradient_stop const *stops, int count) {
	if (draw_context->open) {
		draw_context->sink->EndFigure(D2D1_FIGURE_END_OPEN);
		draw_context->open = false;
	}
	draw_context->sink->Close();

	ComPointer<ID2D1LinearGradientBrush> brush = create_linear_gradient_brush(draw_context, start_x, start_y, end_x, end_y, stops, count);
	draw_context->target->FillGeometry(draw_context->path, brush);

	draw_context->sink->Release();
	draw_context->path->Release();
	factory->CreatePathGeometry(&draw_context->path);
	draw_context->path->Open(&draw_context->sink);
	draw_context->sink->SetFillMode(D2D1_FILL_MODE_WINDING);
}

void gral_draw_context_stroke(gral_draw_context *draw_context, float line_width, float red, float green, float blue, float alpha) {
	if (draw_context->open) {
		draw_context->sink->EndFigure(D2D1_FIGURE_END_OPEN);
		draw_context->open = false;
	}
	draw_context->sink->Close();

	ComPointer<ID2D1SolidColorBrush> brush;
	draw_context->target->CreateSolidColorBrush(D2D1::ColorF(red, green, blue, alpha), &brush);
	draw_context->target->DrawGeometry(draw_context->path, brush, line_width, stroke_style);

	draw_context->sink->Release();
	draw_context->path->Release();
	factory->CreatePathGeometry(&draw_context->path);
	draw_context->path->Open(&draw_context->sink);
	draw_context->sink->SetFillMode(D2D1_FILL_MODE_WINDING);
}

void gral_draw_context_stroke_linear_gradient(gral_draw_context *draw_context, float line_width, float start_x, float start_y, float end_x, float end_y, gral_gradient_stop const *stops, int count) {
	if (draw_context->open) {
		draw_context->sink->EndFigure(D2D1_FIGURE_END_OPEN);
		draw_context->open = false;
	}
	draw_context->sink->Close();

	ComPointer<ID2D1LinearGradientBrush> brush = create_linear_gradient_brush(draw_context, start_x, start_y, end_x, end_y, stops, count);
	draw_context->target->DrawGeometry(draw_context->path, brush, line_width, stroke_style);

	draw_context->sink->Release();
	draw_context->path->Release();
	factory->CreatePathGeometry(&draw_context->path);
	draw_context->path->Open(&draw_context->sink);
	draw_context->sink->SetFillMode(D2D1_FILL_MODE_WINDING);
}

void gral_draw_context_draw_clipped(gral_draw_context *draw_context, void (*callback)(gral_draw_context *draw_context, void *user_data), void *user_data) {
	if (draw_context->open) {
		draw_context->sink->EndFigure(D2D1_FIGURE_END_OPEN);
		draw_context->open = false;
	}
	draw_context->sink->Close();

	ComPointer<ID2D1Layer> layer;
	draw_context->target->CreateLayer(&layer);
	draw_context->target->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(), draw_context->path), layer);

	draw_context->sink->Release();
	draw_context->path->Release();
	factory->CreatePathGeometry(&draw_context->path);
	draw_context->path->Open(&draw_context->sink);
	draw_context->sink->SetFillMode(D2D1_FILL_MODE_WINDING);

	callback(draw_context, user_data);

	draw_context->target->PopLayer();
}

void gral_draw_context_draw_transformed(gral_draw_context *draw_context, float a, float b, float c, float d, float e, float f, void (*callback)(gral_draw_context *draw_context, void *user_data), void *user_data) {
	D2D1_MATRIX_3X2_F matrix;
	draw_context->target->GetTransform(&matrix);
	draw_context->target->SetTransform(D2D1::Matrix3x2F(a, b, c, d, e, f) * matrix);
	callback(draw_context, user_data);
	draw_context->target->SetTransform(matrix);
}


/*===========
    WINDOW
 ===========*/

gral_window *gral_window_create(gral_application *application, int width, int height, char const *title, gral_window_interface const *iface, void *user_data) {
	adjust_window_size(width, height);
	HWND hwnd = CreateWindow(L"gral_window", utf8_to_utf16(title), WS_OVERLAPPEDWINDOW, 0, 0, width, height, NULL, NULL, hInstance, NULL);
	WindowData *window_data = new WindowData();
	window_data->iface = iface;
	window_data->user_data = user_data;
	window_data->target = NULL;
	window_data->cursor = LoadCursor(NULL, IDC_ARROW);
	SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)window_data);
	window_count++;
	return (gral_window *)hwnd;
}

void gral_window_show(gral_window *window) {
	ShowWindow((HWND)window, SW_SHOW);
}

void gral_window_set_title(gral_window *window, char const *title) {
	SetWindowText((HWND)window, utf8_to_utf16(title));
}

void gral_window_request_redraw(gral_window *window, int x, int y, int width, int height) {
	RECT rect;
	rect.left = x;
	rect.top = y;
	rect.right = x + width;
	rect.bottom = y + height;
	InvalidateRect((HWND)window, &rect, FALSE);
}

void gral_window_set_minimum_size(gral_window *window, int minimum_width, int minimum_height) {
	WindowData *window_data = (WindowData *)GetWindowLongPtr((HWND)window, GWLP_USERDATA);
	adjust_window_size(minimum_width, minimum_height);
	window_data->minimum_width = minimum_width;
	window_data->minimum_height = minimum_height;
}

static HCURSOR get_cursor(int cursor) {
	switch (cursor) {
	case GRAL_CURSOR_DEFAULT:
		return LoadCursor(NULL, IDC_ARROW);
	case GRAL_CURSOR_HAND:
		return LoadCursor(NULL, IDC_HAND);
	case GRAL_CURSOR_TEXT:
		return LoadCursor(NULL, IDC_IBEAM);
	case GRAL_CURSOR_HORIZONTAL_ARROWS:
		return LoadCursor(NULL, IDC_SIZEWE);
	case GRAL_CURSOR_VERTICAL_ARROWS:
		return LoadCursor(NULL, IDC_SIZENS);
	case GRAL_CURSOR_NONE:
		return NULL;
	default:
		return NULL;
	}
}
void gral_window_set_cursor(gral_window *window, int cursor) {
	WindowData *window_data = (WindowData *)GetWindowLongPtr((HWND)window, GWLP_USERDATA);
	window_data->cursor = get_cursor(cursor);
	SetCursor(window_data->cursor);
}

void gral_window_hide_cursor(gral_window *window) {
	ShowCursor(FALSE);
}

void gral_window_show_cursor(gral_window *window) {
	ShowCursor(TRUE);
}

void gral_window_warp_cursor(gral_window *window, float x, float y) {
	POINT point;
	point.x = (LONG)x;
	point.y = (LONG)y;
	ClientToScreen((HWND)window, &point);
	SetCursorPos(point.x, point.y);
}

void gral_window_lock_pointer(gral_window *window) {
	WindowData *window_data = (WindowData *)GetWindowLongPtr((HWND)window, GWLP_USERDATA);
	GetCursorPos(&window_data->locked_pointer);
	window_data->is_pointer_locked = true;
}

void gral_window_unlock_pointer(gral_window *window) {
	WindowData *window_data = (WindowData *)GetWindowLongPtr((HWND)window, GWLP_USERDATA);
	window_data->is_pointer_locked = false;
}

void gral_window_show_open_file_dialog(gral_window *window, void (*callback)(char const *file, void *user_data), void *user_data) {
	WCHAR file_name[MAX_PATH];
	file_name[0] = '\0';
	OPENFILENAME ofn;
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = (HWND)window;
	ofn.lpstrFile = file_name;
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
	if (GetOpenFileName(&ofn)) {
		callback(utf16_to_utf8(file_name), user_data);
	}
}

void gral_window_show_save_file_dialog(gral_window *window, void (*callback)(char const *file, void *user_data), void *user_data) {
	WCHAR file_name[MAX_PATH];
	file_name[0] = '\0';
	OPENFILENAME ofn;
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = (HWND)window;
	ofn.lpstrFile = file_name;
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_OVERWRITEPROMPT;
	if (GetSaveFileName(&ofn)) {
		callback(utf16_to_utf8(file_name), user_data);
	}
}

void gral_window_clipboard_copy(gral_window *window, char const *text) {
	OpenClipboard((HWND)window);
	EmptyClipboard();
	int length = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
	HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE, length*sizeof(WCHAR));
	LPWSTR pointer = (LPWSTR)GlobalLock(handle);
	MultiByteToWideChar(CP_UTF8, 0, text, -1, pointer, length);
	GlobalUnlock(handle);
	SetClipboardData(CF_UNICODETEXT, handle);
	CloseClipboard();
}

void gral_window_clipboard_paste(gral_window *window, void (*callback)(char const *text, void *user_data), void *user_data) {
	OpenClipboard((HWND)window);
	if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
		HGLOBAL handle = GetClipboardData(CF_UNICODETEXT);
		LPWSTR text = (LPWSTR)GlobalLock(handle);
		callback(utf16_to_utf8(text), user_data);
		GlobalUnlock(handle);
	}
	CloseClipboard();
}

static void CALLBACK timer_completion_routine(LPVOID lpArgToCompletionRoutine, DWORD dwTimerLowValue, DWORD dwTimerHighValue) {
	gral_timer *timer = (gral_timer *)lpArgToCompletionRoutine;
	timer->callback(timer->user_data);
}

gral_timer *gral_timer_create(int milliseconds, void (*callback)(void *user_data), void *user_data) {
	gral_timer *timer = new gral_timer();
	timer->callback = callback;
	timer->user_data = user_data;
	timer->timer = CreateWaitableTimer(NULL, FALSE, NULL);
	LARGE_INTEGER due_time;
	due_time.QuadPart = (LONGLONG)milliseconds * -10000;
	SetWaitableTimer(timer->timer, &due_time, milliseconds, &timer_completion_routine, timer, FALSE);
	return timer;
}

void gral_timer_delete(gral_timer *timer) {
	CancelWaitableTimer(timer->timer);
	CloseHandle(timer->timer);
	delete timer;
}

struct MainThreadCallbackData {
	void (*callback)(void *user_data);
	void *user_data;
};

static void CALLBACK main_thread_completion_routine(ULONG_PTR parameter) {
	MainThreadCallbackData *callback_data = (MainThreadCallbackData *)parameter;
	callback_data->callback(callback_data->user_data);
	delete callback_data;
}

void gral_run_on_main_thread(void (*callback)(void *user_data), void *user_data) {
	MainThreadCallbackData *callback_data = new MainThreadCallbackData();
	callback_data->callback = callback;
	callback_data->user_data = user_data;
	HANDLE thread = OpenThread(THREAD_SET_CONTEXT, FALSE, main_thread_id);
	QueueUserAPC(&main_thread_completion_routine, thread, (ULONG_PTR)callback_data);
	CloseHandle(thread);
}


/*=========
    FILE
 =========*/

gral_file *gral_file_open_read(char const *path) {
	HANDLE handle = CreateFile(utf8_to_utf16(path), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	return handle == INVALID_HANDLE_VALUE ? NULL : (gral_file *)handle;
}

gral_file *gral_file_open_write(char const *path) {
	HANDLE handle = CreateFile(utf8_to_utf16(path), GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
	return handle == INVALID_HANDLE_VALUE ? NULL : (gral_file *)handle;
}

gral_file *gral_file_get_standard_input() {
	return (gral_file *)GetStdHandle(STD_INPUT_HANDLE);
}

gral_file *gral_file_get_standard_output() {
	return (gral_file *)GetStdHandle(STD_OUTPUT_HANDLE);
}

gral_file *gral_file_get_standard_error() {
	return (gral_file *)GetStdHandle(STD_ERROR_HANDLE);
}

void gral_file_close(gral_file *file) {
	CloseHandle(file);
}

size_t gral_file_read(gral_file *file, void *buffer, size_t size) {
	DWORD bytes_read;
	ReadFile(file, buffer, (DWORD)size, &bytes_read, NULL);
	return bytes_read;
}

void gral_file_write(gral_file *file, void const *buffer, size_t size) {
	DWORD bytes_written;
	WriteFile(file, buffer, (DWORD)size, &bytes_written, NULL);
}

size_t gral_file_get_size(gral_file *file) {
	LARGE_INTEGER size;
	GetFileSizeEx(file, &size);
	return size.QuadPart;
}

void *gral_file_map(gral_file *file, size_t size) {
	HANDLE mapping = CreateFileMapping(file, NULL, PAGE_READONLY, 0, 0, NULL);
	LPVOID address = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, size);
	CloseHandle(mapping);
	return address;
}

void gral_file_unmap(void *address, size_t size) {
	UnmapViewOfFile(address);
}

int gral_file_get_type(char const *path) {
	DWORD attributes = GetFileAttributes(utf8_to_utf16(path));
	if (attributes != INVALID_FILE_ATTRIBUTES) {
		if (attributes & FILE_ATTRIBUTE_DIRECTORY) return GRAL_FILE_TYPE_DIRECTORY;
		else return GRAL_FILE_TYPE_REGULAR;
	}
	return GRAL_FILE_TYPE_INVALID;
}

void gral_file_rename(char const *old_path, char const *new_path) {
	MoveFileEx(utf8_to_utf16(old_path), utf8_to_utf16(new_path), MOVEFILE_REPLACE_EXISTING);
}

void gral_file_remove(char const *path) {
	DeleteFile(utf8_to_utf16(path));
}

void gral_directory_create(char const *path) {
	CreateDirectory(utf8_to_utf16(path), NULL);
}

void gral_directory_iterate(char const *path_utf8, void (*callback)(char const *name, void *user_data), void *user_data) {
	int length = MultiByteToWideChar(CP_UTF8, 0, path_utf8, -1, NULL, 0);
	Buffer<wchar_t> path_utf16(length + 2);
	MultiByteToWideChar(CP_UTF8, 0, path_utf8, -1, path_utf16, length);
	StringCchCat(path_utf16, path_utf16.get_length(), L"\\*");
	WIN32_FIND_DATA find_data;
	HANDLE handle = FindFirstFile(path_utf16, &find_data);
	do {
		callback(utf16_to_utf8(find_data.cFileName), user_data);
	} while (FindNextFile(handle, &find_data));
	FindClose(handle);
}

void gral_directory_remove(char const *path) {
	RemoveDirectory(utf8_to_utf16(path));
}

char *gral_get_current_working_directory() {
	DWORD utf16_length = GetCurrentDirectory(0, NULL);
	Buffer<wchar_t> utf16_buffer(utf16_length);
	GetCurrentDirectory(utf16_length, utf16_buffer);
	int utf8_length = WideCharToMultiByte(CP_UTF8, 0, utf16_buffer, utf16_length, NULL, 0, NULL, NULL);
	char *utf8_buffer = (char *)malloc(utf8_length);
	WideCharToMultiByte(CP_UTF8, 0, utf16_buffer, utf16_length, utf8_buffer, utf8_length, NULL, NULL);
	return utf8_buffer;
}

struct gral_directory_watcher {
	HANDLE directory;
	OVERLAPPED overlapped;
	void (*callback)(void *user_data);
	void *user_data;
	char buffer[4096];
	static void CALLBACK completion_routine(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped) {
		gral_directory_watcher *watcher = (gral_directory_watcher *)lpOverlapped->hEvent;
		if (dwErrorCode == ERROR_OPERATION_ABORTED) {
			delete watcher;
			return;
		}
		DWORD offset = 0;
		while (TRUE) {
			PFILE_NOTIFY_INFORMATION information = (PFILE_NOTIFY_INFORMATION)(watcher->buffer + offset);
			if (information->Action != FILE_ACTION_RENAMED_OLD_NAME) {
				watcher->callback(watcher->user_data);
			}
			if (information->NextEntryOffset == 0) {
				break;
			}
			offset += information->NextEntryOffset;
		}
		watcher->watch();
	}
	gral_directory_watcher(char const *path, void (*callback)(void *user_data), void *user_data) {
		directory = CreateFile(utf8_to_utf16(path), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL);
		ZeroMemory(&overlapped, sizeof(overlapped));
		overlapped.hEvent = this;
		this->callback = callback;
		this->user_data = user_data;
	}
	~gral_directory_watcher() {
		CloseHandle(directory);
	}
	void watch() {
		DWORD const notify_filter = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_LAST_ACCESS | FILE_NOTIFY_CHANGE_CREATION | FILE_NOTIFY_CHANGE_SECURITY;
		ReadDirectoryChangesW(directory, buffer, sizeof(buffer), FALSE, notify_filter, NULL, &overlapped, &completion_routine);
	}
	void cancel() {
		CancelIoEx(directory, &overlapped);
	}
};

gral_directory_watcher *gral_directory_watch(char const *path, void (*callback)(void *user_data), void *user_data) {
	gral_directory_watcher *watcher = new gral_directory_watcher(path, callback, user_data);
	watcher->watch();
	return watcher;
}

void gral_directory_watcher_delete(gral_directory_watcher *watcher) {
	watcher->cancel();
}


/*=========
    TIME
 =========*/

static double get_frequency() {
	LARGE_INTEGER frequency;
	QueryPerformanceFrequency(&frequency);
	return (double)frequency.QuadPart;
}

double gral_time_get_monotonic() {
	static double const frequency = get_frequency();
	LARGE_INTEGER count;
	QueryPerformanceCounter(&count);
	return count.QuadPart / frequency;
}


/*==========
    AUDIO
 ==========*/

static HRESULT process_output(IMFTransform *transform, IMFSample *sample) {
	MFT_OUTPUT_DATA_BUFFER output_data_buffer;
	output_data_buffer.dwStreamID = 0;
	output_data_buffer.pSample = sample;
	output_data_buffer.dwStatus = 0;
	output_data_buffer.pEvents = NULL;
	DWORD status;
	HRESULT result = transform->ProcessOutput(0, 1, &output_data_buffer, &status);
	if (output_data_buffer.pEvents) {
		output_data_buffer.pEvents->Release();
	}
	return result;
}

static int fill_buffer(IMFTransform *resampler, BYTE *device_buffer, UINT32 device_buffer_size, int (*callback)(float *buffer, int frames, void *user_data), void *user_data) {
	while (device_buffer_size > 0) {
		ComPointer<IMFMediaBuffer> output_buffer;
		MFCreateMemoryBuffer(device_buffer_size, &output_buffer);
		ComPointer<IMFSample> output_sample;
		MFCreateSample(&output_sample);
		output_sample->AddBuffer(output_buffer);
		while (process_output(resampler, output_sample) == MF_E_TRANSFORM_NEED_MORE_INPUT) {
			ComPointer<IMFMediaBuffer> input_buffer;
			MFCreateMemoryBuffer(1024 * 2 * sizeof(float), &input_buffer);
			BYTE *input_pointer;
			input_buffer->Lock(&input_pointer, NULL, NULL);
			int actual_frames = callback((float *)input_pointer, 1024, user_data);
			input_buffer->Unlock();
			if (actual_frames == 0) {
				resampler->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, NULL);
				resampler->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, NULL);
				process_output(resampler, output_sample);
				BYTE *output_pointer;
				DWORD output_size;
				output_buffer->Lock(&output_pointer, NULL, &output_size);
				CopyMemory(device_buffer, output_pointer, output_size);
				output_buffer->Unlock();
				device_buffer += output_size;
				device_buffer_size -= output_size;
				ZeroMemory(device_buffer, device_buffer_size);
				return 0;
			}
			input_buffer->SetCurrentLength(actual_frames * 2 * sizeof(float));
			ComPointer<IMFSample> input_sample;
			MFCreateSample(&input_sample);
			input_sample->AddBuffer(input_buffer);
			resampler->ProcessInput(0, input_sample, 0);
		}
		BYTE *output_pointer;
		DWORD output_size;
		output_buffer->Lock(&output_pointer, NULL, &output_size);
		CopyMemory(device_buffer, output_pointer, output_size);
		output_buffer->Unlock();
		device_buffer += output_size;
		device_buffer_size -= output_size;
	}
	return 1;
}

static ComPointer<IMFTransform> create_resampler(WAVEFORMATEX *format) {
	ComPointer<IMFTransform> resampler;
	CoCreateInstance(__uuidof(CResamplerMediaObject), NULL, CLSCTX_ALL, __uuidof(IMFTransform), (void **)&resampler);
	ComPointer<IMFMediaType> input_type;
	MFCreateMediaType(&input_type);
	input_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
	input_type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float);
	input_type->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2);
	input_type->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 44100);
	input_type->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 32 / 8 * 2);
	input_type->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 32 / 8 * 2 * 44100);
	input_type->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 32);
	input_type->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
	resampler->SetInputType(0, input_type, 0);
	ComPointer<IMFMediaType> output_type;
	MFCreateMediaType(&output_type);
	MFInitMediaTypeFromWaveFormatEx(output_type, format, sizeof(WAVEFORMATEX) + format->cbSize);
	resampler->SetOutputType(0, output_type, 0);
	return resampler;
}

void gral_audio_play(int (*callback)(float *buffer, int frames, void *user_data), void *user_data) {
	ComPointer<IMMDeviceEnumerator> device_enumerator;
	CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void **)&device_enumerator);
	ComPointer<IMMDevice> device;
	device_enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &device);
	ComPointer<IAudioClient> audio_client;
	device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void **)&audio_client);
	WAVEFORMATEX *format;
	audio_client->GetMixFormat(&format);
	ComPointer<IMFTransform> resampler = create_resampler(format);
	audio_client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, 0, 0, format, NULL);
	UINT32 buffer_size;
	audio_client->GetBufferSize(&buffer_size);
	IAudioRenderClient *render_client;
	audio_client->GetService(__uuidof(IAudioRenderClient), (void **)&render_client);
	HANDLE event = CreateEvent(NULL, FALSE, FALSE, NULL);
	audio_client->SetEventHandle(event);
	resampler->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, NULL);
	BYTE *buffer;
	render_client->GetBuffer(buffer_size, &buffer);
	int running = fill_buffer(resampler, buffer, buffer_size * format->nBlockAlign, callback, user_data);
	render_client->ReleaseBuffer(buffer_size, 0);
	audio_client->Start();
	UINT32 padding;
	while (running) {
		WaitForSingleObject(event, INFINITE);
		audio_client->GetCurrentPadding(&padding);
		if (buffer_size - padding > 0) {
			render_client->GetBuffer(buffer_size - padding, &buffer);
			running = fill_buffer(resampler, buffer, (buffer_size - padding) * format->nBlockAlign, callback, user_data);
			render_client->ReleaseBuffer(buffer_size - padding, 0);
		}
	}
	do {
		WaitForSingleObject(event, INFINITE);
		audio_client->GetCurrentPadding(&padding);
	} while (padding > 0);
	audio_client->Stop();
	CloseHandle(event);
	render_client->Release();
	CoTaskMemFree(format);
}


/*=========
    MIDI
 =========*/

struct gral_midi {
	gral_midi_interface const *iface;
	void *user_data;
	HMIDIIN midi;
};

static void CALLBACK midi_callback(HMIDIIN hMidiIn, UINT wMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
	// TODO: call the callbacks on the main thread
	gral_midi *midi = (gral_midi *)dwInstance;
	switch (wMsg) {
	case MIM_DATA:
		{
			if ((LOBYTE(LOWORD(dwParam1)) & 0xF0) == 0x80) {
				midi->iface->note_off(HIBYTE(LOWORD(dwParam1)), LOBYTE(HIWORD(dwParam1)), midi->user_data);
			}
			else if ((LOBYTE(LOWORD(dwParam1)) & 0xF0) == 0x90) {
				midi->iface->note_on(HIBYTE(LOWORD(dwParam1)), LOBYTE(HIWORD(dwParam1)), midi->user_data);
			}
			else if ((LOBYTE(LOWORD(dwParam1)) & 0xF0) == 0xB0) {
				midi->iface->control_change(HIBYTE(LOWORD(dwParam1)), LOBYTE(HIWORD(dwParam1)), midi->user_data);
			}
			return;
		}
	default:
		return;
	}
}

gral_midi *gral_midi_create(gral_application *application, char const *name, gral_midi_interface const *iface, void *user_data) {
	gral_midi *midi = new gral_midi();
	midi->iface = iface;
	midi->user_data = user_data;
	midi->midi = NULL;
	return midi;
	if (midiInGetNumDevs() > 0) {
		midiInOpen(&midi->midi, 0, (DWORD_PTR)&midi_callback, (DWORD_PTR)midi, CALLBACK_FUNCTION);
		midiInStart(midi->midi);
	}
	return midi;
}

void gral_midi_delete(gral_midi *midi) {
	if (midi->midi) {
		midiInClose(midi->midi);
	}
	delete midi;
}
