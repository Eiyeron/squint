# Squint
## "Maybe if I squint, those pixels will look better"

![](.meta/screenshot.png)

Squint is a live upscaler companion program for [Aseprite][aseprite] that allows you to see live what your drawing would look like upscaled through one those filters:

- xBR-lv1-noblend (simpler, but less fancy)
- xBR-lv2 (fancier with extra smoothness and settings)

## Usage

### Aseprite setup
- Install the extension named `squint-client.aseprite-extension`. Either double-click the file or install it through Aseprite by locating the "Extensions" tab in the Preferences menu, pressing the "Add Extension" button and locating the extension.
- In the File menu, a new "Connect to Squint" entry should have appeared. If you don't see it, you probably haven't reloaded Aseprite or you may have accidentally disabled the extension.
- When clicking on the new menu entry, a small window should appear. It's non-blocking so you can still edit the current sprite while it's on.
- Aseprite is now waiting for squint to wait for a connection.
- You can now launch squint and see it showing your sprite once the communication is done.
- As a quirk from the first version of the script, the script will shut down if you switch to another sprite. This is [well known][issue_7] and is planned to be fixed.

### Controls
- F1 to toggle the help screen.
- F2 to toggle the background's color between white and dark gray.
- Right-click or TAB to toggle the options screen.
- S to save the current result into `saved.png`.
- F12 to screenshot.

## Compilation

Squint requires a compiler with C++17 and C11 support and [CMake][cmake].

It has a few dependencies: [raylib], [raygui] and [IXWebSocket][ixwebsocket].

### Tested setups
- Windows 10 x64 and Visual Studio 2019
- Archlinux x64 and GCC 11.1.0
    - The AUR's `aseprite` package doesn't build with websockets and scripting enabled. You might want to tweak the PKGBUILD to compile a version with those.

### Dependency handling
- Raylib will be downloaded automatically by CMake if it wasn't installed on your computer
- The other libraries will be fetched once you type `git submodules init --update` at the root of the repository.

### Compilation steps
- Install a compiler.
- Create a build folder inside the repo's root.
- Open a terminal there and press
```shell
    cmake build .. -DUSE_ZLIB=OFF
```
- CMake should create either a Makefile or a Visual Studio solution (or anything else if you precised it). Use your favorite IDE to use those to compile.
- If everything is alright, you should get a `squint` (or `squint.exe`) in the build folder.

[aseprite]: https://aseprite.org
[cmake]: https://cmake.org
[raylib]: https://raylib.com
[raygui]: https://github.com/raysan5/raygui
[ixwebsocket]: https://github.com/machinezone/IXWebSocket
[issue_7]: https://github.com/Eiyeron/squint/issues/7