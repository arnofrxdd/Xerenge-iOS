// macOS and iOS: librexruntime defines and exports rex_frame_clock_provider.
// Both the host and rexgpu-plume link rexruntime, so it is accessed directly.
// A dlsym fallback is also provided.
#ifdef __APPLE__
#include "frame_clock_provider.h"

#include <atomic>

#include <dlfcn.h>

RexFrameClockProviderFn RexFrameClockProvider() {
  if (rex_frame_clock_provider) {
    return rex_frame_clock_provider;
  }
  static std::atomic<RexFrameClockProviderFn*> slot{nullptr};
  RexFrameClockProviderFn* found = slot.load(std::memory_order_acquire);
  if (!found) {
    found = static_cast<RexFrameClockProviderFn*>(dlsym(RTLD_DEFAULT, "rex_frame_clock_provider"));
    if (!found) {
      found = static_cast<RexFrameClockProviderFn*>(dlsym(RTLD_MAIN_ONLY, "rex_frame_clock_provider"));
    }
    if (found) {
      slot.store(found, std::memory_order_release);
    }
  }
  return found ? *found : nullptr;
}
#endif
