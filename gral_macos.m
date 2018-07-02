/*

Copyright (c) 2016-2018 Elias Aebi

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
	NSAttributedString *attributedString = [[NSAttributedString alloc]
		initWithString:[NSString stringWithUTF8String:text]
		attributes:[NSDictionary dictionaryWithObject:font forKey:NSFontAttributeName]
	];
	return (struct gral_text *)attributedString;
}

void gral_text_delete(struct gral_text *text) {
	[(NSAttributedString *)text release];
}

void gral_text_set_bold(struct gral_text *text, int start_index, int end_index) {
	// TODO: implement
}

float gral_text_get_width(struct gral_text *text, struct gral_draw_context *draw_context) {
	CTLineRef line = CTLineCreateWithAttributedString((CFAttributedStringRef)text);
	CGRect bounds = CTLineGetImageBounds(line, (CGContextRef)draw_context);
	CFRelease(line);
	return bounds.size.width;
}

float gral_text_index_to_x(struct gral_text *text, int index) {
	// convert UTF-8 index to UTF-16
	NSUInteger i16 = 0;
	NSString *string = [(NSAttributedString *)text string];
	for (int i8 = 0; i8 < index; i16++) {
		unichar c1 = [string characterAtIndex:i16];
		uint32_t c; // UTF-32 code point
		if ((c1 & 0xFC00) == 0xD800) {
			i16++;
			unichar c2 = [string characterAtIndex:i16];
			c = (((c1 & 0x03FF) << 10) | (c2 & 0x03FF)) + 0x10000;
		}
		else {
			c = c1;
		}
		if (c <= 0x7F) i8 += 1;
		else if (c <= 0x7FF) i8 += 2;
		else if (c <= 0xFFFF) i8 += 3;
		else i8 += 4;
	}
	CTLineRef line = CTLineCreateWithAttributedString((CFAttributedStringRef)text);
	CGFloat offset = CTLineGetOffsetForStringIndex(line, i16, NULL);
	CFRelease(line);
	return offset;
}

int gral_text_x_to_index(struct gral_text *text, float x) {
	CTLineRef line = CTLineCreateWithAttributedString((CFAttributedStringRef)text);
	CFIndex index = CTLineGetStringIndexForPosition(line, CGPointMake(x, 0.f));
	CFRelease(line);
	// convert UTF-16 index to UTF-8
	int i8 = 0;
	NSString *string = [(NSAttributedString *)text string];
	for (NSUInteger i16 = 0; i16 < index; i16++) {
		unichar c1 = [string characterAtIndex:i16];
		uint32_t c; // UTF-32 code point
		if ((c1 & 0xFC00) == 0xD800) {
			i16++;
			unichar c2 = [string characterAtIndex:i16];
			c = (((c1 & 0x03FF) << 10) | (c2 & 0x03FF)) + 0x10000;
		}
		else {
			c = c1;
		}
		if (c <= 0x7F) i8 += 1;
		else if (c <= 0x7FF) i8 += 2;
		else if (c <= 0xFFFF) i8 += 3;
		else i8 += 4;
	}
	return i8;
}

void gral_font_get_metrics(struct gral_window *window, float size, float *ascent, float *descent) {
	NSFont *font = [NSFont systemFontOfSize:size];
	if (ascent) *ascent = font.ascender;
	if (descent) *descent = -font.descender;
}

void gral_draw_context_draw_text(struct gral_draw_context *draw_context, struct gral_text *text, float x, float y, float red, float green, float blue, float alpha) {
	CTLineRef line = CTLineCreateWithAttributedString((CFAttributedStringRef)text);
	CGContextSetRGBFillColor((CGContextRef)draw_context, red, green, blue, alpha);
	CGContextTranslateCTM((CGContextRef)draw_context, x, y);
	CGContextSetTextMatrix((CGContextRef)draw_context, CGAffineTransformMakeScale(1.f, -1.f));
	CFArrayRef glyphRuns = CTLineGetGlyphRuns(line);
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
	CFRelease(line);
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

void gral_draw_context_fill(struct gral_draw_context *draw_context, float red, float green, float blue, float alpha) {
	CGContextSetRGBFillColor((CGContextRef)draw_context, red, green, blue, alpha);
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
	CGContextSetRGBStrokeColor((CGContextRef)draw_context, red, green, blue, alpha);
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

@interface GralView: NSView<NSTextInputClient> {
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
	[[self inputContext] handleEvent:event];
}
- (void)keyUp:(NSEvent *)event {

}
- (void)timer:(NSTimer *)timer {
	if (!interface.timer(user_data)) {
		[timer invalidate];
	}
}
// NSTextInputClient implementation
- (BOOL)hasMarkedText {
	return NO;
}
- (NSRange)markedRange {
	return NSMakeRange(NSNotFound, 0);
}
- (NSRange)selectedRange {
	return NSMakeRange(NSNotFound, 0);
}
- (void)setMarkedText:(id)string selectedRange:(NSRange)selectedRange replacementRange:(NSRange)replacementRange {

}
- (void)unmarkText {

}
- (NSArray *)validAttributesForMarkedText {
	return [NSArray array];
}
- (NSAttributedString *)attributedSubstringForProposedRange:(NSRange)range actualRange:(NSRangePointer)actualRange {
	return nil;
}
- (void)insertText:(id)string replacementRange:(NSRange)replacementRange {
	if ([string isKindOfClass:[NSString class]]) {
		interface.text([(NSString *)string UTF8String], user_data);
	}
}
- (NSUInteger)characterIndexForPoint:(NSPoint)point {
	return NSNotFound;
}
- (NSRect)firstRectForCharacterRange:(NSRange)range actualRange:(NSRangePointer)actualRange {
	return NSMakeRect(0, 0, 0, 0);
}
- (void)doCommandBySelector:(SEL)selector {

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
	return (struct gral_window *)window;
}

void gral_window_delete(struct gral_window *window) {
	[(GralWindow *)window release];
}

void gral_window_show(struct gral_window *window) {
	[(GralWindow *)window makeKeyAndOrderFront:nil];
}

void gral_window_hide(struct gral_window *window) {
	// TODO: implement
}

void gral_window_request_redraw(struct gral_window *window) {
	[[(GralWindow *)window contentView] setNeedsDisplay:YES];
}

void gral_window_set_minimum_size(struct gral_window *window, int minimum_width, int minimum_height) {
	[(GralWindow *)window setContentMinSize:NSMakeSize(minimum_width, minimum_height)];
}

void gral_window_set_cursor(struct gral_window *window, int cursor) {
	// TODO: implement
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

void gral_window_set_timer(struct gral_window *window, int milliseconds) {
	GralView *view = [(GralWindow *)window contentView];
	[NSTimer scheduledTimerWithTimeInterval:milliseconds/1000.0 target:view selector:@selector(timer:) userInfo:nil repeats:YES];
}


/*=========
    FILE
 =========*/

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

struct gral_file *gral_file_open_read(const char *path) {
	int fd = open(path, O_RDONLY);
	return fd == -1 ? NULL : (struct gral_file *)(intptr_t)fd;
}

struct gral_file *gral_file_open_write(const char *path) {
	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	return fd == -1 ? NULL : (struct gral_file *)(intptr_t)fd;
}

struct gral_file *gral_file_get_stdin(void) {
	return (struct gral_file *)STDIN_FILENO;
}

struct gral_file *gral_file_get_stdout(void) {
	return (struct gral_file *)STDOUT_FILENO;
}

struct gral_file *gral_file_get_stderr(void) {
	return (struct gral_file *)STDERR_FILENO;
}

void gral_file_close(struct gral_file *file) {
	close((int)(intptr_t)file);
}

size_t gral_file_read(struct gral_file *file, void *buffer, size_t size) {
	return read((int)(intptr_t)file, buffer, size);
}

void gral_file_write(struct gral_file *file, const void *buffer, size_t size) {
	write((int)(intptr_t)file, buffer, size);
}

size_t gral_file_get_size(struct gral_file *file) {
	struct stat stat;
	fstat((int)(intptr_t)file, &stat);
	return stat.st_size;
}


/*==========
    AUDIO
 ==========*/

static void audio_callback(void *user_data, AudioQueueRef queue, AudioQueueBufferRef buffer) {
	int (*callback)(int16_t *buffer, int frames) = user_data;
	int frames = buffer->mAudioDataBytesCapacity / (2 * sizeof(int16_t));
	if (callback(buffer->mAudioData, frames, NULL)) {
		buffer->mAudioDataByteSize = buffer->mAudioDataBytesCapacity;
		AudioQueueEnqueueBuffer(queue, buffer, buffer->mPacketDescriptionCount, buffer->mPacketDescriptions);
	}
	else {
		AudioQueueStop(queue, NO);
		CFRunLoopStop(CFRunLoopGetCurrent());
	}
}

void gral_audio_play(int (*callback)(int16_t *buffer, int frames, void *user_data), void *user_data) {
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
