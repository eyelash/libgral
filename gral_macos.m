/*

Copyright (c) 2016-2024 Elias Aebi

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#include "gral.h"
#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>
#include <stdlib.h>

static NSUInteger get_next_code_point(CFStringRef string, NSUInteger i, uint32_t *code_point) {
	unichar c1 = CFStringGetCharacterAtIndex(string, i);
	if ((c1 & 0xFC00) == 0xD800) {
		unichar c2 = CFStringGetCharacterAtIndex(string, i+1);
		*code_point = (((c1 & 0x03FF) << 10) | (c2 & 0x03FF)) + 0x10000;
		return 2;
	}
	else {
		*code_point = c1;
		return 1;
	}
}
static NSUInteger utf8_index_to_utf16(CFStringRef string, int index) {
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
static int utf16_index_to_utf8(CFStringRef string, NSUInteger index) {
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

@interface CallbackObject: NSObject {
@public
	void (*callback)(void *user_data);
	void *user_data;
}
@end
@implementation CallbackObject
- (void)invoke:(id)object {
	callback(user_data);
}
@end


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
- (BOOL)applicationOpenUntitledFile:(NSApplication *)sender {
	interface.open_empty(user_data);
	return YES;
}
- (void)application:(NSApplication *)sender openFiles:(NSArray<NSString *> *)filenames {
	for (NSUInteger i = 0; i < [filenames count]; i++) {
		const char *path = [[filenames objectAtIndex:i] UTF8String];
		interface.open_file(path, user_data);
	}
}
- (void)applicationWillTerminate:(NSNotification *)notification {
	interface.quit(user_data);
}
@end

struct gral_application *gral_application_create(char const *id, struct gral_application_interface const *interface, void *user_data) {
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

static void image_release_callback(void *info, const void *data, size_t size) {
	free(data);
}

struct gral_image *gral_image_create(int width, int height, void *data) {
	CGColorSpaceRef color_space = CGColorSpaceCreateDeviceRGB();
	CGDataProviderRef data_provider = CGDataProviderCreateWithData(NULL, data, width * height * 4, image_release_callback);
	CGImageRef image = CGImageCreate(width, height, 8, 8 * 4, width * 4, color_space, kCGBitmapByteOrder32Big | kCGImageAlphaLast, data_provider, NULL, NO, kCGRenderingIntentDefault);
	CGDataProviderRelease(data_provider);
	CGColorSpaceRelease(color_space);
	return (struct gral_image *)image;
}

void gral_image_delete(struct gral_image *image) {
	CGImageRelease((CGImageRef)image);
}

struct gral_font *gral_font_create(struct gral_window *window, char const *name, float size) {
	CFStringRef string = CFStringCreateWithCString(NULL, name, kCFStringEncodingUTF8);
	CTFontRef font = CTFontCreateWithName(string, size, NULL);
	CFRelease(string);
	return (struct gral_font *)font;
}

struct gral_font *gral_font_create_default(struct gral_window *window, float size) {
	return (struct gral_font *)CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, size, NULL);
}

struct gral_font *gral_font_create_monospace(struct gral_window *window, float size) {
	return (struct gral_font *)CTFontCreateUIFontForLanguage(kCTFontUIFontUserFixedPitch, size, NULL);
}

void gral_font_delete(struct gral_font *font) {
	CFRelease(font);
}

void gral_font_get_metrics(struct gral_window *window, struct gral_font *font, float *ascent, float *descent) {
	if (ascent) *ascent = CTFontGetAscent((CTFontRef)font);
	if (descent) *descent = CTFontGetDescent((CTFontRef)font);
}

struct gral_text *gral_text_create(struct gral_window *window, char const *text, struct gral_font *font) {
	CFStringRef string = CFStringCreateWithCString(NULL, text, kCFStringEncodingUTF8);
	CFMutableDictionaryRef attributes = CFDictionaryCreateMutable(NULL, 1, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFDictionarySetValue(attributes, kCTFontAttributeName, font);
	CFAttributedStringRef attributed_string = CFAttributedStringCreate(NULL, string, attributes);
	CFRelease(string);
	CFRelease(attributes);
	CFMutableAttributedStringRef mutable_attributed_string = CFAttributedStringCreateMutableCopy(NULL, 0, attributed_string);
	CFRelease(attributed_string);
	return (struct gral_text *)mutable_attributed_string;
}

void gral_text_delete(struct gral_text *text) {
	CFRelease(text);
}

void gral_text_set_bold(struct gral_text *text, int start_index, int end_index) {
	CFStringRef string = CFAttributedStringGetString((CFAttributedStringRef)text);
	CFIndex loc = utf8_index_to_utf16(string, start_index);
	CFIndex len = utf8_index_to_utf16(string, end_index) - loc;
	CTFontRef font = CFAttributedStringGetAttribute((CFAttributedStringRef)text, loc, kCTFontAttributeName, NULL);
	CTFontRef bold_font = CTFontCreateCopyWithSymbolicTraits(font, 0, NULL, kCTFontTraitBold, kCTFontTraitBold);
	if (bold_font) {
		CFAttributedStringSetAttribute((CFMutableAttributedStringRef)text, CFRangeMake(loc, len), kCTFontAttributeName, bold_font);
		CFRelease(bold_font);
	}
}

void gral_text_set_italic(struct gral_text *text, int start_index, int end_index) {
	CFStringRef string = CFAttributedStringGetString((CFAttributedStringRef)text);
	CFIndex loc = utf8_index_to_utf16(string, start_index);
	CFIndex len = utf8_index_to_utf16(string, end_index) - loc;
	CTFontRef font = CFAttributedStringGetAttribute((CFAttributedStringRef)text, loc, kCTFontAttributeName, NULL);
	CTFontRef italic_font = CTFontCreateCopyWithSymbolicTraits(font, 0, NULL, kCTFontTraitItalic, kCTFontTraitItalic);
	if (italic_font) {
		CFAttributedStringSetAttribute((CFMutableAttributedStringRef)text, CFRangeMake(loc, len), kCTFontAttributeName, italic_font);
		CFRelease(italic_font);
	}
}

void gral_text_set_color(struct gral_text *text, int start_index, int end_index, float red, float green, float blue, float alpha) {
	CFStringRef string = CFAttributedStringGetString((CFAttributedStringRef)text);
	CFIndex loc = utf8_index_to_utf16(string, start_index);
	CFIndex len = utf8_index_to_utf16(string, end_index) - loc;
	CGColorRef color = CGColorCreateGenericRGB(red, green, blue, alpha);
	CFAttributedStringSetAttribute((CFMutableAttributedStringRef)text, CFRangeMake(loc, len), kCTForegroundColorAttributeName, color);
	CGColorRelease(color);
}

float gral_text_get_width(struct gral_text *text, struct gral_draw_context *draw_context) {
	CTLineRef line = CTLineCreateWithAttributedString((CFAttributedStringRef)text);
	CGRect bounds = CTLineGetImageBounds(line, (CGContextRef)draw_context);
	CFRelease(line);
	return bounds.size.width;
}

float gral_text_index_to_x(struct gral_text *text, int index) {
	CTLineRef line = CTLineCreateWithAttributedString((CFAttributedStringRef)text);
	CFStringRef string = CFAttributedStringGetString((CFAttributedStringRef)text);
	CGFloat offset = CTLineGetOffsetForStringIndex(line, utf8_index_to_utf16(string, index), NULL);
	CFRelease(line);
	return offset;
}

int gral_text_x_to_index(struct gral_text *text, float x) {
	CTLineRef line = CTLineCreateWithAttributedString((CFAttributedStringRef)text);
	CFIndex index = CTLineGetStringIndexForPosition(line, CGPointMake(x, 0.0f));
	CFRelease(line);
	if (index == kCFNotFound) {
		return 0;
	}
	CFStringRef string = CFAttributedStringGetString((CFAttributedStringRef)text);
	return utf16_index_to_utf8(string, index);
}

void gral_draw_context_draw_image(struct gral_draw_context *draw_context, struct gral_image *image, float x, float y) {
	size_t width = CGImageGetWidth((CGImageRef)image);
	size_t height = CGImageGetHeight((CGImageRef)image);
	CGContextScaleCTM((CGContextRef)draw_context, 1.0f, -1.0f);
	CGContextDrawImage((CGContextRef)draw_context, CGRectMake(x, -(y + height), width, height), (CGImageRef)image);
	CGContextScaleCTM((CGContextRef)draw_context, 1.0f, -1.0f);
}

void gral_draw_context_draw_text(struct gral_draw_context *draw_context, struct gral_text *text, float x, float y, float red, float green, float blue, float alpha) {
	CTLineRef line = CTLineCreateWithAttributedString((CFAttributedStringRef)text);
	CGContextTranslateCTM((CGContextRef)draw_context, x, y);
	CGContextSetTextMatrix((CGContextRef)draw_context, CGAffineTransformMakeScale(1.0f, -1.0f));
	CFArrayRef glyphRuns = CTLineGetGlyphRuns(line);
	for (int i = 0; i < CFArrayGetCount(glyphRuns); i++) {
		CTRunRef run = CFArrayGetValueAtIndex(glyphRuns, i);
		CGPoint const *positions = CTRunGetPositionsPtr(run);
		CGGlyph const *glyphs = CTRunGetGlyphsPtr(run);
		int count = CTRunGetGlyphCount(run);
		CFDictionaryRef attributes = CTRunGetAttributes(run);
		CTFontRef font = CFDictionaryGetValue(attributes, kCTFontAttributeName);
		CGColorRef color = (CGColorRef)CFDictionaryGetValue(attributes, kCTForegroundColorAttributeName);
		if (color) {
			CGContextSetFillColorWithColor((CGContextRef)draw_context, color);
		}
		else {
			CGContextSetRGBFillColor((CGContextRef)draw_context, red, green, blue, alpha);
		}
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

void gral_draw_context_fill_linear_gradient(struct gral_draw_context *draw_context, float start_x, float start_y, float end_x, float end_y, struct gral_gradient_stop const *stops, int count) {
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

static int get_key(UInt16 key_code) {
	switch (key_code) {
	case kVK_Return:
		return GRAL_KEY_ENTER;
	case kVK_Tab:
		return GRAL_KEY_TAB;
	case kVK_Delete:
		return GRAL_KEY_BACKSPACE;
	case kVK_ForwardDelete:
		return GRAL_KEY_DELETE;
	case kVK_LeftArrow:
		return GRAL_KEY_ARROW_LEFT;
	case kVK_UpArrow:
		return GRAL_KEY_ARROW_UP;
	case kVK_RightArrow:
		return GRAL_KEY_ARROW_RIGHT;
	case kVK_DownArrow:
		return GRAL_KEY_ARROW_DOWN;
	case kVK_PageUp:
		return GRAL_KEY_PAGE_UP;
	case kVK_PageDown:
		return GRAL_KEY_PAGE_DOWN;
	case kVK_Home:
		return GRAL_KEY_HOME;
	case kVK_End:
		return GRAL_KEY_END;
	case kVK_Escape:
		return GRAL_KEY_ESCAPE;
	default:
		{
			TISInputSourceRef input_source = TISCopyCurrentKeyboardInputSource();
			CFDataRef uchr = TISGetInputSourceProperty(input_source, kTISPropertyUnicodeKeyLayoutData);
			UCKeyboardLayout const *keyboard_layout = (UCKeyboardLayout const *)CFDataGetBytePtr(uchr);
			UInt32 dead_key_state = 0;
			UniChar string[255];
			UniCharCount string_length = 0;
			UCKeyTranslate(keyboard_layout, key_code, kUCKeyActionDown, 0, LMGetKbdType(), 0, &dead_key_state, 255, &string_length, string);
			if (string_length > 0) {
				CFStringRef characters = CFStringCreateWithCharactersNoCopy(NULL, string, string_length, kCFAllocatorNull);
				uint32_t code_point;
				get_next_code_point(characters, 0, &code_point);
				CFRelease(characters);
				return code_point;
			}
			return 0;
		}
	}
}
static int get_modifiers(NSEventModifierFlags modifier_flags) {
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
- (void)windowDidBecomeKey:(NSNotification *)notification {
	interface.focus_enter(user_data);
}
- (void)windowDidResignKey:(NSNotification *)notification {
	interface.focus_leave(user_data);
}
@end

@interface GralView: NSView<NSTextInputClient> {
@public
	struct gral_window_interface interface;
	void *user_data;
	BOOL is_pointer_locked;
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
	interface.draw((struct gral_draw_context *)context, rect.origin.x, rect.origin.y, rect.size.width, rect.size.height, user_data);
}
- (void)setFrameSize:(NSSize)size {
	[super setFrameSize:size];
	interface.resize(size.width, size.height, user_data);
}
- (BOOL)acceptsFirstMouse:(NSEvent *)event {
	return YES;
}
- (void)mouseEntered:(NSEvent *)event {
	interface.mouse_enter(user_data);
}
- (void)mouseExited:(NSEvent *)event {
	interface.mouse_leave(user_data);
}
- (void)mouseMoved:(NSEvent *)event {
	if (is_pointer_locked) {
		interface.mouse_move_relative([event deltaX], [event deltaY], user_data);
	}
	else {
		NSPoint location = [self convertPoint:[event locationInWindow] fromView:nil];
		interface.mouse_move(location.x, location.y, user_data);
	}
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
	int modifiers = get_modifiers([event modifierFlags]);
	interface.mouse_button_press(location.x, location.y, GRAL_PRIMARY_MOUSE_BUTTON, modifiers, user_data);
	if ([event clickCount] == 2) {
		interface.double_click(location.x, location.y, GRAL_PRIMARY_MOUSE_BUTTON, modifiers, user_data);
	}
}
- (void)rightMouseDown:(NSEvent *)event {
	NSPoint location = [self convertPoint:[event locationInWindow] fromView:nil];
	int modifiers = get_modifiers([event modifierFlags]);
	interface.mouse_button_press(location.x, location.y, GRAL_SECONDARY_MOUSE_BUTTON, modifiers, user_data);
	if ([event clickCount] == 2) {
		interface.double_click(location.x, location.y, GRAL_SECONDARY_MOUSE_BUTTON, modifiers, user_data);
	}
}
- (void)otherMouseDown:(NSEvent *)event {
	NSPoint location = [self convertPoint:[event locationInWindow] fromView:nil];
	int modifiers = get_modifiers([event modifierFlags]);
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
	[self interpretKeyEvents:[NSArray arrayWithObject:event]];
	unsigned short key_code = [event keyCode];
	int key = get_key(key_code);
	if (key) {
		interface.key_press(key, key_code, get_modifiers([event modifierFlags]), user_data);
	}
}
- (void)keyUp:(NSEvent *)event {
	unsigned short key_code = [event keyCode];
	int key = get_key(key_code);
	if (key) {
		interface.key_release(key, key_code, user_data);
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

struct gral_window *gral_window_create(struct gral_application *application, int width, int height, char const *title, struct gral_window_interface const *interface, void *user_data) {
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
	view->is_pointer_locked = NO;
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

void gral_window_set_title(struct gral_window *window, char const *title) {
	[(GralWindow *)window setTitle:[NSString stringWithUTF8String:title]];
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
		return [NSCursor arrowCursor];
	case GRAL_CURSOR_HAND:
		return [NSCursor pointingHandCursor];
	case GRAL_CURSOR_TEXT:
		return [NSCursor IBeamCursor];
	case GRAL_CURSOR_HORIZONTAL_ARROWS:
		return [NSCursor resizeLeftRightCursor];
	case GRAL_CURSOR_VERTICAL_ARROWS:
		return [NSCursor resizeUpDownCursor];
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

void gral_window_hide_cursor(struct gral_window *window) {
	CGDisplayHideCursor(kCGNullDirectDisplay);
}

void gral_window_show_cursor(struct gral_window *window) {
	CGDisplayShowCursor(kCGNullDirectDisplay);
}

void gral_window_warp_cursor(struct gral_window *window_, float x, float y) {
	GralWindow *window = (GralWindow *)window_;
	NSPoint point = [[window contentView] convertPoint:NSMakePoint(x, y) toView:nil];
	point = NSMakePoint(NSMinX(window.frame) + point.x, NSMaxY(NSScreen.screens[0].frame) - (NSMinY(window.frame) + point.y));
	CGWarpMouseCursorPosition(point);
}

void gral_window_lock_pointer(struct gral_window *window) {
	GralView *view = (GralView *)[(GralWindow *)window contentView];
	CGAssociateMouseAndMouseCursorPosition(NO);
	view->is_pointer_locked = YES;
}

void gral_window_unlock_pointer(struct gral_window *window) {
	GralView *view = (GralView *)[(GralWindow *)window contentView];
	CGAssociateMouseAndMouseCursorPosition(YES);
	view->is_pointer_locked = NO;
}

void gral_window_show_open_file_dialog(struct gral_window *window, void (*callback)(char const *file, void *user_data), void *user_data) {
	NSOpenPanel *panel = [NSOpenPanel openPanel];
	if ([panel runModal] == NSModalResponseOK) {
		callback([[[panel URL] path] UTF8String], user_data);
	}
}

void gral_window_show_save_file_dialog(struct gral_window *window, void (*callback)(char const *file, void *user_data), void *user_data) {
	NSSavePanel *panel = [NSSavePanel savePanel];
	if ([panel runModal] == NSModalResponseOK) {
		callback([[[panel URL] path] UTF8String], user_data);
	}
}

void gral_window_clipboard_copy(struct gral_window *window, char const *text) {
	NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
	[pasteboard clearContents];
	[pasteboard setString:[NSString stringWithUTF8String:text] forType:NSPasteboardTypeString];
}

void gral_window_clipboard_paste(struct gral_window *window, void (*callback)(char const *text, void *user_data), void *user_data) {
	NSString *text = [[NSPasteboard generalPasteboard] stringForType:NSPasteboardTypeString];
	if (text) {
		callback([text UTF8String], user_data);
	}
}

struct gral_timer *gral_timer_create(int milliseconds, void (*callback)(void *user_data), void *user_data) {
	CallbackObject *callback_object = [[CallbackObject alloc] init];
	callback_object->callback = callback;
	callback_object->user_data = user_data;
	NSTimer *timer = [NSTimer scheduledTimerWithTimeInterval:milliseconds/1000.0 target:callback_object selector:@selector(invoke:) userInfo:nil repeats:YES];
	[callback_object release];
	return (struct gral_timer *)timer;
}

void gral_timer_delete(struct gral_timer *timer) {
	[(NSTimer *)timer invalidate];
}

void gral_run_on_main_thread(void (*callback)(void *user_data), void *user_data) {
	CallbackObject *callback_object = [[CallbackObject alloc] init];
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
#include <sys/mman.h>
#include <stdio.h>
#include <dirent.h>

struct gral_file *gral_file_open_read(char const *path) {
	int fd = open(path, O_RDONLY);
	return fd == -1 ? NULL : (struct gral_file *)(intptr_t)fd;
}

struct gral_file *gral_file_open_write(char const *path) {
	int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0666);
	return fd == -1 ? NULL : (struct gral_file *)(intptr_t)fd;
}

struct gral_file *gral_file_get_standard_input(void) {
	return (struct gral_file *)STDIN_FILENO;
}

struct gral_file *gral_file_get_standard_output(void) {
	return (struct gral_file *)STDOUT_FILENO;
}

struct gral_file *gral_file_get_standard_error(void) {
	return (struct gral_file *)STDERR_FILENO;
}

void gral_file_close(struct gral_file *file) {
	close((int)(intptr_t)file);
}

size_t gral_file_read(struct gral_file *file, void *buffer, size_t size) {
	return read((int)(intptr_t)file, buffer, size);
}

void gral_file_write(struct gral_file *file, void const *buffer, size_t size) {
	write((int)(intptr_t)file, buffer, size);
}

size_t gral_file_get_size(struct gral_file *file) {
	struct stat stat;
	fstat((int)(intptr_t)file, &stat);
	return stat.st_size;
}

void *gral_file_map(struct gral_file *file, size_t size) {
	return mmap(NULL, size, PROT_READ, MAP_PRIVATE, (int)(intptr_t)file, 0);
}

void gral_file_unmap(void *address, size_t size) {
	munmap(address, size);
}

void gral_file_rename(char const *old_path, char const *new_path) {
	rename(old_path, new_path);
}

void gral_file_remove(char const *path) {
	unlink(path);
}

void gral_directory_create(char const *path) {
	mkdir(path, 0777);
}

void gral_directory_iterate(char const *path, void (*callback)(char const *name, void *user_data), void *user_data) {
	DIR *directory = opendir(path);
	struct dirent *entry;
	while (entry = readdir(directory)) {
		callback(entry->d_name, user_data);
	}
	closedir(directory);
}

void gral_directory_remove(char const *path) {
	rmdir(path);
}

struct gral_directory_watcher *gral_directory_watch(char const *path, void (*callback)(void *user_data), void *user_data) {
	// TODO: implement
	return NULL;
}

void gral_directory_watcher_delete(struct gral_directory_watcher *directory_watcher) {
	// TODO: implement
}


/*=========
    TIME
 =========*/

double gral_time_get_monotonic(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec + ts.tv_nsec / 1e9;
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
