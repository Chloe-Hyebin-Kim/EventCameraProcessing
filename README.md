# Event Camera Processing Prototype

## Purpose
This prototype processes event camera data for image processing research,
targeting the Prophesee **Metavision EVK4 HD / IMX636** (1280x720) event camera.
It can read a `.raw` recording, connect to a live EVK4 HD camera, or (for testing)
a plain CSV event stream, and render the result as debug images and an MP4 video.

## Pipeline
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

## Build
- `EventProcessing.Core` (static lib), `EventProcessing.Console` (exe), and
  `EventProcessing.Diag` (MFC GUI exe), x64, Visual Studio 2019 (`v142` toolset).
- OpenCV 4.4.0 is bundled under `ocv440/`.
- RAW/live input requires the [Prophesee Metavision SDK](https://www.prophesee.ai/)
  (Windows installer, version 5.x - see [installation
  docs](https://docs.prophesee.ai/stable/installation/windows.html)). Since
  SDK 5.0 it isn't free standalone, but it's included with any EVK4 purchase
  and with a PRO license; the fully open-source
  [OpenEB](https://github.com/prophesee-ai/openeb) covers the same Base/Core/
  Stream/HAL modules this project uses if you don't have SDK access, but you'd
  need to build it from source. `Metavision.props` picks the SDK up
  automatically from the default install path `C:\Program Files\Prophesee`
  (override by setting the `MetavisionSDKDir` MSBuild/environment property).
  If the SDK isn't found, `EventProcessing.Core` and `EventProcessing.Console`
  still build fine with only CSV input supported (RAW/live are compiled out
  via the `EVENTCORE_HAVE_METAVISION` macro) - but `EventProcessing.Diag`
  needs the SDK to build at all (see below).
  - **After installing**, the installer does not update your environment for
    you - add `<install dir>\bin` (default: `C:\Program Files\Prophesee\bin`)
    to `PATH`, and set `MV_HAL_PLUGIN_PATH` to `<install dir>\lib\metavision\hal\plugins`,
    otherwise the built exe will fail to start with a missing-DLL error (same
    class of issue as the `opencv_world440d.dll` one) or fail to find the
    camera plugin at runtime.
  - This project targets SDK 5.x's module layout (`Stream`, formerly named
    `Driver` pre-5.0 - `metavision/sdk/stream/camera.h`, `metavision_sdk_stream.lib`).
    If you're on an older SDK 4.x install for some reason, you'd need to
    revert those two to the old `driver` naming.
- `EventProcessing.Diag` needs the MFC component of the VS Build Tools
  installed ("C++ MFC for latest v142 build tools (x86 & x64)" in the VS
  Installer).

## Example
```
EventProcessing.Console.exe recording.raw output 10000 30
EventProcessing.Console.exe events.csv output 1000 30
EventProcessing.Console.exe live output 10000 30
```
Arguments: `<input.raw|input.csv|live> [outputDir=output] [windowUs=10000] [fps=30]`

## Output (in outputDir)
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
tune them for your setup - see the caveat below).

Step 4 from the original request ("공 관측을 통해 데이터 전송", e.g. ball
speed/launch angle/spin) is *not* implemented here - that's a separate
analysis task once frames are being reliably captured; NGSSensorDiag (or a
similar tool) already covers that role.

**This project requires the Metavision SDK to build at all** (unlike Core/
Console, which degrade gracefully to CSV-only). A live-preview tool has no
useful CSV-only mode.

### Known limitation (found via testing against a real recording)
Ball detection (`BallDetector`'s "largest contour" heuristic) is not very
robust on noisy real-world event data - on a real 7-iron swing RAW recording
used to validate this feature, it frequently locked onto other objects (a
shoe, a club) instead of the ball, which prevents "Ready" from ever firing
reliably. This looked like it might be related to the sample's `.bias` file
having all-zero sensor bias values (i.e. an uncalibrated/default sensor
sensitivity, which increases background noise). Tightening or loosening the
`stableMovePx`/`missToleranceUs` thresholds did not fully fix it; a more
robust fix (background/temporal filtering, or a real tracker) is a follow-up,
not something this change attempts. Tune the thresholds per your own
camera/bias setup, and treat this as the current known limitation of the
"automatic ready detection" step specifically.
