#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <SDL2/SDL.h>

struct Camera {
    glm::vec3 pos;
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

    void recalculateViewport(SDL_Event e){
        float aspect = e.window.data1 / (float)e.window.data2;
        glViewport(0,0,e.window.data1,e.window.data2); 
    }
};