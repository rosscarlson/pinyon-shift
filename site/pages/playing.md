---
title: Settings and controls
section: Playing
order: 1
description: Keyboard shortcuts, graphics presets, language, controllers, saves and photos in Pinyon Shift.
---

# Settings and controls

Most settings live in the game, on <kbd>F6</kbd>, and apply at once. MSAA and the language are among the few that need a restart.

## Keys

| Key | Action |
| --- | --- |
| <kbd>F6</kbd> | Settings. Also **SETTINGS** in the pause menu |
| <kbd>F3</kbd> | Performance panel |
| <kbd>F7</kbd> | Achievements |
| <kbd>F8</kbd> | Save the current frame as a PNG photo |
| <kbd>F10</kbd> | Trainer: credits, game speed, time of day, free camera. Needs cheats on |
| <kbd>F11</kbd> | Fullscreen. Also <kbd>Alt</kbd>+<kbd>Enter</kbd> |

On a Mac, where the F-keys are media keys, use <kbd>Cmd</kbd>+<kbd>,</kbd> for the settings, <kbd>Ctrl</kbd>+<kbd>Cmd</kbd>+<kbd>F</kbd> for fullscreen, <kbd>Cmd</kbd>+<kbd>Shift</kbd>+<kbd>P</kbd> for a photo, <kbd>Cmd</kbd>+<kbd>T</kbd> for the trainer and <kbd>Cmd</kbd>+<kbd>Q</kbd> to quit.

On a controller, hold **Back + Start** together (View + Menu on a Steam Deck) to open the settings, and **LS + RS** for the performance panel; both chords can be changed in **F6 > CONTROLS > CONTROLLER**.

### Driving on a keyboard

| Key | Control |
| --- | --- |
| <kbd>W</kbd> / <kbd>S</kbd> | Throttle / brake |
| <kbd>A</kbd> <kbd>D</kbd> or <kbd>←</kbd> <kbd>→</kbd> | Steer. The arrows also move through menus |
| <kbd>Space</kbd> | Handbrake; accept in menus (also <kbd>Enter</kbd> or a left click) |
| <kbd>E</kbd> / <kbd>Q</kbd> | Shift up / down. <kbd>E</kbd> and <kbd>Backspace</kbd> are also B (back) |
| <kbd>R</kbd> | Rewind |
| <kbd>C</kbd> | Camera |
| <kbd>Z</kbd> | Clutch |
| <kbd>I</kbd> <kbd>J</kbd> <kbd>K</kbd> <kbd>L</kbd> | Look around |
| <kbd>Tab</kbd> or <kbd>M</kbd> / <kbd>Esc</kbd> | Back / Start (pause) |
| <kbd>Shift</kbd>+arrows | D-pad |

Keys can be rebound in **F6 > CONTROLS**, and **RESET KEYS** returns to this layout. An install from before this layout keeps the keys it had until then.

## Graphics presets

| Preset | Resolution | MSAA | Output | Rate | Notes |
| --- | --- | --- | --- | ---: | --- |
| Low-spec 60 | 1x | Off | Bilinear | 60 | The default for a new install. The game's own texture filtering, no FXAA. Needs 4 CPU cores and about 1.1 GB of graphics memory |
| Balanced 40 | 1x | Off | Bilinear | 40 | As Low-spec 60 at a third of the CPU work of 120. Paces evenly on 120 Hz and 40 Hz displays. Recommended below 4 CPU cores |
| Performance 120 | 1x | Off | FSR 1 | 120 | The default on a discrete GPU with 6 GB or more, 6 or more CPU threads and a 120 Hz display |
| Quality 60 | 2x | 4x | Bilinear | 60 | |

1x is the console's 1280 × 720; up to 4x is available. Output scaling is bilinear, CAS or FSR 1. Ultrawide displays get a wider field of view with the HUD kept at 16:9. Bloom, motion blur and depth of field are optional. **TEXTURE GAMMA > FAST** makes terrain and foliage a little darker and saves about 8 % of the GPU's frame on a Mac; it makes no difference on a desktop GPU. On Linux, **SCENE SHADING > COARSE** shades the scene once per 2 × 2 pixels: a little softer, and about 8 % of the GPU's frame saved on a Steam Deck.

## HDR

On an HDR display, turn on HDR in the system first (on Windows, **Settings > Display > Use HDR**), then **F6 > DISPLAY > HDR**. **HDR PAPER WHITE** sets the brightness of white menus and the HUD, **HDR HIGHLIGHTS** how far bright highlights reach, and **HDR CALIBRATION** sets both from test patterns. **HDR COMPARE** shows SDR and HDR side by side. Every HDR setting applies at once. HDR is tested on Windows and on Android (AYN Odin 2 Portal); on Linux and macOS it's offered when the driver provides an HDR output. On Android, HDR appears only on displays that report HDR10, and the brightness slider doesn't change the HDR image: set **HDR PAPER WHITE** to match the menus to your usual brightness. Run **HDR CALIBRATION** once: the display reports about 420 nits on the Odin 2 Portal, and the peak square shows where it really clips.

## Immersive camera

**F6 > DISPLAY > IMMERSIVE CAMERA** makes the camera react to the car: it is pushed back under acceleration and thrown forward under braking, jolts when the throttle or brake goes on, leans into corners, shakes a little at speed and under hard braking, and moves with slow breathing and head sway in the cockpit. The cockpit view gets the full effect, the hood view a little less, and the chase and bumper views only the G-forces. Choose OFF, SUBTLE, DEFAULT or STRONG, or set G-force, lean, speed shake and head motion on their own rows (CUSTOM). Changes apply at once and keep your own save. The immersive camera isn't available on Android or with the v4 title update yet.

## Your music

**F6 > AUDIO > YOUR MUSIC** plays your own MP3 and WAV files instead of the game's radio once you are in the festival, as a soundtrack from the Xbox 360 dashboard did: the game silences its radio by itself and keeps its effects. The title screen keeps the game's own music. Put the files in the `music` folder of the install's state folder (`%LOCALAPPDATA%\PinyonShift\source\<version>\.local\preview\music`, created when you turn it on), in subfolders if you like; they play in name order or shuffled. The AUDIO page also has MUSIC VOLUME, SHUFFLE, NEXT TRACK and PREVIOUS TRACK and shows what is playing; <kbd>F9</kbd> and <kbd>Shift</kbd>+<kbd>F9</kbd> skip forward and back. On macOS and ARM builds only WAV files play for now.

## Custom radio stations

The launcher's **Radio** button turns your music into radio stations. Each one takes the place of Horizon Bass Arena, Pulse or Rocks, so you can have up to three. Give a station a name, add music folders or songs (any format FFmpeg reads), and optionally a PNG logo, which is fitted into 210 by 100 pixels for the station popup. **Keep the station's own songs too** adds your music to the station's playlist instead of replacing it. **Build stations** encodes the music and turns the stations on from the next start; the name shows in the game's radio options and loading tips in every language. Building needs [FFmpeg](https://ffmpeg.org/download.html) (on Windows, `winget install Gyan.FFmpeg`); **Locate FFmpeg** points the launcher at it. The stations copy the game's 680 MB radio bank, so they need that much disk space, and your music files are never changed. Deleting a station brings the original back after the next build.

## Language

The game runs in any of the disc's 18 languages. Press <kbd>F6</kbd>, open **Profile > Language** and choose a language and region with Left and Right, then restart the game.

## Controllers

Connect the controller before starting the game. XInput controllers work as they are; DirectInput devices use SDL's mappings. If one isn't recognized, or its buttons are in the wrong places, open **F6 > CONTROLS > MAP CONTROLLER**, choose it, then **MAP BUTTONS** and press each control it asks for; a control the controller lacks is skipped after a few seconds. Then try the controller: hold **Start** to save the mapping or **Back** to start over. It is kept in `config/controller_mappings.txt`, and **RESET TO DEFAULT MAPPING** removes it. If a controller doesn't show up at all, close Steam or other remapping software, reconnect it and relaunch.

Racing wheels are experimental and not yet qualified on hardware ([#337](https://github.com/arcanite24/pinyon-shift/issues/337)). A wheel base reaches the game as an Xbox 360 wheel with force feedback, the way the console reported one, instead of as a controller: steering on the wheel, throttle and brake on the pedals. The base is found on its own when SDL knows it as a wheel; otherwise set `wheel_device` to its USB IDs (`vvvv:pppp`) or part of its name. Pedals and buttons are set in `config` with `wheel_throttle`, `wheel_brake`, `wheel_clutch` and `wheel_buttons`; a pedal set on its own USB cable is named before a `|`, as in `wheel_brake = "SimJack|axis1"`. Turn on `wheel_log_inputs` and the log lists each axis and button number as you move it. `PinyonWheelSetup` (built from `tools/wheel-setup`) does this for you: it asks you to turn the wheel, press each pedal and each button on any device, including an EmuWheel/vJoy device, and writes the `wheel_*` keys into `pinyon_shift.toml`. When several devices share a name and USB ID it names them by device path (`path:...`), `wheel_ffb_device` sends force feedback to another device than the steering one, and other gamepads are hidden while a wheel is connected (`wheel_hide_gamepads`). The clutch pedal presses LB, the game's clutch. **F6 > CONTROLS** sets how the game's force feedback is read (right minus left motor, one motor alone, rumble or off), its strength, its direction, steering sensitivity and a centring spring. Force feedback starts at 25% because how the game's two motor values map onto a wheel is not yet traced; keep it low on a direct-drive base. **RECORD WHEEL FFB** (`wheel_ffb_trace`) writes every force feedback value the game sends, with your steering and pedals, to `ffb-trace-<time>.csv` beside the runtime log; attaching one to [#337](https://github.com/arcanite24/pinyon-shift/issues/337) helps settle the mapping. Keep **RUMBLE** at 100%: it scales wheel forces too.

## Saves

Saves are in the install's state folder, `%LOCALAPPDATA%\PinyonShift\source\<version>\.local\preview\user` (under `data` for a portable install).

- **Backups.** After each save, the save files are copied to `backups\saves`. The last 10 are kept. Restore one from **SETTINGS > PROFILE > SAVE BACKUPS**.
- **Photos.** <kbd>F8</kbd> writes PNGs to the `photos` folder in the state folder.
- **Saves from Xenia.** A Xenia save is signed with its profile's XUID, so copying it over doesn't work. Import it with the tool from a repository checkout:

```powershell
.\tools\import-xenia-save.ps1 -StateRoot .local\preview -Source 'C:\Xenia\content\E030000012345678\4D5309C9'
```

The previous Pinyon Shift save is kept; `-Restore` switches back to it.

## Treasure Map

The Treasure Map add-on was sold through a service that no longer exists. It's included and on by default. Turn it off in the launcher's **Settings**. Once a save's map is revealed it stays revealed.
