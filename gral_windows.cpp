/*

Copyright (c) 2016, Elias Aebi
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include "gral.h"
#define UNICODE
#define _UNICODE
#include <Windows.h>
#include <windowsx.h>
#include <gdiplus.h>
#define _USE_MATH_DEFINES
#include <math.h>

static HINSTANCE hInstance;
static ULONG_PTR gdi_token;

struct gral_painter {
	Gdiplus::Graphics graphics;
	Gdiplus::GraphicsPath path;
	Gdiplus::PointF point;
	gral_painter(HDC hdc): graphics(hdc), path(Gdiplus::FillModeWinding) {
		graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
	}
};

class UTF16String {
	wchar_t *data;
public:
	UTF16String(const char *utf8) {
		int length = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
		data = new wchar_t[length];
		MultiByteToWideChar(CP_UTF8, 0, utf8, -1, data, length);
	}
	~UTF16String() {
		delete[] data;
	}
	operator const WCHAR*() const {
		return data;
	}
};

struct gral_text {
	UTF16String string;
	Gdiplus::Font font;
	gral_text(const char *utf8, float size): string(utf8), font(Gdiplus::FontFamily::GenericSansSerif(), size, 0, Gdiplus::UnitPixel) {}
};

struct WindowData {
	gral_window_interface iface;
	void *user_data;
	bool mouse_inside;
	WindowData(): mouse_inside(false) {}
};


/*================
    APPLICATION
 ================*/

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	WindowData *window_data = (WindowData *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
	switch (uMsg) {
	case WM_PAINT: {
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hwnd, &ps);
		gral_painter painter(hdc);
		window_data->iface.draw(&painter, window_data->user_data);
		EndPaint(hwnd, &ps);
		return 0;
	}
	case WM_MOUSEMOVE: {
		if (!window_data->mouse_inside) {
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
		window_data->iface.mouse_button_press(GRAL_PRIMARY_MOUSE_BUTTON, window_data->user_data);
		return 0;
	}
	case WM_MBUTTONDOWN: {
		window_data->iface.mouse_button_press(GRAL_MIDDLE_MOUSE_BUTTON, window_data->user_data);
		return 0;
	}
	case WM_RBUTTONDOWN: {
		window_data->iface.mouse_button_press(GRAL_SECONDARY_MOUSE_BUTTON, window_data->user_data);
		return 0;
	}
	case WM_LBUTTONUP: {
		window_data->iface.mouse_button_release(GRAL_PRIMARY_MOUSE_BUTTON, window_data->user_data);
		return 0;
	}
	case WM_MBUTTONUP: {
		window_data->iface.mouse_button_release(GRAL_MIDDLE_MOUSE_BUTTON, window_data->user_data);
		return 0;
	}
	case WM_RBUTTONUP: {
		window_data->iface.mouse_button_release(GRAL_SECONDARY_MOUSE_BUTTON, window_data->user_data);
		return 0;
	}
	case WM_MOUSEWHEEL: {
		return 0;
	}
	case WM_CHAR: {
		if ((wParam & 0xFC00) == 0xD800) {
			
		}
		else if ((wParam & 0xFC00) == 0xDC00) {
			
		}
		else {
			window_data->iface.character(wParam, window_data->user_data);
		}
		return 0;
	}
	case WM_SIZE: {
		window_data->iface.resize(LOWORD(lParam), HIWORD(lParam), window_data->user_data);
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
	case WM_CLOSE: {
		if (window_data->iface.close(window_data->user_data)) {
			DestroyWindow(hwnd);
		}
		return 0;
	}
	case WM_DESTROY: {
		PostQuitMessage(0);
		return 0;
	}
	default:
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
}

void gral_init(int *argc, char ***argv) {
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
	window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
	window_class.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
	window_class.lpszMenuName = NULL;
	window_class.lpszClassName = L"gral_window";
	RegisterClass(&window_class);
}

int gral_run(void) {
	MSG message;
	while (GetMessage(&message, NULL, 0, 0)) {
		TranslateMessage(&message);
		DispatchMessage(&message);
	}
	return 0;
}

gral_application *gral_application_create(const char *id, gral_application_interface *iface, void *user_data) {
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
	window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
	window_class.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
	window_class.lpszMenuName = NULL;
	window_class.lpszClassName = L"gral_window";
	RegisterClass(&window_class);
	iface->initialize(user_data);
	return NULL;
}

void gral_application_delete(struct gral_application *application) {
	Gdiplus::GdiplusShutdown(gdi_token);
}

int gral_application_run(struct gral_application *application, int argc, char **argv) {
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

gral_text *gral_text_create(struct gral_window *window, const char *utf8, float size) {
	return new gral_text(utf8, size);
}

void gral_text_delete(struct gral_text *text) {
	delete text;
}

void gral_painter_draw_text(struct gral_painter *painter, struct gral_text *text, float x, float y, float red, float green, float blue, float alpha) {
	Gdiplus::SolidBrush brush(make_color(red, green, blue, alpha));
	painter->graphics.DrawString(text->string, -1, &text->font, Gdiplus::PointF(x, y), &brush);
}

void gral_painter_new_path(gral_painter *painter) {
	painter->path.Reset();
	painter->path.SetFillMode(Gdiplus::FillModeWinding);
}

void gral_painter_close_path(gral_painter *painter) {
	painter->path.CloseFigure();
}

void gral_painter_move_to(gral_painter *painter, float x, float y) {
	painter->path.StartFigure();
	painter->point = Gdiplus::PointF(x, y);
}

void gral_painter_line_to(gral_painter *painter, float x, float y) {
	painter->path.AddLine(painter->point, Gdiplus::PointF(x, y));
	painter->point = Gdiplus::PointF(x, y);
}

void gral_painter_curve_to(gral_painter *painter, float x1, float y1, float x2, float y2, float x, float y) {
	painter->path.AddBezier(painter->point, Gdiplus::PointF(x1, y1), Gdiplus::PointF(x2, y2), Gdiplus::PointF(x, y));
	painter->point = Gdiplus::PointF(x, y);
}

void gral_painter_add_rectangle(gral_painter *painter, float x, float y, float width, float height) {
	painter->path.AddRectangle(Gdiplus::RectF(x, y, width, height));
}

static float degrees(float angle) {
	return angle * (180.f / (float)M_PI);
}

void gral_painter_add_arc(gral_painter *painter, float cx, float cy, float radius, float start_angle, float end_angle) {
	float sweep_angle = degrees(end_angle - start_angle);
	if (sweep_angle < 0.f) sweep_angle += 360.f;
	painter->path.AddArc(Gdiplus::RectF(cx-radius, cy-radius, 2.f*radius, 2.f*radius), degrees(start_angle), sweep_angle);
	painter->point = Gdiplus::PointF(cx+cosf(end_angle)*radius, cy+sinf(end_angle)*radius);
}

void gral_painter_fill(gral_painter *painter, float red, float green, float blue, float alpha) {
	Gdiplus::SolidBrush brush(make_color(red, green, blue, alpha));
	painter->graphics.FillPath(&brush, &painter->path);
}

void gral_painter_stroke(gral_painter *painter, float line_width, float red, float green, float blue, float alpha) {
	Gdiplus::Pen pen(make_color(red, green, blue, alpha), line_width);
	pen.SetLineCap(Gdiplus::LineCapRound, Gdiplus::LineCapRound, Gdiplus::DashCapRound);
	pen.SetLineJoin(Gdiplus::LineJoinRound);
	painter->graphics.DrawPath(&pen, &painter->path);
}


/*===========
    WINDOW
 ===========*/

gral_window *gral_window_create(gral_application *application, int width, int height, const char *title, gral_window_interface *iface, void *user_data) {
	HWND hwnd = CreateWindow(L"gral_window", UTF16String(title), WS_OVERLAPPEDWINDOW, 0, 0, width, height, NULL, NULL, hInstance, NULL);
	WindowData *window_data = new WindowData();
	window_data->iface = *iface;
	window_data->user_data = user_data;
	SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)window_data);
	ShowWindow(hwnd, SW_SHOW);
	return (gral_window *)hwnd;
}

void gral_window_delete(gral_window *window) {
	WindowData *window_data = (WindowData *)GetWindowLongPtr((HWND)window, GWLP_USERDATA);
	delete window_data;
}

void gral_window_request_redraw(gral_window *window) {
	RedrawWindow((HWND)window, NULL, NULL, RDW_ERASE|RDW_INVALIDATE);
}


/*==========
    AUDIO
 ==========*/

void gral_audio_play(int(*callback)(int16_t *buffer, int frames)) {
	HWAVEOUT hwo;
	WAVEFORMATEX wfx;
	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nChannels = 2;
	wfx.nSamplesPerSec = 44100;
	wfx.nAvgBytesPerSec = 44100 * 2 * 2;
	wfx.nBlockAlign = 2 * 2;
	wfx.wBitsPerSample = 16;
	wfx.cbSize = 0;
	waveOutOpen(&hwo, WAVE_MAPPER, &wfx, NULL, NULL, CALLBACK_NULL);
	// TODO: implement
	waveOutClose(hwo);
}
