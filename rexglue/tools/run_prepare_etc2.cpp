#include <iostream>
#include <dlfcn.h>
#include <cstdint>

typedef int (*xerenge_prepare_etc2_fn)(const char*, const char*, void (*)(void*, uint32_t, uint32_t), void*);

static void progress(void*, uint32_t done, uint32_t total) {
  if (done % 50 == 0 || done == total) {
    std::cout << "\rPreparing ETC2 textures: " << done << "/" << total << std::flush;
  }
}

int main(int argc, char** argv) {
  if (argc < 4) {
    std::cerr << "Usage: " << argv[0] << " <path_to_librexgpu-plume.dylib> <game_dir> <cache_root>\n";
    return 1;
  }
  const char* dylib_path = argv[1];
  const char* game_dir = argv[2];
  const char* cache_root = argv[3];

  void* handle = dlopen(dylib_path, RTLD_NOW);
  if (!handle) {
    std::cerr << "Cannot open " << dylib_path << ": " << dlerror() << "\n";
    return 1;
  }
  auto fn = reinterpret_cast<xerenge_prepare_etc2_fn>(dlsym(handle, "xerenge_prepare_etc2"));
  if (!fn) {
    std::cerr << "Cannot find symbol xerenge_prepare_etc2: " << dlerror() << "\n";
    return 1;
  }
  std::cout << "Scanning and preparing ETC2 textures from: " << game_dir << "\n";
  std::cout << "Output cache root: " << cache_root << "\n";
  int count = fn(game_dir, cache_root, progress, nullptr);
  std::cout << "\nFinished! " << count << " textures pre-transcoded into ETC2 cache.\n";
  return 0;
}
