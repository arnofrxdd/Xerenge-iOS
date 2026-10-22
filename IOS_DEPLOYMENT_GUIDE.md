# iOS Deployment & Device Operations Guide

This guide documents the exact commands, workflows, and procedures used to build, code-sign, deploy, transfer assets/cache, launch, and monitor **Burnout Revenge** on a physical iPhone using Xcode and Apple's `devicectl` utility.

---

## 1. Environment & Device Identifiers

| Parameter | Value |
| :--- | :--- |
| **Connected Device** | iPhone 13 |
| **Device UDID** | `00008110-000C49061E08401E` |
| **Bundle Identifier** | `com.xerenge.burnout` |
| **Xcode Project** | `/Users/arnav/Projects/Xerenge/ios/build_test/burnout.xcodeproj` |
| **Compiled App Bundle** | `/Users/arnav/Projects/Xerenge/ios/build_test/Release-iphoneos/burnout.app` |
| **Game Data Source (Mac)** | `/Users/arnav/Projects/Xerenge/game/` (3.6 GB, 803 files) |
| **ETC2 Texture Cache (Mac)** | `/Users/arnav/Projects/Xerenge/etc2_cache/etc2/` (753 MB, 19,416 files) |

---

## 2. Building and Code-Signing the Release App

To produce an optimized, hardware-accelerated binary (`-O3`, ARM64 NEON vectorization enabled):

```bash
# Touch main to trigger incremental recompilation if headers changed
touch /Users/arnav/Projects/Xerenge/rexglue/src/main.cpp

# Compile and code-sign the Release configuration
xcodebuild \
  -project /Users/arnav/Projects/Xerenge/ios/build_test/burnout.xcodeproj \
  -scheme burnout \
  -configuration Release \
  -destination 'generic/platform=iOS' \
  -allowProvisioningUpdates \
  build
```

**Key details:**
- `xcodebuild` automatically applies the provisioning profile and developer certificate (`Apple Development`).
- The signed `.app` bundle is written to `/Users/arnav/Projects/Xerenge/ios/build_test/Release-iphoneos/burnout.app`.

---

## 3. Installing the App onto the iPhone

Deploy the `.app` bundle to the connected physical device via Apple CoreDevice:

```bash
xcrun devicectl device install app \
  --device 00008110-000C49061E08401E \
  /Users/arnav/Projects/Xerenge/ios/build_test/Release-iphoneos/burnout.app
```

**Success confirmation:**
Outputs `App installed:` with the bundle ID (`com.xerenge.burnout`) and sandbox UUID.

---

## 4. Copying Game Assets into `Documents/`

The game assets (3.6 GB) include `default.xex`, track geometry, audio archives (`eatrax`), and vehicle models. They must reside in the app's `Documents` sandbox directory:

```bash
xcrun devicectl device copy to \
  --timeout 1800 \
  --device 00008110-000C49061E08401E \
  --domain-type appDataContainer \
  --domain-identifier com.xerenge.burnout \
  --destination Documents \
  --source /Users/arnav/Projects/Xerenge/game
```

**Key parameters:**
- `--timeout 1800`: Sets a 30-minute timeout so large multi-gigabyte transfers over USB/Lightning do not abort early.
- `--domain-type appDataContainer`: Targets the sandboxed data container of the app (`/private/var/mobile/Containers/Data/Application/<UUID>/`).
- `--domain-identifier com.xerenge.burnout`: Identifies the exact app container to write into.
- `--destination Documents`: Places the contents of `game/` directly into `Documents/` so that `default.xex` is located at `Documents/default.xex`.

---

## 5. Copying the Pre-Cached ETC2 Textures into `Library/Caches/`

To prevent the iPhone CPU from transcoding desktop BC textures during gameplay, deploy the pre-transcoded native ETC2 textures:

```bash
xcrun devicectl device copy to \
  --timeout 1800 \
  --device 00008110-000C49061E08401E \
  --domain-type appDataContainer \
  --domain-identifier com.xerenge.burnout \
  --destination Library/Caches \
  --source /Users/arnav/Projects/Xerenge/etc2_cache/etc2
```

**Resuming transfers:**
- `devicectl` checks existing files on the device and skips files that have already been transferred.
- If a socket times out due to packet limits, simply re-running the command resumes transfer from the remaining files.

---

## 6. Inspecting Files Inside the App Container

To verify which files exist inside any sandbox folder on the iPhone without opening Xcode:

```bash
# Check the root of the app container:
xcrun devicectl device info files \
  --device 00008110-000C49061E08401E \
  --domain-type appDataContainer \
  --domain-identifier com.xerenge.burnout

# Check Documents/ (e.g. verify default.xex):
xcrun devicectl device info files \
  --device 00008110-000C49061E08401E \
  --domain-type appDataContainer \
  --domain-identifier com.xerenge.burnout \
  --subdirectory Documents

# Count how many cached ETC2 textures are installed on the device:
xcrun devicectl device info files \
  --device 00008110-000C49061E08401E \
  --domain-type appDataContainer \
  --domain-identifier com.xerenge.burnout \
  --subdirectory Library/Caches | wc -l
```

---

## 7. Launching the Game & Monitoring Live Console

Launch the game on the phone while streaming stdout/stderr back to the terminal:

```bash
xcrun devicectl device process launch \
  --device 00008110-000C49061E08401E \
  --console \
  com.xerenge.burnout
```

**What to look for in the console output:**
- `=== BURNOUT: paths.game_data_root='.../Documents' ===` — Confirms game data was discovered.
- `[mvk-info] Created 3 swapchain images with size (1440, 960)` — Confirms Metal Vulkan surface binding.
- `movie: skipping logo clip ...` — Confirms XERENGE_SKIP_LOGOS is active and title boot begins.

---

## 8. Pulling Live In-Game Logs from the Device

In Release builds, detailed engine diagnostics (frame timings, texture cache hit rates, audio status) are written to `Documents/logs/burnout_001.log`. To download the active log file:

```bash
xcrun devicectl device copy from \
  --device 00008110-000C49061E08401E \
  --domain-type appDataContainer \
  --domain-identifier com.xerenge.burnout \
  --source Documents/logs/burnout_001.log \
  --destination ./burnout_live.log
```

---

## 9. Capturing iPhone Screenshots Remotely

Take a direct, lossless PNG screenshot of what is currently rendered on the iPhone's screen:

```bash
xcrun devicectl device capture screenshot \
  --device 00008110-000C49061E08401E \
  --destination ./screenshot_current.png
```

---

## 10. Container Reset (Troubleshooting Corruption)

If the app sandbox container becomes corrupted (for instance, if `Documents` was accidentally overwritten by a file), uninstalling the app cleans the entire container:

```bash
xcrun devicectl device uninstall app \
  --device 00008110-000C49061E08401E \
  com.xerenge.burnout
```

When you re-install the app afterwards, iOS automatically generates a brand-new, clean data container with fresh `Documents/`, `Library/`, and `tmp/` directories.
