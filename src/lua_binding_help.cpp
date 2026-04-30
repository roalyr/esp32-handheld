#include "lua_binding_help.h"

#include "config.h"

namespace {

static const char kLuaBindingHelpBody[] = R"DOC(LUA DESKTOP APP SCHEMA
=====================

Discovery rules:
- Desktop only scans the SD card root for .lua files.
- The file must define APP_METADATA and APP.
- APP_METADATA.desktop must be true.
- APP_METADATA.name must be a non-empty string.
- APP_METADATA.icon must be 13 rows of text. Spaces are empty pixels. Any other character is a lit pixel.

Host object injected into SD apps:
- host.close()
  Close the running app and return to the desktop.
- host.notice(message, duration_ms)
  Show a transient notice on top of the desktop host.
- host.source_path
  Full SD path of the launched app file.

APP callbacks used by the desktop host:
- function APP:init(descriptor)
  Called once when the app launches.
- function APP:update()
  Called every frame while the app is active.
- function APP:draw(descriptor)
  Called every frame. The app is responsible for drawing the full screen.
- function APP:input(key)
  Called for just-pressed, repeating, and long-press key events.

CUSTOM MODULE: gfx
==================
- gfx.clear()
  Clear the display buffer.
- gfx.send()
  Send the current buffer to the LCD.
- gfx.pixel(x, y)
- gfx.line(x1, y1, x2, y2)
- gfx.rect(x, y, w, h)
- gfx.fillRect(x, y, w, h)
- gfx.circle(x, y, r)
- gfx.fillCircle(x, y, r)
- gfx.text(x, y, str)
- gfx.setFont(size)
  0=small, 1=medium, 2=large, 3=tiny.
- gfx.setColor(color)
  0=black, 1=white.
- gfx.width()
- gfx.height()

Example:
  gfx.clear()
  gfx.setFont(0)
  gfx.text(2, 10, "Hello")
  gfx.rect(0, 12, 40, 10)
  gfx.send()

CUSTOM MODULE: input
====================
- input.pressed(key)
  True if the key was just pressed this frame.
- input.held(key)
  True while the key is currently held.
- input.anyPressed()
  True if any key was just pressed this frame.
- input.getKeys()
  Returns an array of keys that are currently held.

Key constants:
- input.KEY_ESC
- input.KEY_BKSP
- input.KEY_TAB
- input.KEY_ENTER
- input.KEY_SHIFT
- input.KEY_ALT
- input.KEY_UP
- input.KEY_DOWN
- input.KEY_LEFT
- input.KEY_RIGHT

Example:
  if key == input.KEY_ENTER then
      host.notice("Pressed Enter", 1000)
  end

CUSTOM MODULE: sys
==================
- sys.millis()
  Milliseconds since boot.
- sys.delay(ms)
- sys.yield()
- sys.time()
  Returns {hours=, minutes=, seconds=}.
- sys.timeStr()
  Returns HH:MM:SS.
- sys.version()
  Returns firmware version string.
- print(...)
  Global serial print helper.

Example:
  local now = sys.time()
  print("time", sys.timeStr(), now.hours, now.minutes)

CUSTOM MODULE: fs
=================
- fs.list(path)
  Returns an array of entries or nil, err.
  Entry fields:
  name, path, is_dir, size, modified, modified_time, modified_short, modified_full
- fs.read(path)
  Returns file contents or nil, err.
- fs.write(path, content)
  Writes a full text file. Returns true or nil, err.

Example:
  local entries, err = fs.list("/")
  if not entries then
      print("fs.list failed", err)
  else
      for _, entry in ipairs(entries) do
          print(entry.name, entry.is_dir and "dir" or entry.size)
      end
  end

CUSTOM MODULE: ui
=================
- ui.header(title, rightText)
- ui.footer(leftHint, rightHint)
- ui.confirm(message, yesSelected)
  Draws the shared yes/no prompt.
- ui.message(message, buttonLabel, invertButton)
  Draws the shared informational dialog.
- ui.choice3(message, label0, label1, label2, selectedIndex)
  Draws the shared 3-option dialog.
- ui.viewFile(path, label)
  Open an SD text file in the native RO viewer.
- ui.editFile(path, label)
  Open an SD text file in the native RW editor.
  Files above the RW cap are rejected.
- ui.takeEditorResult()
  After a Lua-owned RW edit session returns, this gives a table with:
  action, save, path, label, source_kind, content

Example:
  ui.header("Files", "1/8")
  ui.footer("ENT:Open", "ESC:Back")

CUSTOM MODULE: t9
=================
- t9.reset()
- t9.input(key)
- t9.update()
- t9.getText()
- t9.setText(str)
- t9.getCursorByte()
- t9.getCursorChar()
- t9.setCursor(pos)
- t9.isPending()
- t9.isShifted()
- t9.commit()
- t9.getCharCount()

Example:
  t9.reset()
  t9.setText("hello")
  print(t9.getText(), t9.getCursorByte())

STANDARD LIBRARIES
==================
The firmware opens Lua standard libraries with luaL_openlibs(). Common libraries like math,
string, table, coroutine, utf8, and debug are available. For SD card access, use fs.* instead
of Lua's own file I/O.

MEMORY NOTES
============
The Lua VM does not have a separate fixed memory pool in this firmware. It is created with
luaL_newstate(), so Lua allocations come from the same general heap reported by the firmware.
That means free heap is the practical upper bound for additional Lua allocations, minus other
runtime allocations and fragmentation.
)DOC";

}  // namespace

String buildLuaBindingHelpText() {
    String text;
    text.reserve(sizeof(kLuaBindingHelpBody) + 96);
    text += "ESP32 Handheld Lua reference\n";
    text += "Firmware revision: ";
    text += FIRMWARE_VERSION;
    text += "\n\n";
    text += kLuaBindingHelpBody;
    return text;
}