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
#include "TerrainChunk.hpp"

#include <filesystem>
#include <sstream>

class TerrainMap {
public:
    TerrainMap(int worldSizeX, int worldSizeZ, int chunkSize, float cellSize);

    void build();
    void render(bool wire=false);
    void applyBrush(const Brush& b, const glm::vec3& hit, bool lower=false);
    void updateDirtyChunks();
    float getHeightGlobal(float x, float z);

    void save(const std::string& folderPath);
    void load(const std::string& folderPath);

    // std::vector<TerrainChunk>& GetChunks();
    std::vector<std::unique_ptr<TerrainChunk>>& GetChunks();
    // TerrainChunk* getChunkAt(glm::vec3 worldPos);
    TerrainChunk* getChunkAt(const glm::vec3& worldPos);

private:
    int chunksX, chunksZ;      // number of chunks in each direction
    int chunkSize;             // number of samples per chunk
    float cellSize;
    // std::vector<TerrainChunk> chunks;
    std::vector<std::unique_ptr<TerrainChunk>> chunks;

};