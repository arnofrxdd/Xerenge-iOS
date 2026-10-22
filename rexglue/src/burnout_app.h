// burnout - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <rex/rex_app.h>

#include "online_lobby.h"
#include "plume_d3d.h"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

class BurnoutApp : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
#if defined(__APPLE__)
    setenv("XERENGE_D3D_TARGETS", "1", 1);
    setenv("XERENGE_D3D_UI", "1", 1);
    setenv("XERENGE_SKIP_LOGOS", "1", 1);
    setenv("XERENGE_REAL_SHADERS", "1", 1);
    setenv("XERENGE_D3D_DRAWS", "1", 1);
    setenv("XERENGE_SECONDARY_TICKS", "1", 1);
    setenv("XERENGE_WORKERS", "2", 1);
    setenv("XERENGE_REFLECTION_SPLIT", "1", 1);
    setenv("XERENGE_ASYNC_PRESENT", "1", 1);
    setenv("XERENGE_RENDER_RESOLUTION", "1386x640", 1);
    setenv("XERENGE_NO_SECOND_TARGET", "1", 1);
    setenv("XERENGE_NO_MOTION_BLUR", "1", 1);
    setenv("XERENGE_AA", "fxaa", 1);
    setenv("XERENGE_VBLANK_HZ", "60", 1);
    setenv("XERENGE_POWER_MODE", "stock", 1);
    setenv("XERENGE_NO_WRITE_WATCH", "1", 1);
#endif
    return std::unique_ptr<BurnoutApp>(new BurnoutApp(ctx, "burnout",
        PPCImageConfig));
  }

  void OnConfigurePaths(rex::PathConfig& paths) override {
    fprintf(stderr, "=== BURNOUT OnConfigurePaths CALLED ===\n");
    rex::cvar::SetFlagByName("gpu_plugin", "plume");
    rex::cvar::SetFlagByName("gpu_backend", "plume");
    rex::cvar::SetFlagByName("protect_zero", "false");
    rex::cvar::SetFlagByName("vulkan_async_skip_incomplete_frames", "false");
#if defined(__APPLE__)
    const char* home = std::getenv("HOME");
    fprintf(stderr, "=== BURNOUT: home=%s ===\n", home ? home : "NULL");
    if (home) {
      std::filesystem::path home_path(home);
      std::filesystem::path docs = home_path / "Documents";
      mkdir(docs.c_str(), 0755);
      mkdir((docs / "logs").c_str(), 0755);
      paths.user_data_root = docs;
      paths.cache_root = home_path / "Library" / "Caches";

      std::error_code ec;
      std::filesystem::path docs_game = docs / "game";
      if (std::filesystem::exists(docs / "default.xex", ec)) {
        paths.game_data_root = docs;
      } else if (std::filesystem::exists(docs_game / "default.xex", ec)) {
        paths.game_data_root = docs_game;
      } else if (std::filesystem::is_directory(docs, ec)) {
        paths.game_data_root = docs;
      }
    }
    if (paths.game_data_root.empty()) {
      auto exe_dir = rex::filesystem::GetExecutableFolder();
      std::error_code ec;
      if (std::filesystem::exists(exe_dir / "game" / "default.xex", ec)) {
        paths.game_data_root = exe_dir / "game";
      } else if (std::filesystem::exists(exe_dir / "default.xex", ec)) {
        paths.game_data_root = exe_dir;
      }
    }
    if (!paths.game_data_root.empty()) {
      rex::cvar::SetFlagByName("game_data_root", paths.game_data_root.string());
    }
    fprintf(stderr, "=== BURNOUT: paths.game_data_root='%s' ===\n", paths.game_data_root.string().c_str());
#endif
  }

  void OnPostSetup() override {
    xerenge::plume_d3d::InstallPlumeRuntimeHooks();
    // --online: this copy's lobby is up (and heard on the LAN) from the start,
    // not only once its own player signs in.
    xerenge::online::StartLobbyNetwork();
  }
  // void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {}
  // std::unique_ptr<rex::ui::ImGuiDialog> CreateAchievementsOverlay() override;
  // std::unique_ptr<rex::ui::AchievementNotificationDialog>
  // CreateAchievementNotificationDialog() override;
  // void OnShutdown() override {}
  // void OnConfigurePaths(rex::PathConfig& paths) override {}
};
