// Pinyon Shift wheel setup: binds a racing wheel, pedals and buttons for the
// game's SDL wheel driver (shiftglue-sdk feat/racing-wheels) and writes the
// wheel_* keys into pinyon_shift.toml.
//
// It reads devices through the same SDL3 source and joystick defaults the
// game uses, so the device names, paths and axis, button and hat numbers it
// writes are the ones the game sees.

#define _CRT_SECURE_NO_WARNINGS
#define _SILENCE_CXX20_U8PATH_DEPRECATION_WARNING
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <tlhelp32.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr int kLogicalWidth = 640;
constexpr int kLogicalHeight = 400;
constexpr float kLine = 10.0f;
constexpr int kColumns = kLogicalWidth / 8;

// ---------------------------------------------------------------------------
// Devices

struct Device {
  SDL_JoystickID id = 0;
  SDL_Joystick* joystick = nullptr;
  std::string name;
  std::string path;
  uint16_t vendor = 0;
  uint16_t product = 0;
  bool haptic = false;
  std::vector<int16_t> axes, base_axes, prev_axes;
  std::vector<bool> buttons, base_buttons, prev_buttons;
  std::vector<uint8_t> hats, base_hats, prev_hats;
  std::string activity;
  uint64_t activity_ms = 0;
};

// What identifies a device in a binding, kept apart from the live Device so a
// binding survives the device being unplugged during the wizard.
struct DeviceRef {
  SDL_JoystickID id = 0;
  std::string name, path;
  uint16_t vendor = 0, product = 0;
  bool operator==(const DeviceRef& o) const { return id == o.id; }
};

std::vector<Device> g_devices;

DeviceRef RefOf(const Device& d) {
  return {d.id, d.name, d.path, d.vendor, d.product};
}

Device* FindDevice(SDL_JoystickID id) {
  for (auto& d : g_devices) {
    if (d.id == id) return &d;
  }
  return nullptr;
}

std::string Lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

void ReadState(Device& d) {
  for (size_t i = 0; i < d.axes.size(); i++) d.axes[i] = SDL_GetJoystickAxis(d.joystick, int(i));
  for (size_t i = 0; i < d.buttons.size(); i++)
    d.buttons[i] = SDL_GetJoystickButton(d.joystick, int(i));
  for (size_t i = 0; i < d.hats.size(); i++) d.hats[i] = SDL_GetJoystickHat(d.joystick, int(i));
}

void SnapshotBaseline(Device& d) {
  ReadState(d);
  d.base_axes = d.axes;
  d.base_buttons = d.buttons;
  d.base_hats = d.hats;
}

void OpenDevice(SDL_JoystickID id) {
  if (FindDevice(id)) return;
  SDL_Joystick* joystick = SDL_OpenJoystick(id);
  if (!joystick) return;
  Device d;
  d.id = id;
  d.joystick = joystick;
  const char* name = SDL_GetJoystickName(joystick);
  const char* path = SDL_GetJoystickPath(joystick);
  d.name = name ? name : "";
  d.path = path ? path : "";
  d.vendor = SDL_GetJoystickVendor(joystick);
  d.product = SDL_GetJoystickProduct(joystick);
  d.haptic = SDL_IsJoystickHaptic(joystick);
  d.axes.assign(size_t(std::max(SDL_GetNumJoystickAxes(joystick), 0)), 0);
  d.buttons.assign(size_t(std::max(SDL_GetNumJoystickButtons(joystick), 0)), false);
  d.hats.assign(size_t(std::max(SDL_GetNumJoystickHats(joystick), 0)), 0);
  SnapshotBaseline(d);
  d.prev_axes = d.axes;
  d.prev_buttons = d.buttons;
  d.prev_hats = d.hats;
  g_devices.push_back(std::move(d));
}

void CloseDevice(SDL_JoystickID id) {
  for (size_t i = 0; i < g_devices.size(); i++) {
    if (g_devices[i].id == id) {
      SDL_CloseJoystick(g_devices[i].joystick);
      g_devices.erase(g_devices.begin() + ptrdiff_t(i));
      return;
    }
  }
}

const char* HatName(uint8_t mask) {
  if (mask & SDL_HAT_UP) return "up";
  if (mask & SDL_HAT_RIGHT) return "right";
  if (mask & SDL_HAT_DOWN) return "down";
  if (mask & SDL_HAT_LEFT) return "left";
  return "?";
}

uint8_t FirstHatBit(uint8_t value) {
  for (uint8_t bit : {uint8_t(SDL_HAT_UP), uint8_t(SDL_HAT_RIGHT), uint8_t(SDL_HAT_DOWN),
                      uint8_t(SDL_HAT_LEFT)}) {
    if (value & bit) return bit;
  }
  return 0;
}

// Tracks what each device last did, for the live panel.
void UpdateActivity(uint64_t now) {
  for (auto& d : g_devices) {
    for (size_t i = 0; i < d.axes.size(); i++) {
      if (std::abs(int(d.axes[i]) - int(d.prev_axes[i])) > 2500) {
        d.activity = "axis" + std::to_string(i) + " " + std::to_string(d.axes[i]);
        d.activity_ms = now;
        d.prev_axes[i] = d.axes[i];
      }
    }
    for (size_t i = 0; i < d.buttons.size(); i++) {
      if (d.buttons[i] != d.prev_buttons[i]) {
        d.activity = "button " + std::to_string(i) + (d.buttons[i] ? " down" : " up");
        d.activity_ms = now;
        d.prev_buttons[i] = d.buttons[i];
      }
    }
    for (size_t i = 0; i < d.hats.size(); i++) {
      if (d.hats[i] != d.prev_hats[i]) {
        d.activity = "hat" + std::to_string(i) + " " + (d.hats[i] ? HatName(d.hats[i]) : "centre");
        d.activity_ms = now;
        d.prev_hats[i] = d.hats[i];
      }
    }
  }
}

// The shortest text that names exactly this device among those connected, in
// the forms the game's WheelDeviceMatches accepts.
std::string DeviceMatch(const DeviceRef& ref) {
  if (ref.vendor || ref.product) {
    int same = 0;
    for (const auto& d : g_devices) {
      if (d.vendor == ref.vendor && d.product == ref.product) same++;
    }
    if (same <= 1) {
      char text[16];
      std::snprintf(text, sizeof(text), "%04x:%04x", ref.vendor, ref.product);
      return text;
    }
  }
  const std::string name = Lower(ref.name);
  if (!name.empty() && name.find('|') == std::string::npos &&
      name.find(',') == std::string::npos && name.find('=') == std::string::npos) {
    int containing = 0;
    for (const auto& d : g_devices) {
      if (Lower(d.name).find(name) != std::string::npos) containing++;
    }
    if (containing <= 1) return ref.name;
  }
  return "path:" + ref.path;
}

// ---------------------------------------------------------------------------
// Bindings

struct AxisBind {
  bool set = false;
  DeviceRef dev;
  int axis = -1;
  int16_t rest = 0;
  int16_t peak = 0;  // value at full travel
};

enum class InputKind { kButton, kHat, kAxis };

struct ButtonBind {
  DeviceRef dev;
  InputKind kind = InputKind::kButton;
  int index = -1;
  uint8_t hat_mask = 0;
  bool positive = true;  // axis travels upwards when pressed
};

struct Control {
  const char* name;   // pad_remap name the game parses
  const char* label;  // what the player is asked to press
};

const Control kControls[] = {
    {"A", "Xbox A"},
    {"B", "Xbox B"},
    {"X", "Xbox X"},
    {"Y", "Xbox Y"},
    {"LB", "Left bumper (LB)"},
    {"RB", "Right bumper (RB)"},
    {"LS", "Left stick click (LS)"},
    {"RS", "Right stick click (RS)"},
    {"BACK", "Back / View"},
    {"START", "Start / Menu"},
    {"UP", "D-pad UP"},
    {"DOWN", "D-pad DOWN"},
    {"LEFT", "D-pad LEFT"},
    {"RIGHT", "D-pad RIGHT"},
};
constexpr int kControlCount = int(sizeof(kControls) / sizeof(kControls[0]));

struct Bindings {
  AxisBind steering;
  bool steering_invert = false;
  AxisBind pedals[3];  // throttle, brake, clutch
  std::optional<ButtonBind> buttons[kControlCount];
  std::optional<DeviceRef> ffb;  // empty: the steering device
};

Bindings g_bind;

const char* kPedalNames[3] = {"THROTTLE", "BRAKE", "CLUTCH"};
const char* kPedalKeys[3] = {"wheel_throttle", "wheel_brake", "wheel_clutch"};

bool AxisTaken(SDL_JoystickID id, int axis, int ignore_pedal = -1) {
  if (g_bind.steering.set && g_bind.steering.dev.id == id && g_bind.steering.axis == axis)
    return true;
  for (int i = 0; i < 3; i++) {
    if (i != ignore_pedal && g_bind.pedals[i].set && g_bind.pedals[i].dev.id == id &&
        g_bind.pedals[i].axis == axis)
      return true;
  }
  return false;
}

std::string Prefix(const DeviceRef& dev) {
  return g_bind.steering.set && dev == g_bind.steering.dev ? "" : DeviceMatch(dev) + "|";
}

std::string PedalValue(int i) {
  const AxisBind& p = g_bind.pedals[i];
  if (!p.set) return "";
  return Prefix(p.dev) + (p.peak > p.rest ? "+" : "-") + "axis" + std::to_string(p.axis);
}

std::string ButtonInput(const ButtonBind& b) {
  std::string text = Prefix(b.dev);
  switch (b.kind) {
    case InputKind::kButton: return text + std::to_string(b.index);
    case InputKind::kHat:
      return text + "h" + std::to_string(b.index) + "." + HatName(b.hat_mask);
    case InputKind::kAxis:
      return text + (b.positive ? "+" : "-") + "axis" + std::to_string(b.index);
  }
  return text;
}

std::string ButtonsValue() {
  std::string out;
  for (int i = 0; i < kControlCount; i++) {
    if (!g_bind.buttons[i]) continue;
    if (!out.empty()) out += ",";
    out += std::string(kControls[i].name) + "=" + ButtonInput(*g_bind.buttons[i]);
  }
  return out;
}

std::string Describe(const DeviceRef& d) {
  return "\"" + d.name + "\"";
}

std::string TomlString(const std::string& value) {
  std::string out = "\"";
  for (char c : value) {
    if (c == '\\' || c == '"') out += '\\';
    out += c;
  }
  return out + "\"";
}

std::vector<std::pair<std::string, std::string>> ConfigLines() {
  std::vector<std::pair<std::string, std::string>> kv;
  kv.push_back({"wheel_enabled", "true"});
  kv.push_back({"wheel_device", TomlString(DeviceMatch(g_bind.steering.dev))});
  kv.push_back({"wheel_steering_axis", std::to_string(g_bind.steering.axis)});
  kv.push_back({"wheel_steering_invert", g_bind.steering_invert ? "true" : "false"});
  for (int i = 0; i < 3; i++) kv.push_back({kPedalKeys[i], TomlString(PedalValue(i))});
  kv.push_back({"wheel_buttons", TomlString(ButtonsValue())});
  kv.push_back({"wheel_ffb_device", TomlString(g_bind.ffb ? DeviceMatch(*g_bind.ffb) : "")});
  kv.push_back({"wheel_hide_gamepads", "true"});
  return kv;
}

// ---------------------------------------------------------------------------
// Config file

std::string Utf8(const fs::path& p) {
  const auto text = p.u8string();
  return std::string(text.begin(), text.end());
}

fs::path EnvPath(const wchar_t* name) {
  const wchar_t* value = _wgetenv(name);
  return value && *value ? fs::path(value) : fs::path();
}

std::string Trim(std::string s) {
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
  size_t start = 0;
  while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) start++;
  return s.substr(start);
}

std::vector<fs::path> FindConfigs(const fs::path& base) {
  std::vector<fs::path> roots;
  std::vector<fs::path> found;
  std::error_code ec;
  auto add_file = [&](const fs::path& file) {
    if (fs::is_regular_file(file, ec)) found.push_back(file);
  };
  if (auto state = EnvPath(L"PINYON_SHIFT_STATE_ROOT"); !state.empty()) {
    add_file(state / "config" / "pinyon_shift.toml");
  }
  if (auto install = EnvPath(L"PINYON_SHIFT_INSTALL_ROOT"); !install.empty()) {
    roots.push_back(install);
  }
  const fs::path local = EnvPath(L"LOCALAPPDATA");
  if (!local.empty()) {
    std::ifstream pref(local / "PinyonShift" / "install-root.txt");
    std::string line;
    if (pref && std::getline(pref, line) && !Trim(line).empty()) {
      roots.push_back(fs::u8path(Trim(line)));
    }
    roots.push_back(local / "PinyonShift");
  }
  // A portable launcher keeps its data beside itself.
  roots.push_back(base / "data");
  roots.push_back(base.parent_path() / "data");
  for (const auto& root : roots) {
    const fs::path source = root / "source";
    if (!fs::is_directory(source, ec)) continue;
    for (const auto& version : fs::directory_iterator(source, ec)) {
      add_file(version.path() / ".local" / "preview" / "config" / "pinyon_shift.toml");
    }
  }
  std::sort(found.begin(), found.end());
  found.erase(std::unique(found.begin(), found.end()), found.end());
  std::sort(found.begin(), found.end(), [&](const fs::path& a, const fs::path& b) {
    return fs::last_write_time(a, ec) > fs::last_write_time(b, ec);
  });
  return found;
}

bool GameRunning() {
  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snapshot == INVALID_HANDLE_VALUE) return false;
  PROCESSENTRY32W entry{};
  entry.dwSize = sizeof(entry);
  bool running = false;
  for (BOOL ok = Process32FirstW(snapshot, &entry); ok; ok = Process32NextW(snapshot, &entry)) {
    if (_wcsicmp(entry.szExeFile, L"pinyon_shift.exe") == 0) {
      running = true;
      break;
    }
  }
  CloseHandle(snapshot);
  return running;
}

std::string Timestamp() {
  std::time_t now = std::time(nullptr);
  std::tm local{};
  localtime_s(&local, &now);
  char text[32];
  std::strftime(text, sizeof(text), "%Y%m%d-%H%M%S", &local);
  return text;
}

// Replaces or adds top-level `key = value` lines, keeping everything else.
bool UpdateToml(const fs::path& file, const std::vector<std::pair<std::string, std::string>>& kv,
                fs::path& backup, std::string& error) {
  std::ifstream in(file, std::ios::binary);
  if (!in) {
    error = "Could not read the config file.";
    return false;
  }
  std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  in.close();
  const std::string newline = text.find("\r\n") != std::string::npos ? "\r\n" : "\n";
  std::vector<std::string> lines;
  {
    std::string line;
    std::istringstream stream(text);
    while (std::getline(stream, line)) {
      if (!line.empty() && line.back() == '\r') line.pop_back();
      lines.push_back(line);
    }
  }
  size_t table = lines.size();
  for (size_t i = 0; i < lines.size(); i++) {
    if (Trim(lines[i]).starts_with("[")) {
      table = i;
      break;
    }
  }
  std::vector<std::string> added;
  for (const auto& [key, value] : kv) {
    bool replaced = false;
    for (size_t i = 0; i < table; i++) {
      const std::string t = Trim(lines[i]);
      if (t.starts_with(key)) {
        const std::string after = Trim(t.substr(key.size()));
        if (after.starts_with("=")) {
          lines[i] = key + " = " + value;
          replaced = true;
        }
      }
    }
    if (!replaced) added.push_back(key + " = " + value);
  }
  if (!added.empty()) {
    added.insert(added.begin(), "# Racing wheel bindings (PinyonWheelSetup)");
    lines.insert(lines.begin() + ptrdiff_t(table), added.begin(), added.end());
  }
  std::string out;
  for (const auto& line : lines) out += line + newline;

  std::error_code ec;
  backup = file;
  backup += ".wheel-backup-" + Timestamp();
  fs::copy_file(file, backup, fs::copy_options::overwrite_existing, ec);
  if (ec) {
    error = "Could not back up the config: " + ec.message();
    return false;
  }
  fs::path temporary = file;
  temporary += ".tmp";
  {
    std::ofstream o(temporary, std::ios::binary | std::ios::trunc);
    if (!o.write(out.data(), std::streamsize(out.size()))) {
      error = "Could not write the config.";
      return false;
    }
  }
  fs::rename(temporary, file, ec);
  if (ec) {
    error = "Could not replace the config: " + ec.message();
    return false;
  }
  return true;
}

// ---------------------------------------------------------------------------
// Force feedback test

std::string TestForce(const DeviceRef& ref) {
  Device* d = FindDevice(ref.id);
  if (!d) return "Device not connected.";
  SDL_Haptic* haptic = SDL_OpenHapticFromJoystick(d->joystick);
  if (!haptic) return std::string("Could not open force feedback: ") + SDL_GetError();
  std::string result;
  if (SDL_GetHapticFeatures(haptic) & SDL_HAPTIC_CONSTANT) {
    for (int16_t level : {int16_t(4900), int16_t(-4900)}) {  // 15 %
      SDL_HapticEffect effect;
      SDL_zero(effect);
      effect.type = SDL_HAPTIC_CONSTANT;
      effect.constant.direction.type = SDL_HAPTIC_STEERING_AXIS;
      effect.constant.length = 500;
      effect.constant.level = level;
      const int id = SDL_CreateHapticEffect(haptic, &effect);
      if (id < 0) {
        result = std::string("Constant force refused: ") + SDL_GetError();
        break;
      }
      SDL_RunHapticEffect(haptic, id, 1);
      SDL_Delay(600);
      SDL_DestroyHapticEffect(haptic, id);
    }
    if (result.empty()) result = "Sent a gentle push one way, then the other (15%).";
  } else {
    result = "This device has no constant force; the game would use rumble only.";
  }
  SDL_CloseHaptic(haptic);
  return result;
}

// ---------------------------------------------------------------------------
// Wizard

enum class Step { kConfig, kSteer, kPedal, kButtons, kFfb, kSummary, kDone };

struct Wizard {
  Step step = Step::kConfig;
  int pedal = 0;    // in kPedal
  int control = 0;  // in kButtons
  fs::path base;
  std::vector<fs::path> configs;
  int config_choice = 0;
  std::optional<fs::path> browsed;
  std::mutex browse_mutex;
  bool game_running = false;
  // Detection in the current step.
  bool detected = false;
  SDL_JoystickID det_id = 0;
  int det_axis = -1;
  int16_t det_rest = 0, det_peak = 0;
  std::optional<ButtonBind> det_button;
  uint64_t released_ms = 0;
  // FFB step.
  int ffb_choice = 0;  // 0: steering device, then haptic devices in order
  std::string ffb_message;
  // Result.
  std::string message;
  fs::path saved_to, backup;
  bool saved = false;
} g;

fs::path ChosenConfig() {
  std::lock_guard lock(g.browse_mutex);
  if (g.browsed) return *g.browsed;
  if (g.config_choice >= 0 && g.config_choice < int(g.configs.size()))
    return g.configs[size_t(g.config_choice)];
  return {};
}

void ResetDetection() {
  g.detected = false;
  g.det_id = 0;
  g.det_axis = -1;
  g.det_rest = g.det_peak = 0;
  g.det_button.reset();
  g.released_ms = 0;
  for (auto& d : g_devices) SnapshotBaseline(d);
}

void Enter(Step step) {
  g.step = step;
  ResetDetection();
}

std::vector<Device*> HapticDevices() {
  std::vector<Device*> out;
  for (auto& d : g_devices) {
    if (d.haptic) out.push_back(&d);
  }
  return out;
}

void Next() {
  switch (g.step) {
    case Step::kConfig: Enter(Step::kSteer); break;
    case Step::kSteer:
      g.pedal = 0;
      Enter(Step::kPedal);
      break;
    case Step::kPedal:
      if (++g.pedal < 3) {
        ResetDetection();
      } else {
        g.control = 0;
        Enter(Step::kButtons);
      }
      break;
    case Step::kButtons:
      if (++g.control < kControlCount) {
        ResetDetection();
      } else {
        g.ffb_choice = 0;
        g.ffb_message.clear();
        Enter(Step::kFfb);
      }
      break;
    case Step::kFfb: Enter(Step::kSummary); break;
    case Step::kSummary: Enter(Step::kDone); break;
    case Step::kDone: break;
  }
}

void Back() {
  switch (g.step) {
    case Step::kConfig: break;
    case Step::kSteer: Enter(Step::kConfig); break;
    case Step::kPedal:
      if (g.pedal > 0) {
        g.pedal--;
        g_bind.pedals[g.pedal] = {};
        ResetDetection();
      } else {
        Enter(Step::kSteer);
      }
      break;
    case Step::kButtons:
      if (g.control > 0) {
        g.control--;
        g_bind.buttons[g.control].reset();
        ResetDetection();
      } else {
        g.pedal = 2;
        Enter(Step::kPedal);
      }
      break;
    case Step::kFfb:
      g.control = kControlCount - 1;
      Enter(Step::kButtons);
      break;
    case Step::kSummary: Enter(Step::kFfb); break;
    case Step::kDone: break;
  }
}

// Largest axis movement since the step began, among axes not already bound.
void TrackAxes(int threshold, int ignore_pedal) {
  int best = 0;
  Device* best_dev = nullptr;
  int best_axis = -1;
  for (auto& d : g_devices) {
    for (size_t i = 0; i < d.axes.size(); i++) {
      if (g.step != Step::kSteer && AxisTaken(d.id, int(i), ignore_pedal)) continue;
      const int delta = int(d.axes[i]) - int(d.base_axes[i]);
      if (std::abs(delta) > std::abs(best)) {
        best = delta;
        best_dev = &d;
        best_axis = int(i);
      }
    }
  }
  if (!g.detected) {
    if (best_dev && std::abs(best) >= threshold) {
      g.detected = true;
      g.det_id = best_dev->id;
      g.det_axis = best_axis;
      g.det_rest = best_dev->base_axes[size_t(best_axis)];
      g.det_peak = best_dev->axes[size_t(best_axis)];
    }
    return;
  }
  // Follow the detected axis to its full travel.
  if (Device* d = FindDevice(g.det_id)) {
    const int16_t value = d->axes[size_t(g.det_axis)];
    if (std::abs(int(value) - int(g.det_rest)) > std::abs(int(g.det_peak) - int(g.det_rest))) {
      g.det_peak = value;
    }
  }
}

bool DetectedReleased() {
  Device* d = FindDevice(g.det_id);
  if (!d) return false;
  return std::abs(int(d->axes[size_t(g.det_axis)]) - int(g.det_rest)) < 3000;
}

void AcceptAxis() {
  Device* d = FindDevice(g.det_id);
  if (!d) return;
  AxisBind bind;
  bind.set = true;
  bind.dev = RefOf(*d);
  bind.axis = g.det_axis;
  bind.rest = g.det_rest;
  bind.peak = g.det_peak;
  if (g.step == Step::kSteer) {
    g_bind.steering = bind;
    // Turned left: the game wants left negative.
    g_bind.steering_invert = g.det_peak > g.det_rest;
  } else {
    g_bind.pedals[g.pedal] = bind;
  }
  Next();
}

void TrackButtons() {
  if (g.det_button) return;
  for (auto& d : g_devices) {
    for (size_t i = 0; i < d.buttons.size(); i++) {
      if (d.buttons[i] && !d.base_buttons[i]) {
        g.det_button = ButtonBind{RefOf(d), InputKind::kButton, int(i), 0, true};
        g.det_id = d.id;
        return;
      }
    }
    for (size_t i = 0; i < d.hats.size(); i++) {
      const uint8_t pressed = uint8_t(d.hats[i] & ~d.base_hats[i]);
      if (pressed) {
        g.det_button = ButtonBind{RefOf(d), InputKind::kHat, int(i), FirstHatBit(pressed), true};
        g.det_id = d.id;
        return;
      }
    }
    for (size_t i = 0; i < d.axes.size(); i++) {
      if (AxisTaken(d.id, int(i))) continue;
      const int delta = int(d.axes[i]) - int(d.base_axes[i]);
      if (std::abs(delta) >= 24000) {
        g.det_button = ButtonBind{RefOf(d), InputKind::kAxis, int(i), 0, delta > 0};
        g.det_id = d.id;
        g.det_axis = int(i);
        g.det_rest = d.base_axes[i];
        return;
      }
    }
  }
}

bool ButtonReleased() {
  Device* d = FindDevice(g.det_id);
  if (!d || !g.det_button) return false;
  const ButtonBind& b = *g.det_button;
  switch (b.kind) {
    case InputKind::kButton: return !d->buttons[size_t(b.index)];
    case InputKind::kHat: return (d->hats[size_t(b.index)] & b.hat_mask) == 0;
    case InputKind::kAxis:
      return std::abs(int(d->axes[size_t(b.index)]) - int(g.det_rest)) < 6000;
  }
  return true;
}

void Save() {
  const auto kv = ConfigLines();
  // A copy beside the wizard, whatever happens to the config.
  {
    std::ofstream record(g.base / "last-wheel-bindings.toml", std::ios::trunc);
    record << "# PinyonWheelSetup " << Timestamp() << "\n";
    for (const auto& [k, v] : kv) record << k << " = " << v << "\n";
  }
  const fs::path config = ChosenConfig();
  if (config.empty()) {
    g.saved = false;
    g.message = "No game config found. The lines are in last-wheel-bindings.toml beside this EXE;"
                " paste them into pinyon_shift.toml after the game has run once.";
    return;
  }
  std::string error;
  if (UpdateToml(config, kv, g.backup, error)) {
    g.saved = true;
    g.saved_to = config;
    g.message.clear();
  } else {
    g.saved = false;
    g.message = error + " The lines are in last-wheel-bindings.toml beside this EXE.";
  }
}

// ---------------------------------------------------------------------------
// Drawing

SDL_Renderer* g_renderer = nullptr;

void Color(uint8_t r, uint8_t gr, uint8_t b) {
  SDL_SetRenderDrawColor(g_renderer, r, gr, b, 255);
}

void Text(int col, float row, const std::string& text) {
  std::string clipped = text;
  const int room = kColumns - col;
  if (room <= 0) return;
  if (int(clipped.size()) > room) clipped = clipped.substr(0, size_t(room - 2)) + "..";
  for (char& c : clipped) {
    if (static_cast<unsigned char>(c) >= 128) c = '?';
  }
  SDL_RenderDebugText(g_renderer, float(col * 8), row * kLine, clipped.c_str());
}

// Wraps text onto following rows; returns the next free row.
float Para(int col, float row, const std::string& text) {
  const int width = kColumns - col - 1;
  std::istringstream words(text);
  std::string word, line;
  while (words >> word) {
    // A path has no spaces: break it at the width.
    while (int(word.size()) > width) {
      if (!line.empty()) {
        Text(col, row, line);
        row += 1;
        line.clear();
      }
      Text(col, row, word.substr(0, size_t(width)));
      row += 1;
      word = word.substr(size_t(width));
    }
    if (!line.empty() && int(line.size() + 1 + word.size()) > width) {
      Text(col, row, line);
      row += 1;
      line.clear();
    }
    line += (line.empty() ? "" : " ") + word;
  }
  if (!line.empty()) {
    Text(col, row, line);
    row += 1;
  }
  return row;
}

void White() { Color(230, 230, 230); }
void Grey() { Color(140, 140, 150); }
void Yellow() { Color(250, 210, 80); }
void Green() { Color(110, 220, 120); }
void Red() { Color(240, 110, 100); }

void DrawDevices(float row, uint64_t now) {
  Grey();
  Text(1, row, "Connected devices (" + std::to_string(g_devices.size()) +
                   ") - move anything to see it light up:");
  row += 1;
  int shown = 0;
  for (const auto& d : g_devices) {
    if (row > 38) {
      Grey();
      Text(1, row, "... " + std::to_string(g_devices.size() - size_t(shown)) + " more");
      break;
    }
    const bool active = now - d.activity_ms < 1500 && d.activity_ms != 0;
    char ids[16];
    std::snprintf(ids, sizeof(ids), "%04x:%04x", d.vendor, d.product);
    char counts[32];
    std::snprintf(counts, sizeof(counts), "%dA %dB %dH%s", int(d.axes.size()),
                  int(d.buttons.size()), int(d.hats.size()), d.haptic ? " FFB" : "");
    if (active) Green(); else White();
    std::string name = d.name;
    if (name.size() > 30) name = name.substr(0, 29) + "~";
    Text(2, row, name);
    Grey();
    Text(33, row, ids);
    Text(43, row, counts);
    if (active) {
      Green();
      Text(60, row, d.activity);
    }
    row += 1;
    shown++;
  }
}

void Draw(uint64_t now) {
  Color(18, 20, 26);
  SDL_RenderClear(g_renderer);
  Yellow();
  Text(1, 0.5f, "PINYON SHIFT - RACING WHEEL SETUP");
  Grey();
  Text(48, 0.5f, "Esc back   S skip   Enter OK");
  float row = 2.5f;

  switch (g.step) {
    case Step::kConfig: {
      White();
      row = Para(1, row,
                 "Binds your wheel (or EmuWheel/vJoy device), pedals and buttons for Pinyon "
                 "Shift and writes them into the game's config. Keep EmuWheel running if you "
                 "use it, so its vJoy device is listed below.");
      row += 0.5f;
      const fs::path chosen = ChosenConfig();
      if (chosen.empty()) {
        Red();
        row = Para(1, row,
                   "No pinyon_shift.toml found. Run the game once first, or press B to browse "
                   "for it. Without one the bindings are saved beside this EXE.");
      } else {
        Grey();
        Text(1, row, "Config to update:");
        row += 1;
        Green();
        row = Para(2, row, Utf8(chosen));
        if (g.configs.size() > 1 && !g.browsed) {
          Grey();
          Text(2, row, "Tab: next of " + std::to_string(g.configs.size()) + " found configs");
          row += 1;
        }
      }
      Grey();
      Text(1, row, "B: browse for pinyon_shift.toml");
      row += 1.5f;
      if (g.game_running) {
        Red();
        row = Para(1, row, "Pinyon Shift is running. Close it before saving.");
      }
      Yellow();
      Text(1, row, "Press Enter to start.");
      row += 1.5f;
      break;
    }
    case Step::kSteer: {
      Yellow();
      Text(1, row, "STEP 1 - STEERING");
      row += 1.5f;
      White();
      row = Para(1, row,
                 "Centre the wheel, then turn it to the LEFT (at least a quarter turn) and "
                 "come back to centre.");
      row += 0.5f;
      if (g.detected) {
        Device* d = FindDevice(g.det_id);
        Green();
        Text(1, row, "Got: " + (d ? Describe(RefOf(*d)) : std::string("?")) + " axis" +
                         std::to_string(g.det_axis) + "  " + std::to_string(g.det_rest) + " -> " +
                         std::to_string(g.det_peak));
        row += 1;
        Grey();
        Text(1, row, "Return to centre to continue, or Backspace to retry.");
      } else {
        Grey();
        Text(1, row, "Waiting for a wheel axis...");
      }
      row += 1.5f;
      break;
    }
    case Step::kPedal: {
      Yellow();
      Text(1, row, "STEP " + std::to_string(2 + g.pedal) + " - " + kPedalNames[g.pedal] +
                       " PEDAL");
      row += 1.5f;
      White();
      row = Para(1, row,
                 std::string("Press the ") + kPedalNames[g.pedal] +
                     " pedal all the way down, then release it." +
                     (g.pedal == 2 ? " (The game's clutch is LB; S skips if you have none.)"
                                   : " (S skips.)"));
      row += 0.5f;
      if (g.detected) {
        Device* d = FindDevice(g.det_id);
        Green();
        Text(1, row, "Got: " + (d ? Describe(RefOf(*d)) : std::string("?")) + " axis" +
                         std::to_string(g.det_axis) + "  rest " + std::to_string(g.det_rest) +
                         " -> " + std::to_string(g.det_peak));
        row += 1;
        if (std::abs(int(g.det_rest)) < 12000) {
          Red();
          row = Para(1, row,
                     "This axis rests mid-range, so the game would see the pedal half pressed. "
                     "Give it a full range in EmuWheel/vJoy, or press Backspace to try again.");
        }
        Grey();
        Text(1, row, "Release the pedal to continue, Backspace to retry.");
      } else {
        Grey();
        Text(1, row, "Waiting for a pedal axis...");
      }
      row += 1.5f;
      break;
    }
    case Step::kButtons: {
      Yellow();
      Text(1, row, "STEP 5 - BUTTONS  (" + std::to_string(g.control + 1) + " of " +
                       std::to_string(kControlCount) + ")");
      row += 1.5f;
      White();
      Text(1, row, "Press the button you want as:  ");
      Yellow();
      Text(31, row, kControls[g.control].label);
      row += 1;
      Grey();
      row = Para(1, row,
                 "Any device: wheel rim, button box, shifter paddles, a hat or a lever axis. "
                 "S skips this one, Esc goes back one.");
      row += 0.5f;
      if (g.det_button) {
        Green();
        Text(1, row, "Got: " + Describe(g.det_button->dev) + " " +
                         ButtonInput(*g.det_button).substr(Prefix(g.det_button->dev).size()));
      } else {
        Grey();
        Text(1, row, "Waiting for a button...");
      }
      row += 1.5f;
      int col = 1;
      for (int i = 0; i < kControlCount; i++) {
        const std::string tag = std::string(kControls[i].name) + (g_bind.buttons[i] ? "+" : "-");
        if (g_bind.buttons[i]) Green(); else Grey();
        Text(col, row, tag);
        col += int(tag.size()) + 2;
      }
      row += 1.5f;
      break;
    }
    case Step::kFfb: {
      Yellow();
      Text(1, row, "STEP 6 - FORCE FEEDBACK DEVICE");
      row += 1.5f;
      White();
      row = Para(1, row,
                 "Which device should receive the game's force feedback? With EmuWheel choose "
                 "the vJoy device (EmuWheel passes it to your base). Choose the base itself only "
                 "if nothing else holds it.");
      row += 0.5f;
      auto haptics = HapticDevices();
      if (g.ffb_choice == 0) Green(); else White();
      Text(1, row, std::string(g.ffb_choice == 0 ? "> " : "  ") +
                       "0  The steering device " + Describe(g_bind.steering.dev));
      row += 1;
      for (size_t i = 0; i < haptics.size() && i < 9; i++) {
        const bool chosen = g.ffb_choice == int(i) + 1;
        if (chosen) Green(); else White();
        Text(1, row, std::string(chosen ? "> " : "  ") + std::to_string(i + 1) + "  " +
                         Describe(RefOf(*haptics[i])));
        row += 1;
      }
      if (haptics.empty()) {
        Red();
        Text(3, row, "No device reports force feedback to SDL.");
        row += 1;
      }
      row += 0.5f;
      Grey();
      Text(1, row, "Number keys choose, T tests (gentle 15% push), Enter continues.");
      row += 1;
      if (!g.ffb_message.empty()) {
        Yellow();
        row = Para(1, row, g.ffb_message);
      }
      row += 0.5f;
      break;
    }
    case Step::kSummary: {
      Yellow();
      Text(1, row, "REVIEW - these lines go into pinyon_shift.toml");
      row += 1.5f;
      for (const auto& [k, v] : ConfigLines()) {
        Grey();
        Text(1, row, k);
        White();
        row = Para(24, row, v);
      }
      row += 0.5f;
      if (g.game_running) {
        Red();
        Text(1, row, "Close Pinyon Shift before saving.");
        row += 1;
      }
      Yellow();
      Text(1, row, "Enter saves (the old config is backed up). Esc goes back.");
      return;  // the summary needs the room
    }
    case Step::kDone: {
      if (g.saved) {
        Green();
        Text(1, row, "SAVED");
        row += 1.5f;
        White();
        row = Para(1, row, "Updated " + Utf8(g.saved_to));
        row = Para(1, row, "Backup: " + Utf8(g.backup));
        row += 0.5f;
        row = Para(1, row,
                   "Start the game from the launcher. In F6 > CONTROLS you can set force "
                   "feedback strength, direction and steering sensitivity. Run this again any "
                   "time to rebind.");
      } else {
        Red();
        row = Para(1, row, g.message);
      }
      row += 1;
      Grey();
      Text(1, row, "Press Enter or Esc to close.");
      return;
    }
  }
  DrawDevices(std::max(row, 22.0f), now);
}

void OnBrowse(void*, const char* const* files, int) {
  if (files && files[0]) {
    std::lock_guard lock(g.browse_mutex);
    g.browsed = fs::u8path(files[0]);
  }
}

// --selftest <toml>: plugs in SDL virtual devices (a "vJoy Device" wheel and
// a pedal set) and drives every step as a player would, then saves. Used to
// check detection and the config writer on a PC without a wheel.
struct SelfTest {
  bool on = false;
  bool quit = false;
  SDL_JoystickID wheel = 0, pedals = 0;
  int frame = 0;
  int phase = -1;
} g_test;

SDL_JoystickID AttachVirtual(const char* name, uint16_t vendor, uint16_t product,
                             SDL_JoystickType type, int axes, int buttons, int hats) {
  SDL_VirtualJoystickDesc desc;
  SDL_INIT_INTERFACE(&desc);
  desc.type = static_cast<Uint16>(type);
  desc.naxes = static_cast<Uint16>(axes);
  desc.nbuttons = static_cast<Uint16>(buttons);
  desc.nhats = static_cast<Uint16>(hats);
  desc.vendor_id = vendor;
  desc.product_id = product;
  desc.name = name;
  return SDL_AttachVirtualJoystick(&desc);
}

void SetAxis(SDL_JoystickID id, int axis, int16_t value) {
  if (Device* d = FindDevice(id)) SDL_SetJoystickVirtualAxis(d->joystick, axis, value);
}
void SetButton(SDL_JoystickID id, int button, bool down) {
  if (Device* d = FindDevice(id)) SDL_SetJoystickVirtualButton(d->joystick, button, down);
}
void SetHat(SDL_JoystickID id, int hat, uint8_t value) {
  if (Device* d = FindDevice(id)) SDL_SetJoystickVirtualHat(d->joystick, hat, value);
}

void StartSelfTest() {
  g_test.on = true;
  g_test.wheel = AttachVirtual("vJoy Device", 0x1234, 0xBEAD, SDL_JOYSTICK_TYPE_WHEEL, 6, 16, 1);
  g_test.pedals = AttachVirtual("SimJack Pro Pedals", 0x16D0, 0x0D5A, SDL_JOYSTICK_TYPE_UNKNOWN,
                                3, 4, 0);
  OpenDevice(g_test.wheel);
  OpenDevice(g_test.pedals);
  // Pedals rest at the bottom of their range, the lever (axis5) too.
  SetAxis(g_test.pedals, 0, -32768);
  SetAxis(g_test.pedals, 1, 32767);  // a brake wired backwards
  SetAxis(g_test.pedals, 2, -32768);
  SetAxis(g_test.wheel, 5, -32768);
  SDL_UpdateJoysticks();
  for (auto& d : g_devices) SnapshotBaseline(d);
}

void SelfTestTick() {
  const int phase = int(g.step) * 100 + (g.step == Step::kPedal ? g.pedal : 0) +
                    (g.step == Step::kButtons ? g.control : 0);
  if (phase != g_test.phase) {
    g_test.phase = phase;
    g_test.frame = 0;
  }
  const int f = ++g_test.frame;
  const bool press = f >= 3 && f < 15;
  switch (g.step) {
    case Step::kConfig:
      if (f == 5) Next();
      break;
    case Step::kSteer:
      SetAxis(g_test.wheel, 0, press ? int16_t(-30000) : int16_t(0));
      break;
    case Step::kPedal: {
      const int16_t rest = g.pedal == 1 ? 32767 : -32768;
      SetAxis(g_test.pedals, g.pedal, press ? int16_t(-1 - rest) : rest);
      break;
    }
    case Step::kButtons: {
      const int c = g.control;
      if (c == 2) {  // X: skipped, as a player without one would
        if (f == 5) {
          g_bind.buttons[c].reset();
          Next();
        }
      } else if (c == 7) {  // RS on the pedal set's button
        SetButton(g_test.pedals, 0, press);
      } else if (c == 8) {  // BACK on a lever axis
        SetAxis(g_test.wheel, 5, press ? int16_t(32767) : int16_t(-32768));
      } else if (c >= 10) {  // the d-pad on the hat
        static const uint8_t dirs[4] = {SDL_HAT_UP, SDL_HAT_DOWN, SDL_HAT_LEFT, SDL_HAT_RIGHT};
        SetHat(g_test.wheel, 0, press ? dirs[c - 10] : uint8_t(SDL_HAT_CENTERED));
      } else {
        SetButton(g_test.wheel, c, press);
      }
      break;
    }
    case Step::kFfb:
      if (f == 5) {
        g_bind.ffb.reset();
        Next();
      }
      break;
    case Step::kSummary:
      if (f == 5) {
        Save();
        Next();
      }
      break;
    case Step::kDone:
      if (f == 20) g_test.quit = true;
      break;
  }
}

}  // namespace

int main(int argc, char** argv) {
  SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC)) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Pinyon wheel setup", SDL_GetError(), nullptr);
    return 1;
  }
  SDL_Window* window = nullptr;
  if (!SDL_CreateWindowAndRenderer("Pinyon Shift - Racing Wheel Setup", kLogicalWidth * 2,
                                   kLogicalHeight * 2, SDL_WINDOW_RESIZABLE, &window,
                                   &g_renderer)) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Pinyon wheel setup", SDL_GetError(), nullptr);
    return 1;
  }
  SDL_SetRenderLogicalPresentation(g_renderer, kLogicalWidth, kLogicalHeight,
                                   SDL_LOGICAL_PRESENTATION_LETTERBOX);
  SDL_SetRenderVSync(g_renderer, 1);

  const char* base = SDL_GetBasePath();
  g.base = base ? fs::u8path(base) : fs::current_path();
  g.configs = FindConfigs(g.base);
  const bool self_test = argc > 2 && std::string(argv[1]) == "--selftest";
  if (self_test) {
    g.browsed = fs::u8path(argv[2]);
  } else if (argc > 1) {
    const fs::path given = fs::u8path(argv[1]);
    std::error_code ec;
    if (fs::is_regular_file(given, ec)) g.browsed = given;
  }
  g.game_running = GameRunning();

  int count = 0;
  if (SDL_JoystickID* ids = SDL_GetJoysticks(&count)) {
    for (int i = 0; i < count; i++) OpenDevice(ids[i]);
    SDL_free(ids);
  }
  if (self_test) StartSelfTest();

  uint64_t last_process_check = 0;
  bool running = true;
  while (running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      switch (e.type) {
        case SDL_EVENT_QUIT: running = false; break;
        case SDL_EVENT_JOYSTICK_ADDED: OpenDevice(e.jdevice.which); break;
        case SDL_EVENT_JOYSTICK_REMOVED: CloseDevice(e.jdevice.which); break;
        case SDL_EVENT_KEY_DOWN: {
          if (e.key.repeat) break;
          const SDL_Keycode key = e.key.key;
          const bool enter = key == SDLK_RETURN || key == SDLK_KP_ENTER;
          if (g.step == Step::kDone) {
            if (enter || key == SDLK_ESCAPE) running = false;
            break;
          }
          if (key == SDLK_ESCAPE) {
            if (g.step == Step::kConfig) running = false; else Back();
          } else if (key == SDLK_BACKSPACE) {
            ResetDetection();
          } else if (key == SDLK_S && (g.step == Step::kPedal || g.step == Step::kButtons)) {
            if (g.step == Step::kPedal) g_bind.pedals[g.pedal] = {};
            else g_bind.buttons[g.control].reset();
            Next();
          } else if (g.step == Step::kConfig) {
            if (key == SDLK_B) {
              static const SDL_DialogFileFilter filter[] = {{"Pinyon Shift config", "toml"}};
              SDL_ShowOpenFileDialog(OnBrowse, nullptr, window, filter, 1, nullptr, false);
            } else if (key == SDLK_TAB && !g.configs.empty()) {
              g.config_choice = (g.config_choice + 1) % int(g.configs.size());
            } else if (enter) {
              Next();
            }
          } else if (enter && (g.step == Step::kSteer || g.step == Step::kPedal) && g.detected) {
            AcceptAxis();
          } else if (enter && g.step == Step::kButtons && g.det_button) {
            g_bind.buttons[g.control] = *g.det_button;
            Next();
          } else if (g.step == Step::kFfb) {
            auto haptics = HapticDevices();
            if (key >= SDLK_0 && key <= SDLK_9) {
              const int choice = int(key - SDLK_0);
              if (choice <= int(haptics.size())) g.ffb_choice = choice;
              g.ffb_message.clear();
            } else if (key == SDLK_T) {
              DeviceRef target = g_bind.steering.dev;
              if (g.ffb_choice > 0 && g.ffb_choice <= int(haptics.size()))
                target = RefOf(*haptics[size_t(g.ffb_choice - 1)]);
              g.ffb_message = "Testing " + Describe(target) + "... ";
              Draw(SDL_GetTicks());
              SDL_RenderPresent(g_renderer);
              g.ffb_message += TestForce(target);
            } else if (enter) {
              if (g.ffb_choice > 0 && g.ffb_choice <= int(haptics.size()) &&
                  !(RefOf(*haptics[size_t(g.ffb_choice - 1)]) == g_bind.steering.dev)) {
                g_bind.ffb = RefOf(*haptics[size_t(g.ffb_choice - 1)]);
              } else {
                g_bind.ffb.reset();
              }
              Next();
            }
          } else if (g.step == Step::kSummary && enter) {
            g.game_running = GameRunning();
            if (!g.game_running) {
              Save();
              Next();
            }
          }
          break;
        }
        default: break;
      }
    }

    if (g_test.on) {
      SelfTestTick();
      if (g_test.quit) running = false;
    }
    const uint64_t now = SDL_GetTicks();
    for (auto& d : g_devices) ReadState(d);
    UpdateActivity(now);
    if (now - last_process_check > 2000) {
      g.game_running = GameRunning();
      last_process_check = now;
    }

    // Detection and auto-advance.
    if (g.step == Step::kSteer) {
      TrackAxes(10000, -1);
      if (g.detected && DetectedReleased()) {
        if (!g.released_ms) g.released_ms = now;
        if (now - g.released_ms > 400) AcceptAxis();
      }
    } else if (g.step == Step::kPedal) {
      TrackAxes(16000, g.pedal);
      if (g.detected && DetectedReleased() && std::abs(int(g.det_rest)) >= 12000) {
        if (!g.released_ms) g.released_ms = now;
        if (now - g.released_ms > 300) AcceptAxis();
      }
    } else if (g.step == Step::kButtons) {
      TrackButtons();
      if (g.det_button && ButtonReleased()) {
        if (!g.released_ms) g.released_ms = now;
        if (now - g.released_ms > 250) {
          g_bind.buttons[g.control] = *g.det_button;
          Next();
        }
      }
    }

    Draw(now);
    SDL_RenderPresent(g_renderer);
  }

  for (auto& d : g_devices) SDL_CloseJoystick(d.joystick);
  SDL_DestroyRenderer(g_renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
