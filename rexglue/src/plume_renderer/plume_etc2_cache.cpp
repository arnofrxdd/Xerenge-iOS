/**
 * @file        plume_renderer/plume_etc2_cache.cpp
 * @brief       The title's DXT textures as ETC2, kept from one transcode to the next.
 */
#include "plume_renderer/plume_etc2_cache.h"

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <thread>
#include <unordered_map>

#include <fmt/format.h>
#include <rex/logging.h>
#include <xxhash.h>

namespace rex::plume_renderer {
namespace {

// Bumped whenever the ETC2 made from the same blocks changes (the encoder, its
// settings, the decode before it): files of another version are not read.
constexpr uint32_t kVersion = 1;
constexpr uint32_t kMagic = 0x32434558;  // "XEC2"
// What this run keeps in memory; past it, the oldest go (the files stay). Less
// on a phone, whose memory the game itself needs and whose storage is fast.
#if defined(__ANDROID__) || defined(__APPLE__)
constexpr size_t kMemoryLimit = size_t(64) << 20;
#else
constexpr size_t kMemoryLimit = size_t(256) << 20;
#endif

struct Entry {
  int alpha = 0;
  uint32_t row_texels = 0;
  std::vector<uint8_t> data;
};

struct Header {
  uint32_t magic;
  uint32_t version;
  int32_t alpha;
  uint32_t row_texels;
  uint64_t size;
};

struct State {
  std::mutex mutex;
  std::unordered_map<uint64_t, std::shared_ptr<const Entry>> entries;
  std::deque<uint64_t> order;  // oldest first
  size_t bytes = 0;
  std::filesystem::path dir;   // empty: memory only
  bool enabled = std::getenv("XERENGE_NO_ETC2_CACHE") == nullptr;

  // Files to write, on a thread of their own.
  std::deque<std::pair<uint64_t, std::shared_ptr<const Entry>>> pending;
  std::condition_variable wake;
  bool writer_started = false;
  int writing = 0;  // taken off pending, not yet on disk

  uint64_t memory_hits = 0, disk_hits = 0, misses = 0, written = 0;
  double miss_ms = 0;
};

// Never destroyed: the writer thread waits on it to the end of the process.
State& S() {
  static State* state = new State;
  return *state;
}

std::filesystem::path FileFor(const std::filesystem::path& dir, uint64_t key) {
  return dir / fmt::format("{:016x}.etc2", key);
}

void Remember(State& s, uint64_t key, std::shared_ptr<const Entry> entry) {
  if (s.entries.emplace(key, entry).second) {
    s.order.push_back(key);
    s.bytes += entry->data.size();
  }
  while (s.bytes > kMemoryLimit && !s.order.empty()) {
    auto it = s.entries.find(s.order.front());
    if (it != s.entries.end()) {
      s.bytes -= it->second->data.size();
      s.entries.erase(it);
    }
    s.order.pop_front();
  }
}

void Writer() {
  State& s = S();
  for (;;) {
    std::pair<uint64_t, std::shared_ptr<const Entry>> job;
    std::filesystem::path dir;
    {
      std::unique_lock lock(s.mutex);
      s.wake.wait(lock, [&] { return !s.pending.empty(); });
      job = std::move(s.pending.front());
      s.pending.pop_front();
      ++s.writing;
      dir = s.dir;
    }
    const std::filesystem::path file = FileFor(dir, job.first);
    const std::filesystem::path partial = file.string() + ".part";
    {
      std::ofstream out(partial, std::ios::binary | std::ios::trunc);
      const Header header{kMagic, kVersion, job.second->alpha, job.second->row_texels,
                          job.second->data.size()};
      out.write(reinterpret_cast<const char*>(&header), sizeof(header));
      out.write(reinterpret_cast<const char*>(job.second->data.data()),
                std::streamsize(job.second->data.size()));
      if (!out) {
        std::lock_guard lock(s.mutex);
        --s.writing;
        continue;
      }
    }
    std::error_code error;
    std::filesystem::rename(partial, file, error);
    std::lock_guard lock(s.mutex);
    --s.writing;
    if (!error) {
      ++s.written;
    }
  }
}

std::shared_ptr<const Entry> ReadFile(const std::filesystem::path& file) {
  std::ifstream in(file, std::ios::binary);
  if (!in) {
    return nullptr;
  }
  Header header{};
  if (!in.read(reinterpret_cast<char*>(&header), sizeof(header)) || header.magic != kMagic ||
      header.version != kVersion || header.size > (size_t(64) << 20)) {
    return nullptr;
  }
  auto entry = std::make_shared<Entry>();
  entry->alpha = header.alpha;
  entry->row_texels = header.row_texels;
  entry->data.resize(size_t(header.size));
  if (!in.read(reinterpret_cast<char*>(entry->data.data()), std::streamsize(header.size))) {
    return nullptr;
  }
  return entry;
}

void LogNow(State& s) {
  REXLOG_INFO(
      "plume: ETC2 cache - {} from memory, {} from disk, {} transcoded ({:.0f} ms), {} written, "
      "{} MiB held",
      s.memory_hits, s.disk_hits, s.misses, s.miss_ms, s.written, s.bytes >> 20);
}

void MaybeLog(State& s) {
  const uint64_t total = s.memory_hits + s.disk_hits + s.misses;
  if (total % 500 == 0) {
    LogNow(s);
  }
}

}  // namespace

Etc2Cache& Etc2Cache::Instance() {
  static Etc2Cache cache;
  return cache;
}

void Etc2Cache::SetRoot(const std::filesystem::path& root) {
  State& s = S();
  std::lock_guard lock(s.mutex);
  if (!s.enabled || root.empty()) {
    return;
  }
  std::error_code error;
  const std::filesystem::path dir = root / "etc2";
  std::filesystem::create_directories(dir, error);
  if (error) {
    REXLOG_WARN("plume: ETC2 cache - cannot make {}: {}", dir.string(), error.message());
    return;
  }
  s.dir = dir;
  if (!s.writer_started) {
    s.writer_started = true;
    std::thread(Writer).detach();
  }
  REXLOG_INFO("plume: ETC2 cache at {}", dir.string());
}

uint64_t Etc2Cache::Key(uint32_t format, const std::vector<uint8_t>& blocks, uint32_t row_bytes,
                        uint32_t width, uint32_t height, int alpha_in) {
  const uint64_t shape = (uint64_t(format) << 56) ^ (uint64_t(width) << 40) ^
                         (uint64_t(height) << 24) ^ (uint64_t(row_bytes) << 4) ^
                         uint64_t(alpha_in + 1);
  return XXH3_64bits_withSeed(blocks.data(), blocks.size(), shape ^ kVersion);
}

bool Etc2Cache::Find(uint64_t key, int* alpha, std::vector<uint8_t>* out, uint32_t* row_texels) {
  State& s = S();
  if (!s.enabled) {
    return false;
  }
  std::shared_ptr<const Entry> entry;
  std::filesystem::path dir;
  {
    std::lock_guard lock(s.mutex);
    if (auto it = s.entries.find(key); it != s.entries.end()) {
      entry = it->second;
      ++s.memory_hits;
      MaybeLog(s);
    }
    dir = s.dir;
  }
  if (!entry && !dir.empty()) {
    entry = ReadFile(FileFor(dir, key));
    if (!entry && dir.has_parent_path()) {
      entry = ReadFile(FileFor(dir.parent_path(), key));
    }
    if (entry) {
      std::lock_guard lock(s.mutex);
      Remember(s, key, entry);
      ++s.disk_hits;
      MaybeLog(s);
    }
  }
  if (!entry) {
    return false;
  }
  *alpha = entry->alpha;
  *row_texels = entry->row_texels;
  *out = entry->data;
  return true;
}

void Etc2Cache::Store(uint64_t key, int alpha, const std::vector<uint8_t>& data,
                      uint32_t row_texels) {
  State& s = S();
  if (!s.enabled) {
    return;
  }
  auto entry = std::make_shared<Entry>();
  entry->alpha = alpha;
  entry->row_texels = row_texels;
  entry->data = data;
  std::lock_guard lock(s.mutex);
  Remember(s, key, entry);
  if (!s.dir.empty()) {
    s.pending.emplace_back(key, std::move(entry));
    s.wake.notify_one();
  }
}

void Etc2Cache::Flush() {
  State& s = S();
  for (;;) {
    {
      std::lock_guard lock(s.mutex);
      if ((s.pending.empty() && s.writing == 0) || s.dir.empty()) {
        LogNow(s);
        // What was prepared ahead stays on disk only: the installer runs in the
        // game's process, and the game reads what it draws as it needs it.
        s.entries.clear();
        s.order.clear();
        s.bytes = 0;
        return;
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
}

void Etc2Cache::NoteMiss(double milliseconds) {
  State& s = S();
  std::lock_guard lock(s.mutex);
  ++s.misses;
  s.miss_ms += milliseconds;
  MaybeLog(s);
}

}  // namespace rex::plume_renderer
