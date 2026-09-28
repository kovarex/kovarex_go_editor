#include <app/Platform.hpp>

#include <game/Sgf.hpp>

#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX  // the build defines it already, for Agui
#endif
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <system_error>

namespace platform {

namespace {

// What the exe registers .sgf files under.
constexpr const wchar_t* PROG_ID = L"GoEditor.sgf";

std::wstring Widen(const std::string& text, UINT codePage = CP_UTF8)
{
  if (text.empty()) return {};
  const int n = MultiByteToWideChar(codePage, 0, text.data(), int(text.size()), nullptr, 0);
  std::wstring wide(size_t(n), L'\0');
  MultiByteToWideChar(codePage, 0, text.data(), int(text.size()), wide.data(), n);
  return wide;
}

std::string Narrow(const std::wstring& wide)
{
  if (wide.empty()) return {};
  const int n = WideCharToMultiByte(CP_UTF8, 0, wide.data(), int(wide.size()), nullptr, 0, nullptr, nullptr);
  std::string text(size_t(n), '\0');
  WideCharToMultiByte(CP_UTF8, 0, wide.data(), int(wide.size()), text.data(), n, nullptr, nullptr);
  return text;
}

// The Windows code page for a CA[] charset name, 0 for UTF-8 (nothing to
// convert), or -1 when the name means nothing to us.
int CodePageFor(std::string name)
{
  std::erase_if(name, [](char c) { return c == '-' || c == '_' || c == ' '; });
  std::transform(name.begin(), name.end(), name.begin(), [](char c) { return char(std::tolower(static_cast<unsigned char>(c))); });

  struct Entry {
    const char* name;
    int         codePage;
  };
  static constexpr Entry TABLE[] = {
    { "utf8", 0 },          { "utf8bom", 0 },
    { "gb2312", 936 },      { "gbk", 936 },          { "cp936", 936 },       { "euccn", 936 },  { "gb231280", 936 },
    { "gb18030", 54936 },
    { "big5", 950 },        { "big5hkscs", 950 },    { "cp950", 950 },
    { "shiftjis", 932 },    { "sjis", 932 },         { "mskanji", 932 },     { "cp932", 932 },  { "windows31j", 932 },
    { "eucjp", 20932 },
    { "euckr", 949 },       { "ksc56011987", 949 },  { "cp949", 949 },       { "uhc", 949 },
    { "iso88591", 28591 },  { "latin1", 28591 },     { "l1", 28591 },        { "ascii", 28591 }, { "usascii", 28591 },
    { "windows1252", 1252 }, { "cp1252", 1252 },
    { "iso88592", 28592 },  { "latin2", 28592 },     { "windows1250", 1250 }, { "cp1250", 1250 },
    { "windows1251", 1251 }, { "cp1251", 1251 },     { "koi8r", 20866 },
  };
  for (const Entry& e : TABLE) {
    if (name == e.name) return e.codePage;
  }
  return -1;
}

std::string Convert(const std::string& bytes, UINT codePage)
{
  return Narrow(Widen(bytes, codePage));
}

bool HasNonAscii(const std::string& bytes)
{
  return std::any_of(bytes.begin(), bytes.end(), [](char c) { return static_cast<unsigned char>(c) >= 0x80; });
}

// HKCU\Software\Classes\<key> gets `value` as its default (or `name`) value.
bool SetValue(const std::wstring& key, const wchar_t* name, const std::wstring& value, std::string* error)
{
  HKEY handle = nullptr;
  const std::wstring full = L"Software\\Classes\\" + key;
  LSTATUS status = RegCreateKeyExW(HKEY_CURRENT_USER, full.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &handle, nullptr);
  if (status == ERROR_SUCCESS) {
    status = RegSetValueExW(handle, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()),
                            DWORD((value.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(handle);
  }
  if (status != ERROR_SUCCESS && error) *error = "Couldn't write the registry key " + Narrow(full) + ".";
  return status == ERROR_SUCCESS;
}

std::wstring GetValue(HKEY root, const std::wstring& key, const wchar_t* name)
{
  wchar_t buffer[1024];
  DWORD   size = sizeof(buffer);
  if (RegGetValueW(root, key.c_str(), name, RRF_RT_REG_SZ, nullptr, buffer, &size) != ERROR_SUCCESS) return {};
  return buffer;
}

std::wstring OpenCommand()
{
  return L"\"" + ExecutablePath().wstring() + L"\" \"%1\"";
}

// Explorer's own record of what the user picked with "Open with... Always",
// which beats anything registered under Classes.
const wchar_t* USER_CHOICE = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.sgf\\UserChoice";

}  // namespace

std::optional<std::string> ReadText(const std::filesystem::path& path, std::string* error)
{
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    if (error) *error = "Couldn't open " + ToUtf8(path.filename()) + ".";
    return std::nullopt;
  }
  std::string bytes{ std::istreambuf_iterator<char>(in), {} };

  // Byte order marks settle it.
  if (bytes.size() >= 3 && bytes.compare(0, 3, "\xEF\xBB\xBF") == 0) return bytes.substr(3);
  if (bytes.size() >= 2 && (bytes.compare(0, 2, "\xFF\xFE") == 0 || bytes.compare(0, 2, "\xFE\xFF") == 0)) {
    const bool bigEndian = bytes[0] == '\xFE';
    std::wstring wide;
    for (size_t i = 2; i + 1 < bytes.size(); i += 2) {
      const auto lo = static_cast<unsigned char>(bytes[i + (bigEndian ? 1 : 0)]);
      const auto hi = static_cast<unsigned char>(bytes[i + (bigEndian ? 0 : 1)]);
      wide += wchar_t(lo | (hi << 8));
    }
    return Narrow(wide);
  }

  // Files are often labelled wrong -- CA[GB2312] on a file a server wrote in
  // UTF-8 -- and real text in a legacy charset is almost never valid UTF-8
  // by accident. So well-formed UTF-8 with anything beyond ASCII in it is
  // taken as UTF-8, whatever the label says.
  if (sgf::IsUtf8(bytes) && HasNonAscii(bytes)) return bytes;
  if (!HasNonAscii(bytes)) return bytes;

  const std::string declared = sgf::DeclaredCharset(bytes);
  const int codePage = declared.empty() ? -1 : CodePageFor(declared);
  if (codePage == 0) return bytes;
  return Convert(bytes, codePage > 0 ? UINT(codePage) : CP_ACP);
}

bool WriteText(const std::filesystem::path& path, const std::string& text, std::string* error)
{
  std::filesystem::path temp = path;
  temp += L".tmp";
  {
    std::ofstream out(temp, std::ios::binary | std::ios::trunc);
    if (!out || !out.write(text.data(), std::streamsize(text.size())) || !out.flush()) {
      if (error) *error = "Couldn't write " + ToUtf8(path.filename()) + ".";
      return false;
    }
  }
  std::error_code code;
  std::filesystem::rename(temp, path, code);
  if (code) {
    std::filesystem::remove(temp, code);
    if (error) *error = "Couldn't replace " + ToUtf8(path.filename()) + ".";
    return false;
  }
  return true;
}

std::string ToUtf8(const std::filesystem::path& path)
{
  return Narrow(path.wstring());
}

std::filesystem::path FromUtf8(const std::string& text)
{
  return std::filesystem::path(Widen(text));
}

std::filesystem::path DocumentsFolder()
{
  PWSTR folder = nullptr;
  std::filesystem::path result;
  if (SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &folder) == S_OK) result = folder;
  CoTaskMemFree(folder);
  if (result.empty()) result = std::filesystem::current_path();
  return result;
}

std::vector<std::filesystem::path> Drives()
{
  std::vector<std::filesystem::path> drives;
  wchar_t buffer[512];
  const DWORD n = GetLogicalDriveStringsW(DWORD(std::size(buffer)), buffer);
  for (const wchar_t* d = buffer; d < buffer + n && *d; d += wcslen(d) + 1) drives.emplace_back(d);
  return drives;
}

std::filesystem::path ExecutablePath()
{
  wchar_t buffer[MAX_PATH * 4];
  const DWORD n = GetModuleFileNameW(nullptr, buffer, DWORD(std::size(buffer)));
  return std::filesystem::path(std::wstring(buffer, n));
}

std::vector<std::filesystem::path> CommandLineFiles()
{
  std::vector<std::filesystem::path> files;
  int     count = 0;
  LPWSTR* args  = CommandLineToArgvW(GetCommandLineW(), &count);
  if (!args) return files;
  for (int i = 1; i < count; ++i) files.emplace_back(args[i]);
  LocalFree(args);
  return files;
}

bool AssociateSgfFiles(std::string* error)
{
  const std::wstring exe  = ExecutablePath().wstring();
  const std::wstring name = ExecutablePath().filename().wstring();

  bool ok = SetValue(L".sgf", nullptr, PROG_ID, error) &&
            SetValue(L".sgf", L"Content Type", L"application/x-go-sgf", error) &&
            SetValue(L".sgf\\OpenWithProgids", PROG_ID, L"", error) &&
            SetValue(PROG_ID, nullptr, L"Go game record", error) &&
            SetValue(std::wstring(PROG_ID) + L"\\DefaultIcon", nullptr, L"\"" + exe + L"\",0", error) &&
            SetValue(std::wstring(PROG_ID) + L"\\shell\\open\\command", nullptr, OpenCommand(), error) &&
            // So it is in the "Open with" list as well, under its own name.
            SetValue(L"Applications\\" + name + L"\\shell\\open\\command", nullptr, OpenCommand(), error) &&
            SetValue(L"Applications\\" + name + L"\\SupportedTypes", L".sgf", L"", error);
  if (!ok) return false;

  // A choice the user made in "Open with" wins over everything above. It is
  // theirs to change, so it is only cleared when it points somewhere else --
  // and when Windows won't let it be, they are told how to change it.
  const std::wstring chosen = GetValue(HKEY_CURRENT_USER, USER_CHOICE, L"ProgId");
  if (!chosen.empty() && chosen != PROG_ID) {
    RegDeleteKeyW(HKEY_CURRENT_USER, USER_CHOICE);
    if (!GetValue(HKEY_CURRENT_USER, USER_CHOICE, L"ProgId").empty()) {
      if (error) {
        *error = "Registered, but Windows is set to open .sgf files with another program. Right-click a .sgf file, "
                 "choose Open with > Choose another app, pick this one and tick \"Always use this app\".";
      }
      SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
      return false;
    }
  }

  SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
  return true;
}

bool IsSgfAssociated()
{
  const std::wstring chosen = GetValue(HKEY_CURRENT_USER, USER_CHOICE, L"ProgId");
  if (!chosen.empty() && chosen != PROG_ID) return false;
  if (GetValue(HKEY_CURRENT_USER, L"Software\\Classes\\.sgf", nullptr) != PROG_ID) return false;
  const std::wstring command =
      GetValue(HKEY_CURRENT_USER, std::wstring(L"Software\\Classes\\") + PROG_ID + L"\\shell\\open\\command", nullptr);
  return command == OpenCommand();
}

bool OpenInBrowser(const std::string& url, std::string* error)
{
  // ShellExecute's result is an HINSTANCE only for old times' sake: above 32
  // is success, anything else an error code.
  const auto result = reinterpret_cast<INT_PTR>(
      ShellExecuteW(nullptr, L"open", Widen(url).c_str(), nullptr, nullptr, SW_SHOWNORMAL));
  if (result > 32) return true;
  if (error) *error = "Couldn't open the web browser (error " + std::to_string(result) + ").";
  return false;
}

}  // namespace platform
