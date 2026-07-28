# Event Camera Processing Prototype

## Purpose
This prototype processes event camera data for image processing research.
The input event stream is assumed to have timestamp, x, y, and polarity.

## Pipeline
1. Load event stream from CSV
2. Select events within a time window
3. Generate positive, negative, and merged event accumulation images
4. Apply basic noise filtering
5. Detect a ball candidate using contour analysis
6. Save debug images

## Input Format (ex)
t_us,x,y,p
0,120,240,1
3,121,240,1
7,122,241,-1

## Example
EventProcessing.Console.exe events.csv 1280 720 0 1000

## Output
01_positive_event.png
02_negative_event.png
03_merged_event.png
04_binary_mask.png
05_debug_result.png
