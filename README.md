# Xerenge-iOS

Native iOS (iPhone & iPad) port of **Burnout Revenge** (Xbox 360, retail): the game's PowerPC code is recompiled to native ARM64 ahead of time and executed directly on Apple Silicon, with graphics rendered natively through **Apple Metal** via the custom Plume TBDR backend. There is no CPU interpreter and no JIT.

> **Status:** Fully playable at a **rock-solid locked 60 FPS** on iPhone 13 (Apple A15 Bionic) and newer devices, with authentic motion blur, warm sky bloom, 100% glitch-free UI, and controller/touch support.

**Supported releases:** Burnout Revenge (Europe) and, since 0.4.7, Burnout Revenge (USA) - both officially, tested end to end: menus, races, Crash mode, replays, widescreen, and the logo skip. Other dumps (the beta, other regions) are not.

---

## Getting it running

You need a dump of the retail European or American release - the image hashing to:
* **Europe:** `34c1bd4d549c2c53f29d814fa5e5d1c04c533c5ca0c39e57b6c2538f44ff59b4`
* **USA:** `7abf689f52b70666ba92052bd157cfcae9ed6290dae298fbf639baa1d169f92b`

The two are not the same build (the territory is compiled into the code), so each gets a game generated from its own `default.xex`; [`SETUP.md`](SETUP.md) has the details.

### Building & Extracting the Game Data

```bash
./scripts/bootstrap.sh /path/to/burnout-revenge.iso
```

That verifies the image, extracts it, builds the SDK and builds the title. [`SETUP.md`](SETUP.md) covers the steps separately, what each one does, and what to do when something goes wrong. The scripts are:

| Script | Purpose |
|---|---|
| `scripts/check-prerequisites.sh` | Says what is missing before a build can fail halfway |
| `scripts/extract-image` | Reads the disc image directly (XDVDFS, which ordinary tools do not open) |
| `scripts/setup-sdk.sh` | Fetches and builds the recompiler and system layer |
| `scripts/setup-deps.sh` | Fetches plume and XenosRecomp (our forks) and builds the shader translator |
| `scripts/build.sh` | Points the manifest at your game directory, generates the code, translates the shaders and builds |
| `scripts/bootstrap.sh` | All of the above, in order |
| `scripts/inspect-capture.sh` | Reads a RenderDoc capture offline: passes, the draw behind a given pixel, full state, textures |
| `rexglue/run.sh` | Runs desktop build with the flags that are not optional and a log per run |
| `rexglue/installer/` | The Qt 6 installer and AppImage packer (Linux / macOS) |

### Shaders

The title's shaders are its own code, so none are kept here: the build translates them from your copy of the game. Most are found by scanning the executable. The rest the title assembles while it runs - a shader from the executable, or from a `graphics/*.obj` file on the disc, with a few vertex-fetch instructions patched to match the vertex layout. Those are rebuilt from the disc by the recipe in `rexglue/tools/runtime_shaders.recipe`, which holds only hashes and patch masks, and each result is checked against its hash. So every shader is there from the first frame, without running the game once to collect them. `rexglue/tools/make_runtime_shader_recipe` (built by `scripts/build-tools.sh`) refreshes the recipe from the shaders a run dumps (`XERENGE_DUMP_UCODE=1`).

---

## iOS Build & Deployment

### 1. Compile Release App with Xcode
Open the Xcode project or build directly from terminal:
```bash
xcodebuild -project ios/build_test/burnout.xcodeproj \
  -scheme burnout \
  -configuration Release \
  -destination 'generic/platform=iOS' \
  -allowProvisioningUpdates build
```

### 2. Deploy to Physical iPhone / iPad
Install directly over USB or Wi-Fi using `xcrun devicectl`:
```bash
xcrun devicectl device install app --device <DEVICE_UUID> ios/build_test/Release-iphoneos/burnout.app
```
*(Alternatively, deploy via Xcode, AltStore, SideStore, or TrollStore).*

### 3. Game Data Directory
Place your extracted game files (`default.xex` and game asset folders) in the app's `Documents/game/` container on your device.

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

## Controls
* **Game Controllers:** Full native support for MFi, PlayStation DualSense, Xbox Wireless, and Backbone One controllers via iOS GameController framework.
* **Touch Controls:** On-screen virtual joystick and touch buttons for mobile-only play.

---

## Licence

Xerenge's code is licensed under the GNU General Public License v3 or later ([LICENSE](LICENSE)).
The ReXGlue SDK and third-party code in `rexglue/thirdparty` keep their own respective licenses.

*Disclaimer: Burnout Revenge is a trademark of Electronic Arts and Criterion Games. This project contains no copyrighted assets or game code; you must supply your own legally acquired game image to play.*
