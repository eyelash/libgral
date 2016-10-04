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
#import <Cocoa/Cocoa.h>
#import <AudioToolbox/AudioToolbox.h>

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
	@public
		struct gral_window_interface interface;
		void *user_data;
}
@end
@implementation GralWindowDelegate
- (BOOL)windowShouldClose:(id)sender {
	return interface.close(user_data);
}
@end

@interface GralView: NSView {
	@public
		struct gral_window_interface interface;
		void *user_data;
}
@end
@implementation GralView
- (BOOL)acceptsFirstResponder {
	return YES;
}
- (BOOL)isFlipped {
	return YES;
}
- (void)drawRect:(NSRect)rect {
	interface.draw(NULL, user_data);
}
- (void)setFrameSize:(NSSize)size {
	[super setFrameSize:size];
	interface.resize(size.width, size.height, user_data);
}
- (void)mouseEntered:(NSEvent *)event {
	interface.mouse_enter(user_data);
}
- (void)mouseExited:(NSEvent *)event {
	interface.mouse_leave(user_data);
}
- (void)mouseMoved:(NSEvent *)event {
	NSPoint location = [event locationInWindow];
	location = [self convertPoint:location fromView:nil];
	interface.mouse_move(location.x, location.y, user_data);
}
- (void)mouseDragged:(NSEvent *)event {
	[self mouseMoved:event];
}
- (void)rightMouseDragged:(NSEvent *)event {
	[self mouseMoved:event];
}
- (void)otherMouseDragged:(NSEvent *)event {
	[self mouseMoved:event];
}
- (void)mouseDown:(NSEvent *)event {
	interface.mouse_button_press(1, user_data);
}
- (void)rightMouseDown:(NSEvent *)event {
	interface.mouse_button_press(2, user_data);
}
- (void)otherMouseDown:(NSEvent *)event {
	//interface.mouse_button_press(3, user_data);
}
- (void)mouseUp:(NSEvent *)event {
	interface.mouse_button_release(1, user_data);
}
- (void)rightMouseUp:(NSEvent *)event {
	interface.mouse_button_release(2, user_data);
}
- (void)otherMouseUp:(NSEvent *)event {
	//interface.mouse_button_release(3, user_data);
}
- (void)scrollWheel:(NSEvent *)event {
	//interface.scroll([event scrollingDeltaX], [event scrollingDeltaY], user_data);
}
- (void)keyDown:(NSEvent *)event {

}
- (void)keyUp:(NSEvent *)event {

}
@end

struct gral_window *gral_window_create(int width, int height, const char *title, struct gral_window_interface *interface, void *user_data) {
	GralWindow *window = [[GralWindow alloc]
		initWithContentRect:CGRectMake(0, 0, width, height)
		styleMask:NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|NSWindowStyleMaskMiniaturizable|NSWindowStyleMaskResizable
		backing:NSBackingStoreBuffered
		defer:NO
	];
	GralWindowDelegate *delegate = [[GralWindowDelegate alloc] init];
	delegate->interface = *interface;
	delegate->user_data = user_data;
	[window setDelegate:delegate];
	[window setTitle:[[NSString alloc] initWithUTF8String:title]];
	GralView *view = [[GralView alloc] init];
	view->interface = *interface;
	view->user_data = user_data;
	NSTrackingArea *trackingArea = [[NSTrackingArea alloc]
		initWithRect:CGRectMake(0, 0, width, height)
		options:NSTrackingMouseEnteredAndExited|NSTrackingMouseMoved|NSTrackingActiveInKeyWindow|NSTrackingInVisibleRect
		owner:view
		userInfo:nil
	];
	[view addTrackingArea:trackingArea];
	[window setContentView:view];
	[window makeKeyAndOrderFront:nil];
	return (struct gral_window *)window;
}


/*==========
    AUDIO
 ==========*/

static void audio_callback(void *user_data, AudioQueueRef queue, AudioQueueBufferRef buffer) {
	int (*callback)(int16_t *buffer, int frames) = user_data;
	int frames = buffer->mAudioDataBytesCapacity / (2 * sizeof(int16_t));
	if (callback(buffer->mAudioData, frames)) {
		buffer->mAudioDataByteSize = buffer->mAudioDataBytesCapacity;
		AudioQueueEnqueueBuffer(queue, buffer, buffer->mPacketDescriptionCount, buffer->mPacketDescriptions);
	}
	else {
		AudioQueueStop(queue, NO);
		CFRunLoopStop(CFRunLoopGetCurrent());
	}
}

void gral_audio_play(int (*callback)(int16_t *buffer, int frames)) {
	AudioQueueRef queue;
	AudioStreamBasicDescription format = {
		.mSampleRate = 44100,
		.mFormatID = kAudioFormatLinearPCM,
		.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger|kLinearPCMFormatFlagIsPacked,
		.mBytesPerPacket = 2 * sizeof(int16_t),
		.mFramesPerPacket = 1,
		.mBytesPerFrame = 2 * sizeof(int16_t),
		.mChannelsPerFrame = 2,
		.mBitsPerChannel = sizeof(int16_t) * 8
	};
	AudioQueueNewOutput(&format, &audio_callback, callback, CFRunLoopGetCurrent(), kCFRunLoopCommonModes, 0, &queue);
	AudioQueueBufferRef buffers[3];
	for (int i = 0; i < 3; i++) {
		AudioQueueAllocateBuffer(queue, 1024 * 2 * sizeof(int16_t), &buffers[i]);
		audio_callback(callback, queue, buffers[i]);
	}
	AudioQueueStart(queue, NULL);
	CFRunLoopRun();
	AudioQueueDispose(queue, NO);
}
