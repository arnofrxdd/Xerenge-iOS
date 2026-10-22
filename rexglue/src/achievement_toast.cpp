// An achievement's notice over the game: a dark rounded panel at the top, the
// achievement's icon, "Achievement unlocked" and "<score> G - <name>", for six
// seconds, without pausing the game or taking its input. Drawn here with the
// processor, once per achievement, into a picture the renderer lays over the
// window (plume_overlay.cpp), which it takes with the frame clock
// (game_timing.cpp). The awards themselves are the SDK's (XGI 0x000B0008,
// AchievementManager), with the names and icons of the disc's own XDBF.
//
// XERENGE_TEST_ACHIEVEMENT=<id> shows that one's notice soon after the start.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <rex/logging.h>
#include <rex/system/kernel_state.h>
#include <rex/system/util/xdbf_utils.h>

#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include <imstb_truetype.h>
#define STBI_ONLY_PNG
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

extern const unsigned char kToastFont[];
extern const size_t kToastFontSize;

namespace {

// Straight RGBA, 0..1, composited "over".
struct Canvas {
  int width;
  int height;
  std::vector<float> pixels;

  Canvas(int w, int h) : width(w), height(h), pixels(size_t(w) * h * 4, 0.0f) {}

  void Blend(int x, int y, float r, float g, float b, float a) {
    if (x < 0 || y < 0 || x >= width || y >= height || a <= 0.0f) {
      return;
    }
    float* p = &pixels[(size_t(y) * width + x) * 4];
    const float out = a + p[3] * (1.0f - a);
    if (out <= 0.0f) {
      return;
    }
    p[0] = (r * a + p[0] * p[3] * (1.0f - a)) / out;
    p[1] = (g * a + p[1] * p[3] * (1.0f - a)) / out;
    p[2] = (b * a + p[2] * p[3] * (1.0f - a)) / out;
    p[3] = out;
  }

  std::vector<uint8_t> Bytes() const {
    std::vector<uint8_t> out(pixels.size());
    for (size_t i = 0; i < pixels.size(); ++i) {
      out[i] = uint8_t(std::lround(std::clamp(pixels[i], 0.0f, 1.0f) * 255.0f));
    }
    return out;
  }
};

// How much of the pixel at (x, y) a rounded rectangle covers, smoothed over a
// pixel at its edge.
float RoundedCoverage(float x, float y, float left, float top, float right, float bottom,
                      float radius) {
  const float cx = std::clamp(x, left + radius, right - radius);
  const float cy = std::clamp(y, top + radius, bottom - radius);
  const float distance = std::hypot(x - cx, y - cy) - radius;
  return std::clamp(0.5f - distance, 0.0f, 1.0f);
}

void FillRounded(Canvas& canvas, float left, float top, float right, float bottom, float radius,
                 float r, float g, float b, float a) {
  for (int y = int(top); y < int(std::ceil(bottom)); ++y) {
    for (int x = int(left); x < int(std::ceil(right)); ++x) {
      const float coverage = RoundedCoverage(x + 0.5f, y + 0.5f, left, top, right, bottom, radius);
      canvas.Blend(x, y, r, g, b, a * coverage);
    }
  }
}

// The icon (RGBA from its PNG), scaled into size x size at (left, top),
// bilinear, with slightly rounded corners.
void DrawIcon(Canvas& canvas, const uint8_t* rgba, int icon_w, int icon_h, int left, int top,
              int size) {
  const auto texel = [&](int x, int y, int c) {
    x = std::clamp(x, 0, icon_w - 1);
    y = std::clamp(y, 0, icon_h - 1);
    return rgba[(size_t(y) * icon_w + x) * 4 + c] / 255.0f;
  };
  for (int y = 0; y < size; ++y) {
    for (int x = 0; x < size; ++x) {
      const float u = (x + 0.5f) * icon_w / size - 0.5f;
      const float v = (y + 0.5f) * icon_h / size - 0.5f;
      const int x0 = int(std::floor(u));
      const int y0 = int(std::floor(v));
      const float fx = u - x0;
      const float fy = v - y0;
      float c[4];
      for (int i = 0; i < 4; ++i) {
        const float top_row = texel(x0, y0, i) * (1 - fx) + texel(x0 + 1, y0, i) * fx;
        const float bottom_row = texel(x0, y0 + 1, i) * (1 - fx) + texel(x0 + 1, y0 + 1, i) * fx;
        c[i] = top_row * (1 - fy) + bottom_row * fy;
      }
      const float coverage =
          RoundedCoverage(x + 0.5f, y + 0.5f, 0.0f, 0.0f, float(size), float(size), size * 0.08f);
      canvas.Blend(left + x, top + y, c[0], c[1], c[2], c[3] * coverage);
    }
  }
}

// UTF-8 to code points; what is not valid UTF-8 is taken byte by byte.
std::vector<int> CodePoints(const std::string& text) {
  std::vector<int> out;
  for (size_t i = 0; i < text.size();) {
    const unsigned char c = text[i];
    int extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
    int code = extra == 3 ? c & 0x07 : extra == 2 ? c & 0x0F : extra == 1 ? c & 0x1F : c;
    if (extra && i + extra >= text.size()) {
      extra = 0;
      code = c;
    }
    for (int k = 1; k <= extra; ++k) {
      code = (code << 6) | (text[i + k] & 0x3F);
    }
    out.push_back(code);
    i += extra + 1;
  }
  return out;
}

class Font {
 public:
  Font() {
    ok_ = stbtt_InitFont(&info_, kToastFont, stbtt_GetFontOffsetForIndex(kToastFont, 0)) != 0;
  }

  // The text's advance at pixel_height.
  float Width(const std::string& text, float pixel_height) const {
    if (!ok_) {
      return 0.0f;
    }
    const float scale = stbtt_ScaleForPixelHeight(&info_, pixel_height);
    float width = 0.0f;
    const std::vector<int> codes = CodePoints(text);
    for (size_t i = 0; i < codes.size(); ++i) {
      int advance = 0, bearing = 0;
      stbtt_GetCodepointHMetrics(&info_, codes[i], &advance, &bearing);
      width += advance * scale;
      if (i + 1 < codes.size()) {
        width += stbtt_GetCodepointKernAdvance(&info_, codes[i], codes[i + 1]) * scale;
      }
    }
    return width;
  }

  // Draws the text with its baseline at y, from x.
  void Draw(Canvas& canvas, const std::string& text, float x, float baseline, float pixel_height,
            float r, float g, float b) const {
    if (!ok_) {
      return;
    }
    const float scale = stbtt_ScaleForPixelHeight(&info_, pixel_height);
    const std::vector<int> codes = CodePoints(text);
    for (size_t i = 0; i < codes.size(); ++i) {
      const float shift = x - std::floor(x);
      int x0, y0, x1, y1;
      stbtt_GetCodepointBitmapBoxSubpixel(&info_, codes[i], scale, scale, shift, 0.0f, &x0, &y0,
                                          &x1, &y1);
      const int w = x1 - x0;
      const int h = y1 - y0;
      if (w > 0 && h > 0) {
        std::vector<unsigned char> glyph(size_t(w) * h);
        stbtt_MakeCodepointBitmapSubpixel(&info_, glyph.data(), w, h, w, scale, scale, shift, 0.0f,
                                          codes[i]);
        const int left = int(std::floor(x)) + x0;
        const int top = int(std::lround(baseline)) + y0;
        for (int gy = 0; gy < h; ++gy) {
          for (int gx = 0; gx < w; ++gx) {
            canvas.Blend(left + gx, top + gy, r, g, b, glyph[size_t(gy) * w + gx] / 255.0f);
          }
        }
      }
      int advance = 0, bearing = 0;
      stbtt_GetCodepointHMetrics(&info_, codes[i], &advance, &bearing);
      x += advance * scale;
      if (i + 1 < codes.size()) {
        x += stbtt_GetCodepointKernAdvance(&info_, codes[i], codes[i + 1]) * scale;
      }
    }
  }

 private:
  stbtt_fontinfo info_{};
  bool ok_ = false;
};

struct Toast {
  std::vector<uint8_t> pixels;
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t generation = 0;
};

// The notice for one achievement, at its size for a 1080-line window.
std::shared_ptr<Toast> MakeToast(const rex::system::AchievementInfo& achievement) {
  static const Font font;
  constexpr float kHeight = 122.0f;
  constexpr float kRadius = 30.0f;
  constexpr int kIcon = 64;
  constexpr float kIconLeft = 28.0f;
  constexpr float kTextLeft = kIconLeft + kIcon + 26.0f;
  constexpr float kTextSize = 30.0f;
  const std::string title = "Achievement unlocked";
  const std::string line = std::to_string(achievement.gamerscore) + " G - " + achievement.label;
  const float text_width = std::max(font.Width(title, kTextSize), font.Width(line, kTextSize));
  const int width = int(std::ceil(std::clamp(kTextLeft + text_width + 56.0f, 600.0f, 1500.0f)));
  const int height = int(kHeight);

  Canvas canvas(width, height);
  FillRounded(canvas, 0.0f, 0.0f, float(width), kHeight, kRadius, 0.11f, 0.13f, 0.11f, 0.92f);

  // The icon from the disc's XDBF, a PNG.
  if (auto* kernel_state = REX_KERNEL_STATE(); kernel_state && achievement.image_id) {
    const auto db = kernel_state->title_xdbf();
    const auto block =
        db.is_valid() ? db.GetEntry(rex::system::util::XdbfSection::kImage, achievement.image_id)
                      : rex::system::util::XdbfBlock{nullptr, 0};
    int w = 0, h = 0, channels = 0;
    if (stbi_uc* icon = block ? stbi_load_from_memory(block.buffer, int(block.size), &w, &h,
                                                     &channels, 4)
                              : nullptr) {
      DrawIcon(canvas, icon, w, h, int(kIconLeft), int((kHeight - kIcon) / 2), kIcon);
      stbi_image_free(icon);
    }
  }

  font.Draw(canvas, title, kTextLeft, 50.0f, kTextSize, 0.55f, 0.80f, 0.33f);
  font.Draw(canvas, line, kTextLeft, 94.0f, kTextSize, 0.96f, 0.96f, 0.96f);

  auto toast = std::make_shared<Toast>();
  toast->pixels = canvas.Bytes();
  toast->width = uint32_t(width);
  toast->height = uint32_t(height);
  return toast;
}

constexpr double kShownSeconds = 6.0;
constexpr double kFadeInSeconds = 0.25;
constexpr double kFadeOutSeconds = 0.5;

std::mutex g_mutex;
std::deque<std::shared_ptr<Toast>> g_waiting;
std::shared_ptr<Toast> g_current;
std::shared_ptr<Toast> g_previous;  // kept for a renderer still copying it
std::chrono::steady_clock::time_point g_started;
uint32_t g_generation = 0;

void Notify(const rex::system::AchievementEvent& event) {
  auto toast = MakeToast(event.achievement);
  REXLOG_INFO("achievements: notice for {:08X} \"{}\" ({}x{})", event.achievement.id,
              event.achievement.label, toast->width, toast->height);
  std::lock_guard lock(g_mutex);
  toast->generation = ++g_generation;
  g_waiting.push_back(std::move(toast));
}

}  // namespace

void XerengeAchievementToastStart() {
  auto* kernel_state = REX_KERNEL_STATE();
  if (!kernel_state) {
    return;
  }
  kernel_state->achievements().RegisterNotificationCallback(&Notify);
}

void XerengeOverlayFrame(const uint8_t** pixels, uint32_t* width, uint32_t* height,
                         uint32_t* generation, float* opacity) {
  // XERENGE_TEST_ACHIEVEMENT: once, some seconds in, when the titles are loaded.
  static const char* test = std::getenv("XERENGE_TEST_ACHIEVEMENT");
  static const auto start = std::chrono::steady_clock::now();
  static bool tested = false;
  if (test && !tested && std::chrono::steady_clock::now() - start > std::chrono::seconds(8)) {
    tested = true;
    if (auto* kernel_state = REX_KERNEL_STATE()) {
      kernel_state->achievements().ShowAchievementNotification(
          uint32_t(std::strtoul(test, nullptr, 0)));
    }
  }

  std::lock_guard lock(g_mutex);
  const auto now = std::chrono::steady_clock::now();
  double elapsed = std::chrono::duration<double>(now - g_started).count();
  if (g_current && elapsed >= kShownSeconds) {
    g_previous = std::move(g_current);
  }
  if (!g_current && !g_waiting.empty()) {
    g_current = std::move(g_waiting.front());
    g_waiting.pop_front();
    g_started = now;
    elapsed = 0.0;
  }
  if (!g_current) {
    *opacity = 0.0f;
    return;
  }
  *pixels = g_current->pixels.data();
  *width = g_current->width;
  *height = g_current->height;
  *generation = g_current->generation;
  *opacity = float(std::min({1.0, elapsed / kFadeInSeconds, (kShownSeconds - elapsed) / kFadeOutSeconds}));
}
