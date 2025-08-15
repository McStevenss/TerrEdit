#pragma once
#include <SDL2/SDL.h>
// #include <GL/glew.h>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cstdio>
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <limits>
#include <cmath>
#include <Shader.hpp>
#include "Terrain.hpp"
#include "Camera.hpp"
//ImGui + SDL
#include <SDL2/SDL.h>
#include "imgui/imgui.h"
#include "imgui/imgui_impl_sdl2.h"
#include "imgui/imgui_impl_opengl3.h"


class Engine {

    public:
        Engine();
        
        void Initialize();
        void Start();
        
    private:
        
        Camera cam;
        void buildCircle(std::vector<glm::vec3>& out, float radius, int segments=64);
        void GenCircleGL();
        void HandleInput(float dt);
        void CreateFrameBuffer();
        void RescaleFramebuffer(float width, float height);
        void BindFramebuffer();
        void UnbindFramebuffer();
        ImVec2 RenderGUI();
        Shader* heightMapShader;
        Shader* heightMapColorShader;
        Terrain* terrain;
        Brush brush;

        int ScreenWidth=1920;
        int ScreenHeight=1080;
        SDL_Window* win;
        SDL_GLContext glctx;

        GLuint ringVBO=0;
        GLuint ringVAO=0;
        std::vector<glm::vec3> ringVerts;
        
        bool running=true;
        float aspect=ScreenWidth/ScreenHeight;
        bool wire=false;
        bool rmb=false; 
        bool lmb=false; 
        bool mmb=false; 
        bool shift=false;
        bool flatshade=false;
        bool projectCircle=true;

        float EditorWindowWidth;
        float EditorWindowHeight;

        int mx=0,my=0;
        // ------------ Config ------------
        const int   GRID_SIZE   = 128;          // 128x128 height samples
        const float TILE_SIZE   = 533.333f;     // WoW ADT ~533.333m, optional
        const float CELL_SIZE   = TILE_SIZE / (GRID_SIZE - 1);

        GLuint FBO = 0;
        GLuint texture_id = 0;
        GLuint RBO = 0;

};

