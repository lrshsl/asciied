# AsciiEd

![Ascii Ed](docs/asciied_face.png)

A simple editor for ASCII art. It allows you to draw images with nothing more
than basic ASCII text.

The controls are mostly keyboard driven, but if your terminal supports it, you
can use the mouse for drawing.

In order to draw something on the screen, click and drag with your mouse over
the draw area. You should see some `x`s being drawn.

## Installation

Download source code and compile.

Needs to be installed and available:
- ncurses (is dynamically linked)
- make
- gcc
- git

For *nix:

```sh
git clone https://github.com/lrshsl/asciied
cd asciied
make release
```

Run:
```sh
./build/asciied_release
```

If you want to `install` it, you could just move the binary to a location which
is in your `$PATH`, for example with `cp -i build/asciied_release /usr/local/bin/asciied`.

The saved buffers are found under the installation directory in the `saves/` subdirectory.


## Getting started

### Modes

The editor supports several "Modes", in which keyboard and mouse signals mean
different things. The modes and their keybindings are explained in the
[modes.md](docs/modes.md) file.


### Change drawing character

To draw with another character, press the `<space>` key on your keyboard once,
followed by the character you want to use. Try `<space>` + `|`, `<space>` + `.`
or `<space>` + `o` and draw again and you should see the respective characters
appear on the screen!

Alternatively you can use the arrow keys on your keyboard to move the cursor,
and enter (written `<CR>`) or backspace (`<BS>`) to add respectively delete one
character at a time. This can be especially useful when drawing very precise
and regular shapes.


### Change colors

To select another color for drawing simply click on the respective color on the
top. In the status bar on the bottom right there is an indicator which shows
you which color you are currently drawing with.

Commonly used colors can be saved and accessed using the [quick
palette](docs/colors.md#quick-palette).

