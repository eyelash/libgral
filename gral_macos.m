/*

Copyright (c) 2016-2017 Elias Aebi

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#include "gral.h"
#import <Cocoa/Cocoa.h>
#import <AudioToolbox/AudioToolbox.h>


/*================
    APPLICATION
 ================*/

@interface GralApplicationDelegate: NSObject<NSApplicationDelegate> {
@public
	struct gral_application_interface interface;
	void *user_data;
}
@end
@implementation GralApplicationDelegate
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
	return YES;
}
- (void)applicationDidFinishLaunching:(NSNotification *) notification {
	interface.initialize(user_data);
}
@end

struct gral_application *gral_application_create(const char *id, const struct gral_application_interface *interface, void *user_data) {
	NSApplication *application = [NSApplication sharedApplication];
	GralApplicationDelegate *delegate = [[GralApplicationDelegate alloc] init];
	delegate->interface = *interface;
	delegate->user_data = user_data;
	[application setDelegate:delegate];
	return (struct gral_application *)application;
}

void gral_application_delete(struct gral_application *application) {

}

int gral_application_run(struct gral_application *application, int argc, char **argv) {
	[NSApp run];
	return 0;
}


/*============
    DRAWING
 ============*/

struct gral_text *gral_text_create(struct gral_window *window, const char *text, float size) {
	NSFont *font = [NSFont systemFontOfSize:size];
	NSAttributedString *attributedString = [[NSAttributedString alloc] initWithString:[NSString stringWithUTF8String:text] attributes:@{NSFontAttributeName:font}];
	CTLineRef line = CTLineCreateWithAttributedString((CFAttributedStringRef)attributedString);
	[attributedString release];
	return (struct gral_text *)line;
}

void gral_text_delete(struct gral_text *text) {
	CFRelease((CTLineRef)text);
}

float gral_text_get_width(struct gral_text *text, struct gral_draw_context *draw_context) {
	CGRect bounds = CTLineGetImageBounds((CTLineRef)text, (CGContextRef)draw_context);
	return bounds.size.width;
}

void gral_font_get_metrics(struct gral_window *window, float size, float *ascent, float *descent) {
	NSFont *font = [NSFont systemFontOfSize:size];
	if (ascent) *ascent = font.ascender;
	if (descent) *descent = -font.descender;
}

void gral_draw_context_draw_text(struct gral_draw_context *draw_context, struct gral_text *text, float x, float y, float red, float green, float blue, float alpha) {
	CGContextSetFillColorWithColor((CGContextRef)draw_context, [[NSColor colorWithRed:red green:green blue:blue alpha:alpha] CGColor]);
	CGContextTranslateCTM((CGContextRef)draw_context, x, y);
	CGContextSetTextMatrix((CGContextRef)draw_context, CGAffineTransformMakeScale(1.f, -1.f));
	CFArrayRef glyphRuns = CTLineGetGlyphRuns((CTLineRef)text);
	for (int i = 0; i < CFArrayGetCount(glyphRuns); i++) {
		CTRunRef run = CFArrayGetValueAtIndex(glyphRuns, i);
		const CGPoint *positions = CTRunGetPositionsPtr(run);
		const CGGlyph *glyphs = CTRunGetGlyphsPtr(run);
		int count = CTRunGetGlyphCount(run);
		CFDictionaryRef attributes = CTRunGetAttributes(run);
		CTFontRef font = CFDictionaryGetValue(attributes, kCTFontAttributeName);
		CTFontDrawGlyphs(font, glyphs, positions, count, (CGContextRef)draw_context);
	}
	CGContextTranslateCTM((CGContextRef)draw_context, -x, -y);
}

void gral_draw_context_close_path(struct gral_draw_context *draw_context) {
	CGContextClosePath((CGContextRef)draw_context);
}

void gral_draw_context_move_to(struct gral_draw_context *draw_context, float x, float y) {
	CGContextMoveToPoint((CGContextRef)draw_context, x, y);
}

void gral_draw_context_line_to(struct gral_draw_context *draw_context, float x, float y) {
	CGContextAddLineToPoint((CGContextRef)draw_context, x, y);
}

void gral_draw_context_curve_to(struct gral_draw_context *draw_context, float x1, float y1, float x2, float y2, float x, float y) {
	CGContextAddCurveToPoint((CGContextRef)draw_context, x1, y1, x2, y2, x, y);
}

void gral_draw_context_add_rectangle(struct gral_draw_context *draw_context, float x, float y, float width, float height) {
	CGContextAddRect((CGContextRef)draw_context, CGRectMake(x, y, width, height));
}

void gral_draw_context_add_arc(struct gral_draw_context *draw_context, float cx, float cy, float radius, float start_angle, float sweep_angle) {
	CGContextAddArc((CGContextRef)draw_context, cx, cy, radius, start_angle, start_angle + sweep_angle, sweep_angle < 0.f);
}

void gral_draw_context_fill(struct gral_draw_context *draw_context, float red, float green, float blue, float alpha) {
	CGContextSetFillColorWithColor((CGContextRef)draw_context, [[NSColor colorWithRed:red green:green blue:blue alpha:alpha] CGColor]);
	CGContextFillPath((CGContextRef)draw_context);
}

void gral_draw_context_fill_linear_gradient(struct gral_draw_context *draw_context, float start_x, float start_y, float end_x, float end_y, const struct gral_gradient_stop *stops, int count) {
	CGFloat *locations = malloc(count*sizeof(CGFloat));
	CFMutableArrayRef colorArray = CFArrayCreateMutable(kCFAllocatorDefault, count, &kCFTypeArrayCallBacks);
	for (int i = 0; i < count; i++) {
		locations[i] = stops[i].position;
		CFArrayAppendValue(colorArray, [[NSColor colorWithRed:stops[i].red green:stops[i].green blue:stops[i].blue alpha:stops[i].alpha] CGColor]);
	}
	CGGradientRef gradient = CGGradientCreateWithColors(NULL, colorArray, locations);
	free(locations);
	CFRelease(colorArray);
	CGContextSaveGState((CGContextRef)draw_context);
	CGContextClip((CGContextRef)draw_context);
	CGContextDrawLinearGradient((CGContextRef)draw_context, gradient, CGPointMake(start_x, start_y), CGPointMake(end_x, end_y), kCGGradientDrawsBeforeStartLocation|kCGGradientDrawsAfterEndLocation);
	CGContextRestoreGState((CGContextRef)draw_context);
	CGGradientRelease(gradient);
}

void gral_draw_context_stroke(struct gral_draw_context *draw_context, float line_width, float red, float green, float blue, float alpha) {
	CGContextSetLineWidth((CGContextRef)draw_context, line_width);
	CGContextSetLineCap((CGContextRef)draw_context, kCGLineCapRound);
	CGContextSetLineJoin((CGContextRef)draw_context, kCGLineJoinRound);
	CGContextSetStrokeColorWithColor((CGContextRef)draw_context, [[NSColor colorWithRed:red green:green blue:blue alpha:alpha] CGColor]);
	CGContextStrokePath((CGContextRef)draw_context);
}

void gral_draw_context_push_clip(struct gral_draw_context *draw_context) {
	CGContextSaveGState((CGContextRef)draw_context);
	CGContextClip((CGContextRef)draw_context);
}

void gral_draw_context_pop_clip(struct gral_draw_context *draw_context) {
	CGContextRestoreGState((CGContextRef)draw_context);
}


/*===========
    WINDOW
 ===========*/

@interface GralWindow: NSWindow<NSWindowDelegate> {
@public
	struct gral_window_interface interface;
	void *user_data;
}
@end
@implementation GralWindow
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
	CGContextRef context = [[NSGraphicsContext currentContext] graphicsPort];
	interface.draw((struct gral_draw_context *)context, user_data);
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
	NSPoint location = [event locationInWindow];
	location = [self convertPoint:location fromView:nil];
	interface.mouse_button_press(location.x, location.y, GRAL_PRIMARY_MOUSE_BUTTON, user_data);
}
- (void)rightMouseDown:(NSEvent *)event {
	NSPoint location = [event locationInWindow];
	location = [self convertPoint:location fromView:nil];
	interface.mouse_button_press(location.x, location.y, GRAL_SECONDARY_MOUSE_BUTTON, user_data);
}
- (void)otherMouseDown:(NSEvent *)event {
	//interface.mouse_button_press(3, user_data);
}
- (void)mouseUp:(NSEvent *)event {
	NSPoint location = [event locationInWindow];
	location = [self convertPoint:location fromView:nil];
	interface.mouse_button_release(location.x, location.y, GRAL_PRIMARY_MOUSE_BUTTON, user_data);
}
- (void)rightMouseUp:(NSEvent *)event {
	NSPoint location = [event locationInWindow];
	location = [self convertPoint:location fromView:nil];
	interface.mouse_button_release(location.x, location.y, GRAL_SECONDARY_MOUSE_BUTTON, user_data);
}
- (void)otherMouseUp:(NSEvent *)event {
	//interface.mouse_button_release(3, user_data);
}
- (void)scrollWheel:(NSEvent *)event {
	interface.scroll([event scrollingDeltaX], [event scrollingDeltaY], user_data);
}
- (void)keyDown:(NSEvent *)event {

}
- (void)keyUp:(NSEvent *)event {

}
@end

struct gral_window *gral_window_create(struct gral_application *application, int width, int height, const char *title, const struct gral_window_interface *interface, void *user_data) {
	GralWindow *window = [[GralWindow alloc]
		initWithContentRect:CGRectMake(0, 0, width, height)
		styleMask:NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|NSWindowStyleMaskMiniaturizable|NSWindowStyleMaskResizable
		backing:NSBackingStoreBuffered
		defer:NO
	];
	window->interface = *interface;
	window->user_data = user_data;
	[window setDelegate:window];
	[window setTitle:[NSString stringWithUTF8String:title]];
	GralView *view = [[GralView alloc] init];
	view->interface = *interface;
	view->user_data = user_data;
	NSTrackingArea *trackingArea = [[NSTrackingArea alloc]
		initWithRect:CGRectMake(0, 0, width, height)
		options:NSTrackingMouseEnteredAndExited|NSTrackingMouseMoved|NSTrackingActiveAlways|NSTrackingInVisibleRect
		owner:view
		userInfo:nil
	];
	[view addTrackingArea:trackingArea];
	[trackingArea release];
	[window setContentView:view];
	[view release];
	[window makeKeyAndOrderFront:nil];
	return (struct gral_window *)window;
}

void gral_window_delete(struct gral_window *window) {
	[(GralWindow *)window release];
}

void gral_window_request_redraw(struct gral_window *window) {
	[[(GralWindow *)window contentView] setNeedsDisplay:YES];
}

void gral_window_set_minimum_size(struct gral_window *window, int minimum_width, int minimum_height) {
	[(GralWindow *)window setContentMinSize:NSMakeSize(minimum_width, minimum_height)];
}

void gral_window_show_open_file_dialog(struct gral_window *window, void (*callback)(const char *file, void *user_data), void *user_data) {
	NSOpenPanel *panel = [NSOpenPanel openPanel];
	if ([panel runModal] == NSFileHandlingPanelOKButton) {
		callback([[[panel URL] path] UTF8String], user_data);
	}
}

void gral_window_show_save_file_dialog(struct gral_window *window, void (*callback)(const char *file, void *user_data), void *user_data) {
	NSSavePanel *panel = [NSSavePanel savePanel];
	if ([panel runModal] == NSFileHandlingPanelOKButton) {
		callback([[[panel URL] path] UTF8String], user_data);
	}
}

void gral_window_clipboard_copy(struct gral_window *window, const char *text) {
	NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
	[pasteboard clearContents];
	[pasteboard setString:[NSString stringWithUTF8String:text] forType:NSPasteboardTypeString];
}

void gral_window_clipboard_request_paste(struct gral_window *window) {
	NSString *text = [[NSPasteboard generalPasteboard] stringForType:NSPasteboardTypeString];
	if (text) {
		GralView *view = (GralView *)[(GralWindow *)window contentView];
		view->interface.paste([text UTF8String], view->user_data);
	}
}


/*=========
    FILE
 =========*/

void gral_file_read(const char *file, void (*callback)(const char *data, size_t size, void *user_data), void *user_data) {
	NSData *data = [NSData dataWithContentsOfFile:[NSString stringWithUTF8String:file]];
	if (data) {
		callback(data.bytes, data.length, user_data);
	}
}

void gral_file_write(const char *file, const char *data, size_t size) {
	[[NSData dataWithBytesNoCopy:(char *)data length:size freeWhenDone:NO] writeToFile:[NSString stringWithUTF8String:file] atomically:YES];
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
