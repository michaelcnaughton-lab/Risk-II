# Risk map: colored troop placement

C++17 / SDL2 desktop prototype using the original muted atlas artwork and its
42 territory masks. Includes the slow hover glow, persistent selection highlight,
six army colors, raised numbered medallions, placement controls, undo and saves.
This is a placement editor; turn rules, combat and movement are not implemented.

## Build and run on macOS

In Terminal, enter this extracted RiskMapCpp folder:

```sh
brew install sdl2
bash build.sh
./risk-map
```

Install Apple's compiler tools with `xcode-select --install` if necessary.
Equivalent direct compiler command (run in this folder):

```sh
g++ -std=c++17 -O2 $(sdl2-config --cflags) main.cpp -o risk-map $(sdl2-config --libs)
```

On Debian/Ubuntu install `g++` and `libsdl2-dev`, then use the same build script.
SDL2 is the only nonstandard dependency. No SDL_ttf, SDL_image or Python needed.
Optional CMake configuration is also included.

## Place troops

1. Choose red, blue, green, gold, purple or black in the palette (keys 1–6).
2. Set **Troops per click** using the minus/plus buttons (or keyboard -/+).
3. Left-click a territory to select it and add that many troops.
4. The raised colored medallion shows that territory's total troop count.

The **Add Troops** button adds to the selected territory. Right-click a territory
or use **Remove Troops** to subtract the current batch. Counts never go below zero;
the medallion disappears when the army reaches zero. Maximum: 9,999 per territory,
with placement batches from 1 to 999.

Each territory holds one army color. Clicking a territory held by a different
color selects it without adding or recoloring. Use **Set Color** to deliberately
change its color while keeping its count, then add troops normally.

- **Clear Territory**, Delete or Backspace: remove all troops from the selection.
- **Z**: undo the last troop edit (up to 100 edits in the current session).
- **S**: save. Placements also save when you close the window or press Q.
- **Escape** or left-click ocean: clear the selection without changing armies.
- **Q**: quit.

Only troop edits enter undo history. Palette and batch choices do not.
The current territory stays lightened; hovering another slowly pulses it over
2.8 seconds. Medallions are also clickable. The map retains its aspect ratio
when resized, with HiDPI mouse coordinates converted to renderer pixels.

## Saved placements

The program restores armies at startup. SDL supplies a user-writable preferences
directory; the save file is `troops.txt`, under the `RiskMapCpp/RiskMap` application
preferences folder (normally `~/Library/Application Support/RiskMapCpp/RiskMap/`
on macOS). The first run starts empty. Remove that file while the app is closed
to start a fresh board. Writes use a temporary file before replacing the save.
Malformed saves are rejected with an error instead of partially applying them.
Undo history, current palette color, selection and batch size are not saved.

## Files

- `main.cpp`: SDL rendering, mouse/keyboard controls, sidebar and integration tests.
- `Troops.hpp`: army state, editing, undo, save/load validation and count limits.
- `Drawing.hpp`: medallion shading and built-in text, requiring no extra assets.
- `MapInteraction.hpp`: hover pulse, selection and coordinate mapping.
- `assets/map.bmp`: original approved map artwork, unchanged.
- `assets/territories.bmp`: original grayscale mask, unchanged; 0 is ocean, 1–42
  correspond to the names in `assets/territories.txt`.
- `preview.png`: illustrative rendered placements (not preloaded into your game).

Anchors are calculated inside each territory mask, away from its borders.
The inherited mask is an initial fit to the raster artwork; fine coastline and
border details may still need refinement. It is not a game adjacency graph.

## Verification

```sh
SDL_VIDEODRIVER=dummy ./risk-map --self-test
SDL_VIDEODRIVER=dummy ./risk-map --snapshot troops.bmp
```

The self-test exercises all 42 masks and medallion anchors; hover timing;
selection; actual mouse/keyboard events for adding, removing, recoloring,
palette and batch controls; undo; count limits; save/load and malformed-save
rejection; resizing and SDL rendering. Test and snapshot modes do not read or
write your normal saved game. `--assets /path/to/assets` overrides asset discovery.

Compiled with warnings enabled and verified with SDL2's Linux software renderer.
Native macOS GUI execution was not available in the build environment.
