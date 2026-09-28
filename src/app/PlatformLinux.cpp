// Platform.hpp on Linux. (PlatformWindows.cpp is the same on Windows; each
// compiles to nothing on the other.)
#ifdef __linux__

#include <app/Platform.hpp>

#include <game/Sgf.hpp>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <iconv.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string_view>
#include <system_error>
#include <thread>

extern char** environ;

// The icon, embedded for the About page; given to the desktop as well.
namespace ui {
extern const unsigned char APP_ICON_PNG[];
extern const std::size_t   APP_ICON_PNG_SIZE;
}  // namespace ui

namespace platform {

namespace {

// What the desktop knows the program by: its .desktop file's name, and the
// icon's.
constexpr const char* DESKTOP_FILE = "kovarex_go_editor.desktop";
constexpr const char* ICON_NAME    = "kovarex_go_editor";
// The MIME type SGF files are, as shared-mime-info has it.
constexpr const char* SGF_MIME     = "application/x-go-sgf";

std::filesystem::path Home()
{
  const char* home = std::getenv("HOME");
  return home ? std::filesystem::path(home) : std::filesystem::current_path();
}

// $XDG_DATA_HOME, ~/.local/share by default: where the user's own .desktop
// files, icons and MIME types go.
std::filesystem::path DataHome()
{
  const char* data = std::getenv("XDG_DATA_HOME");
  return data && *data ? std::filesystem::path(data) : Home() / ".local" / "share";
}

// The iconv name for a CA[] charset name, "" for UTF-8 (nothing to convert),
// or nullptr when the name means nothing to us.
const char* CharsetFor(std::string name)
{
  std::erase_if(name, [](char c) { return c == '-' || c == '_' || c == ' '; });
  std::transform(name.begin(), name.end(), name.begin(), [](char c) { return char(std::tolower(static_cast<unsigned char>(c))); });

  struct Entry {
    const char* name;
    const char* charset;
  };
  static constexpr Entry TABLE[] = {
    { "utf8", "" },          { "utf8bom", "" },
    { "gb2312", "GBK" },     { "gbk", "GBK" },        { "cp936", "GBK" },     { "euccn", "GBK" }, { "gb231280", "GBK" },
    { "gb18030", "GB18030" },
    { "big5", "BIG5" },      { "big5hkscs", "BIG5-HKSCS" }, { "cp950", "CP950" },
    { "shiftjis", "CP932" }, { "sjis", "CP932" },     { "mskanji", "CP932" }, { "cp932", "CP932" }, { "windows31j", "CP932" },
    { "eucjp", "EUC-JP" },
    { "euckr", "CP949" },    { "ksc56011987", "CP949" }, { "cp949", "CP949" }, { "uhc", "CP949" },
    { "iso88591", "ISO-8859-1" }, { "latin1", "ISO-8859-1" }, { "l1", "ISO-8859-1" },
    { "ascii", "ISO-8859-1" },    { "usascii", "ISO-8859-1" },
    { "windows1252", "CP1252" },  { "cp1252", "CP1252" },
    { "iso88592", "ISO-8859-2" }, { "latin2", "ISO-8859-2" }, { "windows1250", "CP1250" }, { "cp1250", "CP1250" },
    { "windows1251", "CP1251" },  { "cp1251", "CP1251" },     { "koi8r", "KOI8-R" },
  };
  for (const Entry& e : TABLE) {
    if (name == e.name) return e.charset;
  }
  return nullptr;
}

// `bytes` in `charset`, as UTF-8. What can't be converted is left out.
std::string Convert(const std::string& bytes, const char* charset)
{
  iconv_t cd = iconv_open("UTF-8//IGNORE", charset);
  if (cd == iconv_t(-1)) return bytes;
  std::string out(bytes.size() * 4 + 16, '\0');
  char*  in       = const_cast<char*>(bytes.data());
  size_t inLeft   = bytes.size();
  char*  to       = out.data();
  size_t outLeft  = out.size();
  while (inLeft > 0) {
    if (iconv(cd, &in, &inLeft, &to, &outLeft) != size_t(-1)) break;
    if (errno != EILSEQ && errno != EINVAL) break;
    ++in;  // a byte that isn't in the charset: skipped
    --inLeft;
  }
  iconv_close(cd);
  out.resize(out.size() - outLeft);
  return out;
}

bool HasNonAscii(const std::string& bytes)
{
  return std::any_of(bytes.begin(), bytes.end(), [](char c) { return static_cast<unsigned char>(c) >= 0x80; });
}

// UTF-16 in the given byte order as UTF-8, surrogate pairs and all.
std::string FromUtf16(const std::string& bytes, size_t start, bool bigEndian)
{
  std::string out;
  const auto unit = [&](size_t i) -> char32_t {
    const auto a = static_cast<unsigned char>(bytes[i]), b = static_cast<unsigned char>(bytes[i + 1]);
    return bigEndian ? char32_t(a << 8 | b) : char32_t(b << 8 | a);
  };
  for (size_t i = start; i + 1 < bytes.size(); i += 2) {
    char32_t c = unit(i);
    if (c >= 0xD800 && c < 0xDC00 && i + 3 < bytes.size()) {
      const char32_t low = unit(i + 2);
      if (low >= 0xDC00 && low < 0xE000) {
        c = 0x10000 + ((c - 0xD800) << 10) + (low - 0xDC00);
        i += 2;
      }
    }
    if (c < 0x80) {
      out += char(c);
    } else if (c < 0x800) {
      out += char(0xC0 | c >> 6);
      out += char(0x80 | (c & 0x3F));
    } else if (c < 0x10000) {
      out += char(0xE0 | c >> 12);
      out += char(0x80 | (c >> 6 & 0x3F));
      out += char(0x80 | (c & 0x3F));
    } else {
      out += char(0xF0 | c >> 18);
      out += char(0x80 | (c >> 12 & 0x3F));
      out += char(0x80 | (c >> 6 & 0x3F));
      out += char(0x80 | (c & 0x3F));
    }
  }
  return out;
}

// Runs `program` with `args`, found on the PATH, and waits for it. Its
// standard output, if `output` is given. False if it couldn't be run or
// failed.
bool Run(const char* program, std::vector<std::string> args, std::string* output = nullptr)
{
  int pipeFds[2] = { -1, -1 };
  if (output && pipe(pipeFds) != 0) return false;
  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init(&actions);
  if (output) {
    posix_spawn_file_actions_adddup2(&actions, pipeFds[1], STDOUT_FILENO);
    posix_spawn_file_actions_addclose(&actions, pipeFds[0]);
  }
  args.insert(args.begin(), program);
  std::vector<char*> argv;
  for (std::string& a : args) argv.push_back(a.data());
  argv.push_back(nullptr);
  pid_t      pid     = 0;
  const bool started = posix_spawnp(&pid, program, &actions, nullptr, argv.data(), environ) == 0;
  posix_spawn_file_actions_destroy(&actions);
  if (output) {
    close(pipeFds[1]);
    if (started) {
      std::array<char, 512> buffer;
      for (ssize_t n; (n = read(pipeFds[0], buffer.data(), buffer.size())) > 0;) output->append(buffer.data(), size_t(n));
    }
    close(pipeFds[0]);
  }
  if (!started) return false;
  int status = 0;
  waitpid(pid, &status, 0);
  return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

std::string Trim(std::string text)
{
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) text.pop_back();
  return text;
}

// The line the .desktop file runs the program with.
std::string ExecLine()
{
  return "Exec=\"" + ExecutablePath().string() + "\" %f";
}

bool WriteFile(const std::filesystem::path& path, std::string_view data, std::string* error)
{
  std::error_code code;
  std::filesystem::create_directories(path.parent_path(), code);
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (out && out.write(data.data(), std::streamsize(data.size()))) return true;
  if (error) *error = "Couldn't write " + path.string() + ".";
  return false;
}

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
    return FromUtf16(bytes, 2, bytes[0] == '\xFE');
  }

  // As on Windows: well-formed UTF-8 is UTF-8, whatever the label says.
  if (!HasNonAscii(bytes) || sgf::IsUtf8(bytes)) return bytes;

  const std::string declared = sgf::DeclaredCharset(bytes);
  const char*       charset  = declared.empty() ? nullptr : CharsetFor(declared);
  if (charset && !*charset) return bytes;
  return Convert(bytes, charset ? charset : "CP1252");
}

bool WriteText(const std::filesystem::path& path, const std::string& text, std::string* error)
{
  std::filesystem::path temp = path;
  temp += ".tmp";
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

// Linux paths are bytes, and in practice UTF-8.
std::string ToUtf8(const std::filesystem::path& path)
{
  return path.string();
}

std::filesystem::path FromUtf8(const std::string& text)
{
  return std::filesystem::path(text);
}

std::filesystem::path DocumentsFolder()
{
  // xdg-user-dirs' record of it, with $HOME in it spelled out.
  std::ifstream dirs(Home() / ".config" / "user-dirs.dirs");
  for (std::string line; std::getline(dirs, line);) {
    constexpr std::string_view KEY = "XDG_DOCUMENTS_DIR=\"";
    if (!line.starts_with(KEY) || line.back() != '"') continue;
    std::string value = line.substr(KEY.size(), line.size() - KEY.size() - 1);
    if (value.starts_with("$HOME")) value = Home().string() + value.substr(5);
    std::error_code code;
    if (std::filesystem::is_directory(value, code)) return value;
  }
  std::error_code code;
  if (std::filesystem::is_directory(Home() / "Documents", code)) return Home() / "Documents";
  return Home();
}

std::vector<std::filesystem::path> Drives()
{
  return { "/", Home() };
}

std::filesystem::path ExecutablePath()
{
  std::error_code code;
  return std::filesystem::read_symlink("/proc/self/exe", code);
}

std::vector<std::filesystem::path> CommandLineFiles()
{
  // The arguments as they were given, one after another with a nul after each.
  std::ifstream           in("/proc/self/cmdline", std::ios::binary);
  std::vector<std::filesystem::path> files;
  bool                    first = true;
  for (std::string arg; std::getline(in, arg, '\0');) {
    if (!first) files.emplace_back(arg);
    first = false;
  }
  return files;
}

bool AssociateSgfFiles(std::string* error)
{
  const std::filesystem::path data = DataHome();

  // The icon, where the desktop looks for it by name.
  const std::filesystem::path icon = data / "icons" / "hicolor" / "256x256" / "apps" / (std::string(ICON_NAME) + ".png");
  if (!WriteFile(icon, { reinterpret_cast<const char*>(ui::APP_ICON_PNG), ui::APP_ICON_PNG_SIZE }, error)) return false;

  // .sgf as a MIME type, in case the system doesn't know it already.
  const std::string mime =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<mime-info xmlns=\"http://www.freedesktop.org/standards/shared-mime-info\">\n"
      "  <mime-type type=\"" + std::string(SGF_MIME) + "\">\n"
      "    <comment>Go game record</comment>\n"
      "    <glob pattern=\"*.sgf\"/>\n"
      "  </mime-type>\n"
      "</mime-info>\n";
  if (!WriteFile(data / "mime" / "packages" / "kovarex_go_editor-sgf.xml", mime, error)) return false;
  Run("update-mime-database", { (data / "mime").string() });

  // The program, as the desktop's menus and "Open with" know it.
  const std::string desktop = "[Desktop Entry]\n"
                              "Type=Application\n"
                              "Name=Go Editor\n"
                              "Comment=Record, edit and annotate games of Go\n" +
                              ExecLine() + "\n"
                              "Icon=" + std::string(ICON_NAME) + "\n"
                              "MimeType=" + std::string(SGF_MIME) + ";\n"
                              "Terminal=false\n"
                              "Categories=Game;BoardGame;\n";
  if (!WriteFile(data / "applications" / DESKTOP_FILE, desktop, error)) return false;
  Run("update-desktop-database", { (data / "applications").string() });

  if (!Run("xdg-mime", { "default", DESKTOP_FILE, SGF_MIME })) {
    if (error) *error = "Registered, but xdg-mime couldn't make it the default: is xdg-utils installed?";
    return false;
  }
  return true;
}

bool IsSgfAssociated()
{
  // What the desktop opens .sgf files with, and whether that .desktop file
  // runs this program -- not another build of the same name.
  std::string chosen;
  if (!Run("xdg-mime", { "query", "default", SGF_MIME }, &chosen) || Trim(chosen) != DESKTOP_FILE) return false;
  std::ifstream desktop(DataHome() / "applications" / DESKTOP_FILE);
  for (std::string line; std::getline(desktop, line);) {
    if (Trim(line) == ExecLine()) return true;
  }
  return false;
}

bool WindowFitsOnScreen(void*)
{
  // raylib's window, and the part of its monitor that windows may use -- the
  // screen less the desktop's panels.
  GLFWwindow* window  = glfwGetCurrentContext();
  GLFWmonitor* monitor = glfwGetPrimaryMonitor();
  if (!window || !monitor) return true;  // nothing to go on: leave it be
  int x = 0, y = 0, width = 0, height = 0;
  glfwGetMonitorWorkarea(monitor, &x, &y, &width, &height);
  int w = 0, h = 0, left = 0, top = 0, right = 0, bottom = 0;
  glfwGetWindowSize(window, &w, &h);
  glfwGetWindowFrameSize(window, &left, &top, &right, &bottom);
  return width <= 0 || height <= 0 || (w + left + right <= width && h + top + bottom <= height);
}

bool OpenInBrowser(const std::string& url, std::string* error)
{
  // xdg-open hands it to whatever the desktop uses; waited for on a thread of
  // its own, so a slow one doesn't hold the editor up.
  std::string whereTo = url;
  std::error_code code;
  if (!std::filesystem::exists("/usr/bin/xdg-open", code) && !std::filesystem::exists("/bin/xdg-open", code)) {
    if (error) *error = "Couldn't open the web browser: xdg-open isn't installed.";
    return false;
  }
  std::thread([whereTo] { Run("xdg-open", { whereTo }); }).detach();
  return true;
}

}  // namespace platform

#endif  // __linux__
