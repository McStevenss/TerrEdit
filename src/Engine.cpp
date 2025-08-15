#include "Engine.h"

Engine::Engine()
{
    cam.pos = glm::vec3(TILE_SIZE*0.5f, 150.0f, -TILE_SIZE*0.2f);
    Initialize();


    heightMapShader = new Shader("shaders/hmap.vs","shaders/hmap.fs", "shaders/hmap.g");
    heightMapColorShader = new Shader("shaders/hmap_color.vs","shaders/hmap_color.fs");
    terrain = new Terrain(GRID_SIZE, CELL_SIZE);
    terrain->buildMesh();
    buildCircle(ringVerts, 1.0f);
    GenCircleGL();

}

void Engine::Initialize()
{
    // SDL init
    SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    win = SDL_CreateWindow("Mini WoW Terrain Editor", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1600, 900, SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
    glctx = SDL_GL_CreateContext(win);

    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        std::cerr << "Failed to initialize GLAD!" << std::endl;
        return;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

     // Setup Platform/Renderer bindings
    ImGui_ImplSDL2_InitForOpenGL(win, glctx);
    ImGui_ImplOpenGL3_Init("#version 410");
    SDL_GL_SetSwapInterval(1);
    
    glEnable(GL_DEPTH_TEST);
    glCullFace(GL_BACK);

}

void Engine::Start()
{
    uint32_t prevTicks = SDL_GetTicks();

    while(running)
    {
        // --- Timing ---
        uint32_t now = SDL_GetTicks();
        float dt = (now - prevTicks) * 0.001f; prevTicks = now;
        
        HandleInput(dt);

        // --- Picking ---
        int w,h; SDL_GetWindowSize(win,&w,&h);
        glm::mat4 View = cam.view();
        glm::mat4 Projection = cam.proj(w/(float)h);
        glm::mat4 VP = Projection*View; 
        glm::mat4 invVP = glm::inverse(VP);
        // NDC ray from mouse
        float xN = (2.0f*mx/(float)w - 1.0f);
        float yN = (1.0f - 2.0f*my/(float)h);
        glm::vec4 p0 = invVP * glm::vec4(xN,yN,-1,1); p0/=p0.w;
        glm::vec4 p1 = invVP * glm::vec4(xN,yN, 1,1); p1/=p1.w;
        glm::vec3 ro = glm::vec3(p0); glm::vec3 rd = glm::normalize(glm::vec3(p1-p0));
        glm::vec3 hit;
        bool hasHit = terrain->rayHeightmapIntersect(ro, rd, 4000.0f, hit);

        // --- Brush apply ---
        if(hasHit){
            if(lmb){ brush.mode = BrushMode::RaiseLower; terrain->applyBrush(brush, hit, shift);}
            if(mmb){ brush.mode = BrushMode::Smooth; terrain->applyBrush(brush, hit, false);}
        }


        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // Begin the Info panel window
        ImGui::Begin("Info panel");
            ImGui::Text("Text test");
        ImGui::End();

        
        terrain->updateMeshIfDirty();
        // --- Render ---
        glClearColor(0.52f,0.75f,0.95f,1);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

        glm::mat4 Model(1.0f);
        glm::mat4 MVP = Projection * View * Model;
        glm::mat3 NrmM = glm::mat3(1.0f);
            
        heightMapShader->use();
        heightMapShader->setMat4("uMVP", MVP);
        heightMapShader->setBool("uFlatShading", flatshade);
        heightMapShader->setMat4("uModel", Model);
        heightMapShader->setMat3("uNrmM", NrmM);
        heightMapShader->setVec3("uCamPos", cam.pos);
        terrain->Render(wire);

        // Draw brush ring at hit position
        if(hasHit){
            std::vector<glm::vec3> ring; buildCircle(ring, brush.radius, 96);
            // update ring VBO with scaled circle at hit.y
            for(auto& v : ring){ v.y = 0.0f; }

            glBindBuffer(GL_ARRAY_BUFFER, ringVBO);
            glBufferData(GL_ARRAY_BUFFER, ring.size()*sizeof(glm::vec3), ring.data(), GL_DYNAMIC_DRAW);

            glm::mat4 Mring = glm::translate(glm::mat4(1.0f), glm::vec3(hit.x, hit.y + 0.05f, hit.z));
            // glm::mat4 VP = P*V; 

            heightMapColorShader->use();
            heightMapColorShader->setMat4("uVP", VP);
            heightMapColorShader->setMat4("uM", Mring);
            heightMapColorShader->setVec4("uColor", glm::vec4(0.0f,0.0f,0.0f,1.0f));
            

            glBindVertexArray(ringVAO);
            glDrawArrays(GL_LINE_LOOP, 0, (GLint)ring.size());
            glBindVertexArray(0);
        }



          // Render ImGui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        SDL_GL_SwapWindow(win);
    }

}

void Engine::buildCircle(std::vector<glm::vec3>& out, float radius, int segments){
    out.clear(); out.reserve(segments);
    for(int i=0;i<segments;++i){
        float a = (i/(float)segments)*6.2831853f;
        out.emplace_back(radius*cosf(a), 0.0f, radius*sinf(a));
    }
}

void Engine::GenCircleGL()
{
    glGenVertexArrays(1, &ringVAO); glGenBuffers(1, &ringVBO);
    glBindVertexArray(ringVAO); glBindBuffer(GL_ARRAY_BUFFER, ringVBO);
    glBufferData(GL_ARRAY_BUFFER, ringVerts.size()*sizeof(glm::vec3), ringVerts.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,0,(void*)0); glBindVertexArray(0);
}

void Engine::HandleInput(float dt)
{
    // int mx=0,my=0;
    SDL_GetMouseState(&mx,&my);
    SDL_Event e; 
    while(SDL_PollEvent(&e))
    {
        ImGui_ImplSDL2_ProcessEvent(&e);
        if(e.type==SDL_QUIT) running=false;
        // if(e.type==SDL_WINDOWEVENT && e.window.event==SDL_WINDOWEVENT_SIZE_CHANGED){ aspect = e.window.data1 / (float)e.window.data2; glViewport(0,0,e.window.data1,e.window.data2); }
        if(e.type==SDL_WINDOWEVENT && e.window.event==SDL_WINDOWEVENT_SIZE_CHANGED){ cam.recalculateViewport(e); }
        if(e.type==SDL_MOUSEBUTTONDOWN){ if(e.button.button==SDL_BUTTON_RIGHT) rmb=true; if(e.button.button==SDL_BUTTON_LEFT) lmb=true; if(e.button.button==SDL_BUTTON_MIDDLE) mmb=true; }
        if(e.type==SDL_MOUSEBUTTONUP){ if(e.button.button==SDL_BUTTON_RIGHT) rmb=false; if(e.button.button==SDL_BUTTON_LEFT) lmb=false; if(e.button.button==SDL_BUTTON_MIDDLE) mmb=false; }
        if(e.type==SDL_MOUSEWHEEL){ if(e.wheel.y>0) brush.radius*=1.1f; if(e.wheel.y<0) brush.radius/=1.1f; brush.radius = glm::clamp(brush.radius, 1.0f, 100.0f); }
        if(e.type==SDL_KEYDOWN){
            if(e.key.keysym.sym==SDLK_ESCAPE) running=false;
            if(e.key.keysym.sym==SDLK_LCTRL) brush.Falloff=false;
            if(e.key.keysym.sym==SDLK_LSHIFT || e.key.keysym.sym==SDLK_RSHIFT) shift=true;
            if(e.key.keysym.sym==SDLK_TAB) flatshade=!flatshade;
            if(e.key.keysym.sym==SDLK_b) brush.strength = glm::max(0.1f, brush.strength*0.9f);
            if(e.key.keysym.sym==SDLK_v) brush.strength = glm::min(10.0f, brush.strength*1.1f);
            if(e.key.keysym.sym==SDLK_f){ wire=!wire; }
            if(e.key.keysym.sym==SDLK_r){ terrain->resetHeightMap();}
            if(e.key.keysym.sym==SDLK_F5){ terrain->saveHMap("tile.hmap"); std::cout<<"Saved tile.hmap\n"; }
            if(e.key.keysym.sym==SDLK_F9){ terrain->loadHMap("tile.hmap"); std::cout<<"Loaded tile.hmap\n"; } 
        }
        
        if(e.type==SDL_KEYUP){ if(e.key.keysym.sym==SDLK_LSHIFT || e.key.keysym.sym==SDLK_RSHIFT) shift=false; }
        if(e.type==SDL_KEYUP){ if(e.key.keysym.sym==SDLK_LCTRL) brush.Falloff=true; }
    }

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

}
