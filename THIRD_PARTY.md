# Third-party software

Protodish itself is under the MIT License (`LICENSE`). The downloads also contain, or were
built with, the following software. The license texts are in `packaging/licenses/` in the
source and next to the program in each download.

| Software | Used for | License | Text |
| --- | --- | --- | --- |
| [Godot Engine](https://godotengine.org) 4.7.2 | The app's window, drawing and controls; the exported program is the Godot runtime | MIT, with third-party components listed in its COPYRIGHT.txt | `godot-LICENSE.txt`, `godot-COPYRIGHT.txt` |
| [godot-cpp](https://github.com/godotengine/godot-cpp) 10.0.0 | C++ bindings linked into the extension library | MIT | `godot-cpp-LICENSE.md` |
| [MinGW-w64 winpthreads](https://www.mingw-w64.org) | Linked statically into the Windows extension library | MIT and BSD-style | `winpthreads-COPYING.txt` |
| [doctest](https://github.com/doctest/doctest) 2.4.12 | Unit tests only (`tests/doctest.h`); not in the downloads | MIT | in the header |

The release builds also link the GCC C++ runtime (libstdc++ and libgcc) statically. The GCC
Runtime Library Exception allows this and asks for no notice.
