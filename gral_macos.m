/*

Copyright (c) 2016-2021 Elias Aebi

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#include "gral.h"
#import <Cocoa/Cocoa.h>

static NSUInteger get_next_code_point(NSString *string, NSUInteger i, uint32_t *code_point) {
	unichar c1 = [string characterAtIndex:i];
	if ((c1 & 0xFC00) == 0xD800) {
		unichar c2 = [string characterAtIndex:i+1];
		*code_point = (((c1 & 0x03FF) << 10) | (c2 & 0x03FF)) + 0x10000;
		return 2;
	}
	else {
		*code_point = c1;
		return 1;
	}
}
static NSUInteger utf8_index_to_utf16(NSString *string, int index) {
	int i8 = 0;
	NSUInteger i16 = 0;
	while (i8 < index) {
		uint32_t c; // UTF-32 code point
		i16 += get_next_code_point(string, i16, &c);
		if (c <= 0x7F) i8 += 1;
		else if (c <= 0x7FF) i8 += 2;
		else if (c <= 0xFFFF) i8 += 3;
		else i8 += 4;
	}
	return i16;
}
static int utf16_index_to_utf8(NSString *string, NSUInteger index) {
	int i8 = 0;
	NSUInteger i16 = 0;
	while (i16 < index) {
		uint32_t c; // UTF-32 code point
		i16 += get_next_code_point(string, i16, &c);
		if (c <= 0x7F) i8 += 1;
		else if (c <= 0x7FF) i8 += 2;
		else if (c <= 0xFFFF) i8 += 3;
		else i8 += 4;
	}
	return i8;
}


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
	CTLineRef line = CTLineCreateWithAttributedString((CFAttributedStringRef)text);
	NSString *string = [(NSAttributedString *)text string];
	CGFloat offset = CTLineGetOffsetForStringIndex(line, utf8_index_to_utf16(string, index), NULL);
	CFRelease(line);
	return offset;
}

int gral_text_x_to_index(struct gral_text *text, float x) {
	CTLineRef line = CTLineCreateWithAttributedString((CFAttributedStringRef)text);
	CFIndex index = CTLineGetStringIndexForPosition(line, CGPointMake(x, 0.f));
	CFRelease(line);
	NSString *string = [(NSAttributedString *)text string];
	return utf16_index_to_utf8(string, index);
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
	CGFloat components[count*4];
	CGFloat locations[count];
	for (int i = 0; i < count; i++) {
		components[i*4+0] = stops[i].red;
		components[i*4+1] = stops[i].green;
		components[i*4+2] = stops[i].blue;
		components[i*4+3] = stops[i].alpha;
		locations[i] = stops[i].position;
	}
	CGColorSpaceRef color_space = CGColorSpaceCreateDeviceRGB();
	CGGradientRef gradient = CGGradientCreateWithColorComponents(color_space, components, locations, count);
	CGContextSaveGState((CGContextRef)draw_context);
	CGContextClip((CGContextRef)draw_context);
	CGContextDrawLinearGradient((CGContextRef)draw_context, gradient, CGPointMake(start_x, start_y), CGPointMake(end_x, end_y), kCGGradientDrawsBeforeStartLocation|kCGGradientDrawsAfterEndLocation);
	CGContextRestoreGState((CGContextRef)draw_context);
	CGGradientRelease(gradient);
	CGColorSpaceRelease(color_space);
}

void gral_draw_context_stroke(struct gral_draw_context *draw_context, float line_width, float red, float green, float blue, float alpha) {
	CGContextSetLineWidth((CGContextRef)draw_context, line_width);
	CGContextSetLineCap((CGContextRef)draw_context, kCGLineCapRound);
	CGContextSetLineJoin((CGContextRef)draw_context, kCGLineJoinRound);
	CGContextSetRGBStrokeColor((CGContextRef)draw_context, red, green, blue, alpha);
	CGContextStrokePath((CGContextRef)draw_context);
}

void gral_draw_context_draw_clipped(struct gral_draw_context *draw_context, void (*callback)(struct gral_draw_context *draw_context, void *user_data), void *user_data) {
	CGContextSaveGState((CGContextRef)draw_context);
	CGContextClip((CGContextRef)draw_context);
	callback(draw_context, user_data);
	CGContextRestoreGState((CGContextRef)draw_context);
}

void gral_draw_context_draw_transformed(struct gral_draw_context *draw_context, float a, float b, float c, float d, float e, float f, void (*callback)(struct gral_draw_context *draw_context, void *user_data), void *user_data) {
	CGContextSaveGState((CGContextRef)draw_context);
	CGContextConcatCTM((CGContextRef)draw_context, CGAffineTransformMake(a, b, c, d, e, f));
	callback(draw_context, user_data);
	CGContextRestoreGState((CGContextRef)draw_context);
}


/*===========
    WINDOW
 ===========*/

static int get_key(NSEvent *event) {
	// TODO: use [event charactersByApplyingModifiers:0] (macOS 10.15+)
	NSString *characters = [[event charactersIgnoringModifiers] lowercaseString];
	if ([characters length] == 0) {
		return 0;
	}
	uint32_t code_point;
	get_next_code_point(characters, 0, &code_point);
	switch (code_point) {
	case NSCarriageReturnCharacter:
		return GRAL_KEY_ENTER;
	case NSDeleteCharacter:
		return GRAL_KEY_BACKSPACE;
	case NSDeleteFunctionKey:
		return GRAL_KEY_DELETE;
	case NSLeftArrowFunctionKey:
		return GRAL_KEY_ARROW_LEFT;
	case NSUpArrowFunctionKey:
		return GRAL_KEY_ARROW_UP;
	case NSRightArrowFunctionKey:
		return GRAL_KEY_ARROW_RIGHT;
	case NSDownArrowFunctionKey:
		return GRAL_KEY_ARROW_DOWN;
	case NSPageUpFunctionKey:
		return GRAL_KEY_PAGE_UP;
	case NSPageDownFunctionKey:
		return GRAL_KEY_PAGE_DOWN;
	case NSHomeFunctionKey:
		return GRAL_KEY_HOME;
	case NSEndFunctionKey:
		return GRAL_KEY_END;
	case '\e':
		return GRAL_KEY_ESCAPE;
	default:
		return code_point;
	}
}
static int get_modifiers(NSEvent *event) {
	NSEventModifierFlags modifier_flags = [event modifierFlags];
	int modifiers = 0;
	if (modifier_flags & NSEventModifierFlagControl) modifiers |= GRAL_MODIFIER_CONTROL;
	if (modifier_flags & NSEventModifierFlagOption) modifiers |= GRAL_MODIFIER_ALT;
	if (modifier_flags & NSEventModifierFlagCommand) modifiers |= GRAL_MODIFIER_CONTROL;
	if (modifier_flags & NSEventModifierFlagShift) modifiers |= GRAL_MODIFIER_SHIFT;
	return modifiers;
}

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
	CGContextRef context = [[NSGraphicsContext currentContext] CGContext];
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
	NSPoint location = [self convertPoint:[event locationInWindow] fromView:nil];
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
	NSPoint location = [self convertPoint:[event locationInWindow] fromView:nil];
	int modifiers = get_modifiers(event);
	interface.mouse_button_press(location.x, location.y, GRAL_PRIMARY_MOUSE_BUTTON, modifiers, user_data);
	if ([event clickCount] == 2) {
		interface.double_click(location.x, location.y, GRAL_PRIMARY_MOUSE_BUTTON, modifiers, user_data);
	}
}
- (void)rightMouseDown:(NSEvent *)event {
	NSPoint location = [self convertPoint:[event locationInWindow] fromView:nil];
	int modifiers = get_modifiers(event);
	interface.mouse_button_press(location.x, location.y, GRAL_SECONDARY_MOUSE_BUTTON, modifiers, user_data);
	if ([event clickCount] == 2) {
		interface.double_click(location.x, location.y, GRAL_SECONDARY_MOUSE_BUTTON, modifiers, user_data);
	}
}
- (void)otherMouseDown:(NSEvent *)event {
	NSPoint location = [self convertPoint:[event locationInWindow] fromView:nil];
	int modifiers = get_modifiers(event);
	interface.mouse_button_press(location.x, location.y, GRAL_MIDDLE_MOUSE_BUTTON, modifiers, user_data);
	if ([event clickCount] == 2) {
		interface.double_click(location.x, location.y, GRAL_MIDDLE_MOUSE_BUTTON, modifiers, user_data);
	}
}
- (void)mouseUp:(NSEvent *)event {
	NSPoint location = [self convertPoint:[event locationInWindow] fromView:nil];
	interface.mouse_button_release(location.x, location.y, GRAL_PRIMARY_MOUSE_BUTTON, user_data);
}
- (void)rightMouseUp:(NSEvent *)event {
	NSPoint location = [self convertPoint:[event locationInWindow] fromView:nil];
	interface.mouse_button_release(location.x, location.y, GRAL_SECONDARY_MOUSE_BUTTON, user_data);
}
- (void)otherMouseUp:(NSEvent *)event {
	NSPoint location = [self convertPoint:[event locationInWindow] fromView:nil];
	interface.mouse_button_release(location.x, location.y, GRAL_MIDDLE_MOUSE_BUTTON, user_data);
}
- (void)scrollWheel:(NSEvent *)event {
	interface.scroll([event scrollingDeltaX], [event scrollingDeltaY], user_data);
}
- (void)keyDown:(NSEvent *)event {
	[[self inputContext] handleEvent:event];
	int key = get_key(event);
	if (key) {
		interface.key_press(key, [event keyCode], get_modifiers(event), user_data);
	}
}
- (void)keyUp:(NSEvent *)event {
	int key = get_key(event);
	if (key) {
		interface.key_release(key, [event keyCode], user_data);
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
	[window makeKeyAndOrderFront:nil];
	return (struct gral_window *)window;
}

void gral_window_delete(struct gral_window *window) {
	[(GralWindow *)window release];
}

void gral_window_request_redraw(struct gral_window *window, int x, int y, int width, int height) {
	[[(GralWindow *)window contentView] setNeedsDisplayInRect:NSMakeRect(x, y, width, height)];
}

void gral_window_set_minimum_size(struct gral_window *window, int minimum_width, int minimum_height) {
	[(GralWindow *)window setContentMinSize:NSMakeSize(minimum_width, minimum_height)];
}

static NSCursor *get_cursor(int cursor) {
	static NSCursor *transparent_cursor = NULL;
	switch (cursor) {
	case GRAL_CURSOR_DEFAULT:
		return NSCursor.arrowCursor;
	case GRAL_CURSOR_HAND:
		return NSCursor.pointingHandCursor;
	case GRAL_CURSOR_TEXT:
		return NSCursor.IBeamCursor;
	case GRAL_CURSOR_HORIZONTAL_ARROWS:
		return NSCursor.resizeLeftRightCursor;
	case GRAL_CURSOR_VERTICAL_ARROWS:
		return NSCursor.resizeUpDownCursor;
	case GRAL_CURSOR_NONE:
		if (transparent_cursor == NULL) {
			NSImage *image = [[NSImage alloc] initWithSize:NSMakeSize(1, 1)];
			transparent_cursor = [[NSCursor alloc] initWithImage:image hotSpot:NSMakePoint(0, 0)];
			[image release];
		}
		return transparent_cursor;
	default:
		return NULL;
	}
}
void gral_window_set_cursor(struct gral_window *window, int cursor) {
	[get_cursor(cursor) set];
}

void gral_window_warp_cursor(struct gral_window *window_, float x, float y) {
	GralWindow *window = (GralWindow *)window_;
	NSPoint point = [[window contentView] convertPoint:NSMakePoint(x, y) toView:nil];
	point = NSMakePoint(NSMinX(window.frame) + point.x, NSMaxY(NSScreen.screens[0].frame) - (NSMinY(window.frame) + point.y));
	CGWarpMouseCursorPosition(point);
}

void gral_window_show_open_file_dialog(struct gral_window *window, void (*callback)(const char *file, void *user_data), void *user_data) {
	NSOpenPanel *panel = [NSOpenPanel openPanel];
	if ([panel runModal] == NSModalResponseOK) {
		callback([[[panel URL] path] UTF8String], user_data);
	}
}

void gral_window_show_save_file_dialog(struct gral_window *window, void (*callback)(const char *file, void *user_data), void *user_data) {
	NSSavePanel *panel = [NSSavePanel savePanel];
	if ([panel runModal] == NSModalResponseOK) {
		callback([[[panel URL] path] UTF8String], user_data);
	}
}

void gral_window_clipboard_copy(struct gral_window *window, const char *text) {
	NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
	[pasteboard clearContents];
	[pasteboard setString:[NSString stringWithUTF8String:text] forType:NSPasteboardTypeString];
}

void gral_window_clipboard_paste(struct gral_window *window, void (*callback)(const char *text, void *user_data), void *user_data) {
	NSString *text = [[NSPasteboard generalPasteboard] stringForType:NSPasteboardTypeString];
	if (text) {
		callback([text UTF8String], user_data);
	}
}

@interface TimerCallbackObject: NSObject {
@public
	int (*callback)(void *user_data);
	void *user_data;
}
@end
@implementation TimerCallbackObject
- (void)invoke:(NSTimer *)timer {
	if (!callback(user_data)) {
		[timer invalidate];
	}
}
@end
struct gral_timer *gral_window_create_timer(struct gral_window *window, int milliseconds, int (*callback)(void *user_data), void *user_data) {
	TimerCallbackObject *callback_object = [[TimerCallbackObject alloc] init];
	callback_object->callback = callback;
	callback_object->user_data = user_data;
	NSTimer *timer = [NSTimer scheduledTimerWithTimeInterval:milliseconds/1000.0 target:callback_object selector:@selector(invoke:) userInfo:nil repeats:YES];
	[callback_object release];
	return (struct gral_timer *)timer;
}

void gral_window_delete_timer(struct gral_window *window, struct gral_timer *timer) {
	[(NSTimer *)timer invalidate];
}

@interface MainThreadCallbackObject: NSObject {
@public
	void (*callback)(void *user_data);
	void *user_data;
}
@end
@implementation MainThreadCallbackObject
- (void)invoke:(id)object {
	callback(user_data);
}
@end
void gral_window_run_on_main_thread(struct gral_window *window, void (*callback)(void *user_data), void *user_data) {
	MainThreadCallbackObject *callback_object = [[MainThreadCallbackObject alloc] init];
	callback_object->callback = callback;
	callback_object->user_data = user_data;
	[callback_object performSelectorOnMainThread:@selector(invoke:) withObject:nil waitUntilDone:NO];
	[callback_object release];
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

#import <AudioToolbox/AudioToolbox.h>

#define FRAMES 1024

typedef struct {
	int (*callback)(float *buffer, int frames, void *user_data);
	void *user_data;
} AudioCallbackData;

static void audio_callback(void *user_data, AudioQueueRef queue, AudioQueueBufferRef buffer) {
	AudioCallbackData *callback_data = user_data;
	int frames = buffer->mAudioDataBytesCapacity / (2 * sizeof(float));
	frames = callback_data->callback(buffer->mAudioData, frames, callback_data->user_data);
	if (frames > 0) {
		buffer->mAudioDataByteSize = frames * 2 * sizeof(float);
		AudioQueueEnqueueBuffer(queue, buffer, buffer->mPacketDescriptionCount, buffer->mPacketDescriptions);
	}
	else {
		AudioQueueStop(queue, NO);
		CFRunLoopStop(CFRunLoopGetCurrent());
	}
}

void gral_audio_play(int (*callback)(float *buffer, int frames, void *user_data), void *user_data) {
	AudioCallbackData callback_data = {callback, user_data};
	AudioQueueRef queue;
	AudioStreamBasicDescription format;
	format.mFormatID = kAudioFormatLinearPCM;
	format.mFormatFlags = kLinearPCMFormatFlagIsFloat;
	format.mSampleRate = 44100;
	format.mBitsPerChannel = 32;
	format.mChannelsPerFrame = 2;
	format.mBytesPerFrame = format.mBitsPerChannel / 8 * format.mChannelsPerFrame;
	format.mFramesPerPacket = 1;
	format.mBytesPerPacket = format.mBytesPerFrame * format.mFramesPerPacket;
	format.mReserved = 0;
	AudioQueueNewOutput(&format, &audio_callback, &callback_data, CFRunLoopGetCurrent(), kCFRunLoopCommonModes, 0, &queue);
	AudioQueueBufferRef buffers[3];
	for (int i = 0; i < 3; i++) {
		AudioQueueAllocateBuffer(queue, FRAMES * 2 * sizeof(float), &buffers[i]);
		audio_callback(&callback_data, queue, buffers[i]);
	}
	AudioQueueStart(queue, NULL);
	CFRunLoopRun();
	AudioQueueDispose(queue, NO);
}
