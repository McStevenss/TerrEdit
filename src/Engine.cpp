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

    CreateFrameBuffer();
}

void Engine::Initialize()
{
    // SDL init
    SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    win = SDL_CreateWindow("Mini WoW Terrain Editor", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, ScreenWidth, ScreenHeight, SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
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

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        ImVec2 imgPos = RenderGUI(); // now it returns the top-left of the image inside window
        ImGui::Render();
        
        BindFramebuffer();

        // --- Picking ---
        SDL_GetWindowSize(win,&ScreenWidth,&ScreenHeight);
        
        float localX = mx - imgPos.x;
        float localY = my - imgPos.y;

        bool insideImage = (localX >= 0 && localX <= EditorWindowWidth &&
                            localY >= 0 && localY <= EditorWindowHeight);
        
        glm::mat4 View = cam.view();
        glm::mat4 Projection = cam.proj(ScreenWidth/(float)ScreenHeight);
        glm::mat4 VP = Projection*View; 
        glm::mat4 invVP = glm::inverse(VP);
        bool hasHit = false;
        glm::vec3 hit;


        if(insideImage){

            float xN = (2.0f * localX / EditorWindowWidth - 1.0f);
            float yN = (1.0f - 2.0f * localY / EditorWindowHeight);
            glm::vec4 p0 = invVP * glm::vec4(xN,yN,-1,1); p0/=p0.w;
            glm::vec4 p1 = invVP * glm::vec4(xN,yN, 1,1); p1/=p1.w;


            glm::vec3 ro = glm::vec3(p0); glm::vec3 rd = glm::normalize(glm::vec3(p1-p0));
 
            hasHit = terrain->rayHeightmapIntersect(ro, rd, 4000.0f, hit);
            
            // --- Brush apply ---
            if(hasHit){
                if(lmb){ brush.mode = BrushMode::RaiseLower; terrain->applyBrush(brush, hit, shift);}
                if(mmb){ brush.mode = BrushMode::Smooth; terrain->applyBrush(brush, hit, false);}
            }
        }



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
            // for(auto& v : ring){ v.y = 0.0f; }
            
            glm::mat4 Mring = glm::mat4(1.0f); // identity

            if(projectCircle){

                for(auto& v : ring)
                {
                    
                    // v.y += terrain->getHeightAt(v.x,v.z);
                    float worldX = v.x + hit.x;
                    float worldZ = v.z + hit.z;
                    v.y = terrain->getHeightAt(worldX, worldZ) + 0.15f; // offset slightly above terrain
                    
                    // update vertex to world-space XZ
                    v.x = worldX;
                    v.z = worldZ;
                }                
            }
            else
            {
                for(auto& v : ring){ v.y = 0.0f; }
                Mring = glm::translate(glm::mat4(1.0f), glm::vec3(hit.x, hit.y + 0.05f, hit.z));
            }

            glBindBuffer(GL_ARRAY_BUFFER, ringVBO);
            glBufferData(GL_ARRAY_BUFFER, ring.size()*sizeof(glm::vec3), ring.data(), GL_DYNAMIC_DRAW);


            // glm::mat4 VP = P*V; 

            heightMapColorShader->use();
            heightMapColorShader->setMat4("uVP", VP);
            heightMapColorShader->setMat4("uM", Mring);
            heightMapColorShader->setVec4("uColor", glm::vec4(0.0f,0.0f,0.0f,1.0f));
            

            glBindVertexArray(ringVAO);
            glDrawArrays(GL_LINE_LOOP, 0, (GLint)ring.size());
            glBindVertexArray(0);
        }


        UnbindFramebuffer();

          // Render ImGui
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

void Engine::CreateFrameBuffer()
{
    glGenFramebuffers(1, &FBO);
    glBindFramebuffer(GL_FRAMEBUFFER, FBO);

	glGenTextures(1, &texture_id);
	glBindTexture(GL_TEXTURE_2D, texture_id);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, ScreenWidth, ScreenHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_id, 0);

	glGenRenderbuffers(1, &RBO);
	glBindRenderbuffer(GL_RENDERBUFFER, RBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, ScreenWidth, ScreenHeight);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, RBO);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!\n";

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);
}

void Engine::RescaleFramebuffer(float width, float height)
{
	glBindTexture(GL_TEXTURE_2D, texture_id);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_id, 0);

	glBindRenderbuffer(GL_RENDERBUFFER, RBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, RBO);
}

void Engine::BindFramebuffer()
{
	glBindFramebuffer(GL_FRAMEBUFFER, FBO);
}

// here we unbind our framebuffer
void Engine::UnbindFramebuffer()
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

ImVec2 Engine::RenderGUI()
{
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(ScreenWidth*0.8f, ScreenHeight), ImGuiCond_Always);
    ImGui::Begin("Editor",
                 nullptr,
                 ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoCollapse |
                 ImGuiWindowFlags_NoTitleBar);

    EditorWindowWidth = ImGui::GetContentRegionAvail().x;
    EditorWindowHeight = ImGui::GetContentRegionAvail().y;

    // rescale framebuffer
    RescaleFramebuffer(EditorWindowWidth, EditorWindowHeight);
    glViewport(0, 0, EditorWindowWidth, EditorWindowHeight);

    // get correct image position **inside the window**
    ImVec2 imgPos = ImGui::GetCursorScreenPos();

    // add image
    ImGui::GetWindowDrawList()->AddImage(
        (void*)texture_id,
        imgPos,
        ImVec2(imgPos.x + EditorWindowWidth, imgPos.y + EditorWindowHeight),
        ImVec2(0,1),
        ImVec2(1,0)
    );

    ImGui::End();


        // --- Settings Window (20%) ---
    ImGui::SetNextWindowPos(ImVec2(ScreenWidth * 0.8f, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(ScreenWidth * 0.2f, ScreenHeight), ImGuiCond_Always);
    ImGui::Begin("Settings",
                 nullptr,
                 ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoCollapse);

    //--------------------------------------------------------------------
    ImGui::SeparatorText("Status");
    if(ImGui::Button("Toggle Wireframe")) { wire = !wire; }
    ImGui::Checkbox("Flat Shading", &flatshade);
    ImGui::Checkbox("Project Circle", &projectCircle);


    //--------------------------------------------------------------------
    ImGui::SeparatorText("Brush Settings");
    ImGui::SliderFloat("Brush Radius", &brush.radius, 0.1f, 100.0f);
    ImGui::SliderFloat("Brush Strength", &brush.strength, 0.01f, 10.0f);

    //--------------------------------------------------------------------
    ImGui::SeparatorText("Keybinds");
    ImGui::Text("[F] Wireframe toggle");
    ImGui::Text("[E/Q] Up/Down");
    ImGui::Text("[TAB] Smooth shade toggle");
    ImGui::Text("[SCRLWHL] Brush radius");
    ImGui::Text("[MB1] Raise terrain");
    ImGui::Text("[Shift + MB1] Lower terrain");
    ImGui::Text("[MMB] Smooth terrain");
    ImGui::Text("[LCTRL + MB1] Raise terrain, no falloff");
    
    ImGui::End();

    return imgPos;
}