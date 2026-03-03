# OpenXRSim Architecture

Audience: intermediate Unreal + XR developers who want to understand how this plugin works end-to-end.

## 1) Big Picture

OpenXRSim is an **Unreal XR-system simulator plugin**.

It plugs into Unreal XR interfaces and simulates:
- HMD tracking
- Controller tracking + input
- Stereo rendering setup
- Room/environment primitives
- Record/replay timelines
- Optional forwarded input over UDP

It is not a full desktop OpenXR runtime replacement at loader/runtime `xr*` boundary.

## 2) Module Layout

Plugin root:
- `OpenXRSim` (Runtime module)
- `OpenXRSimEditor` (Editor-only module)

Runtime owns gameplay/runtime behavior.
Editor owns toolbar, panel, and config convenience UI.

## 3) Core Runtime Components

### `FOpenXRSimModule`
Files:
- `Source/OpenXRSim/Public/OpenXRSimModule.h`
- `Source/OpenXRSim/Private/OpenXRSimModule.cpp`

Role:
- Runtime module entry point.
- Implements:
  - `IHeadMountedDisplayModule`
  - `IInputDeviceModule`
- Creates shared simulator objects:
  - `FOpenXRSimState` (state core)
  - `FOpenXRSimHMD` (XR/HMD implementation)
  - `FOpenXRSimInputDevice` (input injector)

### `FOpenXRSimState`
Files:
- `Source/OpenXRSim/Public/OpenXRSimState.h`
- `Source/OpenXRSim/Private/OpenXRSimState.cpp`

Role:
- Single source of truth for simulator data.
- Thread-safe state container:
  - HMD pose
  - Left/right controller state
  - active room name
  - record/replay buffers and timing
  - diagnostics (sim time, instance label)

Concurrency:
- Protected by `FCriticalSection`.
- Writes mainly from game-thread-side update paths.
- Read by XR queries/input publishing.

### `FOpenXRSimHMD`
Files:
- `Source/OpenXRSim/Public/OpenXRSimHMD.h`
- `Source/OpenXRSim/Private/OpenXRSimHMD.cpp`

Role:
- Unreal XR interface implementation for tracking + stereo.
- Uses `FHeadMountedDisplayBase` + `FSceneViewExtensionBase`.
- Provides:
  - tracked devices enumeration
  - current device poses
  - eye-relative offsets (IPD)
  - stereo view/projection configuration
  - motion controller data query bridge
  - world-start actions (auto room load + optional auto replay)

### `FOpenXRSimInputDevice`
Files:
- `Source/OpenXRSim/Public/OpenXRSimInputDevice.h`
- `Source/OpenXRSim/Private/OpenXRSimInputDevice.cpp`

Role:
- Polls live desktop input.
- Updates sim state.
- Injects simulated XR keys via Unreal input message handler.

Input paths:
- Keyboard/mouse (pose + actions)
- XInput gamepad (actions + optional pose driving)
- UDP forwarding receiver bridge

### `FOpenXRSimForwardReceiver`
Files:
- `Source/OpenXRSim/Private/OpenXRSimForwardReceiver.h`
- `Source/OpenXRSim/Private/OpenXRSimForwardReceiver.cpp`

Role:
- Optional UDP JSON receiver.
- Parses external tracking/action packets.
- Applies incoming values into `FOpenXRSimState`.

### `UOpenXRSimRoomSubsystem`
Files:
- `Source/OpenXRSim/Public/OpenXRSimRoomSubsystem.h`
- `Source/OpenXRSim/Private/OpenXRSimRoomSubsystem.cpp`

Role:
- World subsystem that manages room loading.
- Supports:
  - built-in room
  - JSON room loading
  - startup default room policy
- Spawns/manages one room actor per world.

### `AOpenXRSimRoomActor`
Files:
- `Source/OpenXRSim/Public/OpenXRSimRoomActor.h`
- `Source/OpenXRSim/Private/OpenXRSimRoomActor.cpp`

Role:
- Renders simple room geometry from primitives.
- Builds:
  - built-in hero room
  - JSON boxes/planes
  - anchor debug visuals

### `UOpenXRSimSettings`
Files:
- `Source/OpenXRSim/Public/OpenXRSimSettings.h`

Role:
- Project-level config surface in Unreal Project Settings.
- Controls toggles/speeds/startup/replay/forwarding.

## 4) Editor Components

### `FOpenXRSimEditorModule`
Files:
- `Source/OpenXRSimEditor/Public/OpenXRSimEditorModule.h`
- `Source/OpenXRSimEditor/Private/OpenXRSimEditorModule.cpp`

Role:
- Registers menu + toolbar toggle.
- Registers simulator panel tab.
- Toggle path updates settings and calls `GEngine->InitializeHMDDevice()`.

### `SOpenXRSimPanel`
Files:
- `Source/OpenXRSimEditor/Private/SOpenXRSimPanel.h`
- `Source/OpenXRSimEditor/Private/SOpenXRSimPanel.cpp`

Role:
- Operator UI in editor.
- Actions:
  - toggle live options (XInput, UDP forwarding)
  - load rooms
  - reset origin
  - start/stop record
  - start/stop replay
- Shows live status summary:
  - room, rec/replay state, sim time, instance label

## 5) Runtime Data Flow

```text
Desktop Input / UDP Feed
        |
        v
FOpenXRSimInputDevice + FOpenXRSimForwardReceiver
        |
        v
FOpenXRSimState (thread-safe source of truth)
        |
        +--> FOpenXRSimHMD (tracking + stereo + view data)
        |
        +--> Unreal Input Message Handler (MotionController keys)
        |
        +--> Replay Recorder/Player
```

## 6) Lifecycle Flow

### Startup
1. Unreal loads `OpenXRSim` module.
2. Module registers HMD/input modular features.
3. If simulator enabled, module creates HMD + input device + shared state.
4. On first game-world frame:
   - optional startup room load
   - optional startup replay begin

### Per-frame
1. `FOpenXRSimInputDevice::SendControllerEvents()`:
   - poll keyboard/mouse/gamepad
   - poll forwarding socket (if enabled)
   - update sim state
   - inject XR keys
2. `FOpenXRSimHMD::OnStartGameFrame()`:
   - tick sim state (record/replay timing)
   - provide poses/stereo data for rendering

### Shutdown
1. Module unregisters features.
2. Input receiver/socket objects are destroyed.
3. shared state/HMD/input references released.

## 7) Record/Replay Architecture

State stores frame timeline snapshots:
- time
- hmd pose
- left/right controller state

Record:
- fixed-step accumulation
- writes JSON to replay file

Replay:
- loads JSON frames
- advances timeline
- applies frame state back into live state
- live input updates are suppressed while replay active

## 8) Room Pipeline

Sources:
- built-in procedural room
- JSON room file

Room subsystem:
- ensures `AOpenXRSimRoomActor` exists
- asks actor to rebuild geometry
- updates active room name in sim state

Room actor:
- uses instanced cube meshes for cheap geometry
- draws bounds/anchors for debug visibility

## 9) Multiplayer / Multi-Instance

Model:
- each process keeps its own `FOpenXRSimState`
- intended use: multi-process PIE
- state exposes `InstanceLabel` for easier diagnostics

Recommendation:
- disable single-process PIE for realistic independent clients.

## 10) Threading + Safety Notes

- `FOpenXRSimState` uses mutex lock for reads/writes.
- Keep heavy work outside lock where possible.
- UDP parsing currently runs inside input device tick path; keep packet rates reasonable.

## 11) Input Compatibility Strategy

This plugin injects standard Unreal motion-controller keys:
- trigger/grip axes
- thumbstick axes
- trigger/grip/face-button/stick-click digital keys

Why:
- broad compatibility with Unreal XR gameplay code expecting MotionController keys.
- projects with custom Enhanced Input setups may still need mapping assets.

## 12) Configuration Surface (Important)

Main groups:
- General: enable simulator
- Input:
  - alt-capture behavior
  - movement/rotation speeds
  - xinput toggles
  - data forwarding endpoint
- Stereo:
  - IPD/FOV/ideal render target hints
- Rooms:
  - startup load policy
  - default room path
- Replay:
  - fixed dt
  - auto replay on world start
  - replay directory

## 13) Test Surface

Included smoke test:
- `OpenXRSim.Replay.LoadsAndRuns`
  - validates replay startup + stop lifecycle

Manual verification:
- VR preview without headset
- movement + action injection
- room load paths
- replay determinism
- multi-client behavior
- UDP forwarding path

## 14) Extension Points

Natural next features:
- hand/body tracking state and skeleton outputs
- richer room semantics (labels, nav hints, occlusion data)
- recorded session metadata/assertions for CI
- abstraction layer for non-Windows live input polling
- OpenXR runtime boundary implementation (future separate track)

## 15) Quick “Where Do I Edit?” Guide

- Change startup behavior:
  - `OpenXRSimHMD.cpp` (`ApplyWorldStartupActions`)
- Change input mappings:
  - `OpenXRSimInputDevice.cpp`
- Change forwarding packet support:
  - `OpenXRSimForwardReceiver.cpp`
- Change replay format/logic:
  - `OpenXRSimState.cpp`
- Change room rendering/parsing:
  - `OpenXRSimRoomActor.cpp`, `OpenXRSimRoomSubsystem.cpp`
- Change editor tooling:
  - `SOpenXRSimPanel.cpp`, `OpenXRSimEditorModule.cpp`

