#include "TerrainMap.h"


TerrainMap::TerrainMap(int chunksX, int chunksZ, int chunkSize, float cellSize)
    : chunksX(chunksX), chunksZ(chunksZ), chunkSize(chunkSize), cellSize(cellSize)
{
    chunks.reserve(chunksX * chunksZ);

    for (int cz = 0; cz < chunksZ; ++cz) {
        for (int cx = 0; cx < chunksX; ++cx) {
            float originX = cx * (chunkSize - 1) * cellSize;
            float originZ = cz * (chunkSize - 1) * cellSize;


            chunks.push_back(std::make_unique<TerrainChunk>(chunkSize, cellSize));
            TerrainChunk* chunk = chunks.back().get();
            chunk->position = {originX, 0.0f, originZ};
            chunk->gridX = cx;
            chunk->gridZ = cz;
         
        }
    }
}

void TerrainMap::build() {
    for (auto& chunk : chunks) {
        chunk->buildMesh();
    }
}

void TerrainMap::applyBrush(const Brush& b, const glm::vec3& hit, bool lower) {
    // Determine brush bounds in world coords
    float minX = hit.x - b.radius;
    float maxX = hit.x + b.radius;
    float minZ = hit.z - b.radius;
    float maxZ = hit.z + b.radius;

    for (auto& chunk : chunks) {
        // Compute this chunk’s world bounding box
        float cx0 = chunk->position.x;
        float cz0 = chunk->position.z;
        float cx1 = cx0 + (chunk->hm.size - 1) * cellSize;
        float cz1 = cz0 + (chunk->hm.size - 1) * cellSize;

        // If brush overlaps chunk
        if (maxX >= cx0 && minX <= cx1 &&
            maxZ >= cz0 && minZ <= cz1) {

            // Convert hit point to chunk-local coordinates
            glm::vec3 localHit = hit - chunk->position;
            chunk->applyBrush(b, localHit, lower);
        }
    }
}

void TerrainMap::render(bool wire) {
    for (auto& chunk : chunks) {
        chunk->Render(wire);
    }
}

std::vector<std::unique_ptr<TerrainChunk>>& TerrainMap::GetChunks()
{
    return chunks;
}

// TerrainChunk* TerrainMap::getChunkAt(glm::vec3 worldPos) {
//     int cx = int(worldPos.x / ((chunkSize-1) * cellSize));
//     int cz = int(worldPos.z / ((chunkSize-1) * cellSize));
//     if (cx < 0 || cz < 0 || cx >= chunksX || cz >= chunksZ) return nullptr;
//     // return &chunks[cz * chunksX + cx];
//     return chunks[cz * chunksX + cx].get();
// }

TerrainChunk* TerrainMap::getChunkAt(const glm::vec3& worldPos) {
    for (auto& chunkPtr : chunks) {
        if (chunkPtr->contains(worldPos.x, worldPos.z)) {
            return chunkPtr.get();
        }
    }
    return nullptr;
}

void TerrainMap::updateDirtyChunks()
{   
    for (auto& chunk : chunks) {
        chunk->updateMeshIfDirty();
    }
}


float TerrainMap::getHeightGlobal(float x, float z) {
    for (auto& chunkPtr : chunks) {
        if (chunkPtr->contains(x, z)) {
            glm::vec3 local = glm::vec3(x, 0, z) - chunkPtr->position;
            return chunkPtr->getHeightAt(local.x, local.z);
        }
    }
    return -1.0f;
}


void TerrainMap::save(const std::string& folderPath) {
    namespace fs = std::filesystem;

    // Create the folder if it doesn't exist
    if (!fs::exists(folderPath)) {
        if (!fs::create_directories(folderPath)) {
            std::cerr << "Failed to create folder: " << folderPath << std::endl;
        }
    }

    for (auto& chunk : chunks) {
        // Build a filename for each chunk: e.g., "chunk_0_1.hmp"
        std::ostringstream filename;
        filename << folderPath << "/chunk_" << chunk->gridX << "_" << chunk->gridZ << ".hmap";

        if (!chunk->saveHMap(filename.str())) {
            std::cerr << "Failed to save chunk at (" << chunk->gridX 
                      << ", " << chunk->gridZ << ")" << std::endl;
        }
    }

    std::cout << "TerrainMap saved successfully to " << folderPath << std::endl;
}

void TerrainMap::load(const std::string& folderPath) {
    namespace fs = std::filesystem;

    if (!fs::exists(folderPath) || !fs::is_directory(folderPath)) {
        std::cerr << "Folder does not exist: " << folderPath << std::endl;
    }

    // Clear current chunks
    chunks.clear();
    int numErrors = 0;

    // Iterate over all .hmp files
    for (auto& entry : fs::directory_iterator(folderPath)) {
        if (entry.path().extension() != ".hmap") continue;

        // auto chunk = std::make_unique<TerrainChunk>(chunkSize, cellSize);
        // TerrainChunk chunk(chunkSize, cellSize);
        auto chunk = std::make_unique<TerrainChunk>(chunkSize, cellSize);
        if (!chunk->loadHMap(entry.path().string())) {
            std::cerr << "Failed to load chunk: " << entry.path() << std::endl;
            numErrors++;
        }

        chunks.push_back(std::move(chunk));
    }

    if (!chunks.empty()) {
        std::cout << "chunks not empty"<< std::endl;
    
        int maxX = 0, maxZ = 0;
        for (auto& c : chunks) {
            if (c->gridX > maxX) maxX = c->gridX;
            if (c->gridZ > maxZ) maxZ = c->gridZ;
        }
        chunksX = maxX + 1;
        chunksZ = maxZ + 1;
    }

    if(numErrors > 0){
        std::cout << "TerrainMap failed to load " << numErrors << " chunks from: " << folderPath << std::endl;
    }
    else {
        build();
        updateDirtyChunks();
        std::cout << "TerrainMap loaded successfully from " << folderPath << std::endl;
    }
}
// Optionally, update chunksX and chunksZ based on loaded data
// Update chunksX/Z