#pragma once
#include <vector>
#include <glm/glm.hpp>
#include "glad/glad.h"
#include <cmath>

struct VertexPNUV {
    glm::vec3 p, n;
    glm::vec2 uv;
};

class Terrain {
public:
    enum class BrushMode { RaiseLower, Smooth };
    struct Brush { float radius=6.0f; float strength=1.0f; BrushMode mode=BrushMode::RaiseLower; };

private:
    struct HeightMap {
        int size;
        float cell;
        std::vector<float> h;

        HeightMap(int s, float c) : size(s), cell(c), h(s*s, 0.0f) {}

        float& at(int x,int z){ return h[z*size + x]; }
        float  at(int x,int z) const { return h[z*size + x]; }
        bool inBounds(int x,int z) const { return x>=0 && z>=0 && x<size && z<size; }

        glm::vec3 normalAt(int x,int z) const {
            float hL = inBounds(x-1,z)? at(x-1,z) : at(x,z);
            float hR = inBounds(x+1,z)? at(x+1,z) : at(x,z);
            float hD = inBounds(x,z-1)? at(x,z-1) : at(x,z);
            float hU = inBounds(x,z+1)? at(x,z+1) : at(x,z);
            return glm::normalize(glm::vec3(-(hR-hL)/(2*cell),1.0f,-(hU-hD)/(2*cell)));
        }
    };

    struct TerrainGL {
        GLuint vao=0, vbo=0, ibo=0; GLsizei indexCount=0;
        void destroy(){
            if(ibo) glDeleteBuffers(1,&ibo);
            if(vbo) glDeleteBuffers(1,&vbo);
            if(vao) glDeleteVertexArrays(1,&vao);
            vao=vbo=ibo=0; indexCount=0;
        }
    };

    HeightMap hm;
    TerrainGL mesh;
    bool dirty = true;

public:
    Terrain(int gridSize=64, float cellSize=1.0f) : hm(gridSize, cellSize) {}
    ~Terrain(){ mesh.destroy(); }

    // CPU access
    float heightAt(int x,int z) const { return hm.at(x,z); }
    glm::vec3 normalAt(int x,int z) const { return hm.normalAt(x,z); }
    bool inBounds(int x,int z) const { return hm.inBounds(x,z); }

    // GPU mesh
    void buildMesh();
    void updateMeshIfDirty();
    void render();

    // Brush editing
    void applyBrush(const Brush& b, const glm::vec3& hit, bool lower=false);
};
