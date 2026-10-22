# Before Editing for Mobile / iOS — Please Read First

This document outlines the workflow and architecture rules to follow during the iOS/mobile port development so that upstream updates never overwrite or conflict with our custom mobile features.

---

## 1. Never Re-Extract the Disc
* The 7.8 GB ISO has already been verified and extracted into:
  `Xerenge/game/`
* This data is permanent and does not need to be extracted or verified again.

---

## 2. Incremental Compilation (No Starting from Scratch)
* The build system (CMake + Ninja / Xcode) uses incremental compilation.
* If a single file is updated or edited, only that file will be recompiled (taking ~5–10 seconds), not the entire 90 MB codebase.
* The shader translations (`shader_cache.cpp`) and codegen output are cached.

---

## 3. Strict Code Separation Architecture
To ensure our custom mobile code is never touched or broken by upstream repository updates:

```
Xerenge/
├── rexglue/          <-- Upstream game engine, hooks, & renderer (tracked via git)
├── rexglue-sdk/      <-- Upstream ReXGlue SDK
├── game/             <-- Extracted game assets (static)
└── ios/              <-- OUR CUSTOM MOBILE DIRECTORY (DO NOT TOUCH DURING UPSTREAM UPDATES)
    ├── Xcode/        <-- iOS Xcode project wrapper
    ├── Controls/     <-- Touch controls & CoreMotion Gyroscope tilt-to-steer
    ├── UI/           <-- Custom mobile menus & overlays (SwiftUI / UIKit)
    └── Metal/        <-- iOS CAMetalLayer / MoltenVK surface binding
```

**Rule:** Keep all iOS-specific application logic, touch overlays, and motion sensors inside `ios/` so that changes to `rexglue/` will not conflict with mobile features.

---

## 4. Git Branching Strategy for Upstream Updates
Before writing any iOS code:

1. **Create our development branch:**
   ```bash
   git checkout -b ios-port
   ```

2. **If the upstream GitHub repository ever releases an update:**
   ```bash
   git fetch origin
   git merge origin/main
   ```
   * Git will pull in upstream engine fixes and bug patches automatically.
   * Our `ios/` directory and mobile configurations will remain completely intact.

---

## 5. Quick Reference Paths
* **Decompiled C++ Source Code:** `rexglue/generated/default/` (190 files, ~90 MB)
* **Compiled Mac Desktop Binary:** `rexglue/build/burnout` (32 MB Mach-O ARM64)
* **Mac Test Script:** `cd rexglue && ./run.sh`
