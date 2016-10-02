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
#include <Cocoa/Cocoa.h>

@interface GralApplicationDelegate: NSObject<NSApplicationDelegate>
@end
@implementation GralApplicationDelegate
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
	return YES;
}
@end

void gral_init(int *argc, char ***argv) {
	[NSApplication sharedApplication];
	[NSApp setDelegate:[[GralApplicationDelegate alloc] init]];
}

int gral_run(void) {
	[NSApp run];
	return 0;
}


/*=============
    PAINTING
 =============*/

void gral_painter_set_color(struct gral_painter *painter, float red, float green, float blue, float alpha) {
	[[NSColor colorWithRed:red green:green blue:blue alpha:alpha] set];
}

void gral_painter_draw_rectangle(struct gral_painter *painter, float x, float y, float width, float height) {
	[NSBezierPath fillRect:CGRectMake(x, y, width, height)];
}


/*===========
    WINDOW
 ===========*/

@interface GralWindow: NSWindow
@end
@implementation GralWindow
@end

@interface GralWindowDelegate: NSObject<NSWindowDelegate> {
	@public struct gral_window_interface interface;
}
@end
@implementation GralWindowDelegate
- (BOOL)windowShouldClose:(id)sender {
	return interface.close();
}
@end

@interface GralView: NSView {
	@public struct gral_window_interface interface;
}
@end
@implementation GralView
- (BOOL)isFlipped {
	return YES;
}
- (void)drawRect:(NSRect)rect {
	interface.draw(NULL);
}
- (void)setFrameSize:(NSSize)size {
	[super setFrameSize:size];
	interface.resize(size.width, size.height);
}
@end

struct gral_window *gral_window_create(int width, int height, const char *title, struct gral_window_interface *interface) {
	GralWindow *window = [[GralWindow alloc]
		initWithContentRect:CGRectMake(0, 0, width, height)
		styleMask:NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|NSWindowStyleMaskMiniaturizable|NSWindowStyleMaskResizable
		backing:NSBackingStoreBuffered
		defer:NO
	];
	GralWindowDelegate *delegate = [[GralWindowDelegate alloc] init];
	delegate->interface = *interface;
	[window setDelegate:delegate];
	[window setTitle:[[NSString alloc] initWithUTF8String:title]];
	GralView *view = [[GralView alloc] init];
	view->interface = *interface;
	[window setContentView:view];
	[window makeKeyAndOrderFront:nil];
	return (struct gral_window *)window;
}
