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
#include <Windows.h>
#include <gdiplus.h>

static HINSTANCE hInstance;
static ULONG_PTR gdi_token;

struct WindowData {
	gral_window_interface i;
	void *user_data;
	WindowData(gral_window_interface *i, void *user_data): i(*i), user_data(user_data) {}
};

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	WindowData *window_data = (WindowData *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
	switch (uMsg) {
	case WM_PAINT: {
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hwnd, &ps);
		Gdiplus::Graphics graphics(hdc);
		Gdiplus::SolidBrush brush(Gdiplus::Color(255, 0, 0));
		graphics.FillRectangle(&brush, 10, 10, 100, 100);
		EndPaint(hwnd, &ps);
		return 0;
	}
	case WM_LBUTTONDOWN: {
		window_data->i.mouse_button_press(0, window_data->user_data);
		return 0;
	}
	case WM_LBUTTONUP: {
		window_data->i.mouse_button_release(0, window_data->user_data);
		return 0;
	}
	case WM_SIZE: {
		window_data->i.resize(LOWORD(lParam), HIWORD(lParam), window_data->user_data);
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
	case WM_CLOSE: {
		if (window_data->i.close(window_data->user_data)) {
			DestroyWindow(hwnd);
		}
		return 0;
	}
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	default:
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
}

void gral_init(int *argc, char ***argv) {
	hInstance = GetModuleHandle(NULL);
	Gdiplus::GdiplusStartupInput startup_input;
	Gdiplus::GdiplusStartup(&gdi_token, &startup_input, NULL);
	WNDCLASS window_class = {};
	window_class.lpfnWndProc = &WndProc;
	window_class.hInstance = hInstance;
	window_class.lpszClassName = "gral_window";
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


/*=============
    PAINTING
 =============*/

void gral_painter_set_color(gral_painter *painter, float red, float green, float blue, float alpha) {
	
}

void gral_painter_draw_rectangle(gral_painter *painter, float x, float y, float width, float height) {
	
}


/*===========
    WINDOW
 ===========*/

gral_window *gral_window_create(int width, int height, const char *title, gral_window_interface *i, void *user_data) {
	HWND hwnd = CreateWindow("gral_window", title, WS_OVERLAPPEDWINDOW, 0, 0, width, height, NULL, NULL, hInstance, NULL);
	SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)new WindowData(i, user_data));
	ShowWindow(hwnd, SW_SHOW);
	return (gral_window *)hwnd;
}


/*==========
    AUDIO
 ==========*/

void gral_audio_play(int(*callback)(int16_t *buffer, int frames)) {
	
}
