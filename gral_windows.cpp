/*

Copyright (c) 2016-2021 Elias Aebi

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#include "gral.h"
#define UNICODE
#define _UNICODE
#include <Windows.h>
#include <windowsx.h>
#include <d2d1.h>
#include <dwrite.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <avrt.h>
#define _USE_MATH_DEFINES
#include <math.h>

template <class T> class ComPointer {
	T *pointer;
public:
	ComPointer(): pointer(NULL) {}
	ComPointer(const ComPointer &com_pointer): pointer(com_pointer.pointer) {
		if (pointer) {
			pointer->AddRef();
		}
	}
	~ComPointer() {
		if (pointer) {
			pointer->Release();
		}
	}
	ComPointer &operator =(const ComPointer &com_pointer) {
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

static HINSTANCE hInstance;
static ID2D1Factory *factory;
static ID2D1StrokeStyle *stroke_style;
static IDWriteFactory *dwrite_factory;

struct gral_draw_context {
	ID2D1HwndRenderTarget *target;
	ID2D1PathGeometry *path;
	ID2D1GeometrySink *sink;
	bool open;
	gral_draw_context(): open(false) {}
};

template <class T> class Buffer {
	size_t length;
	T *data;
public:
	Buffer(size_t length): length(length), data(new T[length]) {}
	Buffer(const Buffer &buffer): length(buffer.length), data(new T[length]) {
		for (size_t i = 0; i < length; i++) {
			data[i] = buffer.data[i];
		}
	}
	~Buffer() {
		delete[] data;
	}
	Buffer &operator =(const Buffer &buffer) {
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

static Buffer<wchar_t> utf8_to_utf16(const char *utf8) {
	int length = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
	Buffer<wchar_t> utf16(length);
	MultiByteToWideChar(CP_UTF8, 0, utf8, -1, utf16, length);
	return utf16;
}

static Buffer<char> utf16_to_utf8(const wchar_t *utf16) {
	int length = WideCharToMultiByte(CP_UTF8, 0, utf16, -1, NULL, 0, NULL, NULL);
	Buffer<char> utf8(length);
	WideCharToMultiByte(CP_UTF8, 0, utf16, -1, utf8, length, NULL, NULL);
	return utf8;
}

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
	gral_window_interface iface;
	void *user_data;
	ID2D1HwndRenderTarget *target;
	bool mouse_inside;
	int minimum_width, minimum_height;
	HCURSOR cursor;
	WindowData(): mouse_inside(false), minimum_width(0), minimum_height(0) {}
};


/*================
    APPLICATION
 ================*/

struct gral_application {
	gral_application_interface iface;
	void *user_data;
};

static int get_key(WPARAM wParam) {
	switch (wParam) {
	case VK_RETURN:
		return GRAL_KEY_ENTER;
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
		return 0;
	}
}
int get_modifiers() {
	int modifiers = 0;
	if (GetKeyState(VK_CONTROL) < 0) modifiers |= GRAL_MODIFIER_CONTROL;
	if (GetKeyState(VK_MENU) < 0) modifiers |= GRAL_MODIFIER_ALT;
	if (GetKeyState(VK_SHIFT) < 0) modifiers |= GRAL_MODIFIER_SHIFT;
	return modifiers;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	WindowData *window_data = (WindowData *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
	switch (uMsg) {
	case WM_PAINT: {
		if (window_data->target == NULL) {
			RECT rc;
			GetClientRect(hwnd, &rc);
			D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
			factory->CreateHwndRenderTarget(D2D1::RenderTargetProperties(), D2D1::HwndRenderTargetProperties(hwnd, size), &window_data->target);
		}
		gral_draw_context draw_context;
		draw_context.target = window_data->target;
		factory->CreatePathGeometry(&draw_context.path);
		draw_context.path->Open(&draw_context.sink);
		draw_context.sink->SetFillMode(D2D1_FILL_MODE_WINDING);
		draw_context.target->BeginDraw();
		draw_context.target->Clear(D2D1::ColorF(D2D1::ColorF::White));
		window_data->iface.draw(&draw_context, window_data->user_data);
		if (draw_context.target->EndDraw() == D2DERR_RECREATE_TARGET) {
			draw_context.target->Release();
			window_data->target = NULL;
		}
		draw_context.sink->Release();
		draw_context.path->Release();
		ValidateRect(hwnd, NULL);
		return 0;
	}
	case WM_MOUSEMOVE: {
		if (!window_data->mouse_inside) {
			SetCursor(window_data->cursor);
			window_data->mouse_inside = true;
			window_data->iface.mouse_enter(window_data->user_data);
			TRACKMOUSEEVENT track_mouse_event;
			track_mouse_event.cbSize = sizeof(TRACKMOUSEEVENT);
			track_mouse_event.dwFlags = TME_LEAVE;
			track_mouse_event.hwndTrack = hwnd;
			TrackMouseEvent(&track_mouse_event);
		}
		window_data->iface.mouse_move((float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam), window_data->user_data);
		return 0;
	}
	case WM_MOUSELEAVE: {
		window_data->iface.mouse_leave(window_data->user_data);
		window_data->mouse_inside = false;
		return 0;
	}
	case WM_LBUTTONDOWN: {
		SetCapture(hwnd);
		window_data->iface.mouse_button_press((float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam), GRAL_PRIMARY_MOUSE_BUTTON, get_modifiers(), window_data->user_data);
		return 0;
	}
	case WM_MBUTTONDOWN: {
		SetCapture(hwnd);
		window_data->iface.mouse_button_press((float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam), GRAL_MIDDLE_MOUSE_BUTTON, get_modifiers(), window_data->user_data);
		return 0;
	}
	case WM_RBUTTONDOWN: {
		SetCapture(hwnd);
		window_data->iface.mouse_button_press((float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam), GRAL_SECONDARY_MOUSE_BUTTON, get_modifiers(), window_data->user_data);
		return 0;
	}
	case WM_LBUTTONUP: {
		if ((wParam & (MK_LBUTTON | MK_MBUTTON | MK_RBUTTON)) == 0) {
			ReleaseCapture();
		}
		window_data->iface.mouse_button_release((float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam), GRAL_PRIMARY_MOUSE_BUTTON, window_data->user_data);
		return 0;
	}
	case WM_MBUTTONUP: {
		if ((wParam & (MK_LBUTTON | MK_MBUTTON | MK_RBUTTON)) == 0) {
			ReleaseCapture();
		}
		window_data->iface.mouse_button_release((float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam), GRAL_MIDDLE_MOUSE_BUTTON, window_data->user_data);
		return 0;
	}
	case WM_RBUTTONUP: {
		if ((wParam & (MK_LBUTTON | MK_MBUTTON | MK_RBUTTON)) == 0) {
			ReleaseCapture();
		}
		window_data->iface.mouse_button_release((float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam), GRAL_SECONDARY_MOUSE_BUTTON, window_data->user_data);
		return 0;
	}
	case WM_LBUTTONDBLCLK: {
		SetCapture(hwnd);
		int modifiers = get_modifiers();
		window_data->iface.mouse_button_press((float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam), GRAL_PRIMARY_MOUSE_BUTTON, modifiers, window_data->user_data);
		window_data->iface.double_click((float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam), GRAL_PRIMARY_MOUSE_BUTTON, modifiers, window_data->user_data);
		return 0;
	}
	case WM_MBUTTONDBLCLK: {
		SetCapture(hwnd);
		int modifiers = get_modifiers();
		window_data->iface.mouse_button_press((float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam), GRAL_MIDDLE_MOUSE_BUTTON, modifiers, window_data->user_data);
		window_data->iface.double_click((float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam), GRAL_MIDDLE_MOUSE_BUTTON, modifiers, window_data->user_data);
		return 0;
	}
	case WM_RBUTTONDBLCLK: {
		SetCapture(hwnd);
		int modifiers = get_modifiers();
		window_data->iface.mouse_button_press((float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam), GRAL_SECONDARY_MOUSE_BUTTON, modifiers, window_data->user_data);
		window_data->iface.double_click((float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam), GRAL_SECONDARY_MOUSE_BUTTON, modifiers, window_data->user_data);
		return 0;
	}
	case WM_MOUSEWHEEL: {
		window_data->iface.scroll(0.f, (float)GET_WHEEL_DELTA_WPARAM(wParam)/(float)WHEEL_DELTA, window_data->user_data);
		return 0;
	}
	case WM_MOUSEHWHEEL: {
		window_data->iface.scroll(-(float)GET_WHEEL_DELTA_WPARAM(wParam)/(float)WHEEL_DELTA, 0.f, window_data->user_data);
		return 0;
	}
	case WM_KEYDOWN: {
		int scan_code = (lParam >> 16) & 0xFF;
		window_data->iface.key_press(get_key(wParam), scan_code, get_modifiers(), window_data->user_data);
		return 0;
	}
	case WM_KEYUP: {
		int scan_code = (lParam >> 16) & 0xFF;
		window_data->iface.key_release(get_key(wParam), scan_code, window_data->user_data);
		return 0;
	}
	case WM_CHAR: {
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
			char utf8[5];
			WideCharToMultiByte(CP_UTF8, 0, utf16, -1, utf8, 5, NULL, NULL);
			window_data->iface.text(utf8, window_data->user_data);
		}
		return 0;
	}
	case WM_TIMER: {
		if (!window_data->iface.timer(window_data->user_data)) {
			KillTimer(hwnd, wParam);
		}
		return 0;
	}
	case WM_SIZE: {
		WORD width = LOWORD(lParam);
		WORD height = HIWORD(lParam);
		window_data->iface.resize(width, height, window_data->user_data);
		if (window_data->target) {
			window_data->target->Resize(D2D1::SizeU(width, height));
		}
		RedrawWindow(hwnd, NULL, NULL, RDW_ERASE|RDW_INVALIDATE);
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
	case WM_GETMINMAXINFO: {
		if (window_data) {
			MINMAXINFO *mmi = (MINMAXINFO *)lParam;
			mmi->ptMinTrackSize.x = window_data->minimum_width;
			mmi->ptMinTrackSize.y = window_data->minimum_height;
		}
		return 0;
	}
	case WM_USER: {
		void (*callback)(void *user_data) = (void (*)(void *))wParam;
		void *user_data = (void *)lParam;
		callback(user_data);
		return 0;
	}
	case WM_CLOSE: {
		if (window_data->iface.close(window_data->user_data)) {
			DestroyWindow(hwnd);
		}
		return 0;
	}
	case WM_DESTROY: {
		if (window_data->target) {
			window_data->target->Release();
		}
		delete window_data;
		PostQuitMessage(0);
		return 0;
	}
	default:
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
}

gral_application *gral_application_create(const char *id, const gral_application_interface *iface, void *user_data) {
	hInstance = GetModuleHandle(NULL);
	D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &factory);
	factory->CreateStrokeStyle(D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_LINE_JOIN_ROUND), NULL, 0, &stroke_style);
	DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown **)&dwrite_factory);
	WNDCLASS window_class;
	window_class.style = CS_DBLCLKS;
	window_class.lpfnWndProc = &WndProc;
	window_class.cbClsExtra = 0;
	window_class.cbWndExtra = 0;
	window_class.hInstance = hInstance;
	window_class.hIcon = NULL;
	window_class.hCursor = NULL;
	window_class.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
	window_class.lpszMenuName = NULL;
	window_class.lpszClassName = L"gral_window";
	RegisterClass(&window_class);
	static gral_application application;
	application.iface = *iface;
	application.user_data = user_data;
	return &application;
}

void gral_application_delete(gral_application *application) {
	factory->Release();
	stroke_style->Release();
	dwrite_factory->Release();
}

int gral_application_run(gral_application *application, int argc, char **argv) {
	application->iface.initialize(application->user_data);
	MSG message;
	while (GetMessage(&message, NULL, 0, 0)) {
		TranslateMessage(&message);
		DispatchMessage(&message);
	}
	return 0;
}


/*============
    DRAWING
 ============*/

gral_text *gral_text_create(gral_window *window, const char *utf8, float size) {
	NONCLIENTMETRICS ncm;
	ncm.cbSize = sizeof(NONCLIENTMETRICS);
	SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncm, 0);
	ComPointer<IDWriteTextFormat> format;
	dwrite_factory->CreateTextFormat(ncm.lfMessageFont.lfFaceName, NULL, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, L"en", &format);
	IDWriteTextLayout *layout;
	Buffer<wchar_t> utf16 = utf8_to_utf16(utf8);
	dwrite_factory->CreateTextLayout(utf16, utf16.get_length(), format, INFINITY, INFINITY, &layout);
	return (gral_text *)layout;
}

void gral_text_delete(gral_text *text) {
	IDWriteTextLayout *layout = (IDWriteTextLayout *)text;
	layout->Release();
}

void gral_text_set_bold(gral_text *text, int start_index, int end_index) {
	// TODO: implement
}

float gral_text_get_width(gral_text *text, gral_draw_context *draw_context) {
	IDWriteTextLayout *layout = (IDWriteTextLayout *)text;
	DWRITE_TEXT_METRICS metrics;
	layout->GetMetrics(&metrics);
	return metrics.width;
}

float gral_text_index_to_x(gral_text *text, int index) {
	IDWriteTextLayout *layout = (IDWriteTextLayout *)text;
	float x, y;
	DWRITE_HIT_TEST_METRICS metrics;
	// TODO: convert UTF-8 index to UTF-16
	layout->HitTestTextPosition(index, false, &x, &y, &metrics);
	return x;
}

int gral_text_x_to_index(gral_text *text, float x) {
	IDWriteTextLayout *layout = (IDWriteTextLayout *)text;
	BOOL trailing, inside;
	DWRITE_HIT_TEST_METRICS metrics;
	layout->HitTestPoint(x, 0.f, &trailing, &inside, &metrics);
	// TODO: convert UTF-16 index to UTF-8
	return inside && trailing ? metrics.textPosition + metrics.length : metrics.textPosition;
}

void gral_font_get_metrics(gral_window *window, float size, float *ascent, float *descent) {
	NONCLIENTMETRICS ncm;
	ncm.cbSize = sizeof(NONCLIENTMETRICS);
	SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncm, 0);
	ComPointer<IDWriteFontCollection> font_collection;
	dwrite_factory->GetSystemFontCollection(&font_collection, false);
	UINT32 index;
	BOOL exists;
	font_collection->FindFamilyName(ncm.lfMessageFont.lfFaceName, &index, &exists);
	ComPointer<IDWriteFontFamily> font_family;
	font_collection->GetFontFamily(index, &font_family);
	ComPointer<IDWriteFont> font;
	font_family->GetFirstMatchingFont(DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &font);
	DWRITE_FONT_METRICS metrics;
	font->GetMetrics(&metrics);
	if (ascent) *ascent = (float)metrics.ascent / (float)metrics.designUnitsPerEm * size;
	if (descent) *descent = (float)metrics.descent / (float)metrics.designUnitsPerEm * size;
}

void gral_draw_context_draw_text(gral_draw_context *draw_context, gral_text *text, float x, float y, float red, float green, float blue, float alpha) {
	IDWriteTextLayout *layout = (IDWriteTextLayout *)text;
	DWRITE_LINE_METRICS line_metrics;
	UINT32 count = 1;
	layout->GetLineMetrics(&line_metrics, count, &count);
	ComPointer<ID2D1SolidColorBrush> brush;
	draw_context->target->CreateSolidColorBrush(D2D1::ColorF(red, green, blue, alpha), &brush);
	draw_context->target->DrawTextLayout(D2D1::Point2F(x, y-line_metrics.baseline), layout, brush);
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

void gral_draw_context_fill_linear_gradient(gral_draw_context *draw_context, float start_x, float start_y, float end_x, float end_y, const gral_gradient_stop *stops, int count) {
	if (draw_context->open) {
		draw_context->sink->EndFigure(D2D1_FIGURE_END_OPEN);
		draw_context->open = false;
	}
	draw_context->sink->Close();

	Buffer<D2D1_GRADIENT_STOP> gradient_stops(count);
	for (int i = 0; i < count; i++) {
		gradient_stops[i].position = stops[i].position;
		gradient_stops[i].color = D2D1::ColorF(stops[i].red, stops[i].green, stops[i].blue, stops[i].alpha);
	}
	ComPointer<ID2D1GradientStopCollection> gradient_stop_collection;
	draw_context->target->CreateGradientStopCollection(gradient_stops, count, &gradient_stop_collection);
	ComPointer<ID2D1LinearGradientBrush> brush;
	draw_context->target->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(D2D1::Point2F(start_x, start_y), D2D1::Point2F(end_x, end_y)), gradient_stop_collection, &brush);
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

gral_window *gral_window_create(gral_application *application, int width, int height, const char *title, const gral_window_interface *iface, void *user_data) {
	adjust_window_size(width, height);
	HWND hwnd = CreateWindow(L"gral_window", utf8_to_utf16(title), WS_OVERLAPPEDWINDOW, 0, 0, width, height, NULL, NULL, hInstance, NULL);
	WindowData *window_data = new WindowData();
	window_data->iface = *iface;
	window_data->user_data = user_data;
	window_data->target = NULL;
	window_data->cursor = LoadCursor(NULL, IDC_ARROW);
	SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)window_data);
	ShowWindow(hwnd, SW_SHOW);
	return (gral_window *)hwnd;
}

void gral_window_delete(gral_window *window) {

}

void gral_window_request_redraw(gral_window *window, int x, int y, int width, int height) {
	RECT rect;
	rect.left = x;
	rect.top = y;
	rect.right = x + width;
	rect.bottom = y + height;
	RedrawWindow((HWND)window, &rect, NULL, RDW_ERASE|RDW_INVALIDATE);
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

void gral_window_warp_cursor(gral_window *window, float x, float y) {
	POINT point;
	point.x = (LONG)x;
	point.y = (LONG)y;
	ClientToScreen((HWND)window, &point);
	SetCursorPos(point.x, point.y);
}

void gral_window_show_open_file_dialog(gral_window *window, void (*callback)(const char *file, void *user_data), void *user_data) {
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

void gral_window_show_save_file_dialog(gral_window *window, void (*callback)(const char *file, void *user_data), void *user_data) {
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

void gral_window_clipboard_copy(gral_window *window, const char *text) {
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

void gral_window_clipboard_paste(gral_window *window, void (*callback)(const char *text, void *user_data), void *user_data) {
	OpenClipboard((HWND)window);
	if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
		HGLOBAL handle = GetClipboardData(CF_UNICODETEXT);
		LPWSTR text = (LPWSTR)GlobalLock(handle);
		callback(utf16_to_utf8(text), user_data);
		GlobalUnlock(handle);
	}
	CloseClipboard();
}

void gral_window_set_timer(gral_window *window, int milliseconds) {
	SetTimer((HWND)window, 0, milliseconds, NULL);
}

void gral_window_run_on_main_thread(struct gral_window *window, void (*callback)(void *user_data), void *user_data) {
	PostMessage((HWND)window, WM_USER, (WPARAM)callback, (LPARAM)user_data);
}


/*=========
    FILE
 =========*/

gral_file *gral_file_open_read(const char *path) {
	HANDLE handle = CreateFile(utf8_to_utf16(path), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	return handle == INVALID_HANDLE_VALUE ? NULL : (gral_file *)handle;
}

gral_file *gral_file_open_write(const char *path) {
	HANDLE handle = CreateFile(utf8_to_utf16(path), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	return handle == INVALID_HANDLE_VALUE ? NULL : (gral_file *)handle;
}

gral_file *gral_file_get_stdin() {
	return (gral_file *)GetStdHandle(STD_INPUT_HANDLE);
}

gral_file *gral_file_get_stdout() {
	return (gral_file *)GetStdHandle(STD_OUTPUT_HANDLE);
}

gral_file *gral_file_get_stderr() {
	return (gral_file *)GetStdHandle(STD_ERROR_HANDLE);
}

void gral_file_close(gral_file *file) {
	CloseHandle(file);
}

size_t gral_file_read(gral_file *file, void *buffer, size_t size) {
	DWORD bytes_read;
	ReadFile(file, buffer, size, &bytes_read, NULL);
	return bytes_read;
}

void gral_file_write(gral_file *file, const void *buffer, size_t size) {
	DWORD bytes_written;
	WriteFile(file, buffer, size, &bytes_written, NULL);
}

size_t gral_file_get_size(gral_file *file) {
	return GetFileSize(file, NULL);
}


/*==========
    AUDIO
 ==========*/

static int fill_buffer(float *buffer_float, int frames, int (*callback)(float *buffer, int frames, void *user_data), void *user_data) {
	while (frames > 0) {
		int actual_frames = callback(buffer_float, frames, user_data);
		if (actual_frames == 0) {
			for (int i = 0; i < frames * 2; i++) {
				buffer_float[i] = 0.f;
			}
			return 0;
		}
		buffer_float += actual_frames * 2;
		frames -= actual_frames;
	}
	return 1;
}

static void convert_buffer(float *buffer_float, int32_t *buffer_24, int frames) {
	for (int i = 0; i < frames * 2; i++) {
		uint32_t sample_24 = buffer_float[i] * 8388607.f + 8388608.5f;
		buffer_24[i] = (sample_24 ^ 0x800000) << 8;
	}
}

void gral_audio_play(int (*callback)(float *buffer, int frames, void *user_data), void *user_data) {
	HRESULT hr;
	IMMDeviceEnumerator *device_enumerator;
	CoInitialize(NULL);
	CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void **)&device_enumerator);
	IMMDevice *device;
	device_enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &device);
	IAudioClient *audio_client;
	device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void **)&audio_client);
	REFERENCE_TIME default_period, min_period;
	audio_client->GetDevicePeriod(&default_period, &min_period);
	WAVEFORMATEXTENSIBLE wfx;
	wfx.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
	wfx.Format.nSamplesPerSec = 44100;
	wfx.Format.wBitsPerSample = 32;
	wfx.Format.nChannels = 2;
	wfx.Format.nBlockAlign = wfx.Format.wBitsPerSample / 8 * wfx.Format.nChannels;
	wfx.Format.nAvgBytesPerSec = wfx.Format.nBlockAlign * wfx.Format.nSamplesPerSec;
	wfx.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
	wfx.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
	wfx.Samples.wValidBitsPerSample = 24;
	wfx.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
	hr = audio_client->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, default_period, default_period, (PWAVEFORMATEX)&wfx, NULL);
	UINT32 buffer_size;
	if (hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED) {
		audio_client->GetBufferSize(&buffer_size);
		default_period = (REFERENCE_TIME)((10000.0 * 1000 / wfx.Format.nSamplesPerSec * buffer_size) + 0.5);
		audio_client->Release();
		device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void **)&audio_client);
		audio_client->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, default_period, default_period, (PWAVEFORMATEX)&wfx, NULL);
	}
	audio_client->GetBufferSize(&buffer_size);
	IAudioRenderClient *render_client;
	audio_client->GetService(__uuidof(IAudioRenderClient), (void **)&render_client);
	HANDLE event = CreateEvent(NULL, FALSE, FALSE, NULL);
	audio_client->SetEventHandle(event);
	Buffer<float> buffer_float(buffer_size * 2);
	BYTE *buffer;
	render_client->GetBuffer(buffer_size, &buffer);
	int running = fill_buffer(buffer_float, buffer_size, callback, user_data);
	convert_buffer(buffer_float, (int32_t *)buffer, buffer_size);
	render_client->ReleaseBuffer(buffer_size, 0);
	DWORD task_index = 0;
	HANDLE task = AvSetMmThreadCharacteristics(L"Pro Audio", &task_index);
	audio_client->Start();
	while (running) {
		WaitForSingleObject(event, INFINITE);
		render_client->GetBuffer(buffer_size, &buffer);
		running = fill_buffer(buffer_float, buffer_size, callback, user_data);
		convert_buffer(buffer_float, (int32_t *)buffer, buffer_size);
		render_client->ReleaseBuffer(buffer_size, 0);
	}
	audio_client->Stop();
	AvRevertMmThreadCharacteristics(task);
	CloseHandle(event);
	render_client->Release();
	audio_client->Release();
	device->Release();
	device_enumerator->Release();
	CoUninitialize();
}
