# OpenXR Simulator (OpenXRSim) for Unreal Engine

Vendor-agnostic XR simulation plugin for Unreal Engine 5.5+.

Run XR gameplay on desktop without a physical headset:
- Simulated HMD + controllers
- Stereo rendering path
- Keyboard/mouse-driven tracking and actions
- Xbox/XInput-driven actions and optional pose driving
- UDP data-forwarding path for external trackers/controllers
- Built-in room + JSON room loading
- Record/replay timelines for repeatable testing

## 1) Supported Versions and Platforms

- Engine: Unreal Engine 5.5+ (tested against public UE 5.5/5.6 API surface)
- Primary platform: Windows desktop (input polling path)
- Editor + Runtime modules included
- No engine source modifications required

## 2) What This Plugin Is (and Is Not)

OpenXRSim is an **engine-level XR simulator** using Unreal XR interfaces (`IHeadMountedDisplayModule`, `IXRTrackingSystem`, `IStereoRendering`, `IInputDevice`).

It is **not** a full desktop OpenXR runtime implementation.  
It does not currently emulate the full OpenXR loader/runtime boundary (`xrCreateInstance`, swapchain lifecycle, runtime extensions) that vendor runtime simulators provide.

## 3) Features

- HMD simulation
  - Tracking pose for device id `0` (HMD)
  - Reset origin support
  - Stereo eye offsets via configurable IPD
- Controller simulation
  - Left and right tracked controller poses
  - Trigger/grip/thumbstick analog injection using MotionController keys
  - Face buttons + thumbstick-click injection
  - XInput mapping (LT/LB/LStick -> Left, RT/RB/RStick -> Right)
- Room simulation
  - Built-in procedural hero room
  - JSON room loading from file
  - Optional debug bounds + anchor debug draw
- Deterministic playback tools
  - JSON record output
  - JSON replay input
  - Fixed timestep replay path
  - Optional auto-replay on world start
- Editor tooling
  - Toolbar toggle (`OpenXR Sim`)
  - Panel (`Window -> OpenXR Simulator`) for room/record/replay controls
  - Runtime toggles for XInput and UDP forwarding

## 4) Installation

1. Copy plugin folder into your project:
   - `YourProject/Plugins/OpenXRSim/`
2. Open project in Unreal Editor.
3. Enable plugin:
   - `Edit -> Plugins -> XR -> OpenXR Simulator`
4. Restart editor when prompted.

## 5) Initial Setup

1. Open panel:
   - `Window -> OpenXR Simulator`
2. Click toolbar toggle:
   - `OpenXR Sim` (ACTIVE)
3. Initialize room:
   - `Load Built-in Room` (or JSON room)
4. Start `VR Preview` or `PIE`.

If another HMD plugin is selected first, set plugin priority in `DefaultEngine.ini`:

```ini
[HMDPluginPriority]
OpenXRSim=100
OpenXRHMD=20
OculusHMD=20
SteamVR=20
```

Recommended plugin setting for startup automation:

- `OpenXR Simulator` settings (`Project Settings -> Plugins -> OpenXR Simulator`):
  - `bLoadRoomOnWorldStart = true`
  - `bPreferBuiltInRoomOnWorldStart = true` (or set `DefaultRoomJsonRelative`)
  - `bAutoReplayOnWorldStart = true` with `AutoReplayPath` for deterministic test flows

## 6) Desktop Controls (Default)

- Device select:
  - `1` = HMD
  - `2` = Left controller
  - `3` = Right controller
- Pose driving:
  - Hold `Left Alt` + `W/A/S/D/Q/E` to translate selected device
  - Hold `Left Alt` + mouse move to rotate selected device
- Controller actions (when active device is Left or Right):
  - `LMB` -> Trigger
  - `RMB` -> Grip
  - Arrow keys -> Thumbstick (`X/Y`)
  - `X` -> Primary button
  - `C` -> Secondary button
  - `V` -> Thumbstick click

Gamepad mappings (XInput):
- Left side drives left controller action state:
  - `LT` trigger, `LB` grip, `LStick` thumbstick, `X/Y` primary/secondary, `L3` stick click
- Right side drives right controller action state:
  - `RT` trigger, `RB` grip, `RStick` thumbstick, `A/B` primary/secondary, `R3` stick click
- Optional pose driving:
  - when `bGamepadDrivesActiveDevicePose = true`, sticks move/rotate selected device

## 7) Record / Replay

### Record

1. Open panel -> `Start Record`.
2. Perform simulated movement/actions.
3. Click `Stop Record`.
4. Output path:
   - `Saved/OpenXRSim/Replays/recording.json`

### Replay

1. Click `Start Replay` to run example replay from plugin content.
2. Click `Stop Replay` to cancel.
3. Replay drives simulated poses/actions and overrides live input.

### Auto Replay

Set:
- `bAutoReplayOnWorldStart = true`
- `AutoReplayPath = <relative-or-absolute-path>`

Path resolution order:
1. Absolute path
2. Project-relative path (`ProjectDir`)
3. Plugin-relative path (`Plugins/OpenXRSim`)

## 8) Room System

### Built-in Room

- Procedural 4m x 4m x 2.5m room plus simple furniture boxes.

### JSON Room Input

Example file:
- `Plugins/OpenXRSim/Content/Rooms/Example_CustomRoom.json`

Minimal structure:

```json
{
  "version": 1,
  "name": "Example_CustomRoom",
  "bounds": { "min": [-250, -150, 0], "max": [250, 150, 260] },
  "boxes": [
    { "id": "table", "center": [80, 0, 40], "size": [120, 60, 80] }
  ],
  "planes": [
    { "id": "floor", "type": "floor", "center": [0, 0, 0], "normal": [0, 0, 1], "extents": [500, 300] }
  ]
}
```

Anchors are supported:

```json
{
  "anchors": [
    { "id": "spawn", "pose": { "p": [0, 0, 0], "q": [0, 0, 0, 1] } }
  ]
}
```

## 9) Multiplayer / Multi-Client Testing

For two editor clients with simulated XR:

1. `Play -> Advanced Settings`
2. Set:
   - `Run Under One Process = false`
   - `HMD For Primary Process Only = false`
3. Set `Number of Players = 2`
4. Launch multi-process PIE.

Each process gets an independent simulator state.

## 10) Data Forwarding (UDP JSON)

Enable forwarding:
- `bEnableDataForwarding = true`
- `DataForwardBindAddress` (default `127.0.0.1`)
- `DataForwardPort` (default `7779`)

Supported packet formats:

1. Full state packet:

```json
{
  "hmd": { "p": [0,0,170], "q": [0,0,0,1] },
  "left": {
    "gripPose": { "p": [-20,20,140], "q": [0,0,0,1] },
    "aimPose":  { "p": [-20,20,140], "q": [0,0,0,1] },
    "trigger": 0.2,
    "grip": 0.1,
    "stick": [0.0, 1.0],
    "primary": false,
    "secondary": false,
    "stickClick": false
  },
  "right": {
    "gripPose": { "p": [20,20,140], "q": [0,0,0,1] },
    "aimPose":  { "p": [20,20,140], "q": [0,0,0,1] },
    "trigger": 0.9,
    "grip": 0.0,
    "stick": [0.2, 0.0],
    "primary": true,
    "secondary": false,
    "stickClick": false
  }
}
```

2. Single-device packet:

```json
{
  "device": "left",
  "pose": { "p": [-20,20,140], "q": [0,0,0,1] },
  "trigger": 1.0,
  "grip": 0.0,
  "stick": [0.0, 0.0]
}
```

## 11) Demo Pawn and Input Validation

Use `AOpenXRSimDemoPawn` to sanity-check injected MotionController axes.

File:
- `Source/OpenXRSim/Public/OpenXRSimDemoPawn.h`

Behavior:
- Logs trigger activity when trigger axis exceeds threshold.

## 12) Automation

Included test:
- `OpenXRSim.Replay.LoadsAndRuns`

File:
- `Source/OpenXRSim/Private/Tests/OpenXRSimAutomationTest.cpp`

Use Session Frontend -> Automation to execute.

## 13) Configuration (`UOpenXRSimSettings`)

Settings class:
- `Source/OpenXRSim/Public/OpenXRSimSettings.h`

Key options:
- `bEnableSimulator`
- `bRequireAltForPoseControl`
- `bEnableXInputGamepad`
- `bGamepadDrivesActiveDevicePose`
- translation/rotation speed
- replay fixed timestep
- `bAutoReplayOnWorldStart` / `AutoReplayPath`
- `bEnableDataForwarding` / bind address / port
- IPD/FOV/render target hints
- default room/replay paths
- active device index

## 14) Known Limitations

- Not a full OpenXR runtime boundary simulator.
- No hand/body tracking emulation in current build.
- No controller forwarding from physical devices in current build.
- No swapchain/runtime extension fidelity checks.

## 15) Troubleshooting

- No stereo / wrong XR system selected:
  - Ensure `OpenXR Sim` toggle is active.
  - Raise `OpenXRSim` in `[HMDPluginPriority]`.
  - Temporarily disable other XR plugins during validation.
- No input response:
  - Keep viewport focused.
  - Hold `Left Alt` for pose driving.
  - Select controlled device with `1/2/3`.
- Replay does not start:
  - Check replay JSON path exists.
  - Ensure recording/replay are not active at same time.
- Forwarding not received:
  - Verify sender targets UDP `DataForwardBindAddress:DataForwardPort`.
  - Ensure payload is JSON and includes required fields.
  - Confirm firewall permits local UDP traffic.

## 16) Technical References

- Unreal `IHeadMountedDisplayModule` API  
  https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/HeadMountedDisplay/IHeadMountedDisplayModule
- Unreal `IXRTrackingSystem` API  
  https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/HeadMountedDisplay/IXRTrackingSystem
- Unreal `IInputDevice` API  
  https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/InputDevice/IInputDevice
- OpenXR loader and API-layer model  
  https://registry.khronos.org/OpenXR/specs/1.0/loader.html

## 17) Packaging and Deployment Checklist

1. Build plugin in host project (Development Editor + Development target).
2. Validate runtime:
   - VR Preview launches without headset
   - HMD/Left/Right tracking movement works
   - Trigger/grip/thumbstick + face buttons reach gameplay input
3. Validate rooms:
   - Built-in room load
   - JSON room load
   - anchor debug markers visible
4. Validate deterministic flow:
   - record -> replay round-trip
   - optional auto-replay on world start
5. Validate multi-client:
   - multi-process PIE with 2 clients
6. Validate forwarding (if used):
   - UDP sender drives pose/actions
7. Package plugin:
   - `Plugins -> Package...` in Editor, or use Unreal Automation Tool plugin packaging workflow.
8. Ship with docs + sample assets:
   - `Content/Rooms/*`
   - `Content/Replays/*`
   - this `README.md`
