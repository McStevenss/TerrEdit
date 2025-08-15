#include "Terrain.hpp"
#include <chrono>
#include <iostream>


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

void Terrain::Render(bool wire){
    if (wire) {
        // 1) Depth pre-pass (fill, no color)
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        drawMesh();

        // 2) Wire pass
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);

        glEnable(GL_POLYGON_OFFSET_LINE);
        glPolygonOffset(-1.0f, -1.0f);

        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glDepthFunc(GL_LEQUAL);
        drawMesh();

        // restore
        glDisable(GL_POLYGON_OFFSET_LINE);
        glDisable(GL_CULL_FACE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDepthFunc(GL_LESS);
    } else {
        // Just a filled render
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        drawMesh();
    }
}

void Terrain::drawMesh() {

    updateMeshIfDirty();
    glBindVertexArray(mesh.vao);

    glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, nullptr);
    
    glBindVertexArray(0);
}

void Terrain::resetHeightMap()
{
    std::fill(hm.h.begin(), hm.h.end(), 0.0f); 
    dirty=true; 
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
                if (!b.Falloff){falloff = 1.0f;}
                hm.at(x,z) += sgn * b.strength * falloff * 0.1f; // scale step
                dirty=true;
            }
        }
    }
    else if(b.mode == BrushMode::Smooth)
    { // Smooth
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

float Terrain::getHeightAt(float x, float z) const {
    // int ix = (int)floor(x);
    // int iz = (int)floor(z);
    // float fx = x - ix;
    // float fz = z - iz;

    // // clamp
    // if(!inBounds(ix, iz)) return 0.0f;
    // if(!inBounds(ix+1, iz+1)) return heightAt(ix, iz);

    // // bilinear interpolation
    // float h00 = heightAt(ix, iz);
    // float h10 = heightAt(ix+1, iz);
    // float h01 = heightAt(ix, iz+1);
    // float h11 = heightAt(ix+1, iz+1);

    // float h0 = h00*(1-fx) + h10*fx;
    // float h1 = h01*(1-fx) + h11*fx;

    // return h0*(1-fz) + h1*fz;

    return hm.sampleHeight(x,z);
}
bool Terrain::rayHeightmapIntersect(const glm::vec3 &rayOrigin, const glm::vec3 &rayDistance, float maxDist, glm::vec3 &outHit)
{
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

//Comments so i remember what is done here.
bool Terrain::saveHMap(const std::string& path){
    //Open binary file
    std::ofstream f(path, std::ios::binary);
    if(!f) return false;
    

    //Get the designed header for this custom heightmap file
    HMapHeader hdr;
    
    //Write fileheader (arbitrary) as HMP1 to show what file it is
    hdr.magic[0]='H';hdr.magic[1]='M';hdr.magic[2]='P';hdr.magic[3]='1';
    
    //Writes what size the heightmap is and how big the cells are 
    hdr.size=hm.size; hdr.cell=hm.cell;
    
    //First write header
    f.write((char*)&hdr, sizeof(hdr));

    //Just RAW dump the heightmap data at the rest
    f.write((char*)hm.h.data(), hm.h.size()*sizeof(float));
    return true;
}

//Comments so i remember what is done here.
bool Terrain::loadHMap(const std::string& path){
    auto start = std::chrono::high_resolution_clock::now();
    
    //Open binary file
    std::ifstream f(path, std::ios::binary);
    if(!f) return false;
    
    //Get the header for this fileformat
    HMapHeader hdr;
    f.read((char*)&hdr, sizeof(hdr));
    
    //Verify it is a valid file and format through the magic header
    if(!(hdr.magic[0]=='H'&&hdr.magic[1]=='M'&&hdr.magic[2]=='P'&&hdr.magic[3]=='1')) return false;
    
    //Verify that the size is correct
    if((int)hdr.size != hm.size){ std::cerr<<"Mismatched size in hmap.\n"; return false; }
    
    //Resize height array
    hm.h.resize(hm.size*hm.size);
    f.read((char*)hm.h.data(), hm.h.size()*sizeof(float));
    dirty=true;

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    // Duration in microseconds
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    std::cout << "[Terrain] Heightmap loaded in " << duration_ms << " ms (" 
              << duration_us << " μs)." << std::endl;
 
    return true;
}
