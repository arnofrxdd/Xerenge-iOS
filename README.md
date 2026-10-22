# Xerenge-iOS

Native iOS (iPhone & iPad) port of **Burnout Revenge** (Xbox 360, retail): the game's PowerPC code is recompiled to native ARM64 ahead of time and executed directly on Apple Silicon, with graphics rendered natively through **Apple Metal** via the custom Plume TBDR backend. There is no CPU interpreter and no JIT.

> **Status:** Fully playable at a **rock-solid locked 60 FPS** on iPhone 13 (Apple A15 Bionic) and newer devices, with authentic motion blur, warm sky bloom, 100% glitch-free UI, and controller/touch support.

---

## Highlights & Mobile Breakthroughs

* **Locked 60 FPS Engine Pacing:**
  * Synchronized to the console's fixed 60 Hz simulation loop (`dt = 1/60s`) and display refresh via `CAMetalLayer`.
  * Multi-frame `VdSwap` pacing queue headroom eliminates CPU stalls during massive multi-car crashes and explosions.
* **Authentic Motion Blur & Sky Bloom:**
  * Retains the signature 200 MPH velocity streaks and takedown cinematic motion blur using native per-pixel motion vector targets (`oC1`).
  * Authentic multi-pass golden-hour sky bloom and specular car reflections running at less than 0.3 ms GPU overhead.
* **Apple Silicon TBDR GPU Optimization:**
  * Scene rendered at `1386x640` and cleanly upscaled to native high-DPI OLED screens (e.g. 2532×1170 on iPhone 13).
  * Color, motion vector, and depth attachments fit 100% within the A15's 32 MB System-Level Cache (SLC), preventing DRAM memory bandwidth bottlenecks and keeping the device cool.
* **580 MB Unified Memory Slashed:**
  * Dynamic buffer sizes and draw ring structures are specifically tuned for iOS (`kDummyVertexCount = 1048576`, `kCacheVertexCount = 524288`), ensuring the app stays safely below iOS Jetsam termination limits.
* **Perceptual Traffic & Reflection LOD:**
  * Planar drop-shadows for civilian cars beyond 35 meters are perceptually culled, saving 30+ redundant render passes per frame in heavy traffic.
  * Player vehicle updates 1 dynamic reflection cubemap face per turn, keeping civilian traffic reflections lightweight while keeping the player car dazzling.
* **100% Glitch-Free HUD & Natural FOV:**
  * Tuned mobile camera field-of-view without fisheye distortion.
  * 2D UI quad pass-through preserves razor-sharp text, speedometers, boost flames, and ranking overlays.

---

## Architecture & Submodules

The Xerenge-iOS project consists of three coordinated repositories:

| Repository | Description |
| :--- | :--- |
| **[arnofrxdd/Xerenge-iOS](https://github.com/arnofrxdd/Xerenge-iOS)** | Main title logic, Direct3D draw translation, iOS Xcode project, and build scripts. |
| **[arnofrxdd/rexglue-xerenge](https://github.com/arnofrxdd/rexglue-xerenge)** | ReXGlue SDK fork carrying iOS runtime support, Apple thread QoS tuning, and MoltenVK patches. |
| **[arnofrxdd/plume-xerenge](https://github.com/arnofrxdd/plume-xerenge)** | Low-level Plume renderer fork with native Apple device vendor detection and iOS CocoaWindow stubs. |

---

## Requirements

1. **Retail Game Dump:** A dump of the retail European or American release (`default.xex`).
2. **Mac with Xcode:** macOS Sonoma/Sequoia with Xcode 15+ and command-line tools installed.
3. **Target Device:** iPhone or iPad with Apple A12 Bionic or newer (tested and verified on iPhone 13 / A15 Bionic). Supports iOS 16.0+.

---

## Building & Deploying for iOS

### 1. Extract & Translate Shaders
Ensure dependencies are set up and the game assets are extracted:
```bash
./scripts/bootstrap.sh /path/to/burnout-revenge.iso
```

### 2. Compile Release App with Xcode
Build the Release binary for your device:
```bash
xcodebuild -project ios/build_test/burnout.xcodeproj \
  -scheme burnout \
  -configuration Release \
  -destination 'generic/platform=iOS' \
  -allowProvisioningUpdates build
```

### 3. Deploy to Physical Device
Install directly over USB or Wi-Fi using `xcrun devicectl`:
```bash
xcrun devicectl device install app --device <DEVICE_UUID> ios/build_test/Release-iphoneos/burnout.app
```
*(Alternatively, deploy via Xcode, AltStore, SideStore, or TrollStore).*

### 4. Game Data Directory
Place your extracted game files (`default.xex` and game asset folders) in the app's `Documents/game/` container on your device.

---

## Controls
* **Game Controllers:** Full native support for MFi, PlayStation DualSense, Xbox Wireless, and Backbone One controllers via iOS GameController framework.
* **Touch Controls:** On-screen virtual joystick and touch buttons for mobile-only play.

---

## Licence

Xerenge's code is licensed under the GNU General Public License v3 or later ([LICENSE](LICENSE)).
The ReXGlue SDK and third-party code in `rexglue/thirdparty` keep their own respective licenses.

*Disclaimer: Burnout Revenge is a trademark of Electronic Arts and Criterion Games. This project contains no copyrighted assets or game code; you must supply your own legally acquired game image to play.*
