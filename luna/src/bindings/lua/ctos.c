#include "config.h"
#define _GNU_SOURCE
#include <unistd.h>
#include <malloc.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <string.h>
#include <ctype.h>
#include "services/logger.h"
#include "util/files.h"

#ifdef HAVE_CTOS
#include <ctosapi.h>
#endif

static char *debug_text_str;

#define APPEND_DEBUG_TEXT(str)                                                        \
  debug_text_str = realloc(debug_text_str, strlen(debug_text_str) + strlen(str) + 8); \
  sprintf(debug_text_str + strlen(debug_text_str), strlen(debug_text_str) == 0 ? "%s" : " %s", str);

// utility functions
static double clamp(double x, double min, double max) {
    if (x > max) x = max;
    if (x < min) x = min;
    return x;
}

#ifdef HAVE_CTOS
static void check_lcd_error(lua_State *L, int res) {
    switch(res) {
        case d_OK: return;
        case d_LCD_OUT_OF_RANGE: luaL_error(L, "value out of range");
        case d_LCD_MODE_NOT_SUPPORT: luaL_error(L, "mode not supported");
        case d_LCD_LANGUAGE_NOT_SUPPORT: luaL_error(L, "language not supported");
        case d_LCD_FONT_SIZE_NOT_SUPPORT: luaL_error(L, "font size not supported");
        case d_LCD_PARAMETER_NOT_SUPPORT: luaL_error(L, "parameter not supported");
        case d_LCD_WINDOW_REACH_BOTTOM: luaL_error(L, "window reached bottom");
        case d_LCD_PIC_FORMAT_NOT_SUPPORT: luaL_error(L, "image format not supported");
        case d_LCD_PIC_OPEN_FAILED: luaL_error(L, "failed to open image file");
        case d_LCD_NOT_SUPPORT: luaL_error(L, "operation not supported");
        default: luaL_error(L, "unknown LCD error code: %d", res);
    }
}

static void check_font_error(lua_State *L, int res) {
    switch(res) {
        case d_OK: return;
        case d_FONT_LANGUAGE_NOT_SUPPORT: luaL_error(L, "font language not supported");
        case d_FONT_SIZE_NOT_SUPPORT: luaL_error(L, "font size not supported");
        case d_FONT_STYLE_NOT_SUPPORT: luaL_error(L, "font style not supported");
        case d_FONT_TYPE_ERROR: luaL_error(L, "font type error");
        case d_FONT_INVALID_FONT_FILE: luaL_error(L, "invalid font file path");
        case d_FONT_INDEX_NOT_SUPPORT: luaL_error(L, "font variant not supported");
//        case d_FONT_PARA_ERR: luaL_error(L, ""); // what?
        case d_FONT_FONTID_NOT_SUPPORT: luaL_error(L, "font ID not supported");
        default: luaL_error(L, "unknown font error code: %d", res);
    }
}

static void check_keyboard_error(lua_State *L, int res) {
    switch(res) {
        case d_OK: return;
        case d_KBD_INVALID_KEY: luaL_error(L, "invalid key");
        default: luaL_error(L, "unknown keyboard error code: %d", res);
    }
}
#endif


/*
 * Gets information about the display. Returns the x and y resolution,
 * color depth in bits, and what type of touch screen is supported,
 * if any.
 * 
 * The return value is a table with the following structure:
 * 
 *     resolution:
 *       width: number (of pixels)
 *       height: number (of pixels)
 *     color: number (of bits: 2, 10 or 24)
 *     touch_type: string ("none", "resistor" or "capacitor-1p"
 * 
 * Examples:
 * 
 *     ctos = require("CTOS")
 *     attrs = ctos.display.attributes()
 */
static int display_attributes(lua_State *L) {
#ifdef HAVE_CTOS
    ULONG resolution;
    BYTE color, touch;
    int resWidth, resHeight;
    
    // LDEBUG("lua: ctos.display: getting attributes");
    USHORT res = CTOS_LCDAttributeGet(&resolution, &color, &touch);
    switch(res) {
        case d_OK: break;
        default: return luaL_error(L, "Unexpected error %d while querying CTOS_LCDAttributeGet", res);
    }
    
    resWidth  = (resolution >> 16);
    resHeight = (resolution & 0x0000FFFF);

    lua_newtable(L);
        lua_pushstring(L, "resolution");
        lua_newtable(L);
            lua_pushstring(L, "width");
            lua_pushnumber(L, resWidth);
            lua_settable(L, -3);

            lua_pushstring(L, "height");
            lua_pushnumber(L, resHeight);
            lua_settable(L, -3);
        lua_settable(L, -3);
        
        lua_pushstring(L, "color_depth");
        switch(color) {
            case d_COLOR_MONO: lua_pushnumber(L,  2); break;
            case d_COLOR_262K: lua_pushnumber(L, 10); break;
            case d_COLOR_16M:  lua_pushnumber(L, 24); break;
            default: return luaL_error(L, "Unexpected color depth %d while querying CTOS_LCDAttributeGet", color);
        }
        lua_settable(L, -3);

        lua_pushstring(L, "touch_type");
        switch(touch) {
            case d_TOUCH_NONE:         lua_pushstring(L, "none");         break;
            case d_TOUCH_RESISTOR:     lua_pushstring(L, "resistor");     break;
            case d_TOUCH_CAPACITOR_1P: lua_pushstring(L, "capacitor-1p"); break;
            default: return luaL_error(L, "Unexpected touchscreen type %d while querying CTOS_LCDAttributeGet", touch);
        }
        lua_settable(L, -3);
#else
    lua_newtable(L);
        lua_pushstring(L, "resolution");
        lua_newtable(L);
            lua_pushstring(L, "width");
            lua_pushnumber(L, 320);
            lua_settable(L, -3);

            lua_pushstring(L, "height");
            lua_pushnumber(L, 240);
            lua_settable(L, -3);
        lua_settable(L, -3);
        
        lua_pushstring(L, "color_depth");
        lua_pushnumber(L, 24);
        lua_settable(L, -3);

        lua_pushstring(L, "touch_type");
        lua_pushstring(L, "none");
        lua_settable(L, -3);
#endif
    return 1;
}

/*
 * Sets or gets the current drawing mode, which must be "text" or
 * "graphics".
 * 
 * Examples:
 * 
 *     ctos = require("CTOS")
 *     previous_mode = ctos.display.mode()
 *     ctos.display.mode("graphics")
 */
static int display_mode(lua_State *L) {
    char mode[15];
    if (lua_gettop(L) == 0) {
        luaL_error(L, "can't get current display mode: operation not supported");
    } else {
        const char *str = lua_tostring(L, 1);
        sprintf(mode, "%s", str);
        if (!strcmp(str, "text")) {
            LDEBUG("lua: ctos.display: entering text mode");
#ifdef HAVE_CTOS
            check_lcd_error(L, CTOS_LCDSelectMode(d_LCD_TEXT_MODE));
#endif
        } else if (!strcmp(str, "graphics")) {
            LDEBUG("lua: ctos.display: entering graphics mode");
#ifdef HAVE_CTOS
            check_lcd_error(L, CTOS_LCDSelectMode(d_LCD_GRAPHIC_MODE));
#endif
        }
        else return luaL_error(L, "mode must be 'text' or 'graphics'");
    }
    lua_pushstring(L, mode);
    return 1;
}

/*
 * If in text mode, clears the window. If in graphics mode, clears
 * the visible area of the canvas.
 * 
 * Examples:
 * 
 *     ctos = require("CTOS")
 *     ctos.display.clear()
 */
static int display_clear(lua_State *L) {
    LDEBUG("lua: ctos.display: clearing display");
    *debug_text_str = '\0';
#ifdef HAVE_CTOS
    check_lcd_error(L, CTOS_LCDTClearDisplay());
    check_lcd_error(L, CTOS_LCDGClearWindow());
#endif
    return 0;
}

/*
 * Sets or gets the display contrast.
 * 
 * Note: values are in the range [0, 1], representing minimum and
 * maximum contrast, respectively.
 * 
 * Examples:
 * 
 *     ctos = require("CTOS")
 *     old_contrast = ctos.display.contrast()
 *     ctos.display.contrast(0.75)
 */
static int display_contrast(lua_State *L) {
    double val;
    if (lua_gettop(L) == 0) {
        return luaL_error(L, "can't get display contrast: operation not supported");
    } else {
        unsigned char byteval;
        val = clamp(lua_tonumber(L, 1), 0.0, 1.0);
        byteval = (unsigned char) (val * 0xff);
        LDEBUG("lua: ctos.display: setting contrast to %d", byteval);
#ifdef HAVE_CTOS
        check_lcd_error(L, CTOS_LCDSetContrast(byteval));
#endif
    }
    lua_pushnumber(L, val);
    return 1;
}

/*
 * Clears the entire canvas, not just the visible area.
 * 
 * Examples:
 * 
 *     ctos = require("CTOS")
 *     ctos.canvas.clear()
 */
static int canvas_clear(lua_State *L) {
    LDEBUG("lua: ctos.canvas: clearing canvas");
    *debug_text_str = '\0';
#ifdef HAVE_CTOS
    CTOS_LCDGClearCanvas();
#endif
    return 0;
}

/*
 * Sets or gets the current foreground color to the given red, green
 * and blue values.
 * 
 * Note: each value should be in the range [0, 1] representing minimum
 * and maximum intensity, respectively.
 * 
 * Examples:
 * 
 *     ctos = require("CTOS")
 *     old_r, old_g, old_b = ctos.color.foreground()
 *     ctos.color.foreground(1, 1, 0) -- full yellow
 */
static int color_foreground(lua_State *L) {
    double r, g, b;
    if (lua_gettop(L) == 0) {
        return luaL_error(L, "can't get current foreground color: operation not supported");
    } else {
        r = clamp(lua_tonumber(L, 1), 0.0, 1.0);
        g = clamp(lua_tonumber(L, 2), 0.0, 1.0);
        b = clamp(lua_tonumber(L, 3), 0.0, 1.0);
        int rx = (int)(r * 0xff), gx = (int)(g * 0xff), bx = (int)(b * 0xff);
        unsigned long rgb = (bx << 16) + (gx << 8) + rx;
        (void) rgb;
        // LDEBUG("lua: ctos.color: setting foreground to %lu", rgb);
#ifdef HAVE_CTOS
        check_lcd_error(L, CTOS_LCDForeGndColor(rgb));
#endif
    }
    
    lua_pushnumber(L, r);
    lua_pushnumber(L, g);
    lua_pushnumber(L, b);
    return 3;
}

/*
 * Sets or gets the current background color to the given red, green
 * and blue values.
 * 
 * Note: each value should be in the range [0, 1] representing minimum
 * and maximum intensity, respectively.
 * 
 * Examples:
 * 
 *     ctos = require("CTOS")
 *     old_r, old_g, old_b = ctos.color.background()
 *     ctos.color.background(1, 1, 0) -- full yellow
 */
static int color_background(lua_State *L) {
    double r, g, b;
    if (lua_gettop(L) == 0) {
        return luaL_error(L, "can't get current background color: operation not supported");
    } else {
        r = clamp(lua_tonumber(L, 1), 0.0, 1.0);
        g = clamp(lua_tonumber(L, 2), 0.0, 1.0);
        b = clamp(lua_tonumber(L, 3), 0.0, 1.0);
        int rx = (int)(r * 0xff), gx = (int)(g * 0xff), bx = (int)(b * 0xff);
        unsigned long rgb = (bx << 16) + (gx << 8) + rx;
        (void) rgb;
        // LDEBUG("lua: ctos.color: setting foreground to %lu", rgb);
#ifdef HAVE_CTOS
        check_lcd_error(L, CTOS_LCDBackGndColor(rgb));
#endif
    }
    
    lua_pushnumber(L, r);
    lua_pushnumber(L, g);
    lua_pushnumber(L, b);
    return 3;
}

/*
 * Reads a rectangle of the given size and location from the screen buffer and
 * returns its content in the form of a table, which contains width, height
 * and string of data where each character represents a B, G or R value.
 *
 * Returns nil if reading from the screen buffer is not supported by this
 * device or if any error occurs while reading.
 *
 * Examples:
 *
 *     ctos = require("CTOS")
 *     -- x, y, width, height
 *     local pixels = ctos.canvas.read(3, 3, 2, 2)
 *
 *     -- pixels is now a table with the following contents:
 *     {
 *       width  = [number],
 *       height = [number],
 *       data = "BGRBGRBGRBGR"
 *     }
 *
 */
static int canvas_read(lua_State *L) {
#if HAVE_CTOS_CAPTURE_WINDOW
    unsigned short x = (unsigned short) luaL_checknumber(L, 1);
    unsigned short y = (unsigned short) luaL_checknumber(L, 2);
    unsigned short w = (unsigned short) luaL_checknumber(L, 3);
    unsigned short h = (unsigned short) luaL_checknumber(L, 4);
    int size = w * h * 3;
    BYTE *buf = malloc(size * sizeof(BYTE));
    USHORT err = CTOS_LCDGCaptureWindow(x, y, w, h, buf, size * sizeof(BYTE));
    if (err == d_OK) {
        lua_newtable(L);
        lua_pushstring(L, "width");
        lua_pushnumber(L, w);
        lua_settable(L, -3);

        lua_pushstring(L, "height");
        lua_pushnumber(L, h);
        lua_settable(L, -3);

        lua_pushstring(L, "data");
        lua_pushlstring(L, (char *) buf, size);
        lua_settable(L, -3);

        return 1;
    } else {
        LWARN("error %x while reading bytes from screen", err);
        return 0;
    }
#else
    (void) L;
    return 0;
#endif
}

/*
 * Writes a rectangle of data directly to the screen buffer. The first and
 * second arguments should be the horizontal and vertical pixel locations of
 * the upper-left corner of the image. The third argument should follow the
 * same structure as is returned by `canvas.read()`.
 *
 * Example:
 *
 *     ctos = require("CTOS")
 *     -- a single white pixel at location 3x3
 *     ctos.canvas.write(3, 3, { width = 1, height = 1, data = "BGR" })
 *
 */
static int canvas_write(lua_State *L) {
#if HAVE_CTOS_SHOW_PIC_EX
    unsigned short x = (unsigned short) luaL_checknumber(L, 1);
    unsigned short y = (unsigned short) luaL_checknumber(L, 2);
    unsigned short w, h;
    luaL_checktype(L, 3, LUA_TTABLE);

    lua_getfield(L, 3, "width");
    w = (unsigned short) luaL_checknumber(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 3, "height");
    h = (unsigned short) luaL_checknumber(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 3, "data");
    size_t size;
    const char *buf = luaL_checklstring(L, -1, &size);
    (void) CTOS_LCDGShowPicEx(d_LCD_SHOWPIC_RGB, x, y, (BYTE *) buf, size, w);
    lua_pop(L, 1);

    return 0;
#else
    (void) L;
    return 0;
#endif
}

/*
 * Renders text using the current font at the given x/y coordinates
 * with the given font size. A boolean value specifies whether to draw
 * the text in reverse.
 * 
 * Examples:
 * 
 *     ctos = require("CTOS")
 *     -- x, y, string, font width, font height, reverse
 *     ctos.canvas.text(100, 150, "Hello", 8, 12, false)
 */
static int canvas_text(lua_State *L) {
    unsigned short x           = (unsigned short) luaL_checknumber(L, 1);
    unsigned short y           = (unsigned short) luaL_checknumber(L, 2);
    const char *str            = luaL_checkstring(L, 3);
    unsigned short font_size_x = (unsigned short) luaL_checknumber(L, 4);
    unsigned short font_size_y = (unsigned short) luaL_checknumber(L, 5);
    unsigned short font_size   = (font_size_x << 8) + font_size_y;
    int reverse                = (int) lua_toboolean(L, 6);
    APPEND_DEBUG_TEXT(str);
    (void) x;
    (void) y;
    (void) str;
    (void) font_size;
    (void) reverse;
    // LDEBUG("lua: ctos.canvas: drawing text at %u x %u: %s, with font size %x, reverse = %d", x, y, str, font_size, reverse);
#ifdef HAVE_CTOS
    check_lcd_error(L, CTOS_LCDGTextOut(x, y, (unsigned char *) str, font_size, (BOOL) reverse));
#endif
    return 0;
}

/*
 * Draws a rectangle using the current foreground color at the given
 * position with the given width and height. A boolean value specifies
 * whether to fill the rectangle or not.
 * 
 * Examples:
 * 
 *     ctos = require("CTOS")
 *     ctos.canvas.rect(0, 10, 100, 90, true)
 */
static int canvas_rect(lua_State *L) {
    unsigned short x    = (unsigned short) lua_tonumber(L, 1);
    unsigned short y    = (unsigned short) lua_tonumber(L, 2);
    unsigned short w    = (unsigned short) lua_tonumber(L, 3);
    unsigned short h    = (unsigned short) lua_tonumber(L, 4);
    int            fill = (int)            lua_toboolean(L, 5);
    (void) x;
    (void) y;
    (void) w;
    (void) h;
    (void) fill;
    // LDEBUG("lua: ctos.canvas: drawing rect %u x %u x %u x %u, fill = %d", x, y, w, h, fill);
#ifdef HAVE_CTOS
    check_lcd_error(L, CTOS_LCDGSetBox(x, y, w, h, (BOOL) fill));
#endif
    return 0;
}

static int canvas_pixel(lua_State *L) {
    unsigned short x    = (unsigned short) lua_tonumber(L, 1);
    unsigned short y    = (unsigned short) lua_tonumber(L, 2);
    int            fill = (int)            lua_toboolean(L, 3);
    (void) x;
    (void) y;
    (void) fill;
#ifdef HAVE_CTOS
    check_lcd_error(L, CTOS_LCDGPixel(x, y, (BOOL) fill));
#endif
    return 0;
}

/*
 * Draws an image with the specified filename at the given coordinates.
 * 
 * Examples:
 * 
 *     ctos = require("CTOS")
 *     ctos.canvas.image(100, 50, "button.bmp")
 */
static int canvas_image(lua_State *L) {
    unsigned short x = (unsigned short) lua_tonumber(L, 1);
    unsigned short y = (unsigned short) lua_tonumber(L, 2);
    char *filename = find_readable_file(NULL, lua_tostring(L, 3));
    if (filename) {
        (void) filename;
        (void) x;
        (void) y;
        // LDEBUG("lua: ctos.canvas: drawing image %s at %u x %u", filename, x, y);
#ifdef HAVE_CTOS
        check_lcd_error(L, CTOS_LCDGShowBMPPic(x, y, (BYTE *) filename));
#endif
        free(filename);
    }
    return 0;
}

/*
 * Sets or gets the currently selected TTF font. A second argument can
 * specify regular, italic, bold, or bold-italic. If a second argument
 * is not given, regular is used by default.
 * 
 * Examples:
 * 
 *     ctos = require("CTOS")
 *     old_face = ctos.font.face()
 *     ctos.font.face("Arial", "italic")
 */
static int font_face(lua_State *L) {
    if (lua_gettop(L) == 0) {
        return luaL_error(L, "can't get current font face: operation not supported");
    } else {
        char *filename = strdup(luaL_checkstring(L, 1));
        const char *variant = lua_tostring(L, 2);
        char *f;
        
        // convert font name to lowercase, spaces to hyphens
        for (f = filename; *f; f++) {
            if (*f == ' ') *f = '-';
            else *f = tolower(*f);
        }

        unsigned char index = 0;
        if (variant) {
            if (!strcmp(variant, "regular")) index = 0;
            else if (!strcmp(variant, "italic")) index = 1;
            else if (!strcmp(variant, "bold")) index = 2;
            else if (!strcmp(variant, "bold-italic")) index = 3;
            else if (!strcmp(variant, "bold italic")) index = 3;
            else if (!strcmp(variant, "italic-bold")) index = 3;
            else if (!strcmp(variant, "italic bold")) index = 3;
            else return luaL_error(L, "variant, if present, must be one of 'regular', 'italic', 'bold' or 'bold italic'");
        }

        LDEBUG("lua: ctos.font: selecting font file %s with index %u", filename, index);
#ifdef HAVE_CTOS
        check_font_error(L, CTOS_LCDTTFSelect((BYTE *) filename, index));
#endif
        free(filename);
        return 2;
    }
}

/*
 * Returns the last key in the key buffer, and clears the key buffer.
 * If there are no keys in the key buffer, blocks until a key is
 * pressed.
 * 
 * Examples:
 * 
 *     ctos = require("CTOS")
 *     key = ctos.keypad.getch()
 */
static int keypad_getch(lua_State *L) {
    unsigned char key[2];
    memset(key, 0, sizeof(key));
    LDEBUG("lua: ctos.keypad: waiting for key press");
#ifdef HAVE_CTOS
    check_keyboard_error(L, CTOS_KBDGet(key));
#else
    key[0] = (unsigned char) fgetc(stdin);
#endif
    lua_pushstring(L, (const char *) key);
    return 1;
}

/*
 * Returns only the last character in the key buffer, and clears the
 * key buffer. If there are no keys in the key buffer, nil is returned.
 * 
 * Examples:
 * 
 *     ctos = require("CTOS")
 *     key = ctos.keypad.last()
 */
static int keypad_last(lua_State *L) {
#ifdef HAVE_CTOS
    BYTE key[2];
    USHORT res;
    memset(key, 0, sizeof(key));
    res = CTOS_KBDHit(key);
    // this just means no key is in the buffer
    if (res == d_KBD_INVALID_KEY || key[0] == 0xff) {
        lua_pushnil(L);
        return 1;
    }
    
    if (res != d_OK) {
        LWARN("lua: ctos: unexpected keyboard error code: 0x%x", res);
        lua_pushnil(L);
        return 1;
    }
//    check_keyboard_error(L, res);
    lua_pushstring(L, (const char *) key);
    return 1;
#else
    lua_pushnil(L);
    return 1;
#endif
}

/*
 * Sets or gets whether sound is emitted when a key is pressed.
 * 
 * Examples:
 * 
 *     ctos = require("CTOS")
 *     sound = ctos.keypad.is_sound_enabled()
 *     ctos.keypad.is_sound_enabled(sound)
 */
static int keypad_is_sound_enabled(lua_State *L) {
#ifdef HAVE_CTOS
    BYTE enabled;
    if (lua_gettop(L) == 0) {
        return luaL_error(L, "cannot query whether keypad sound is enabled: operation not supported");
    } else {
        int state = lua_toboolean(L, 1);
        if (state) enabled = d_ON;
        else enabled = d_OFF;
        LDEBUG("lua: ctos.keypad: setting sound enabled = %d", enabled);
        check_keyboard_error(L, CTOS_KBDSetSound(enabled));
    }
    lua_pushboolean(L, enabled == d_ON);
#else
    lua_pushboolean(L, 0);
#endif
    return 0;
}

/*
 * Sets or gets the frequency and duration of the sound emitted by
 * a key press. The frequency is given in Hertz and the duration is
 * given in tens of milliseconds.
 * 
 * Examples:
 * 
 *     ctos = require("CTOS")
 *     freq, dur = ctos.keypad.frequency()
 *     ctos.keypad.frequency(freq + 10, dur + 10)
 */
static int keypad_frequency(lua_State *L) {
    unsigned short freq, duration;
    
    if (lua_gettop(L) == 0) {
        return luaL_error(L, "cannot get frequency and duration: operation not supported");
    } else {
        freq     = (unsigned short) lua_tonumber(L, 1);
        duration = (unsigned short) lua_tonumber(L, 2);
        LDEBUG("lua: ctos.keypad: setting keypad frequency = %u, duration = %u", freq, duration);
#ifdef HAVE_CTOS
        check_keyboard_error(L, CTOS_KBDSetFrequence(freq, duration));
#endif
    }
    
    lua_pushnumber(L, freq);
    lua_pushnumber(L, duration);
    return 2;
}

/*
 * Returns whether any key is currently pressed.
 * 
 * Examples:
 * 
 *     ctos = require("CTOS")
 *     if ctos.keypad.is_any_key_pressed() then
 *       -- ...
 *     end
 */
static int keypad_is_any_key_pressed(lua_State *L) {
    int b = 0;
    // LDEBUG("lua: ctos.keypad: checking if any key is pressed");
#ifdef HAVE_CTOS
    check_keyboard_error(L, CTOS_KBDInKey((BOOL *) &b));
#endif
    lua_pushboolean(L, (int) b);
    return 1;
}

/*
 * Returns the next character in the key buffer, if any, without
 * removing it from the buffer. If the buffer is empty, returns nil.
 * 
 * Examples:
 * 
 *     ctos = require("CTOS")
 *     key = ctos.keypad.peek()
 */
static int keypad_peek(lua_State *L) {
    char key[2];
    memset(key, 0, sizeof(key));
//    LDEBUG("lua: ctos.keypad: checking if a key is in the buffer");
#ifdef HAVE_CTOS
    check_keyboard_error(L, CTOS_KBDInKeyCheck((BYTE *) key));
#endif
    lua_pushstring(L, key);
    return 1;
}

/*
 * Sets or gets whether pressing the F1 key will reset the device.
 * 
 * Examples:
 * 
 *     ctos = require("CTOS")
 *     reset_enabled = ctos.keypad.is_reset_enabled()
 *     ctos.keypad.is_reset_enabled(not reset_enabled)
 */
static int keypad_is_reset_enabled(lua_State *L) {
    int enabled;
    if (lua_gettop(L) == 0) {
        return luaL_error(L, "could not get reset-enabled state: operation not supported");
    } else {
        enabled = lua_toboolean(L, 1);
        (void) enabled;
        // LDEBUG("lua: ctos.keypad: setting reset enabled = %d", enabled);
#ifdef HAVE_CTOS
        check_keyboard_error(L, CTOS_KBDSetResetEnable((BOOL) enabled));
#endif
    }
    
    lua_pushboolean(L, enabled);
    return 1;
}

/*
 * Flushes the keypad buffer.
 * 
 * Examples:
 * 
 *     ctos = require("CTOS")
 *     ctos.keypad.flush()
 */
static int keypad_flush(lua_State *L) {
    // LDEBUG("lua: ctos.keypad: flushing key buffer");
#ifdef HAVE_CTOS
    check_keyboard_error(L, CTOS_KBDBufFlush());
#endif
    return 0;
}

/*
 * Attempts to read track data from a magnetic card swipe. Returns
 * track 1, track 2 and track 3 if successful, or nil if no swipe
 * has occurred. An error is raised if a swipe has occurred but track
 * data could not be read.
 * 
 * Examples:
 * 
 *     ctos = require("CTOS")
 *     track1, track2, track3 = ctos.msr.read()
 */
static int msr_read(lua_State *L) {
    char track1[128], track2[128], track3[128];
    unsigned short track1len, track2len, track3len;
    
    LDEBUG("lua: ctos.msr: checking for mag swipe");
#ifdef HAVE_CTOS
    switch (CTOS_MSRRead((BYTE *) track1, &track1len,
                         (BYTE *) track2, &track2len,
                         (BYTE *) track3, &track3len)) {
        case d_OK:
            lua_pushlstring(L, track1, track1len);
            lua_pushlstring(L, track2, track2len);
            lua_pushlstring(L, track3, track3len);
            return 3;
        case d_MSR_NO_SWIPE:
            return 0;
            break;
        case d_MSR_TRACK_ERROR:
            return luaL_error(L, "MSR track error");
        default:
            return luaL_error(L, "Unexpected MSR error");
    }
#else
    (void) track1;
    (void) track2;
    (void) track3;
    (void) track1len;
    (void) track2len;
    (void) track3len;
    return 0;
#endif
}

/*
 * Prints a string of text at the specified position, or at the current
 * cursor position if no position is specified. Returns the cursor's
 * position after printing.
 * 
 * Examples:
 * 
 *     ctos = require("CTOS")
 *     cursor_pos = ctos.cursor.print("hello")
 *     ctos.cursor.print("world", cursor_pos.x + 5, cursor_pos.y)
 */
static int cursor_print(lua_State *L) {
#ifdef HAVE_CTOS
    const char *str = lua_tostring(L, 1);
    APPEND_DEBUG_TEXT(str);

    if (lua_gettop(L) == 0) {
        // print at current cursor location
        USHORT x = (USHORT) lua_tonumber(L, 2), y = (USHORT) lua_tonumber(L, 3);
        // LDEBUG("lua: ctos.cursor: printing %s at %u x %u", str, x, y);
        check_lcd_error(L, CTOS_LCDTPrintXY(x, y, (UCHAR *) str));
    } else {
        // print at specified location
        // LDEBUG("lua: ctos.cursor: printing %s at current cursor location", str);
        check_lcd_error(L, CTOS_LCDTPrint((UCHAR *) str));
    }
    
    lua_newtable(L);
    lua_pushstring(L, "x");
    lua_pushnumber(L, CTOS_LCDTWhereX());
    lua_settable(L, -3);
    lua_pushstring(L, "y");
    lua_pushnumber(L, CTOS_LCDTWhereY());
    lua_settable(L, -3);
#else
    lua_newtable(L);
    lua_pushstring(L, "x");
    lua_pushnumber(L, 0);
    lua_settable(L, -3);
    lua_pushstring(L, "y");
    lua_pushnumber(L, 0);
    lua_settable(L, -3);
#endif
    return 1;
}

/*
 * Sets or gets the current cursor position.
 * 
 * Examples:
 * 
 *     ctos = require("CTOS")
 *     pos = ctos.cursor.position()
 *     ctos.cursor.position(pos.x, 100 - pos.y)
 */
static int cursor_position(lua_State *L) {
#ifdef HAVE_CTOS
    USHORT x, y;
    if (lua_gettop(L) == 0) {
        // no args given, return current value without changing it
        // LDEBUG("lua: ctos.cursor: querying current location");
        x = CTOS_LCDTWhereX();
        y = CTOS_LCDTWhereY();
    } else {
        // args given
        x = (USHORT) lua_tonumber(L, 1);
        y = (USHORT) lua_tonumber(L, 2);
        // LDEBUG("lua: ctos.cursor: setting current location to %u x %u", x, y);
        check_lcd_error(L, CTOS_LCDTGotoXY(x, y));
    }

    lua_newtable(L);
    lua_pushstring(L, "x");
    lua_pushnumber(L, x);
    lua_settable(L, -3);
    lua_pushstring(L, "y");
    lua_pushnumber(L, y);
    lua_settable(L, -3);
#else
    lua_newtable(L);
    lua_pushstring(L, "x");
    lua_pushnumber(L, 0);
    lua_settable(L, -3);
    lua_pushstring(L, "y");
    lua_pushnumber(L, 0);
    lua_settable(L, -3);
#endif
    return 1;
}

/*
 * Sets or gets whether printed text is being reversed.
 * 
 * Examples:
 * 
 *     ctos = require("CTOS")
 *     reversed = ctos.cursor.is_reversed()
 *     ctos.cursor.is_reversed(not reversed)
 */
static int cursor_is_reversed(lua_State *L) {
    int enabled = 0;
#ifdef HAVE_CTOS
    if (lua_gettop(L) == 0) {
        return luaL_error(L, "can't get whether text is reversed: operation not supported");
    } else {
        enabled = lua_toboolean(L, 1);
        // LDEBUG("lua: ctos.cursor: setting cursor enabled = %d", enabled);
        check_lcd_error(L, CTOS_LCDTSetReverse(enabled ? d_TRUE : d_FALSE));
    }
#endif
    lua_pushboolean(L, enabled);
    return 1;
}

/*
 * Clears the screen from the current cursor location to the end of the
 * current line.
 * 
 * Examples:
 * 
 *     ctos = require("CTOS")
 *     ctos.cursor.print("hello")
 *     ctos.cursor.eol()
 */
static int cursor_eol(lua_State *L) {
    // LDEBUG("lua: ctos.cursor: clearing to end of line");
#ifdef HAVE_CTOS
    check_lcd_error(L, CTOS_LCDTClear2EOL());
#endif
    return 0;
}

/*
 * Sets or gets the current font size.
 * 
 * Examples:
 * 
 *     ctos = require("CTOS")
 *     size = ctos.font.size()
 *     print("font width: " .. size.x .. ", height: " .. size.y)
 *     -- now double it!
 *     new_size = ctos.font.size size.x * 2, size.y * 2
 */
static int font_size(lua_State *L) {
    unsigned short x, y;
    
    if (lua_gettop(L) == 0) {
        // not currently supported by CTOS
        return luaL_error(L, "can't get current font size: operation not supported");
    } else {
        x = (unsigned short) lua_tonumber(L, 1);
        y = (unsigned short) lua_tonumber(L, 2);
        unsigned short size = (x << 8) + y;
        (void) size;
        // LDEBUG("lua: ctos.font: setting font size to %u x %u = %x", x, y, size);
#ifdef HAVE_CTOS
        check_font_error(L, CTOS_LCDTSelectFontSize(size));
#endif
    }
    
    lua_newtable(L);
    lua_pushstring(L, "x");
    lua_pushnumber(L, x);
    lua_settable(L, -3);
    lua_pushstring(L, "y");
    lua_pushnumber(L, y);
    lua_settable(L, -3);
    return 1;
}

/*
 * Sets or gets the current font horizontal and vertical offsets.
 * 
 * Note: negative offsets will shift left/down, while positive offsets
 * will shift right/up.
 * 
 * Examples:
 * 
 *     ctos = require("CTOS")
 *     old_offset = ctos.font.offset()
 *     ctos.font.offset(old_offset.x + 1, old_offset.y - 1)
 */
static int font_offset(lua_State *L) {
#ifdef HAVE_CTOS
    BOOL xdir, ydir;
    int xoff, yoff;
    if (lua_gettop(L) == 0) {
        return luaL_error(L, "can't get current font offsets: operation not supported");
    } else {
        xoff = (int) lua_tonumber(L, 1);
        yoff = (int) lua_tonumber(L, 2);
        if (xoff < 0) { xdir = d_LCD_SHIFTLEFT; xoff = -xoff; }
        else { xdir = d_LCD_SHIFTRIGHT; }
        if (yoff < 0) { ydir = d_LCD_SHIFTDOWN; yoff = -yoff; }
        else { ydir = d_LCD_SHIFTUP; }
        // LDEBUG("lua: ctos.font: setting font offset to %d, %u x %d, %u", xdir, (BYTE) xoff, ydir, (BYTE) yoff);
        check_font_error(L, CTOS_LCDTSetASCIIHorizontalOffset(xdir, (BYTE) xoff));
        check_font_error(L, CTOS_LCDTSetASCIIVerticalOffset(ydir, (BYTE) yoff));
    }
    lua_newtable(L);
    lua_pushstring(L, "x");
    lua_pushnumber(L, xdir == d_LCD_SHIFTLEFT ? -xoff : xoff);
    lua_settable(L, -3);
    lua_pushstring(L, "y");
    lua_pushnumber(L, ydir == d_LCD_SHIFTDOWN ? -yoff : yoff);
    lua_settable(L, -3);
#else
    lua_newtable(L);
    lua_pushstring(L, "x");
    lua_pushnumber(L, 0);
    lua_settable(L, -3);
    lua_pushstring(L, "y");
    lua_pushnumber(L, 0);
    lua_settable(L, -3);
#endif
    return 1;
}

/*
 * Measures the width of the specified string using the currently selected
 * font. Returns the width of the string in pixels.
 * 
 * Examples:
 * 
 *     ctos = require("CTOS")
 *     ctos.font.width("hello, world!")
 */
static int font_width(lua_State *L) {
    unsigned short width = 0;
    const char *str = lua_tostring(L, 1);
    (void) width;
    (void) str;
    // LDEBUG("lua: ctos.font: measuring width of text %s", str);
#ifdef HAVE_CTOS
    check_font_error(L, CTOS_LCDTGetStringWidth((BYTE *) str, &width));
#endif
    lua_pushnumber(L, width);
    return 1;
}


/*
 * When text is printed to the screen via the canvas text or font print
 * functions, it is also captured in a special variable which can later be
 * inspected to examine what text might be on the screen (or what have been
 * recently, before scrolling off-window or being drawn over).
 *
 * When the screen or canvas is cleared, the text is truncated.
 *
 * This function returns the contents of the text variable.
 *
 * Most drawing operations don't interfere with the text capture, so you
 * should not depend on the content of this variable to know for sure what is
 * shown onscreen. It's meant to be used for debugging and unit testing
 * purposes, not for being relied upon in production.
 *
 * Note also that the visible bounds of the screen are ignored where the debug
 * text is concerned, so text drawn offscreen will still be captured.
 * 
 * Examples:
 * 
 *     ctos = require("CTOS")
 *     ctos.canvas.text(100, 150, "Hello", 8, 12, false)
 *     assert(ctos.debug.text() == "Hello")
 */
static int debug_text(lua_State *L) {
    lua_pushstring(L, debug_text_str);
    return 1;
}

/* init functions */
LUALIB_API int luaopen_CTOS(lua_State *L) {
    static const struct luaL_Reg display[] = {
        { "attributes",        display_attributes    },
        { "mode",              display_mode          },
        { "contrast",          display_contrast      },
        { "clear",             display_clear         },
        { NULL,                NULL                       }
    };
    
    static const struct luaL_Reg cursor[] = {
        { "position",          cursor_position      },
        { "print",             cursor_print         },
        { "eol",               cursor_eol           },
        { "is_reversed",       cursor_is_reversed   },
        { NULL,                NULL                          }
    };
    // CTOS_LCDTSelectFontSize
    // CTOS_LCDTSetASCIIVerticalOffset
    // CTOS_LCDTSetASCIIHorizontalOffset
    
    static const struct luaL_Reg canvas[] = {
        { "clear",             canvas_clear       },
        { "text",              canvas_text        },
        { "rect",              canvas_rect        },
        { "pixel",             canvas_pixel       },
        { "image",             canvas_image       },
        { "read",              canvas_read        },
        { "write",             canvas_write       },
        // CTOS_LCDGPixel
        // CTOS_LCDGShowPic
        // CTOS_LCDGShowPicEx
        // CTOS_LCDGClearWindow
        // CTOS_LCDGMoveWindow
        // CTOS_LCDGGetWindowOffset
        { NULL,                NULL                        }
    };
    
    static const struct luaL_Reg color[] = {
        { "foreground",        color_foreground   },
        { "background",        color_background   },
        { NULL,                NULL                        }
    };
    
    static const struct luaL_Reg font[] = {
        { "face",              font_face          },
        { "size",              font_size          },
        { "offset",            font_offset        },
        { "width",             font_width         },
        // CTOS_LCDTTFCheckLanguageSupport
        { NULL,                NULL                        }
    };
    
    static const struct luaL_Reg keypad[] = {
        { "getch",              keypad_getch              },
        { "last",               keypad_last               },
        { "is_sound_enabled",   keypad_is_sound_enabled   },
        { "frequency",          keypad_frequency          },
        { "is_any_key_pressed", keypad_is_any_key_pressed },
        { "peek",               keypad_peek               },
        { "is_reset_enabled",   keypad_is_reset_enabled   },
        { "flush",              keypad_flush              },
        // CTOS_KBDBufGet
        // CTOS_KBDBufPut
        // CTOS_KBDScan
        { NULL,                NULL                           }
    };
    
    static const struct luaL_Reg msr[] = {
        { "read",              msr_read                       },
        { NULL,                NULL                           }
    };
    
    static const struct luaL_Reg debug[] = {
        { "text",              debug_text                     },
        { NULL,                NULL                           }
    };
    
    lua_newtable(L);
        lua_pushstring(L, "display");
        lua_newtable(L);
        luaL_setfuncs(L, display, 0);
        lua_settable(L, -3);
        
        lua_pushstring(L, "canvas");
        lua_newtable(L);
        luaL_setfuncs(L, canvas, 0);
        lua_settable(L, -3);
        
        lua_pushstring(L, "cursor");
        lua_newtable(L);
        luaL_setfuncs(L, cursor, 0);
        lua_settable(L, -3);
        
        lua_pushstring(L, "color");
        lua_newtable(L);
        luaL_setfuncs(L, color, 0);
        lua_settable(L, -3);
        
        lua_pushstring(L, "font");
        lua_newtable(L);
        luaL_setfuncs(L, font, 0);
        lua_settable(L, -3);
        
        lua_pushstring(L, "keypad");
        lua_newtable(L);
        luaL_setfuncs(L, keypad, 0);
        lua_settable(L, -3);
        
        lua_pushstring(L, "msr");
        lua_newtable(L);
        luaL_setfuncs(L, msr, 0);
        lua_settable(L, -3);

        lua_pushstring(L, "debug");
        lua_newtable(L);
        luaL_setfuncs(L, debug, 0);
        lua_settable(L, -3);
        
    return 1;
}

int init_ctos_lua(lua_State *L) {
    debug_text_str = calloc(1, sizeof(char));

    // Get package.preload so we can store builtins in it.
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "preload");
    lua_remove(L, -2); // Remove package

    // Store CTOS module definition at preload.CTOS
    lua_pushcfunction(L, luaopen_CTOS);
    lua_setfield(L, -2, "CTOS");

    return 0;
}

void shutdown_ctos_lua(lua_State *L) {
    free(debug_text_str);
    (void) L;
}
