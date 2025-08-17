//For windows needs to be defined first of all
#define SDL_MAIN_HANDLED
// #define SDL_VIDEO_DRIVER_X11 1
#define SDL_VIDEO_DRIVER_WINDOWS 1


// Mini WoW-like Terrain Editor
// SDL2 + OpenGL (Core) + GLM
// Features:
// - 128x128 heightmap tile (approx like WoW ADT resolution)
// - Brush-based sculpting: Raise/Lower, Smooth
// - Mouse picking via ray → heightmap intersection (ray marching w/ bilinear sampling)
// - Camera: WASD + QE (up/down), Right-Mouse look
// - Adjustable brush radius/strength
// - Save/Load custom .hmap (binary) format
// - Simple lit shading + optional wireframe
//
// Build notes:
//   - Requires SDL2, GLAD, GLM
//   - Compile e.g. (Linux):
//     g++ terrain_editor.cpp -std=c++17 -lSDL2 -lGLEW -lGL -O2 -o terrain_editor
//   - On Windows, link SDL2, GLEW/GLAD, OpenGL32
//
// Controls:
//   - WASD: move, QE: down/up
//   - Right Mouse: hold to look around (FPS camera)
//   - Left Mouse: sculpt (raise). Hold Shift+Left: lower. Middle Mouse: smooth
//   - Mouse Wheel: change brush radius
//   - [ / ] : change brush strength
//   - F5: Save "tile.hmap"   F9: Load "tile.hmap"
//   - F: toggle wireframe
//   - R: reset heights to 0
//
// File format (tile.hmap):
//   struct Header { char magic[4] = "HMP1"; uint32_t size; float cellSize; }
//   followed by size*size floats (row-major)

// Linux compile:
// c++ src/*.cpp lib/build/linux/*.o -I lib/include -lSDL2 -ldl -o bin/TerrEdit -O2 -DNDEBUG

// Windows compile:
//g++ src/*.cpp lib/include/imgui/*.cpp -I lib/include/ -o bin/TerrEdit.exe -lSDL2 -lopengl32 -lgdi32 -lwinmm -luser32 -mwindows -O2 -DNDEBUG

//g++ src/*.cpp lib/build/win/*.o -I lib/include/ -lSDL2 -lopengl32 -lgdi32 -lwinmm -luser32 -mwindows -O2 -o bin/TerrEdit.exe

#include "Engine.h"

// int main(int argc, char** argv){
int main(){
    Engine engine;

    engine.Start();

    return 0;
}


