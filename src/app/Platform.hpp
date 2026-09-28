
// The few things the operating system does for us: reading a file in whatever
// charset it was written in, the folders a file browser starts from,
// registering the program as what opens .sgf files, and the browser.
// PlatformWindows.cpp and PlatformLinux.cpp each do them their own way.
//
// On Windows this is kept apart from everything else because <windows.h> and
// raylib.h cannot share a translation unit -- both define Rectangle,
// CloseWindow, LoadImage and more -- so nothing here includes raylib, and
// nothing else includes windows.h.

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace platform {

// A game file's text as UTF-8, whatever it was saved in: UTF-8 with or
// without a byte order mark, UTF-16, or the legacy charset its CA[] names
// (GB2312, Shift_JIS, EUC-KR, Big5, Latin-1...). A file that names none and
// is not UTF-8 is read in the system's own code page -- on Linux, which has
// none, as Windows-1252. Nullopt if it can't be read, with the reason in
// `error`.
std::optional<std::string> ReadText(const std::filesystem::path& path, std::string* error);

// Writes a temporary file and renames it over the old one, so a crash
// mid-save never leaves half a game behind.
bool WriteText(const std::filesystem::path& path, const std::string& text, std::string* error);

// A path as UTF-8 for showing on screen, and back.
std::string           ToUtf8(const std::filesystem::path& path);
std::filesystem::path FromUtf8(const std::string& text);

// The user's Documents folder, where the file browser first opens.
std::filesystem::path DocumentsFolder();

// Where the file browser can jump to: the drives, as "C:\" and so on, or on
// Linux the root and the home folder.
std::vector<std::filesystem::path> Drives();

// The running program.
std::filesystem::path ExecutablePath();

// The files named on the command line -- what Explorer, or the desktop,
// passes when a .sgf is double-clicked. On Windows read from the wide command
// line, since main()'s argv is in the ANSI code page and would mangle a name
// like 本因坊.sgf.
std::vector<std::filesystem::path> CommandLineFiles();

// Makes double-clicking a .sgf file open it in this program, for the current
// user only -- on Windows under HKEY_CURRENT_USER\Software\Classes, on Linux
// with a .desktop file and the MIME defaults in ~/.local -- so it needs no
// administrator rights. False with the reason if that didn't work.
bool AssociateSgfFiles(std::string* error);

// Whether .sgf files currently open in this program.
bool IsSgfAssociated();

// Whether the window, its frame and title bar included, fits in the part of
// its monitor that windows may use -- the screen less the taskbar.
bool WindowFitsOnScreen(void* windowHandle);

// Opens `url` (UTF-8) in the default web browser. False with the reason if
// the system couldn't.
bool OpenInBrowser(const std::string& url, std::string* error);

}  // namespace platform
