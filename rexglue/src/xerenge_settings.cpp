// Xerenge's settings in the game's own Settings screen: the rows, as
// settings_concept.txt lays them out, and xerenge.conf - the file the
// installer writes and the launcher reads at every start - behind them.
#include "xerenge_settings.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/system/kernel_state.h>

REXCVAR_DECLARE(std::string, game_data_root);
REXCVAR_DECLARE(std::string, lobby_server);
REXCVAR_DECLARE(std::string, gamertag);

void XerengeSetMenuAntiAliasing(int32_t mode);
void XerengeSetMenuSharpening(float strength);
void XerengeSetMenuBloom(bool on);
void XerengeSetMenuMotionBlur(bool on);

namespace xerenge::settings {
namespace {

// --- xerenge.conf ----------------------------------------------------------

std::filesystem::path ConfPath() {
  const std::string root = REXCVAR_GET(game_data_root);
  if (root.empty()) {
    return {};
  }
  return std::filesystem::path(root).parent_path() / "xerenge.conf";
}

std::string Trim(const std::string& text) {
  const size_t begin = text.find_first_not_of(" \t\r");
  if (begin == std::string::npos) {
    return {};
  }
  return text.substr(begin, text.find_last_not_of(" \t\r") - begin + 1);
}

std::map<std::string, std::string> ReadConf() {
  std::map<std::string, std::string> values;
  std::ifstream in(ConfPath());
  std::string line;
  while (std::getline(in, line)) {
    const std::string t = Trim(line);
    const size_t equals = t.find('=');
    if (t.empty() || t[0] == '#' || equals == std::string::npos) {
      continue;
    }
    values[Trim(t.substr(0, equals))] = Trim(t.substr(equals + 1));
  }
  return values;
}

// Rewrites the key's line, or adds one; the rest of the file (comments, the
// keys the menu does not show) as it was.
void WriteConf(const std::string& key, const std::string& value) {
  const std::filesystem::path path = ConfPath();
  if (path.empty()) {
    return;
  }
  std::ifstream in(path);
  std::string text, line;
  bool written = false;
  while (std::getline(in, line)) {
    const std::string t = Trim(line);
    const size_t equals = t.find('=');
    if (!t.empty() && t[0] != '#' && equals != std::string::npos && Trim(t.substr(0, equals)) == key) {
      line = key + " = " + value;
      written = true;
    }
    text += line + "\n";
  }
  in.close();
  if (!written) {
    text += key + " = " + value + "\n";
  }
  std::ofstream(path, std::ios::trunc) << text;
  REXLOG_INFO("menu: {} = {} (in {})", key, value, path.string());
}

std::string EnvValue(const char* name) {
  const char* value = std::getenv(name);
  return value ? value : "";
}

std::string Lower(std::string text) {
  for (auto& ch : text) {
    ch = char(std::tolower(static_cast<unsigned char>(ch)));
  }
  return text;
}

std::string ConfValue(const std::string& key, const std::string& fallback) {
  const auto values = ReadConf();
  const auto found = values.find(key);
  return found == values.end() ? fallback : found->second;
}

bool ConfFlag(const std::string& key, bool fallback) {
  const std::string value = ConfValue(key, fallback ? "true" : "false");
  return value == "true";
}

// --- the rows ----------------------------------------------------------------

struct Option {
  const char* label;
  const char* value;
};

struct Setting {
  int id;
  Page page;
  const char* label;
  // A setting of one key: its options' values in the file, and the default's index.
  const char* key = nullptr;
  std::vector<Option> options;
  int fallback = 0;
  bool restart = true;
  std::function<void(int)> live;  // applied without a restart
  // For those: the option the game started with (its environment, as the launcher
  // or run.sh set it), which is what it shows until one is chosen.
  std::function<int()> running;
  // Or one of several keys: its own reading and writing.
  std::function<int()> get;
  std::function<void(int)> set;
};

std::vector<Option> OffOn() { return {{"OFF", "false"}, {"ON", "true"}}; }

const std::vector<Setting>& Settings() {
  static const std::vector<Setting> settings = [] {
    std::vector<Setting> s;
    s.push_back({1, kMain, "LANGUAGE", "language",
                 {{"ASK AT START", ""}, {"ENGLISH", "0"}, {"ENGLISH (US)", "1"}, {"ESPANOL", "5"},
                  {"NEDERLANDS", "10"}, {"SVENSKA", "11"}, {"SUOMI", "12"}},
                 1});

    s.push_back({10, kGraphics, "KEEP 16:9", "aspect_16_9", OffOn(), 1});
    s.push_back({11, kGraphics, "RENDER RESOLUTION", "render_resolution",
                 {{"THE WINDOW'S", ""}, {"1600X900", "1600x900"}, {"1280X720", "1280x720"},
                  {"960X540", "960x540"}, {"854X480", "854x480"}, {"640X360", "640x360"}},
                 0});
    s.push_back({12, kGraphics, "ANISOTROPIC FILTERING", "anisotropy", {{"OFF", "off"}, {"4X", "4"}, {"8X", "8"}}, 2});
    Setting aa{13, kGraphics, "ANTI-ALIASING", "aa", {{"OFF", "off"}, {"FXAA", "fxaa"}, {"SMAA", "smaa"}}, 0};
    aa.restart = false;
    aa.live = [](int option) { XerengeSetMenuAntiAliasing(option); };
    aa.running = [] {
      const std::string name = Lower(EnvValue("XERENGE_AA"));
      return name == "fxaa" ? 1 : name == "smaa" ? 2 : 0;
    };
    s.push_back(aa);
    Setting sharpen{14, kGraphics, "SHARPENING", "sharpen",
                    {{"OFF", "0"}, {"LIGHT", "0.3"}, {"MEDIUM", "0.6"}, {"STRONG", "1"}}, 0};
    sharpen.restart = false;
    sharpen.live = [](int option) {
      static const float kStrength[] = {0.0f, 0.3f, 0.6f, 1.0f};
      XerengeSetMenuSharpening(kStrength[option]);
    };
    sharpen.running = [] {
      const float strength = std::strtof(EnvValue("XERENGE_SHARPEN").c_str(), nullptr);
      return strength >= 0.8f ? 3 : strength >= 0.45f ? 2 : strength >= 0.15f ? 1 : 0;
    };
    s.push_back(sharpen);
    // Bloom and blur at once as far as the game's passes go (render_patches.cpp,
    // plume's pipelines); the blur's motion vectors, which plume draws only when
    // it starts with the blur on, then follow at the next start.
    Setting bloom{15, kGraphics, "BLOOM", "bloom", OffOn(), 0};
    bloom.restart = false;
    bloom.live = [](int option) { XerengeSetMenuBloom(option == 1); };
    bloom.running = [] { return std::getenv("XERENGE_NO_BLOOM") ? 0 : 1; };
    s.push_back(bloom);
    Setting blur{16, kGraphics, "BLUR", "motion_blur", OffOn(), 0};
    blur.restart = false;
    blur.live = [](int option) { XerengeSetMenuMotionBlur(option == 1); };
    blur.running = [] { return std::getenv("XERENGE_NO_MOTION_BLUR") ? 0 : 1; };
    s.push_back(blur);

    // The frame rates: 30 is fps_30, 120 frame_generation (for the race and
    // Crash mode alike), Crash mode's 60 crash_interpolation.
    Setting game_fps{20, kHacks, "GAME FPS"};
    game_fps.options = {{"30", ""}, {"60", ""}, {"120", ""}};
    game_fps.get = [] {
      return ConfFlag("fps_30", false) ? 0 : ConfFlag("frame_generation", false) ? 2 : 1;
    };
    game_fps.set = [](int option) {
      WriteConf("fps_30", option == 0 ? "true" : "false");
      WriteConf("frame_generation", option == 2 ? "true" : "false");
    };
    s.push_back(game_fps);
    Setting crash_fps{21, kHacks, "CRASH FPS"};
    crash_fps.options = {{"30", ""}, {"60", ""}, {"120", ""}};
    crash_fps.get = [] {
      return !ConfFlag("crash_interpolation", true) ? 0 : ConfFlag("frame_generation", false) ? 2 : 1;
    };
    crash_fps.set = [](int option) {
      WriteConf("crash_interpolation", option == 0 ? "false" : "true");
      if (option == 2) {
        WriteConf("frame_generation", "true");
      }
    };
    s.push_back(crash_fps);
    s.push_back({22, kHacks, "COLOUR GRADING", "colour_grade", {{"AS ON THE CONSOLE", "1"}, {"HALF", "0.5"}, {"OFF", "0"}}, 0});
    s.push_back({23, kHacks, "ASYNCHRONOUS PRESENTATION", "async_present", OffOn(), 0});
    s.push_back({24, kHacks, "EARLY SUBMIT", "early_submit", OffOn(), 1});
    s.push_back({25, kHacks, "PACKED VERTICES", "packed_vertices", OffOn(), 1});
    s.push_back({26, kHacks, "REFLECTIONS IN TURNS", "reflection_split", OffOn(), 0});

    s.push_back({30, kDebug, "DEBUG LOGS", "debug", OffOn(), 0});
    s.push_back({31, kDebug, "POWER MODE", "power_mode", {{"STOCK", "stock"}, {"PERFORMANCE", "performance"}}, 0});
    s.push_back({32, kDebug, "FRAME COUNTER", "fps_counter", OffOn(), 0});
    // RenderDoc's overlay ("Capturing Vulkan") where it is installed, F12 captures.
    s.push_back({33, kDebug, "RENDERDOC", "renderdoc", OffOn(), 1});
    return s;
  }();
  return settings;
}

const Setting* Find(int id) {
  for (const Setting& s : Settings()) {
    if (s.id == id) {
      return &s;
    }
  }
  return nullptr;
}

Page g_page = kMain;
std::map<int, int> g_live;  // live settings chosen in the menus: id -> option
bool g_single_language = false;
bool g_restart = false;


// The game's achievements (the disc's XDBF, as the SDK read it), in its order.
std::vector<rex::system::AchievementInfo> Achievements() {
  auto* kernel_state = REX_KERNEL_STATE();
  return kernel_state ? kernel_state->achievements().ListAchievements()
                      : std::vector<rex::system::AchievementInfo>{};
}

std::string Upper(std::string text) {
  for (char& ch : text) {
    ch = char(std::toupper(static_cast<unsigned char>(ch)));
  }
  return text;
}

}  // namespace

Page CurrentPage() { return g_page; }
void SetPage(Page page) { g_page = page; }
void SetSingleLanguage(bool single) { g_single_language = single; }

std::string PageTitle(Page page) {
  switch (page) {
    case kGraphics: return "[GRAPHICS]";
    case kHacks: return "[HACKS]";
    case kDebug: return "[DEBUG]";
    case kAchievements: {
      const auto list = Achievements();
      size_t earned = 0;
      for (const auto& a : list) {
        earned += REX_KERNEL_STATE()->achievements().IsUnlocked(a.id) ? 1 : 0;
      }
      return "[ACHIEVEMENTS " + std::to_string(earned) + "/" + std::to_string(list.size()) + "]";
    }
    default: return {};
  }
}

std::vector<Row> Rows(Page page) {
  std::vector<Row> rows;
  if (page == kPause) {
    // What changes without a restart, in the order the pause menu shows it.
    for (const int id : {15, 16, 14, 13}) {
      if (const Setting* s = Find(id)) {
        Row row{s->label, {}, s->id};
        for (const Option& o : s->options) {
          row.options.push_back(o.label);
        }
        rows.push_back(std::move(row));
      }
    }
    return rows;
  }
  for (const Setting& s : Settings()) {
    if (s.page != page || (g_single_language && s.key && std::string(s.key) == "language")) {
      continue;
    }
    Row row{s.label, {}, s.id};
    for (const Option& o : s.options) {
      row.options.push_back(o.label);
    }
    rows.push_back(std::move(row));
  }
  if (page == kDebug) {
    // Shown, not chosen here: the installer's Network tab sets them.
    const auto conf = ReadConf();
    const auto value = [&](const char* key) {
      const auto found = conf.find(key);
      return found == conf.end() ? std::string() : found->second;
    };
    // As the game runs with them (its arguments), else as xerenge.conf has them.
    std::string gamertag = REXCVAR_GET(gamertag);
    if (gamertag.empty()) {
      gamertag = value("gamertag");
    }
    std::string server = REXCVAR_GET(lobby_server);
    if (server.empty()) {
      server = value("lobby_server");
    }
    rows.push_back({"GAMERTAG: " + (gamertag.empty() ? std::string("PLAYER") : gamertag), {}, 0});
    rows.push_back({"SERVER: " + (server.empty() ? std::string("NONE") : server), {}, 0});
  }
  return rows;
}

int Get(int id) {
  const Setting* s = Find(id);
  if (!s) {
    return 0;
  }
  if (s->running) {
    // What the game runs with now: as it started, or as chosen since.
    const auto chosen = g_live.find(id);
    return chosen != g_live.end() ? chosen->second : s->running();
  }
  if (s->get) {
    return s->get();
  }
  const std::string value = ConfValue(s->key, s->options[s->fallback].value);
  for (size_t i = 0; i < s->options.size(); ++i) {
    if (value == s->options[i].value) {
      return int(i);
    }
  }
  return s->fallback;
}

void Set(int id, int option) {
  const Setting* s = Find(id);
  if (!s || option < 0 || size_t(option) >= s->options.size()) {
    return;
  }
  if (Get(id) == option) {
    return;
  }
  if (s->set) {
    s->set(option);
  } else {
    WriteConf(s->key, s->options[option].value);
  }
  if (s->live) {
    s->live(option);
    g_live[id] = option;
  }
  g_restart = g_restart || s->restart;
}

std::vector<AchievementRow> AchievementRows() {
  std::vector<AchievementRow> earned, locked;
  auto* kernel_state = REX_KERNEL_STATE();
  if (!kernel_state) {
    return {};
  }
  const auto& manager = kernel_state->achievements();
  for (const auto& a : Achievements()) {
    AchievementRow row{Upper(a.label), std::to_string(a.gamerscore) + " G", "LOCKED"};
    row.description = manager.IsUnlocked(a.id) || a.unachieved_description.empty() ? a.description
                                                                                 : a.unachieved_description;
    if (!manager.IsUnlocked(a.id)) {
      locked.push_back(std::move(row));
      continue;
    }
    // The day, from its FILETIME (100 ns steps since 1601).
    const std::time_t when =
        std::time_t(manager.GetUnlockTime(a.id) / 10000000ull - 11644473600ull);
    char day[16] = "EARNED";
    if (const std::tm* tm = std::localtime(&when)) {
      std::strftime(day, sizeof(day), "%d.%m.%Y", tm);
    }
    row.state = day;
    earned.push_back(std::move(row));
  }
  earned.insert(earned.end(), locked.begin(), locked.end());
  return earned;
}

bool TakeRestartNeeded() {
  const bool restart = g_restart;
  g_restart = false;
  return restart;
}

}  // namespace xerenge::settings

namespace xerenge::settings {

void ApplyAtStart() {
  // Read here as well as by the launcher: a start without it (run.sh, an older
  // launcher) would otherwise still load RenderDoc and show its overlay.
  if (!ConfFlag("renderdoc", true)) {
#if defined(_WIN32)
    _putenv_s("XERENGE_NO_RENDERDOC", "1");
#else
    setenv("XERENGE_NO_RENDERDOC", "1", 1);
#endif
  }
}

}  // namespace xerenge::settings
