<p align="center">
  <img src="3ds_port/assets/artwork/logo.png" alt="Pokémon FireLeaf 3DS Dual Screen" width="480">
</p>

<h1 align="center">Pokémon FireLeaf 3DS Dual Screen</h1>

<p align="center">
  <strong>Return to Kanto. Two screens. A new perspective.</strong><br>
  A native Nintendo 3DS port with a dedicated touch interface and a full Voxel 3D overworld.
</p>

---

Pokémon FireLeaf 3DS brings the classic GBA adventure to the Nintendo 3DS as **native homebrew** (no emulation involved). The game runs smoothly on the top screen, while the bottom display takes over the START menu logic, introducing fully reconstructed, Citro2D-powered versions of the **Fame Checker**, **Teachy TV**, and an interactive help system.

## Features & Innovations

| Feature | Description |
| :--- | :--- |
| **Native ARM11 Code** | The game directly utilizes the 3DS hardware through `libctru`, `citro2d`, and `citro3d`. Zero emulation overhead. |
| **True Multiplayer (UDS)** | Unlike standard GBA ports, FireLeaf features a custom wireless bridge (`3ds_wireless.c`) that translates the GBA Wireless Adapter protocol into **native 3DS Local WLAN (UDS)**. Trade and battle without cables! |
| **Exclusive DualScreen UI** | FireRed-specific interfaces (Fame Checker, Teachy TV) have been migrated to the bottom touch screen and completely rebuilt with responsive, touch-friendly UI. |
| **Kanto in 3D (Voxel Engine)** | Optional 3D mode. Key iconic buildings like the Pokémon Tower, Celadon Dept. Store, Silph Co., and Giovanni's Gym have been meticulously reconstructed in 3D space. |
| **Automatic CIA Forwarder** | The build system generates a dedicated `.cia` application (featuring FireRed or LeafGreen themes) deposited into the `dist/` folder, ready for HOME menu installation via FBI. |

## Installation & Compilation (Build from your own ROM)

As a decompilation port, this project is 100% legal and **contains no copyrighted game code or assets**. To run the project, you must provide your own clean dump of **Pokémon FireRed (U) 1.0 (Squirrels)**.

Our custom Asset Builder will dynamically extract textures, map layouts, and Sappy Audio music directly from your ROM file.

### How to compile on MacOS / Linux

Requirements: **devkitARM** (via devkitPro), `libctru`, `makerom`, `bannertool`, and Python 3.

```sh
# Clone the repository and navigate to the 3ds_port directory
cd 3ds_port

# Run the build process to integrate GBA code with the 3DS backend and generate the forwarder
make release
```

During compilation, the Python scripts (located in the `builder/` directory) will validate your `.gba` file's SHA1 hash, extract MIDI music, Pokémon cries, and 3D tiles, and pack everything into a format readable by the 3DS. The resulting `.cia` and `.3dsx` files will be placed in the `3ds_port/dist/` directory.

## Kanto in Voxel 3D

Our Voxel Engine has been overhauled to correctly parse raw `.map` files and layout structures specific to the Kanto region.

The 3D mode is **off by default**. Open the **OPTIONS** menu on the bottom touch screen and enable **VOXEL 3D** to adjust the camera angle and zoom using the C-Stick, sliders, and D-pad.

## Project Architecture

| Component | Location | Role |
| :--- | :--- | :--- |
| **3DS Backend** | `3ds_port/` | ARM11 code, Citro3D rendering, NDSP audio handling, and touch UI. |
| **Voxel Engine (Kanto)**| `3ds_port/src/voxel/` | 3D physics for buildings, trees, and caves (e.g., Mt. Moon, Victory Road). |
| **Asset Builder** | `builder/` | Dynamic data extractor for the GBA ROM. Includes the Sappy Audio and LZ77 decoder. |
| **Forwarder (CIA)** | `3ds_port/forwarder/` | A lightweight .cia package that dynamically loads the .3dsx executable from the SD card. |

---

## Licensing & Copyright

**Acknowledgments:** This project is a direct fork and adaptation of the brilliant `pokeemerald-3Ds-dualscreen` project by ZallaxDev (available at https://github.com/ZallaxDev/pokeemerald-3Ds-dualscreen). The core Dual Screen engine, Voxel renderer base, and Asset Builder pipeline were originally developed for Pokémon Emerald and have been significantly overhauled here to support FireRed/LeafGreen.

The custom backend code for this port (Voxel engines, 3DS hardware integration, UDS Multiplayer) is covered by the project's original license, with adaptations specifically for FireLeaf.

Pokémon FireLeaf 3DS Dual Screen is an unofficial fan project (homebrew) created for hobbyist purposes. This project is in no way affiliated with, sponsored, or endorsed by Nintendo, Game Freak, Creatures Inc., or The Pokémon Company. "Pokémon" and "Pokémon FireRed" are registered trademarks of their respective legal owners. This repository is intended strictly for educational and research purposes.
