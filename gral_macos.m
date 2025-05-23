/*

Copyright (c) 2016-2025 Elias Aebi

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#include "gral.h"
#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>
#import <CoreAudio/CoreAudio.h>
#import <AudioUnit/AudioUnit.h>
#import <CoreMIDI/CoreMIDI.h>
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

@interface GralCallbackObject: NSObject {
@public
	void (*callback)(void *user_data);
	void *user_data;
}
@end
@implementation GralCallbackObject
- (void)invoke:(id)object {
	callback(user_data);
}
@end


/*================
    APPLICATION
 ================*/

@interface GralApplication: NSApplication<NSApplicationDelegate> {
@public
	struct gral_application_interface const *interface;
	void *user_data;
}
@end
@implementation GralApplication
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
	return YES;
}
- (void)applicationWillFinishLaunching:(NSNotification *)notification {
	interface->start(user_data);
}
- (BOOL)applicationOpenUntitledFile:(NSApplication *)sender {
	interface->open_empty(user_data);
	return YES;
}
- (void)application:(NSApplication *)sender openFiles:(NSArray<NSString *> *)filenames {
	for (NSUInteger i = 0; i < [filenames count]; i++) {
		const char *path = [[filenames objectAtIndex:i] UTF8String];
		interface->open_file(path, user_data);
	}
}
- (void)applicationWillTerminate:(NSNotification *)notification {
	interface->quit(user_data);
}
@end

struct gral_application *gral_application_create(char const *id, struct gral_application_interface const *interface, void *user_data) {
	GralApplication *application = [GralApplication sharedApplication];
	application->interface = interface;
	application->user_data = user_data;
	[application setDelegate:application];
	return (struct gral_application *)application;
}

void gral_application_delete(struct gral_application *application) {

}

int gral_application_run(struct gral_application *application, int argc, char **argv) {
	[(GralApplication *)application run];
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
	CFStringRef string = CFStringCreateWithCString(kCFAllocatorDefault, name, kCFStringEncodingUTF8);
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
	CFStringRef string = CFStringCreateWithCString(kCFAllocatorDefault, text, kCFStringEncodingUTF8);
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
	CGContextSetTextMatrix((CGContextRef)draw_context, CGAffineTransformMakeScale(1.0f, -1.0f));
	CFArrayRef glyph_runs = CTLineGetGlyphRuns(line);
	for (int i = 0; i < CFArrayGetCount(glyph_runs); i++) {
		CTRunRef run = CFArrayGetValueAtIndex(glyph_runs, i);
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
		CGContextSetTextPosition((CGContextRef)draw_context, x, y);
		CTFontDrawGlyphs(font, glyphs, positions, count, (CGContextRef)draw_context);
	}
	CFRelease(line);
}

void gral_draw_context_add_text(struct gral_draw_context *draw_context, struct gral_text *text, float x, float y) {
	CTLineRef line = CTLineCreateWithAttributedString((CFAttributedStringRef)text);
	CGContextSetTextMatrix((CGContextRef)draw_context, CGAffineTransformMakeScale(1.0f, -1.0f));
	CFArrayRef glyph_runs = CTLineGetGlyphRuns(line);
	for (int i = 0; i < CFArrayGetCount(glyph_runs); i++) {
		CTRunRef run = CFArrayGetValueAtIndex(glyph_runs, i);
		CGPoint const *positions = CTRunGetPositionsPtr(run);
		CGGlyph const *glyphs = CTRunGetGlyphsPtr(run);
		int count = CTRunGetGlyphCount(run);
		CFDictionaryRef attributes = CTRunGetAttributes(run);
		CTFontRef font = CFDictionaryGetValue(attributes, kCTFontAttributeName);
		for (int j = 0; j < count; j++) {
			CGContextSetTextPosition((CGContextRef)draw_context, x + positions[j].x, y + positions[j].y);
			CGAffineTransform matrix = CGContextGetTextMatrix((CGContextRef)draw_context);
			CGPathRef path = CTFontCreatePathForGlyph(font, glyphs[j], &matrix);
			CGContextAddPath((CGContextRef)draw_context, path);
			CGPathRelease(path);
		}
	}
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

void gral_draw_context_stroke_linear_gradient(struct gral_draw_context *draw_context, float line_width, float start_x, float start_y, float end_x, float end_y, struct gral_gradient_stop const *stops, int count) {
	CGContextSetLineWidth((CGContextRef)draw_context, line_width);
	CGContextSetLineCap((CGContextRef)draw_context, kCGLineCapRound);
	CGContextSetLineJoin((CGContextRef)draw_context, kCGLineJoinRound);
	CGContextReplacePathWithStrokedPath((CGContextRef)draw_context);
	gral_draw_context_fill_linear_gradient(draw_context, start_x, start_y, end_x, end_y, stops, count);
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

static int get_key(unsigned short key_code) {
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
static int get_key_code(unsigned short key_code) {
	switch (key_code) {
	case 0x00: return 0x1E;
	case 0x01: return 0x1F;
	case 0x02: return 0x20;
	case 0x03: return 0x21;
	case 0x04: return 0x23;
	case 0x05: return 0x22;
	case 0x06: return 0x2C;
	case 0x07: return 0x2D;
	case 0x08: return 0x2E;
	case 0x09: return 0x2F;
	case 0x0A: return 0x56;
	case 0x0B: return 0x30;
	case 0x0C: return 0x10;
	case 0x0D: return 0x11;
	case 0x0E: return 0x12;
	case 0x0F: return 0x13;
	case 0x10: return 0x15;
	case 0x11: return 0x14;
	case 0x12: return 0x02;
	case 0x13: return 0x03;
	case 0x14: return 0x04;
	case 0x15: return 0x05;
	case 0x16: return 0x07;
	case 0x17: return 0x06;
	case 0x18: return 0x0D;
	case 0x19: return 0x0A;
	case 0x1A: return 0x08;
	case 0x1B: return 0x0C;
	case 0x1C: return 0x09;
	case 0x1D: return 0x0B;
	case 0x1E: return 0x1B;
	case 0x1F: return 0x18;
	case 0x20: return 0x16;
	case 0x21: return 0x1A;
	case 0x22: return 0x17;
	case 0x23: return 0x19;
	case 0x24: return 0x1C;
	case 0x25: return 0x26;
	case 0x26: return 0x24;
	case 0x27: return 0x28;
	case 0x28: return 0x25;
	case 0x29: return 0x27;
	case 0x2A: return 0x2B;
	case 0x2B: return 0x33;
	case 0x2C: return 0x35;
	case 0x2D: return 0x31;
	case 0x2E: return 0x32;
	case 0x2F: return 0x34;
	case 0x30: return 0x0F;
	case 0x31: return 0x39;
	case 0x32: return 0x29;
	case 0x33: return 0x0E;
	case 0x35: return 0x01;
	case 0x36: return 0xE05C;
	case 0x37: return 0xE05B;
	case 0x38: return 0x2A;
	case 0x39: return 0x3A;
	case 0x3A: return 0x38;
	case 0x3B: return 0x1D;
	case 0x3C: return 0x36;
	case 0x3D: return 0xE038;
	case 0x3E: return 0xE01D;
	case 0x40: return 0x68;
	case 0x41: return 0x53;
	case 0x43: return 0x37;
	case 0x45: return 0x4E;
	case 0x47: return 0x45;
	case 0x4B: return 0xE035;
	case 0x4C: return 0xE01C;
	case 0x4E: return 0x4A;
	case 0x4F: return 0x69;
	case 0x50: return 0x6A;
	case 0x51: return 0x59;
	case 0x52: return 0x52;
	case 0x53: return 0x4F;
	case 0x54: return 0x50;
	case 0x55: return 0x51;
	case 0x56: return 0x4B;
	case 0x57: return 0x4C;
	case 0x58: return 0x4D;
	case 0x59: return 0x47;
	case 0x5B: return 0x48;
	case 0x5C: return 0x49;
	case 0x60: return 0x3F;
	case 0x61: return 0x40;
	case 0x62: return 0x41;
	case 0x63: return 0x3D;
	case 0x64: return 0x42;
	case 0x65: return 0x43;
	case 0x67: return 0x57;
	case 0x69: return 0x64;
	case 0x6A: return 0x67;
	case 0x6B: return 0x65;
	case 0x6D: return 0x44;
	case 0x6F: return 0x58;
	case 0x71: return 0x66;
	case 0x72: return 0xE052;
	case 0x73: return 0xE047;
	case 0x74: return 0xE049;
	case 0x75: return 0xE053;
	case 0x76: return 0x3E;
	case 0x77: return 0xE04F;
	case 0x78: return 0x3C;
	case 0x79: return 0xE051;
	case 0x7A: return 0x3B;
	case 0x7B: return 0xE04B;
	case 0x7C: return 0xE04D;
	case 0x7D: return 0xE050;
	case 0x7E: return 0xE048;
	default: return 0;
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
	struct gral_window_interface const *interface;
	void *user_data;
}
@end
@implementation GralWindow
- (BOOL)windowShouldClose:(id)sender {
	return interface->close(user_data);
}
- (void)windowDidBecomeKey:(NSNotification *)notification {
	interface->focus_enter(user_data);
}
- (void)windowDidResignKey:(NSNotification *)notification {
	interface->focus_leave(user_data);
}
- (void)dealloc {
	interface->destroy(user_data);
	[super dealloc];
}
@end

@interface GralView: NSView<NSTextInputClient> {
@public
	struct gral_window_interface const *interface;
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
	interface->draw((struct gral_draw_context *)context, rect.origin.x, rect.origin.y, rect.size.width, rect.size.height, user_data);
}
- (void)setFrameSize:(NSSize)size {
	[super setFrameSize:size];
	interface->resize(size.width, size.height, user_data);
}
- (BOOL)acceptsFirstMouse:(NSEvent *)event {
	return YES;
}
- (void)mouseEntered:(NSEvent *)event {
	interface->mouse_enter(user_data);
}
- (void)mouseExited:(NSEvent *)event {
	interface->mouse_leave(user_data);
}
- (void)mouseMoved:(NSEvent *)event {
	if (is_pointer_locked) {
		interface->mouse_move_relative([event deltaX], [event deltaY], user_data);
	}
	else {
		NSPoint location = [self convertPoint:[event locationInWindow] fromView:nil];
		interface->mouse_move(location.x, location.y, user_data);
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
	interface->mouse_button_press(location.x, location.y, GRAL_PRIMARY_MOUSE_BUTTON, modifiers, user_data);
	if ([event clickCount] == 2) {
		interface->double_click(location.x, location.y, GRAL_PRIMARY_MOUSE_BUTTON, modifiers, user_data);
	}
}
- (void)rightMouseDown:(NSEvent *)event {
	NSPoint location = [self convertPoint:[event locationInWindow] fromView:nil];
	int modifiers = get_modifiers([event modifierFlags]);
	interface->mouse_button_press(location.x, location.y, GRAL_SECONDARY_MOUSE_BUTTON, modifiers, user_data);
	if ([event clickCount] == 2) {
		interface->double_click(location.x, location.y, GRAL_SECONDARY_MOUSE_BUTTON, modifiers, user_data);
	}
}
- (void)otherMouseDown:(NSEvent *)event {
	NSPoint location = [self convertPoint:[event locationInWindow] fromView:nil];
	int modifiers = get_modifiers([event modifierFlags]);
	interface->mouse_button_press(location.x, location.y, GRAL_MIDDLE_MOUSE_BUTTON, modifiers, user_data);
	if ([event clickCount] == 2) {
		interface->double_click(location.x, location.y, GRAL_MIDDLE_MOUSE_BUTTON, modifiers, user_data);
	}
}
- (void)mouseUp:(NSEvent *)event {
	NSPoint location = [self convertPoint:[event locationInWindow] fromView:nil];
	interface->mouse_button_release(location.x, location.y, GRAL_PRIMARY_MOUSE_BUTTON, user_data);
}
- (void)rightMouseUp:(NSEvent *)event {
	NSPoint location = [self convertPoint:[event locationInWindow] fromView:nil];
	interface->mouse_button_release(location.x, location.y, GRAL_SECONDARY_MOUSE_BUTTON, user_data);
}
- (void)otherMouseUp:(NSEvent *)event {
	NSPoint location = [self convertPoint:[event locationInWindow] fromView:nil];
	interface->mouse_button_release(location.x, location.y, GRAL_MIDDLE_MOUSE_BUTTON, user_data);
}
- (void)scrollWheel:(NSEvent *)event {
	interface->scroll([event scrollingDeltaX], [event scrollingDeltaY], user_data);
}
- (void)keyDown:(NSEvent *)event {
	[self interpretKeyEvents:[NSArray arrayWithObject:event]];
	unsigned short key_code = [event keyCode];
	interface->key_press(get_key(key_code), get_key_code(key_code), get_modifiers([event modifierFlags]), [event isARepeat], user_data);
}
- (void)keyUp:(NSEvent *)event {
	unsigned short key_code = [event keyCode];
	interface->key_release(get_key(key_code), get_key_code(key_code), user_data);
}
- (void)flagsChanged:(NSEvent *)event {
	unsigned short key_code = [event keyCode];
	NSEventModifierFlags modifier_mask;
	switch (key_code) {
	case kVK_CapsLock:
		modifier_mask = NSEventModifierFlagCapsLock;
		break;
	case kVK_Shift:
		modifier_mask = NX_DEVICELSHIFTKEYMASK;
		break;
	case kVK_RightShift:
		modifier_mask = NX_DEVICERSHIFTKEYMASK;
		break;
	case kVK_Option:
		modifier_mask = NX_DEVICELALTKEYMASK;
		break;
	case kVK_RightOption:
		modifier_mask = NX_DEVICERALTKEYMASK;
		break;
	case kVK_Control:
		modifier_mask = NX_DEVICELCTLKEYMASK;
		break;
	case kVK_RightControl:
		modifier_mask = NX_DEVICERCTLKEYMASK;
		break;
	case kVK_Command:
		modifier_mask = NX_DEVICELCMDKEYMASK;
		break;
	case kVK_RightCommand:
		modifier_mask = NX_DEVICERCMDKEYMASK;
		break;
	default:
		return;
	}
	NSEventModifierFlags modifier_flags = [event modifierFlags];
	if (modifier_flags & modifier_mask) {
		interface->key_press(0, get_key_code(key_code), get_modifiers(modifier_flags), 0, user_data);
	}
	else {
		interface->key_release(0, get_key_code(key_code), user_data);
	}
}
- (void)menuItemActivated:(id)sender {
	int id = [(NSMenuItem *)sender tag];
	return interface->activate_menu_item(id, user_data);
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
		interface->text([(NSString *)string UTF8String], user_data);
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
	window->interface = interface;
	window->user_data = user_data;
	[window setDelegate:window];
	[window setTitle:[NSString stringWithUTF8String:title]];
	return (struct gral_window *)window;
}

void gral_window_show(struct gral_window *window_) {
	GralWindow *window = (GralWindow *)window_;
	GralView *view = [[GralView alloc] init];
	view->interface = window->interface;
	view->user_data = window->user_data;
	view->is_pointer_locked = NO;
	NSTrackingArea *trackingArea = [[NSTrackingArea alloc]
		initWithRect:NSZeroRect
		options:NSTrackingMouseEnteredAndExited|NSTrackingMouseMoved|NSTrackingActiveAlways|NSTrackingInVisibleRect
		owner:view
		userInfo:nil
	];
	[view addTrackingArea:trackingArea];
	[trackingArea release];
	[window setContentView:view];
	[view release];
	[window makeKeyAndOrderFront:nil];
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

void gral_window_show_context_menu(struct gral_window *window, struct gral_menu *menu, float x, float y) {
	[(NSMenu *)menu popUpMenuPositioningItem:nil atLocation:NSMakePoint(x, y) inView:[(GralWindow *)window contentView]];
}

struct gral_menu *gral_menu_create(void) {
	NSMenu *menu = [[NSMenu alloc] init];
	return (struct gral_menu *)menu;
}

void gral_menu_delete(struct gral_menu *menu) {
	[(NSMenu *)menu release];
}

void gral_menu_append_item(struct gral_menu *menu, char const *text, int id) {
	NSMenuItem *item = [[NSMenuItem alloc] init];
	[item setTitle:[NSString stringWithUTF8String:text]];
	[item setTag:id];
	[item setAction:@selector(menuItemActivated:)];
	[(NSMenu *)menu addItem:item];
	[item release];
}

void gral_menu_append_separator(struct gral_menu *menu) {
	[(NSMenu *)menu addItem:[NSMenuItem separatorItem]];
}

void gral_menu_append_submenu(struct gral_menu *menu, char const *text, struct gral_menu *submenu) {
	NSMenuItem *item = [[NSMenuItem alloc] init];
	[item setTitle:[NSString stringWithUTF8String:text]];
	[item setSubmenu:(NSMenu *)submenu];
	[(NSMenu *)menu addItem:item];
	[item release];
	[(NSMenu *)submenu release];
}

struct gral_timer *gral_timer_create(int milliseconds, void (*callback)(void *user_data), void *user_data) {
	GralCallbackObject *callback_object = [[GralCallbackObject alloc] init];
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
	GralCallbackObject *callback_object = [[GralCallbackObject alloc] init];
	callback_object->callback = callback;
	callback_object->user_data = user_data;
	[callback_object performSelectorOnMainThread:@selector(invoke:) withObject:nil waitUntilDone:NO];
	[callback_object release];
}


/*=========
    FILE
 =========*/

#include <sys/event.h>

@interface GralDirectoryWatcher: GralCallbackObject {
@public
	int fd;
}
@end
@implementation GralDirectoryWatcher
- (void)dealloc {
	close(fd);
	[super dealloc];
}
@end

static void gral_directory_watcher_release(void *info) {
	GralDirectoryWatcher *directory_watcher = info;
	[directory_watcher release];
}
static void *gral_directory_watcher_retain(void *info) {
	GralDirectoryWatcher *directory_watcher = info;
	[directory_watcher retain];
	return info;
}

static void directory_watcher_callback(CFFileDescriptorRef fdref, CFOptionFlags flags, void *info) {
	GralDirectoryWatcher *directory_watcher = info;
	int kq = CFFileDescriptorGetNativeDescriptor(fdref);
	struct kevent event;
	kevent(kq, NULL, 0, &event, 1, NULL);
	directory_watcher->callback(directory_watcher->user_data);
	CFFileDescriptorEnableCallBacks(fdref, kCFFileDescriptorReadCallBack);
}

struct gral_directory_watcher *gral_directory_watch(char const *path, void (*callback)(void *user_data), void *user_data) {
	GralDirectoryWatcher *directory_watcher = [[GralDirectoryWatcher alloc] init];
	directory_watcher->callback = callback;
	directory_watcher->user_data = user_data;
	directory_watcher->fd = open(path, O_EVTONLY);
	int kq = kqueue();
	struct kevent event;
	event.ident = directory_watcher->fd;
	event.filter = EVFILT_VNODE;
	event.flags = EV_ADD | EV_CLEAR;
	event.fflags = NOTE_WRITE;
	event.data = 0;
	event.udata = NULL;
	kevent(kq, &event, 1, NULL, 0, NULL);
	CFFileDescriptorContext context;
	context.copyDescription = NULL;
	context.info = directory_watcher;
	context.release = gral_directory_watcher_release;
	context.retain = gral_directory_watcher_retain;
	context.version = 0;
	CFFileDescriptorRef fdref = CFFileDescriptorCreate(kCFAllocatorDefault, kq, TRUE, &directory_watcher_callback, &context);
	CFRunLoopSourceRef source = CFFileDescriptorCreateRunLoopSource(kCFAllocatorDefault, fdref, 0);
	CFRunLoopAddSource(CFRunLoopGetMain(), source, kCFRunLoopDefaultMode);
	CFRelease(source);
	CFFileDescriptorEnableCallBacks(fdref, kCFFileDescriptorReadCallBack);
	[directory_watcher release];
	return (struct gral_directory_watcher *)fdref;
}

void gral_directory_watcher_delete(struct gral_directory_watcher *directory_watcher) {
	CFFileDescriptorInvalidate((CFFileDescriptorRef)directory_watcher);
	CFRelease((CFFileDescriptorRef)directory_watcher);
}


/*==========
    AUDIO
 ==========*/

struct gral_audio {
	void (*callback)(float *buffer, int frames, void *user_data);
	void *user_data;
	AudioComponentInstance instance;
};

static int audio_callback(void *user_data, AudioUnitRenderActionFlags *action_flags, AudioTimeStamp const *time_stamp, unsigned int bus_number, unsigned int number_frames, AudioBufferList *data) {
	struct gral_audio *audio = user_data;
	audio->callback(data->mBuffers[0].mData, number_frames, audio->user_data);
	return 0;
}

struct gral_audio *gral_audio_create(char const *name, void (*callback)(float *buffer, int frames, void *user_data), void *user_data) {
	struct gral_audio *audio = malloc(sizeof(struct gral_audio));
	audio->callback = callback;
	audio->user_data = user_data;
	AudioComponentDescription description;
	description.componentType = kAudioUnitType_Output;
	description.componentSubType = kAudioUnitSubType_DefaultOutput;
	description.componentManufacturer = kAudioUnitManufacturer_Apple;
	description.componentFlags = 0;
	description.componentFlagsMask = 0;
	AudioComponent component = AudioComponentFindNext(NULL, &description);
	AudioComponentInstanceNew(component, &audio->instance);
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
	AudioUnitSetProperty(audio->instance, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &format, sizeof(AudioStreamBasicDescription));
	AURenderCallbackStruct callback_struct;
	callback_struct.inputProc = &audio_callback;
	callback_struct.inputProcRefCon = audio;
	AudioUnitSetProperty(audio->instance, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &callback_struct, sizeof(AURenderCallbackStruct));
	AudioUnitInitialize(audio->instance);
	AudioOutputUnitStart(audio->instance);
	return audio;
}

void gral_audio_delete(struct gral_audio *audio) {
	AudioOutputUnitStop(audio->instance);
	AudioUnitUninitialize(audio->instance);
	AudioComponentInstanceDispose(audio->instance);
	free(audio);
}


/*=========
    MIDI
 =========*/

struct gral_midi {
	struct gral_midi_interface const *interface;
	void *user_data;
	MIDIClientRef client;
	MIDIPortRef port;
};

static void midi_callback(MIDINotification const *notification, void *user_data) {
	struct gral_midi *midi = user_data;
	if (notification->messageID == kMIDIMsgObjectAdded) {
		MIDIObjectAddRemoveNotification *add_remove_notification = (MIDIObjectAddRemoveNotification *)notification;
		if (add_remove_notification->childType == kMIDIObjectType_Source) {
			MIDIPortConnectSource(midi->port, add_remove_notification->child, NULL);
		}
	}
}

static void midi_read_callback(MIDIPacketList const *packet_list, void *user_data, void *srcConnRefCon) {
	struct gral_midi *midi = user_data;
	MIDIPacket const *packet = packet_list->packet;
	for (UInt32 i = 0; i < packet_list->numPackets; i++) {
		for (UInt16 j = 0; j < packet->length; j++) {
			if ((packet->data[j] & 0xF0) == 0x80 && j + 2 < packet->length) {
				Byte note = packet->data[j+1];
				Byte velocity = packet->data[j+2];
				midi->interface->note_off(note, velocity, midi->user_data);
				j += 2;
			}
			else if ((packet->data[j] & 0xF0) == 0x90 && j + 2 < packet->length) {
				Byte note = packet->data[j+1];
				Byte velocity = packet->data[j+2];
				midi->interface->note_on(note, velocity, midi->user_data);
				j += 2;
			}
			else if ((packet->data[j] & 0xF0) == 0xB0 && j + 2 < packet->length) {
				Byte controller = packet->data[j+1];
				Byte value = packet->data[j+2];
				midi->interface->control_change(controller, value, midi->user_data);
				j += 2;
			}
		}
		packet = MIDIPacketNext(packet);
	}
}

struct gral_midi *gral_midi_create(struct gral_application *application, char const *name, struct gral_midi_interface const *interface, void *user_data) {
	struct gral_midi *midi = malloc(sizeof(struct gral_midi));
	midi->interface = interface;
	midi->user_data = user_data;
	CFStringRef string = CFStringCreateWithCString(kCFAllocatorDefault, name, kCFStringEncodingUTF8);
	MIDIClientCreate(string, &midi_callback, midi, &midi->client);
	MIDIInputPortCreate(midi->client, string, &midi_read_callback, midi, &midi->port);
	CFRelease(string);
	ItemCount count = MIDIGetNumberOfSources();
	for (ItemCount i = 0; i < count; i++) {
		MIDIEndpointRef source = MIDIGetSource(i);
		CFStringRef name = NULL;
		MIDIObjectGetStringProperty(source, kMIDIPropertyName, &name);
		MIDIPortConnectSource(midi->port, source, NULL);
	}
	return midi;
}

void gral_midi_delete(struct gral_midi *midi) {
	// MIDIPortDispose();
	MIDIClientDispose(midi->client);
	free(midi);
}
