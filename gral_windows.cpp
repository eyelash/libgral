/*

Copyright (c) 2016-2018 Elias Aebi

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
#define _USE_MATH_DEFINES
#include <math.h>

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
	AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, false);
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
		window_data->iface.mouse_button_press((float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam), GRAL_PRIMARY_MOUSE_BUTTON, window_data->user_data);
		return 0;
	}
	case WM_MBUTTONDOWN: {
		window_data->iface.mouse_button_press((float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam), GRAL_MIDDLE_MOUSE_BUTTON, window_data->user_data);
		return 0;
	}
	case WM_RBUTTONDOWN: {
		window_data->iface.mouse_button_press((float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam), GRAL_SECONDARY_MOUSE_BUTTON, window_data->user_data);
		return 0;
	}
	case WM_LBUTTONUP: {
		window_data->iface.mouse_button_release((float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam), GRAL_PRIMARY_MOUSE_BUTTON, window_data->user_data);
		return 0;
	}
	case WM_MBUTTONUP: {
		window_data->iface.mouse_button_release((float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam), GRAL_MIDDLE_MOUSE_BUTTON, window_data->user_data);
		return 0;
	}
	case WM_RBUTTONUP: {
		window_data->iface.mouse_button_release((float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam), GRAL_SECONDARY_MOUSE_BUTTON, window_data->user_data);
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
	window_class.style = 0;
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
	IDWriteTextFormat *format;
	dwrite_factory->CreateTextFormat(ncm.lfMessageFont.lfFaceName, NULL, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, L"en", &format);
	IDWriteTextLayout *layout;
	Buffer<wchar_t> utf16 = utf8_to_utf16(utf8);
	dwrite_factory->CreateTextLayout(utf16, utf16.get_length(), format, INFINITY, INFINITY, &layout);
	format->Release();
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
	IDWriteFontCollection *font_collection;
	dwrite_factory->GetSystemFontCollection(&font_collection, false);
	UINT32 index;
	BOOL exists;
	font_collection->FindFamilyName(ncm.lfMessageFont.lfFaceName, &index, &exists);
	IDWriteFontFamily *font_family;
	font_collection->GetFontFamily(index, &font_family);
	IDWriteFont *font;
	font_family->GetFirstMatchingFont(DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &font);
	DWRITE_FONT_METRICS metrics;
	font->GetMetrics(&metrics);
	if (ascent) *ascent = (float)metrics.ascent / (float)metrics.designUnitsPerEm * size;
	if (descent) *descent = (float)metrics.descent / (float)metrics.designUnitsPerEm * size;
	font->Release();
	font_family->Release();
	font_collection->Release();
}

void gral_draw_context_draw_text(gral_draw_context *draw_context, gral_text *text, float x, float y, float red, float green, float blue, float alpha) {
	IDWriteTextLayout *layout = (IDWriteTextLayout *)text;
	DWRITE_LINE_METRICS line_metrics;
	UINT32 count = 1;
	layout->GetLineMetrics(&line_metrics, count, &count);
	ID2D1SolidColorBrush *brush;
	draw_context->target->CreateSolidColorBrush(D2D1::ColorF(red, green, blue, alpha), &brush);
	draw_context->target->DrawTextLayout(D2D1::Point2F(x, y-line_metrics.baseline), layout, brush);
	brush->Release();
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

void gral_draw_context_add_rectangle(gral_draw_context *draw_context, float x, float y, float width, float height) {
	gral_draw_context_move_to(draw_context, x, y);
	gral_draw_context_line_to(draw_context, x+width, y);
	gral_draw_context_line_to(draw_context, x+width, y+height);
	gral_draw_context_line_to(draw_context, x, y+height);
	gral_draw_context_close_path(draw_context);
}

void gral_draw_context_fill(gral_draw_context *draw_context, float red, float green, float blue, float alpha) {
	if (draw_context->open) {
		draw_context->sink->EndFigure(D2D1_FIGURE_END_OPEN);
		draw_context->open = false;
	}
	draw_context->sink->Close();

	ID2D1SolidColorBrush *brush;
	draw_context->target->CreateSolidColorBrush(D2D1::ColorF(red, green, blue, alpha), &brush);
	draw_context->target->FillGeometry(draw_context->path, brush);
	brush->Release();

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
	ID2D1GradientStopCollection *gradient_stop_collection;
	draw_context->target->CreateGradientStopCollection(gradient_stops, count, &gradient_stop_collection);
	ID2D1LinearGradientBrush *brush;
	draw_context->target->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(D2D1::Point2F(start_x, start_y), D2D1::Point2F(end_x, end_y)), gradient_stop_collection, &brush);
	draw_context->target->FillGeometry(draw_context->path, brush);
	brush->Release();
	gradient_stop_collection->Release();

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

	ID2D1SolidColorBrush *brush;
	draw_context->target->CreateSolidColorBrush(D2D1::ColorF(red, green, blue, alpha), &brush);
	draw_context->target->DrawGeometry(draw_context->path, brush, line_width, stroke_style);
	brush->Release();

	draw_context->sink->Release();
	draw_context->path->Release();
	factory->CreatePathGeometry(&draw_context->path);
	draw_context->path->Open(&draw_context->sink);
	draw_context->sink->SetFillMode(D2D1_FILL_MODE_WINDING);
}

void gral_draw_context_push_clip(gral_draw_context *draw_context) {
	if (draw_context->open) {
		draw_context->sink->EndFigure(D2D1_FIGURE_END_OPEN);
		draw_context->open = false;
	}
	draw_context->sink->Close();

	ID2D1Layer *layer;
	draw_context->target->CreateLayer(&layer);
	draw_context->target->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(), draw_context->path), layer);
	layer->Release();

	draw_context->sink->Release();
	draw_context->path->Release();
	factory->CreatePathGeometry(&draw_context->path);
	draw_context->path->Open(&draw_context->sink);
	draw_context->sink->SetFillMode(D2D1_FILL_MODE_WINDING);
}

void gral_draw_context_pop_clip(gral_draw_context *draw_context) {
	draw_context->target->PopLayer();
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
	return (gral_window *)hwnd;
}

void gral_window_delete(gral_window *window) {

}

void gral_window_show(gral_window *window) {
	ShowWindow((HWND)window, SW_SHOW);
}

void gral_window_hide(gral_window *window) {
	// TODO: implement
}

void gral_window_request_redraw(gral_window *window) {
	RedrawWindow((HWND)window, NULL, NULL, RDW_ERASE|RDW_INVALIDATE);
}

void gral_window_set_minimum_size(gral_window *window, int minimum_width, int minimum_height) {
	WindowData *window_data = (WindowData *)GetWindowLongPtr((HWND)window, GWLP_USERDATA);
	adjust_window_size(minimum_width, minimum_height);
	window_data->minimum_width = minimum_width;
	window_data->minimum_height = minimum_height;
}

static LPCTSTR get_cursor_name(int cursor) {
	switch (cursor) {
	case GRAL_CURSOR_DEFAULT: return IDC_ARROW;
	case GRAL_CURSOR_TEXT: return IDC_IBEAM;
	case GRAL_CURSOR_HORIZONTAL_ARROWS: return IDC_SIZEWE;
	case GRAL_CURSOR_VERTICAL_ARROWS: return IDC_SIZENS;
	default: return NULL;
	}
}
void gral_window_set_cursor(gral_window *window, int cursor) {
	WindowData *window_data = (WindowData *)GetWindowLongPtr((HWND)window, GWLP_USERDATA);
	window_data->cursor = LoadCursor(NULL, get_cursor_name(cursor));
	SetCursor(window_data->cursor);
}

void gral_window_show_open_file_dialog(gral_window *window, void (*callback)(const char *file, void *user_data), void *user_data) {
	wchar_t file_name[MAX_PATH];
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
	wchar_t file_name[MAX_PATH];
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

void gral_window_clipboard_request_paste(gral_window *window) {
	WindowData *window_data = (WindowData *)GetWindowLongPtr((HWND)window, GWLP_USERDATA);
	OpenClipboard((HWND)window);
	if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
		HGLOBAL handle = GetClipboardData(CF_UNICODETEXT);
		LPWSTR text = (LPWSTR)GlobalLock(handle);
		window_data->iface.paste(utf16_to_utf8(text), window_data->user_data);
		GlobalUnlock(handle);
	}
	CloseClipboard();
}

void gral_window_set_timer(gral_window *window, int milliseconds) {
	SetTimer((HWND)window, 0, milliseconds, NULL);
}


/*=========
    FILE
 =========*/

void gral_file_read(const char *file, void (*callback)(const char *data, size_t size, void *user_data), void *user_data) {
	HANDLE handle = CreateFile(utf8_to_utf16(file), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (handle == INVALID_HANDLE_VALUE) {
		return;
	}
	DWORD size = GetFileSize(handle, NULL);
	Buffer<char> buffer(size);
	DWORD bytes_read;
	ReadFile(handle, buffer, size, &bytes_read, NULL);
	CloseHandle(handle);
	callback(buffer, size, user_data);
}

void gral_file_write(const char *file, const char *data, size_t size) {
	HANDLE handle = CreateFile(utf8_to_utf16(file), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (handle == INVALID_HANDLE_VALUE) {
		return;
	}
	DWORD bytes_written;
	WriteFile(handle, data, size, &bytes_written, NULL);
	CloseHandle(handle);
}


/*==========
    AUDIO
 ==========*/

void gral_audio_play(int (*callback)(int16_t *buffer, int frames, void *user_data), void *user_data) {
	IMMDeviceEnumerator *device_enumerator;
	CoInitialize(NULL);
	CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void **)&device_enumerator);
	IMMDevice *device;
	device_enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &device);
	IAudioClient *audio_client;
	device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void **)&audio_client);
	REFERENCE_TIME default_period, min_period;
	audio_client->GetDevicePeriod(&default_period, &min_period);
	WAVEFORMATEX wfx;
	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nChannels = 2;
	wfx.nSamplesPerSec = 44100;
	wfx.nAvgBytesPerSec = 44100 * 2 * 2;
	wfx.nBlockAlign = 2 * 2;
	wfx.wBitsPerSample = 16;
	wfx.cbSize = 0;
	audio_client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, default_period, 0, &wfx, NULL);
	UINT32 buffer_size;
	audio_client->GetBufferSize(&buffer_size);
	IAudioRenderClient *render_client;
	audio_client->GetService(__uuidof(IAudioRenderClient), (void **)&render_client);
	HANDLE event = CreateEvent(NULL, false, false, NULL);
	audio_client->SetEventHandle(event);
	BYTE *buffer;
	render_client->GetBuffer(buffer_size, &buffer);
	int frames = callback((int16_t *)buffer, buffer_size, user_data);
	render_client->ReleaseBuffer(frames, 0);
	audio_client->Start();
	UINT32 padding;
	while (frames > 0) {
		WaitForSingleObject(event, INFINITE);
		audio_client->GetCurrentPadding(&padding);
		if (buffer_size - padding > 0) {
			render_client->GetBuffer(buffer_size-padding, &buffer);
			frames = callback((int16_t *)buffer, buffer_size-padding, user_data);
			render_client->ReleaseBuffer(frames, 0);
		}
	}
	do {
		WaitForSingleObject(event, INFINITE);
		audio_client->GetCurrentPadding(&padding);
	} while (padding > 0);
	audio_client->Stop();
	CloseHandle(event);
	render_client->Release();
	audio_client->Release();
	device->Release();
	device_enumerator->Release();
	CoUninitialize();
}
