## canvas

### canvas.clear

Clears the entire canvas, not just the visible area.

Examples:

    ctos = require("CTOS")
    ctos.canvas.clear()


### canvas.image

Draws an image with the specified filename at the given coordinates.

Examples:

    ctos = require("CTOS")
    ctos.canvas.image(100, 50, "button.bmp")


### canvas.rect

Draws a rectangle using the current foreground color at the given
position with the given width and height. A boolean value specifies
whether to fill the rectangle or not.

Examples:

    ctos = require("CTOS")
    ctos.canvas.rect(0, 10, 100, 90, true)


### canvas.text

Renders text using the current font at the given x/y coordinates
with the given font size. A boolean value specifies whether to draw
the text in reverse.

Examples:

    ctos = require("CTOS")
    -- x, y, string, font width, font height, reverse
    ctos.canvas.text(100, 150, "Hello", 8, 12, false)


## color

### color.background

Sets or gets the current background color to the given red, green
and blue values.

Note: each value should be in the range [0, 1] representing minimum
and maximum intensity, respectively.

Examples:

    ctos = require("CTOS")
    old_r, old_g, old_b = ctos.color.background()
    ctos.color.background(1, 1, 0) -- full yellow


### color.foreground

Sets or gets the current foreground color to the given red, green
and blue values.

Note: each value should be in the range [0, 1] representing minimum
and maximum intensity, respectively.

Examples:

    ctos = require("CTOS")
    old_r, old_g, old_b = ctos.color.foreground()
    ctos.color.foreground(1, 1, 0) -- full yellow


## cursor

### cursor.eol

Clears the screen from the current cursor location to the end of the
current line.

Examples:

    ctos = require("CTOS")
    ctos.cursor.print("hello")
    ctos.cursor.eol()


### cursor.is_reversed

Sets or gets whether printed text is being reversed.

Examples:

    ctos = require("CTOS")
    reversed = ctos.cursor.is_reversed()
    ctos.cursor.is_reversed(not reversed)


### cursor.position

Sets or gets the current cursor position.

Examples:

    ctos = require("CTOS")
    pos = ctos.cursor.position()
    ctos.cursor.position(pos.x, 100 - pos.y)


### cursor.print

Prints a string of text at the specified position, or at the current
cursor position if no position is specified. Returns the cursor's
position after printing.

Examples:

    ctos = require("CTOS")
    cursor_pos = ctos.cursor.print("hello")
    ctos.cursor.print("world", cursor_pos.x + 5, cursor_pos.y)


## display

### display.attributes

Gets information about the display. Returns the x and y resolution,
color depth in bits, and what type of touch screen is supported,
if any.

The return value is a table with the following structure:

    resolution:
      width: number (of pixels)
      height: number (of pixels)
    color: number (of bits: 2, 10 or 24)
    touch_type: string ("none", "resistor" or "capacitor-1p"

Examples:

    ctos = require("CTOS")
    attrs = ctos.display.attributes()


### display.clear

If in text mode, clears the window. If in graphics mode, clears
the visible area of the canvas.

Examples:

    ctos = require("CTOS")
    ctos.display.clear()


### display.contrast

Sets or gets the display contrast.

Note: values are in the range [0, 1], representing minimum and
maximum contrast, respectively.

Examples:

    ctos = require("CTOS")
    old_contrast = ctos.display.contrast()
    ctos.display.contrast(0.75)


### display.mode

Sets or gets the current drawing mode, which must be "text" or
"graphics".

Examples:

    ctos = require("CTOS")
    previous_mode = ctos.display.mode()
    ctos.display.mode("graphics")


## font

### font.face

Sets or gets the currently selected TTF font. A second argument can
specify regular, italic, bold, or bold-italic. If a second argument
is not given, regular is used by default.

Examples:

    ctos = require("CTOS")
    old_face = ctos.font.face()
    ctos.font.face("Arial", "italic")


### font.offset

Sets or gets the current font horizontal and vertical offsets.

Note: negative offsets will shift left/down, while positive offsets
will shift right/up.

Examples:

    ctos = require("CTOS")
    old_offset = ctos.font.offset()
    ctos.font.offset(old_offset.x + 1, old_offset.y - 1)


### font.size

Sets or gets the current font size.

Examples:

    ctos = require("CTOS")
    size = ctos.font.size()
    print("font width: " .. size.x .. ", height: " .. size.y)
    -- now double it!
    new_size = ctos.font.size size.x * 2, size.y * 2


### font.width

Measures the width of the specified string using the currently selected
font. Returns the width of the string in pixels.

Examples:

    ctos = require("CTOS")
    ctos.font.width("hello, world!")


## keypad

### keypad.flush

Flushes the keypad buffer.

Examples:

    ctos = require("CTOS")
    ctos.keypad.flush()


### keypad.frequency

Sets or gets the frequency and duration of the sound emitted by
a key press. The frequency is given in Hertz and the duration is
given in tens of milliseconds.

Examples:

    ctos = require("CTOS")
    freq, dur = ctos.keypad.frequency()
    ctos.keypad.frequency(freq + 10, dur + 10)


### keypad.getch

Returns the last key in the key buffer, and clears the key buffer.
If there are no keys in the key buffer, blocks until a key is
pressed.

Examples:

    ctos = require("CTOS")
    key = ctos.keypad.getch()


### keypad.is_any_key_pressed

Returns whether any key is currently pressed.

Examples:

    ctos = require("CTOS")
    if ctos.keypad.is_any_key_pressed() then
      -- ...
    end


### keypad.is_reset_enabled

Sets or gets whether pressing the F1 key will reset the device.

Examples:

    ctos = require("CTOS")
    reset_enabled = ctos.keypad.is_reset_enabled()
    ctos.keypad.is_reset_enabled(not reset_enabled)


### keypad.is_sound_enabled

Sets or gets whether sound is emitted when a key is pressed.

Examples:

    ctos = require("CTOS")
    sound = ctos.keypad.is_sound_enabled()
    ctos.keypad.is_sound_enabled(sound)


### keypad.last

Returns only the last character in the key buffer, and clears the
key buffer. If there are no keys in the key buffer, nil is returned.

Examples:

    ctos = require("CTOS")
    key = ctos.keypad.last()


### keypad.peek

Returns the next character in the key buffer, if any, without
removing it from the buffer. If the buffer is empty, returns nil.

Examples:

    ctos = require("CTOS")
    key = ctos.keypad.peek()


## msr

### msr.read

Attempts to read track data from a magnetic card swipe. Returns
track 1, track 2 and track 3 if successful, or nil if no swipe
has occurred. An error is raised if a swipe has occurred but track
data could not be read.

Examples:

    ctos = require("CTOS")
    track1, track2, track3 = ctos.msr.read()


