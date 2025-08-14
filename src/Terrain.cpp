#include "Terrain.hpp"

void Terrain::buildMesh() {
    std::vector<VertexPNUV> verts(hm.size * hm.size);

    // Build vertices
    for(int z = 0; z < hm.size; ++z) {
        for(int x = 0; x < hm.size; ++x) {
            VertexPNUV& v = verts[z*hm.size + x];
            v.p = glm::vec3(x*hm.cell, hm.at(x,z), z*hm.cell);
            v.n = hm.normalAt(x,z);
            v.uv = glm::vec2(x / float(hm.size-1), z / float(hm.size-1));
        }
    }

    // Build indices
    std::vector<uint32_t> idx;
    idx.reserve((hm.size-1)*(hm.size-1)*6);
    for(int z = 0; z < hm.size-1; ++z) {
        for(int x = 0; x < hm.size-1; ++x) {
            uint32_t i0 = z*hm.size + x;
            uint32_t i1 = i0 + 1;
            uint32_t i2 = i0 + hm.size;
            uint32_t i3 = i2 + 1;
            idx.insert(idx.end(), {i0, i2, i1, i1, i2, i3});
        }
    }

    // Create VAO/VBO/IBO if necessary
    if(!mesh.vao) glGenVertexArrays(1,&mesh.vao);
    if(!mesh.vbo) glGenBuffers(1,&mesh.vbo);
    if(!mesh.ibo) glGenBuffers(1,&mesh.ibo);

    glBindVertexArray(mesh.vao);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(VertexPNUV), verts.data(), GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size()*sizeof(uint32_t), idx.data(), GL_STATIC_DRAW);

    GLsizei stride = sizeof(VertexPNUV);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,stride,(void*)offsetof(VertexPNUV,p));
    glEnableVertexAttribArray(1); glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,stride,(void*)offsetof(VertexPNUV,n));
    glEnableVertexAttribArray(2); glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,stride,(void*)offsetof(VertexPNUV,uv));

    glBindVertexArray(0);

    mesh.indexCount = (GLsizei)idx.size();
    dirty = false;
}

void Terrain::updateMeshIfDirty() {
    if(!dirty) return;

    std::vector<VertexPNUV> verts(hm.size * hm.size);
    for(int z = 0; z < hm.size; ++z) {
        for(int x = 0; x < hm.size; ++x) {
            VertexPNUV& v = verts[z*hm.size + x];
            v.p = glm::vec3(x*hm.cell, hm.at(x,z), z*hm.cell);
            v.n = hm.normalAt(x,z);
            v.uv = glm::vec2(x / float(hm.size-1), z / float(hm.size-1));
        }
    }

    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, verts.size()*sizeof(VertexPNUV), verts.data());
    dirty = false;
}

void Terrain::render() {




    updateMeshIfDirty();
    glBindVertexArray(mesh.vao);
    glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void Terrain::applyBrush(const Brush &b, const glm::vec3 &hit, bool lower)
{
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
                dirty=true;
            }
        }
    } else { // Smooth
        for(int dz=-rCells; dz<=rCells; ++dz){
            for(int dx=-rCells; dx<=rCells; ++dx){
                int x = cx + dx, z = cz + dz; if(!hm.inBounds(x,z)) continue;
                float wx = x*hm.cell, wz = z*hm.cell;
                float dist = glm::length(glm::vec2(wx - hit.x, wz - hit.z));
                if(dist > b.radius) continue;
                // average of neighbors
                float sum=0; int cnt=0;
                for(int oz=-1; oz<=1; ++oz){ for(int ox=-1; ox<=1; ++ox){ int xx=x+ox, zz=z+oz; if(hm.inBounds(xx,zz)){ sum+=hm.at(xx,zz); ++cnt; } }}
                float avg = sum / (float)cnt;
                hm.at(x,z) = glm::mix(hm.at(x,z), avg, glm::clamp(b.strength*0.2f, 0.0f, 1.0f));
                dirty=true;
            }
        }
    }
}
