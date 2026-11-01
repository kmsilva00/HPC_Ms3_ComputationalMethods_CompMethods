# Circuit Player (C++17 + raylib)

A small viewer for precomputed circuit measurements. It does **not** simulate electrical behavior; it plays back measurements already stored in CSV files. The bundled example is a boost-converter-style circuit with a power input, inductor, switch, diode, capacitor and load.

## Requirements

- C++17 compiler
- CMake 3.16+
- raylib 4.x or newer, installed and discoverable by CMake

## Build

```sh
cmake -S . -B build
cmake --build build
./build/circuit-player
```

Run from the project root so the relative `data/` and `assets/` paths resolve. On Windows, run `build/Debug/circuit-player.exe` or `build/Release/circuit-player.exe`, depending on your generator.

## Recording

Install FFmpeg, then create a 60 FPS MP4 without opening the interactive player:

```sh
./run.sh --record boost-converter.mp4
```

Recording uses fixed playback steps and writes the complete circuit duration.

## CSV formats

The example files in `data/` are runnable sample input.

`components.csv`
```csv
id,type,x,y
VIN,power_in,100,240
L1,inductor,300,160
J1,junction,400,160
SW1,gate,400,360
D1,diode,500,160
J2,junction,600,160
C1,capacitor,600,360
LOAD,resistor,800,240
```

`connections.csv`
```csv
from,to
VIN,L1
L1,J1
J1,D1
J1,SW1
D1,J2
J2,C1
J2,LOAD
SW1,VIN
C1,VIN
LOAD,VIN
```

`measurements.csv`
```csv
component_id,timestamp,value
P1,0.0,5.0
R1,0.0,100.0
...
```

- Timestamps are simulation seconds and may be different for each component.
- Measurements are sorted on load; duplicate timestamps are allowed, and the last row at a duplicate timestamp wins after stable sorting.
- Before a component's first sample, the displayed value is `—`.
- The sample-and-hold lookup displays the most recent sample at or before the current simulation time.
- The CSV reader supports quoted fields and commas inside quoted fields, but not multiline quoted fields.
- Component IDs and type names are case-sensitive. Supported types: `junction`, `diode`, `resistor`, `inductor`, `capacitor`, `power_in`, `gate`, `load_cell`.
- The example units are defaults only. Change them in `src/Circuit.h` to match the meaning of your data.

## Controls

- Space: play/pause
- Home: restart and pause
- Left / Right: seek backward / forward by 1 second
- Up / Down: increase / decrease playback speed
- Click timeline: seek
- Click `Play` / `Pause`: toggle playback
- Click `Reset`: restart
- Click speed label: cycle through 0.25x, 0.5x, 1x, 2x, 4x

The bundled example is a boost-converter topology: input -> inductor, with the inductor node splitting to a switch and diode, and the diode node splitting to a capacitor and resistor. The outputs of the switch, capacitor and resistor share a return bus back to the input. The player uses a dark grid background. Cyan spots travel from the source of each connection toward its destination; their speed is proportional to the source component's current voltage sample.

## Design notes

- `CircuitModel` owns components, connections and measurements.
- `CsvLoader` loads and validates input.
- `PlaybackController` owns the simulation clock and sample-and-hold lookup.
- `Renderer` owns raylib textures and drawing.
- Textures are cached by component type and unloaded on shutdown.
- Connections are drawn between approximate component centers. For actual circuit diagrams, extend `Connection` with source/target pin names or pin coordinates.
