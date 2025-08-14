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
//  c++ src/*.cpp -I lib/include -lSDL2 -ldl -o bin/TerrEdit -O2 -DNDEBUG

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

// ------------ Config ------------
static const int   GRID_SIZE   = 128;          // 128x128 height samples
static const float TILE_SIZE   = 533.333f;     // WoW ADT ~533.333m, optional
static const float CELL_SIZE   = TILE_SIZE / (GRID_SIZE - 1);
bool FLATSHADE = false;

// ------------ Heightmap data ------------
struct HeightMap {
    int size = GRID_SIZE;
    float cell = CELL_SIZE;
    std::vector<float> h; // size*size

    HeightMap(){ h.assign(size*size, 0.0f); }

    inline float& at(int x, int z){ return h[z*size + x]; }
    inline float  at(int x, int z) const { return h[z*size + x]; }

    bool inBounds(int x, int z) const { return x>=0 && z>=0 && x<size && z<size; }

    // Bilinear sample height at world-space XZ
    float sampleHeight(float wx, float wz) const {
        float gx = wx / cell; // grid space
        float gz = wz / cell;
        int x0 = (int)floorf(gx); int z0 = (int)floorf(gz);
        int x1 = x0 + 1; int z1 = z0 + 1;
        if(x0 < 0 || z0 < 0 || x1 >= size || z1 >= size) return 0.0f;
        float tx = gx - x0; float tz = gz - z0;
        float h00 = at(x0,z0), h10 = at(x1,z0), h01 = at(x0,z1), h11 = at(x1,z1);
        float hx0 = h00*(1-tx) + h10*tx;
        float hx1 = h01*(1-tx) + h11*tx;
        return hx0*(1-tz) + hx1*tz;
    }

    // Approximate normal from central differences
    glm::vec3 normalAt(int x, int z) const {
        auto hL = inBounds(x-1,z) ? at(x-1,z) : at(x,z);
        auto hR = inBounds(x+1,z) ? at(x+1,z) : at(x,z);
        auto hD = inBounds(x,z-1) ? at(x,z-1) : at(x,z);
        auto hU = inBounds(x,z+1) ? at(x,z+1) : at(x,z);
        glm::vec3 n = glm::normalize(glm::vec3(-(hR-hL)/(2*cell), 1.0f, -(hU-hD)/(2*cell)));
        return n;
    }
};

// ------------ Mesh (VBO/IBO) ------------
struct VertexPNUV { glm::vec3 p; glm::vec3 n; glm::vec2 uv; };
struct TerrainGL {
    GLuint vao=0, vbo=0, ibo=0; GLsizei indexCount=0; bool wire=false;
    void destroy(){ if(ibo)glDeleteBuffers(1,&ibo); if(vbo)glDeleteBuffers(1,&vbo); if(vao)glDeleteVertexArrays(1,&vao); vao=vbo=ibo=0; indexCount=0; }
};

static void buildTerrainMesh(const HeightMap& hm, TerrainGL& tg, bool flatShading = false) {
    std::vector<VertexPNUV> verts; verts.resize(hm.size*hm.size);
    for(int z=0; z<hm.size; ++z){
        for(int x=0; x<hm.size; ++x){
            VertexPNUV v{}; v.p = glm::vec3(x*hm.cell, hm.at(x,z), z*hm.cell);
            v.n = hm.normalAt(x,z);
            v.uv = glm::vec2(x/(float)(hm.size-1), z/(float)(hm.size-1));
            verts[z*hm.size + x] = v;
        }
    }
    std::vector<uint32_t> idx; idx.reserve((hm.size-1)*(hm.size-1)*6);
    for(int z=0; z<hm.size-1; ++z){
        for(int x=0; x<hm.size-1; ++x){
            uint32_t i0 = z*hm.size + x;
            uint32_t i1 = i0 + 1;
            uint32_t i2 = i0 + hm.size;
            uint32_t i3 = i2 + 1;
            // two triangles
            idx.push_back(i0); idx.push_back(i2); idx.push_back(i1);
            idx.push_back(i1); idx.push_back(i2); idx.push_back(i3);
        }
    }

    if(!tg.vao) glGenVertexArrays(1, &tg.vao);
    if(!tg.vbo) glGenBuffers(1, &tg.vbo);
    if(!tg.ibo) glGenBuffers(1, &tg.ibo);

    glBindVertexArray(tg.vao);
    glBindBuffer(GL_ARRAY_BUFFER, tg.vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(VertexPNUV), verts.data(), GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, tg.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size()*sizeof(uint32_t), idx.data(), GL_STATIC_DRAW);

    GLsizei stride = sizeof(VertexPNUV);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,stride,(void*)offsetof(VertexPNUV,p));
    glEnableVertexAttribArray(1); glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,stride,(void*)offsetof(VertexPNUV,n));
    glEnableVertexAttribArray(2); glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,stride,(void*)offsetof(VertexPNUV,uv));

    glBindVertexArray(0);
    tg.indexCount = (GLsizei)idx.size();
}




static void updateTerrainVertices(const HeightMap& hm, TerrainGL& tg) {
    // Update VBO positions and normals only (same topology)
    std::vector<VertexPNUV> verts; verts.resize(hm.size*hm.size);
    for(int z=0; z<hm.size; ++z){
        for(int x=0; x<hm.size; ++x){
            VertexPNUV v{}; v.p = glm::vec3(x*hm.cell, hm.at(x,z), z*hm.cell);
            v.n = hm.normalAt(x,z);
            v.uv = glm::vec2(x/(float)(hm.size-1), z/(float)(hm.size-1));
            verts[z*hm.size + x] = v;
        }
    }
    glBindBuffer(GL_ARRAY_BUFFER, tg.vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, verts.size()*sizeof(VertexPNUV), verts.data());
}

// ------------ Camera ------------
struct Camera {
    glm::vec3 pos = glm::vec3(TILE_SIZE*0.5f, 150.0f, -TILE_SIZE*0.2f);
    float yaw=1.00f, pitch=-0.35f; // radians
    float fov=60.0f; float nearP=0.1f, farP=2000.0f;

    glm::mat4 view() const {
        glm::vec3 f;
        f.x = cosf(pitch)*cosf(yaw);
        f.y = sinf(pitch);
        f.z = cosf(pitch)*sinf(yaw);
        return glm::lookAt(pos, pos + glm::normalize(f), glm::vec3(0,1,0));
    }
    glm::mat4 proj(float aspect) const {
        return glm::perspective(glm::radians(fov), aspect, nearP, farP);
    }
};

// ------------ Ray picking into heightmap ------------
static bool rayHeightmapIntersect(const HeightMap& hm, const glm::vec3& rayOrigin, const glm::vec3& rayDistance, float maxDist, glm::vec3& outHit) {
    // Simple ray marching along ray; test when ray.y <= height(wx,wz)
    float t = 0.0f; 
    float step = hm.cell * 0.5f; // step ~ half cell
    glm::vec3 p;

    for(int i=0; i<2048 && t <= maxDist; ++i){
        p = rayOrigin + rayDistance * t;
        if(p.x < 0 || p.z < 0 || p.x > (hm.size-1)*hm.cell || p.z > (hm.size-1)*hm.cell){ t += step; continue; }
        float h = hm.sampleHeight(p.x, p.z);
        if(p.y <= h){
            // refine with binary search
            float t0 = t - step, t1 = t;
            for(int j=0;j<8;++j){
                float tm = 0.5f*(t0+t1);
                glm::vec3 pm = rayOrigin + rayDistance*tm;
                float hmH = hm.sampleHeight(pm.x, pm.z);
                if(pm.y > hmH) t0 = tm;
                else t1 = tm;
            }
            outHit = rayOrigin + rayDistance * t1; outHit.y = hm.sampleHeight(outHit.x, outHit.z);
            return true;
        }
        t += step;
    }
    return false;
}

// ------------ Brush ops ------------
enum class BrushMode { RaiseLower, Smooth };
struct Brush { float radius=6.0f; float strength=1.0f; BrushMode mode=BrushMode::RaiseLower; };

static void applyBrush(HeightMap& hm, const Brush& b, const glm::vec3& hit, bool lower) {
    int cx = (int)roundf(hit.x / hm.cell);
    int cz = (int)roundf(hit.z / hm.cell);
    int rCells = (int)ceilf(b.radius / hm.cell);
    float sgn = lower ? -1.0f : 1.0f;

    if(b.mode == BrushMode::RaiseLower){
        for(int dz=-rCells; dz<=rCells; ++dz){
            for(int dx=-rCells; dx<=rCells; ++dx){
                int x = cx + dx, z = cz + dz; if(!hm.inBounds(x,z)) continue;
                float wx = x*hm.cell, wz = z*hm.cell;
                float dist = glm::length(glm::vec2(wx - hit.x, wz - hit.z));
                if(dist > b.radius) continue;
                float falloff = 0.5f*(cosf(3.14159f * dist / b.radius) + 1.0f); // smooth falloff
                hm.at(x,z) += sgn * b.strength * falloff * 0.1f; // scale step
            }
        }
    } 
    else { // Smooth
        for(int dz=-rCells; dz<=rCells; ++dz){
            for(int dx=-rCells; dx<=rCells; ++dx){
                int x = cx + dx;
                int z = cz + dz; 
                if(!hm.inBounds(x,z)) continue;

                float wx = x*hm.cell, wz = z*hm.cell;
                float dist = glm::length(glm::vec2(wx - hit.x, wz - hit.z));
                
                if(dist > b.radius) continue;
                // average of neighbors
                float sum=0; int cnt=0;
                for(int oz=-1; oz<=1; ++oz){
                    for(int ox=-1; ox<=1; ++ox){
                        int xx=x+ox, zz=z+oz;
                        if(hm.inBounds(xx,zz))
                        {
                            sum+=hm.at(xx,zz); ++cnt; 
                        } 
                    }
                }
                float avg = sum / (float)cnt;
                hm.at(x,z) = glm::mix(hm.at(x,z), avg, glm::clamp(b.strength*0.2f, 0.0f, 1.0f));
            }
        }
    }
}

// ------------ Save/Load ------------
#pragma pack(push,1)
struct HMapHeader { char magic[4]; uint32_t size; float cell; };
#pragma pack(pop)

static bool saveHMap(const std::string& path, const HeightMap& hm){
    std::ofstream f(path, std::ios::binary); if(!f) return false;
    HMapHeader hdr; hdr.magic[0]='H';hdr.magic[1]='M';hdr.magic[2]='P';hdr.magic[3]='1'; hdr.size=hm.size; hdr.cell=hm.cell;
    f.write((char*)&hdr, sizeof(hdr));
    f.write((char*)hm.h.data(), hm.h.size()*sizeof(float));
    return true;
}
static bool loadHMap(const std::string& path, HeightMap& hm){
    std::ifstream f(path, std::ios::binary); if(!f) return false;
    HMapHeader hdr; f.read((char*)&hdr, sizeof(hdr));
    if(!(hdr.magic[0]=='H'&&hdr.magic[1]=='M'&&hdr.magic[2]=='P'&&hdr.magic[3]=='1')) return false;
    if((int)hdr.size != hm.size){ std::cerr<<"Mismatched size in hmap.\n"; return false; }
    hm.h.resize(hm.size*hm.size);
    f.read((char*)hm.h.data(), hm.h.size()*sizeof(float));
    return true;
}

// ------------ Brush circle helper ------------
static void buildCircle(std::vector<glm::vec3>& out, float radius, int segments=64){
    out.clear(); out.reserve(segments);
    for(int i=0;i<segments;++i){
        float a = (i/(float)segments)*6.2831853f;
        out.emplace_back(radius*cosf(a), 0.0f, radius*sinf(a));
    }
}

int main(int argc, char** argv){
    // SDL init
    SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_Window* win = SDL_CreateWindow("Mini WoW Terrain Editor", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1600, 900, SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
    SDL_GLContext glctx = SDL_GL_CreateContext(win);

    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
    std::cerr << "Failed to initialize GLAD!" << std::endl;
    return -1;
    }
    SDL_GL_SetSwapInterval(1);

    

    glEnable(GL_DEPTH_TEST);

    
    Shader heightMapShader("shaders/hmap.vs","shaders/hmap.fs", "shaders/hmap.g");
    Shader heightMapColorShader("shaders/hmap_color.vs","shaders/hmap_color.fs");

    HeightMap hm;
    TerrainGL tg;
    buildTerrainMesh(hm, tg);

    // simple unit circle VBO for brush ring
    GLuint ringVBO=0, ringVAO=0; std::vector<glm::vec3> ringVerts; buildCircle(ringVerts, 1.0f);
    glGenVertexArrays(1, &ringVAO); glGenBuffers(1, &ringVBO);
    glBindVertexArray(ringVAO); glBindBuffer(GL_ARRAY_BUFFER, ringVBO);
    glBufferData(GL_ARRAY_BUFFER, ringVerts.size()*sizeof(glm::vec3), ringVerts.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,0,(void*)0); glBindVertexArray(0);

    Camera cam; bool running=true; bool rmb=false; bool lmb=false; bool mmb=false; bool shift=false;
    Brush brush; float aspect=1600.0f/900.0f; bool wire=false;

    uint32_t prevTicks = SDL_GetTicks();

    while(running){
        // --- Events ---
        SDL_Event e; int mx=0,my=0; SDL_GetMouseState(&mx,&my);
        while(SDL_PollEvent(&e)){
            if(e.type==SDL_QUIT) running=false;
            if(e.type==SDL_WINDOWEVENT && e.window.event==SDL_WINDOWEVENT_SIZE_CHANGED){ aspect = e.window.data1 / (float)e.window.data2; glViewport(0,0,e.window.data1,e.window.data2); }
            if(e.type==SDL_MOUSEBUTTONDOWN){ if(e.button.button==SDL_BUTTON_RIGHT) rmb=true; if(e.button.button==SDL_BUTTON_LEFT) lmb=true; if(e.button.button==SDL_BUTTON_MIDDLE) mmb=true; }
            if(e.type==SDL_MOUSEBUTTONUP){ if(e.button.button==SDL_BUTTON_RIGHT) rmb=false; if(e.button.button==SDL_BUTTON_LEFT) lmb=false; if(e.button.button==SDL_BUTTON_MIDDLE) mmb=false; }
            if(e.type==SDL_MOUSEWHEEL){ if(e.wheel.y>0) brush.radius*=1.1f; if(e.wheel.y<0) brush.radius/=1.1f; brush.radius = glm::clamp(brush.radius, 1.0f, 100.0f); }
            if(e.type==SDL_KEYDOWN){
                if(e.key.keysym.sym==SDLK_ESCAPE) running=false;
                if(e.key.keysym.sym==SDLK_LSHIFT || e.key.keysym.sym==SDLK_RSHIFT) shift=true;
                if(e.key.keysym.sym==SDLK_TAB) FLATSHADE=!FLATSHADE;
                if(e.key.keysym.sym==SDLK_b) brush.strength = glm::max(0.1f, brush.strength*0.9f);
                if(e.key.keysym.sym==SDLK_v) brush.strength = glm::min(10.0f, brush.strength*1.1f);
                if(e.key.keysym.sym==SDLK_f){ wire=!wire; }
                if(e.key.keysym.sym==SDLK_r){ std::fill(hm.h.begin(), hm.h.end(), 0.0f); updateTerrainVertices(hm, tg); }
                if(e.key.keysym.sym==SDLK_F5){ saveHMap("tile.hmap", hm); std::cout<<"Saved tile.hmap\n"; }
                if(e.key.keysym.sym==SDLK_F9){ if(loadHMap("tile.hmap", hm)){ updateTerrainVertices(hm, tg); std::cout<<"Loaded tile.hmap\n"; } }
            }
            if(e.type==SDL_KEYUP){ if(e.key.keysym.sym==SDLK_LSHIFT || e.key.keysym.sym==SDLK_RSHIFT) shift=false; }
        }

        // --- Timing ---
        uint32_t now = SDL_GetTicks(); float dt = (now - prevTicks) * 0.001f; prevTicks = now;

        // --- Camera movement ---
        const Uint8* ks = SDL_GetKeyboardState(nullptr);
        float speed = (ks[SDL_SCANCODE_LCTRL] ? 200.0f : 80.0f);
        glm::vec3 fwd = glm::normalize(glm::vec3(cosf(cam.pitch)*cosf(cam.yaw), 0.0f, cosf(cam.pitch)*sinf(cam.yaw)));
        glm::vec3 right = glm::normalize(glm::cross(fwd, glm::vec3(0,1,0)));
        if(ks[SDL_SCANCODE_W]) cam.pos += fwd * speed * dt;
        if(ks[SDL_SCANCODE_S]) cam.pos -= fwd * speed * dt;
        if(ks[SDL_SCANCODE_A]) cam.pos -= right * speed * dt;
        if(ks[SDL_SCANCODE_D]) cam.pos += right * speed * dt;
        if(ks[SDL_SCANCODE_Q]) cam.pos.y -= speed * dt;
        if(ks[SDL_SCANCODE_E]) cam.pos.y += speed * dt;

        // Mouse look
        static int lastmx=mx, lastmy=my; int dx = mx-lastmx, dy = my-lastmy; lastmx=mx; lastmy=my;
        if(rmb){ cam.yaw += dx*0.0035f; cam.pitch -= dy*0.0035f; cam.pitch = glm::clamp(cam.pitch, -1.5f, 1.5f); }

        // --- Picking ---
        int w,h; SDL_GetWindowSize(win,&w,&h);
        glm::mat4 V = cam.view(); glm::mat4 P = cam.proj(w/(float)h);
        glm::mat4 VP = P*V; glm::mat4 invVP = glm::inverse(VP);
        // NDC ray from mouse
        float xN = (2.0f*mx/(float)w - 1.0f);
        float yN = (1.0f - 2.0f*my/(float)h);
        glm::vec4 p0 = invVP * glm::vec4(xN,yN,-1,1); p0/=p0.w;
        glm::vec4 p1 = invVP * glm::vec4(xN,yN, 1,1); p1/=p1.w;
        glm::vec3 ro = glm::vec3(p0); glm::vec3 rd = glm::normalize(glm::vec3(p1-p0));

        glm::vec3 hit; bool hasHit = rayHeightmapIntersect(hm, ro, rd, 4000.0f, hit);

        // --- Brush apply ---
        if(hasHit){
            if(lmb){ brush.mode = BrushMode::RaiseLower; applyBrush(hm, brush, hit, shift); updateTerrainVertices(hm, tg); }
            if(mmb){ brush.mode = BrushMode::Smooth; applyBrush(hm, brush, hit, false); updateTerrainVertices(hm, tg); }
        }

        // --- Render ---
        glClearColor(0.52f,0.75f,0.95f,1);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

        glm::mat4 M(1.0f);
        glm::mat4 MVP = P * V * M;
        glm::mat3 NrmM = glm::mat3(1.0f);

        glPolygonMode(GL_FRONT_AND_BACK, wire?GL_LINE:GL_FILL);
        heightMapShader.use();
        heightMapShader.setMat4("uMVP", MVP);
        heightMapShader.setBool("uFlatShading", FLATSHADE);
        heightMapShader.setMat4("uModel", M);
        heightMapShader.setMat3("uNrmM", NrmM);
        heightMapShader.setVec3("uCamPos", cam.pos);
        

        glBindVertexArray(tg.vao);
        glDrawElements(GL_TRIANGLES, tg.indexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        // Draw brush ring at hit position
        if(hasHit){
            std::vector<glm::vec3> ring; buildCircle(ring, brush.radius, 96);
            // update ring VBO with scaled circle at hit.y
            for(auto& v : ring){ v.y = 0.0f; }
            glBindBuffer(GL_ARRAY_BUFFER, ringVBO);
            glBufferData(GL_ARRAY_BUFFER, ring.size()*sizeof(glm::vec3), ring.data(), GL_DYNAMIC_DRAW);

            glm::mat4 Mring = glm::translate(glm::mat4(1.0f), glm::vec3(hit.x, hit.y + 0.05f, hit.z));
            glm::mat4 VP = P*V; 

            heightMapColorShader.use();
            heightMapColorShader.setMat4("uVP", VP);
            heightMapColorShader.setMat4("uM", Mring);
            heightMapColorShader.setVec4("uColor", glm::vec4(0.0f,0.0f,0.0f,1.0f));
            

            glBindVertexArray(ringVAO);
            glDrawArrays(GL_LINE_LOOP, 0, (GLint)ring.size());
            glBindVertexArray(0);
        }

        SDL_GL_SwapWindow(win);
    }

    tg.destroy();
    SDL_GL_DeleteContext(glctx); SDL_DestroyWindow(win); SDL_Quit();
    return 0;
}
