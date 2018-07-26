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
#include <gdiplus.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#define _USE_MATH_DEFINES
#include <math.h>

static HINSTANCE hInstance;
static ULONG_PTR gdi_token;

struct gral_draw_context {
	Gdiplus::Graphics graphics;
	Gdiplus::GraphicsPath path;
	Gdiplus::PointF point;
	gral_draw_context(HDC hdc): graphics(hdc), path(Gdiplus::FillModeWinding) {
		graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
		graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
	}
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
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hwnd, &ps);
		gral_draw_context draw_context(hdc);
		window_data->iface.draw(&draw_context, window_data->user_data);
		EndPaint(hwnd, &ps);
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
	Gdiplus::GdiplusStartupInput startup_input;
	Gdiplus::GdiplusStartup(&gdi_token, &startup_input, NULL);
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
	Gdiplus::GdiplusShutdown(gdi_token);
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

static Gdiplus::Color make_color(float r, float g, float b, float a) {
	return Gdiplus::Color((BYTE)(a*255), (BYTE)(r*255), (BYTE)(g*255), (BYTE)(b*255));
}

gral_text *gral_text_create(gral_window *window, const char *utf8, float size) {
	return NULL;
}

void gral_text_delete(gral_text *text) {
	
}

void gral_text_set_bold(gral_text *text, int start_index, int end_index) {
	// TODO: implement
}

float gral_text_get_width(gral_text *text, gral_draw_context *draw_context) {
	return 0.f;
}

float gral_text_index_to_x(gral_text *text, int index) {
	return 0.f;
}

int gral_text_x_to_index(gral_text *text, float x) {
	return 0;
}

void gral_font_get_metrics(gral_window *window, float size, float *ascent, float *descent) {
	
}

void gral_draw_context_draw_text(gral_draw_context *draw_context, gral_text *text, float x, float y, float red, float green, float blue, float alpha) {
	
}

void gral_draw_context_close_path(gral_draw_context *draw_context) {
	draw_context->path.CloseFigure();
}

void gral_draw_context_move_to(gral_draw_context *draw_context, float x, float y) {
	draw_context->path.StartFigure();
	draw_context->point = Gdiplus::PointF(x, y);
}

void gral_draw_context_line_to(gral_draw_context *draw_context, float x, float y) {
	draw_context->path.AddLine(draw_context->point, Gdiplus::PointF(x, y));
	draw_context->point = Gdiplus::PointF(x, y);
}

void gral_draw_context_curve_to(gral_draw_context *draw_context, float x1, float y1, float x2, float y2, float x, float y) {
	draw_context->path.AddBezier(draw_context->point, Gdiplus::PointF(x1, y1), Gdiplus::PointF(x2, y2), Gdiplus::PointF(x, y));
	draw_context->point = Gdiplus::PointF(x, y);
}

void gral_draw_context_fill(gral_draw_context *draw_context, float red, float green, float blue, float alpha) {
	Gdiplus::SolidBrush brush(make_color(red, green, blue, alpha));
	draw_context->graphics.FillPath(&brush, &draw_context->path);
	draw_context->path.Reset();
	draw_context->path.SetFillMode(Gdiplus::FillModeWinding);
}

void gral_draw_context_fill_linear_gradient(gral_draw_context *draw_context, float start_x, float start_y, float end_x, float end_y, float start_red, float start_green, float start_blue, float start_alpha, float end_red, float end_green, float end_blue, float end_alpha) {
	Gdiplus::LinearGradientBrush brush(Gdiplus::PointF(start_x, start_y), Gdiplus::PointF(end_x, end_y), make_color(start_red, start_green, start_blue, start_alpha), make_color(end_red, end_green, end_blue, end_alpha));
	draw_context->graphics.FillPath(&brush, &draw_context->path);
	draw_context->path.Reset();
	draw_context->path.SetFillMode(Gdiplus::FillModeWinding);
}

void gral_draw_context_stroke(gral_draw_context *draw_context, float line_width, float red, float green, float blue, float alpha) {
	Gdiplus::Pen pen(make_color(red, green, blue, alpha), line_width);
	pen.SetLineCap(Gdiplus::LineCapRound, Gdiplus::LineCapRound, Gdiplus::DashCapRound);
	pen.SetLineJoin(Gdiplus::LineJoinRound);
	draw_context->graphics.DrawPath(&pen, &draw_context->path);
	draw_context->path.Reset();
	draw_context->path.SetFillMode(Gdiplus::FillModeWinding);
}

void gral_draw_context_push_clip(gral_draw_context *draw_context) {
	
}

void gral_draw_context_pop_clip(gral_draw_context *draw_context) {
	
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
