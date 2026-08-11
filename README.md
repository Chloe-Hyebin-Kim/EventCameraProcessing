# Event Camera Processing Prototype

## Purpose
This prototype processes event camera data for image processing research,
targeting the Prophesee **Metavision EVK4 HD / IMX636** (1280x720, EVT3
format) event camera. It can read a `.raw` recording, connect to a live
EVK4 HD camera, or (for testing) a plain CSV event stream, and turn that
into viewable images/video - either as a one-shot batch conversion or as a
live preview with automatic ball-detection-triggered capture.

## Projects
- **`EventProcessing.Core`** (static lib) - event loading (CSV / Metavision
  RAW / live camera), accumulation imaging, noise filtering, ball detection,
  and the ready/trigger/capture state machine. Everything else builds on this.
- **`EventProcessing.Console`** (exe) - batch conversion: point it at a
  recording, get an MP4 + debug PNGs out. Works without the Metavision SDK
  (CSV-only) if the SDK isn't available.
- **`EventProcessing.Diag`** (MFC GUI exe) - live preview + automatic capture
  tool. Requires the Metavision SDK to build at all (see below).

## Pipeline (Console)
1. Load events from a Metavision RAW recording (`.raw`), a live EVK4 HD camera
   (`live`), or a CSV file (`.csv`, for testing)
2. Slide a time window (`windowUs`) across the recording
3. For each window, generate positive, negative, and merged event accumulation images
4. Apply basic noise filtering
5. Detect a ball candidate using contour analysis
6. Write each window's debug frame to an output video, and save the first
   window's images individually

## Input
- **RAW**: a Metavision RAW recording captured from an EVK4 HD / IMX636 camera
- **live**: pass `live` (or `camera`) instead of a file path to capture directly
  from the first connected EVK4 HD camera
- **CSV** (ex, for testing without a camera):
  ```
  t_us,x,y,p
  0,120,240,1
  3,121,240,1
  7,122,241,-1
  ```

## Bundled dependencies
Both OpenCV and the Metavision SDK are checked into this repo (like a vendored
build) so `git clone` + open the solution is enough for anyone to build,
without a separate install step:
- **`ocv440/`** - OpenCV 4.4.0 headers/libs, plus `opencv_world440(d).dll` at
  the repo root.
- **`Prophesee/`** - Metavision SDK 5.x `include/`, `lib/`, and `bin/`
  (headers, import libs, and every runtime DLL the SDK needs, including
  transitive third-party ones - see the OpenEB-from-source note below).

`Metavision.props` prefers `Prophesee/` when present, falling back to a
system-wide install (default `C:\Program Files\Prophesee`, override via the
`MetavisionSDKDir` MSBuild/environment property) otherwise. Post-build steps
on `EventProcessing.Console`/`EventProcessing.Diag` copy the right DLLs next
to the built exe automatically, and `MetavisionRuntime::EnsureBundledHalPluginPath()`
(called at the start of both apps) points `MV_HAL_PLUGIN_PATH` at the bundled
camera plugin folder at runtime - so nothing needs to be installed or
configured system-wide on a machine that just clones this repo.

If `Prophesee/` isn't present and no system-wide SDK is found,
`EventProcessing.Core`/`EventProcessing.Console` still build fine with
CSV-only support (RAW/live are compiled out via the `EVENTCORE_HAVE_METAVISION`
macro) - but `EventProcessing.Diag` needs the SDK to build at all, since a
live-preview tool has no useful CSV-only mode.

`EventProcessing.Diag` also needs the MFC component of the VS Build Tools
installed ("C++ MFC for latest v142 build tools (x86 & x64)" in the VS
Installer).

### If you rebuild/update the bundled Metavision SDK
- Get it from the [Prophesee installer](https://docs.prophesee.ai/stable/installation/windows.html)
  if you have SDK access (comes with any EVK4 purchase or a PRO license), or
  build [OpenEB](https://github.com/prophesee-ai/openeb) from source
  (open-source, covers the same Base/Core/Stream/HAL modules) if you don't.
- This project targets SDK 5.x's module layout (`Stream`, formerly named
  `Driver` pre-5.0 - `metavision/sdk/stream/camera.h`, `metavision_sdk_stream.lib`).
- **Building OpenEB from source**: its CMake install step does *not* copy
  every third-party runtime DLL (hdf5, protobuf, libusb, libpng, zlib, its
  own internal OpenCV build, etc.) into `<install>/bin` - some only end up in
  vcpkg's own `installed/x64-windows/bin/`. Check what a built DLL actually
  needs with `dumpbin /dependents <dll>` (VS Developer Command Prompt) and
  copy anything missing from vcpkg's `bin/` into `Prophesee/bin/` too.
- `Prophesee/bin/` is deliberately exempted from `.gitignore`'s generic
  `[Bb]in/` rule (see the bottom of `.gitignore`) - don't remove that
  exception, or `git add` on new DLLs there will silently no-op.
- **Don't duplicate the same DLL into multiple search-path directories**
  "to be safe" (e.g. also copying `metavision_sdk_*.dll`/`libprotobuf.dll`
  into the `hal_plugins/` folder next to the plugin DLLs). That causes two
  independent instances of the same DLL (protobuf in particular) to load in
  one process, which crashes with `google::protobuf::FatalException` in
  `descriptor.cc`. Only `libusb-1.0.dll` needs to be duplicated there
  (it's a direct dependency of the camera plugin DLLs, confirmed via
  `dumpbin`); everything else should live in exactly one place.

## Example (Console)
```
EventProcessing.Console.exe recording.raw output 10000 30
EventProcessing.Console.exe events.csv output 1000 30
EventProcessing.Console.exe live output 10000 30
```
Arguments: `<input.raw|input.csv|live> [outputDir=output] [windowUs=10000] [fps=30]`

### Output (in outputDir)
- `event_video.mp4` - merged event accumulation + ball detection overlay, one
  frame per `windowUs` window, encoded at `fps`
- `01_positive_event.png` / `02_negative_event.png` / `03_merged_event.png` /
  `04_binary_mask.png` / `05_debug_result.png` - images for the first window

## EventProcessing.Diag (live/RAW diag tool)
An MFC GUI tool for watching the live camera (or a RAW file played back in
real time) and automating capture, instead of running the batch console tool:

1. **Searching**: no ball recognized yet, or it hasn't held still long enough.
2. **Ready**: the recognized ball has stayed within `stableMovePx` of the same
   spot for `readySeconds` - this is the "ready" signal.
3. **Trigger**: from Ready, once the ball's center moves faster than
   `shotSpeedPxPerSec`, that's treated as a shot and capture starts.
4. **Capturing**: saves a PNG per accumulation window into
   `outputDir\shot_<timestamp>\` for `captureSeconds`, then returns to Searching.

All of the above thresholds, the accumulation window, and the RAW file / live
camera choice are set directly in the tool's UI (defaults are pre-filled but
tune them for your setup - see the known limitation below).

Step 4 from the original request ("공 관측을 통해 데이터 전송", e.g. ball
speed/launch angle/spin) is *not* implemented here - that's a separate
analysis task once frames are being reliably captured; NGSSensorDiag (or a
similar tool) already covers that role.

## Known limitation: ball detection robustness
`BallDetector`'s "largest contour" heuristic is not very robust on noisy
real-world event data - on a real 7-iron swing RAW recording used to
validate this feature, it frequently locked onto other objects (a shoe, a
club) instead of the ball, which prevents "Ready" from firing reliably. This
looked related to the sample's `.bias` file having all-zero sensor bias
values (i.e. uncalibrated/default sensor sensitivity, which increases
background noise). Tightening or loosening the `stableMovePx`/
`missToleranceUs` thresholds did not fully fix it; a more robust fix
(background/temporal filtering, or a real tracker) is a follow-up, not
something implemented yet. Tune the thresholds per your own camera/bias
setup in the meantime.

## Troubleshooting
Issues actually hit (and fixed) while building this out, in case they recur
on another machine:
- **`opencv_world440(d).dll` / `metavision_sdk_*.dll` not found at
  startup**: the exe was built but the DLL isn't next to it. Rebuild (not
  just Build) so the post-build copy step runs, or check it's not being run
  from a different output folder than where the DLLs got copied.
- **`Metavision HAL exception 101000: No plugin available for input
  stream`**: HAL loaded but couldn't find/load the camera plugin DLL. Check
  `hal_plugins\` next to the exe has `hal_plugin_prophesee.dll`,
  `metavision_psee_hw_layer.dll`, and `libusb-1.0.dll`; also check for a
  stale system-wide `MV_HAL_PLUGIN_PATH` pointing somewhere stale (the app
  now prefers its own bundled plugin folder, but only once rebuilt with
  that fix).
- **`google::protobuf::FatalException` crash in `descriptor.cc`**: two
  copies of `libprotobuf.dll` got loaded into the same process - almost
  always from a DLL being duplicated into more than one directory that's on
  the search path (see the "don't duplicate" note above). Delete any
  `hal_plugins\` folder under the build output and rebuild.
- **MSVC `C4819` code-page warnings** on files with Korean comments: add a
  UTF-8 BOM to the file (`git log` this repo's history for examples) -
  without it, MSVC on a CP949-locale machine misreads UTF-8 source as CP949.
