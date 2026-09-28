// The few things only Windows can do for us: reading a file in whatever
// charset it was written in, the folders a file browser starts from, and
// registering the exe as what opens .sgf files.
//
// Kept apart from everything else because <windows.h> and raylib.h cannot
// share a translation unit -- both define Rectangle, CloseWindow, LoadImage
// and more -- so nothing here includes raylib, and nothing else includes
// windows.h.

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace platform {

// A game file's text as UTF-8, whatever it was saved in: UTF-8 with or
// without a byte order mark, UTF-16, or the legacy charset its CA[] names
// (GB2312, Shift_JIS, EUC-KR, Big5, Latin-1...). A file that names none and
// is not UTF-8 is read in the system's own code page. Nullopt if it can't be
// read, with the reason in `error`.
std::optional<std::string> ReadText(const std::filesystem::path& path, std::string* error);

// Writes a temporary file and renames it over the old one, so a crash
// mid-save never leaves half a game behind.
bool WriteText(const std::filesystem::path& path, const std::string& text, std::string* error);

// A path as UTF-8 for showing on screen, and back.
std::string           ToUtf8(const std::filesystem::path& path);
std::filesystem::path FromUtf8(const std::string& text);

// The user's Documents folder, where the file browser first opens.
std::filesystem::path DocumentsFolder();

// The drives there are, as "C:\" and so on.
std::vector<std::filesystem::path> Drives();

// The running exe.
std::filesystem::path ExecutablePath();

// The files named on the command line -- what Explorer passes when a .sgf is
// double-clicked. Read from the wide command line, since main()'s argv is in
// the ANSI code page and would mangle a name like 本因坊.sgf.
std::vector<std::filesystem::path> CommandLineFiles();

// Makes double-clicking a .sgf file open it in this exe, for the current
// user only (HKEY_CURRENT_USER\Software\Classes), so it needs no
// administrator rights. False with the reason if the registry says no.
bool AssociateSgfFiles(std::string* error);

// Whether .sgf files currently open in this exe.
bool IsSgfAssociated();

}  // namespace platform
