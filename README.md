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
- `EventProcessing.Core` (static lib) and `EventProcessing.Console` (exe),
  x64, Visual Studio 2019 (`v142` toolset).
- OpenCV 4.4.0 is bundled under `ocv440/`.
- RAW/live input requires the [Prophesee Metavision SDK](https://www.prophesee.ai/)
  to be installed. `Metavision.props` picks it up automatically from
  `C:\Program Files\Prophesee` (override by setting the `MetavisionSDKDir`
  MSBuild/environment property). If the SDK isn't found, the build still
  succeeds but only CSV input is supported (RAW/live are compiled out via the
  `EVENTCORE_HAVE_METAVISION` macro).

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
