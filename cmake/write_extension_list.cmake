# Lists the extension in godot/.godot/extension_list.cfg, which Godot reads at startup.
# Without it, a run of the project (no editor) does not load the extension, and the
# editor's first scan of the project crashes in Godot 4.7.2 (DECISIONS M7).
# Usage: cmake -DPROJECT_DIR=<godot project> -P write_extension_list.cmake
set(list_file "${PROJECT_DIR}/.godot/extension_list.cfg")
if(NOT EXISTS "${list_file}")
  file(WRITE "${list_file}" "res://protodish.gdextension\n")
endif()
