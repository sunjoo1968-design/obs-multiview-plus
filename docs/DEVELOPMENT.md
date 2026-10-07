# Development

## Project structure
- `src/`: OBS plugin and Qt user interface.
- `tests/`: layout/tally tests and isolated OBS smoke driver.
- `scripts/`: dependency download, build, runtime test, packaging.
- `docs/`: public development documentation.
- `release/`: locally generated packages; binaries are published through GitHub Releases.
- `.deps/`, `build/`, `dist/`, `.local/`: ignored local dependencies, build output and archived working material.

## Build and validation
Windows x64, OBS 32.2.2, Visual Studio 2022 C++ tools, CMake 3.28+, Node.js 20+ are required.

```powershell
./scripts/build.ps1
ctest --test-dir build -C Release --output-on-failure
./scripts/test-runtime.ps1
```

`-WithSceneAnchor` additionally tests coexistence when SceneAnchor is installed locally. This optional plugin is not bundled.
The runtime script creates a separate portable OBS under `.local/runtime` and does not use the user's scene collection.
The smoke driver is never installed or included in the release ZIP.

## 0.4.0 validation
- Release build and both CTest suites passed.
- Isolated OBS tests passed: nested tally, settings, drag layouts, fullscreen, reopen, clock and resource tiles.
- Optional SceneAnchor coexistence preserved all nine fixture scenes at initial/configured/reopened checkpoints.
- OBS reported zero memory leaks on test shutdown.
- White name text, tally background, and program precedence were checked through UI state. Native GPU overlay appearance is not captured by the widget screenshot test.
- CPU/GPU values were observed in the resource widget; GPU support depends on Windows performance counters and the driver.

## Limits
Resource values describe the OBS process, not whole-machine usage. GPU is the busiest engine percentage.
An unavailable counter is displayed as unavailable, not zero. A delayed Windows PDH provider can delay final resource-monitor shutdown.
Short isolated tests do not certify prolonged live production or every GPU driver. An earlier intermittent SceneAnchor list omission remains unreproduced.

## Release packaging
After validation, `scripts/package.ps1` collects the installation ZIP, source ZIP, DLL and checksums under `release/<version>`.
Publish only the reviewed release files. Keep runtime logs, reference images, local histories and downloaded SDKs out of version control.

## 0.5.0 Controller
Local validated development build; see [report](04-report/controller-integration.report.md). Native Fade and installed Source Switcher fixtures passed. Shutdown leak count 1 matched the existing 0.4.0 comparison baseline; this is not a zero-leak certification. The default runtime test remains strict; use -ExpectedMemoryLeaks 1 only for an explicitly established comparison. Field deployment and external tally work remain pending.
