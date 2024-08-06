# Modes

You will spend most time in the "Draw" mode, referred to as "Normal" mode. In
there you can draw, change colors and characters, save and load files and more.

There are more modes available though, each of which is specialized for one
specific task.

## Normal mode

### Normal mode key reference

||    Group     |     Key     |     Action     |              Explanation              ||
|| ------------ | ----------- | -------------- | ------------------------------------- ||
|| File /       | `<ctrl-q>`  | Quit           | Quit the app                          ||
|| Buffer       | `<ctrl-s>`  | Save           | Save buffer (or selection) to a file  ||
||              | `<ctrl-o>`  | Open           | Load from a file                      ||
||              | `<cltr-n>`  | Copy           | Copy selection                        ||
||              | `<ctrl-r>`  | Reload         | Redraw the current buffer             ||
||              | `<space>x`  | Draw with x    | Select char x for drawing             ||
|| Draw         | `[0-9]`     | Use color      | Select a color from the quick palette ||
|| Character    | `c[0-9]`    | Save color     | Save current color to quick palette   ||
||              | `i`         | Italics        | Toggle italics                        ||
||              | `b`         | Save color     | Toggle bold                           ||
||              | `<ctrl-i>`  | Invert         | Invert fore and background color      ||
|| Mode         | `s`         | Select         | Enter selection mode                  ||
||              | `p`         | Paste          | Enter paste preview mode              ||
|| Single       | `<arrows>`  | Move cursor    | Navigate the cursor                   ||
|| Char         | `<CR>`      | Draw at cursor | Draw a single char under the cursor   ||
|| (No mouse)   | `<BS>`      | Delete char    | Delete / erase under the cursor       ||

For colors, see [quick palette](docs/colors.md#quick-palette).


## Selection mode

Press `s` to enter selection mode.
In selection mode, dragging with the mouse selects areas. Multiple rectangular
areas can be selected simultaneously.

### Selection mode key reference

When the desired areas are selected, an action can be performed on them.

|     Key     | Action |        Explanation        |
| ----------- | ------ | ------------------------- |
| `<ctrl-q>`  | Quit   | Quit the app              |
| `s`         | End    | End selection mode        |
| `g`         | Grab   | Grab and move selection   |
| `c`         | Copy   | Copy selection            |
| `x`         | Cut    | Delete and copy selection |

The copied areas can be pasted using `p`.

## Drag mode
## Preview mode
