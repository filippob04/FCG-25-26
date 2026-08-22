#define GLAD_GL_IMPLEMENTATION
#include "../../resources/glad/gl.h"
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <glm/mat4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/component_wise.hpp>

#include <vector>
#include <string>
#include <iostream>
#include <cstdlib>
#include <ctime>

#include "../../resources/include/matrices.hh"
#include "../../resources/include/mesh.hh"
#include "../../resources/include/hotshaders.hh"
#include "../../resources/include/physics_adv.hh"

/////////////////////////////
// Window and OpenGL setup //
/////////////////////////////

class Setup
{
public:
    static const int window_width = 1024;
    static const int window_height = 768;

    sf::Window* window;

    Setup ()
    {
        sf::ContextSettings settings;
        settings.depthBits = 32;
        settings.stencilBits = 8;
        settings.antiAliasingLevel = 8;
        settings.attributeFlags = sf::ContextSettings::Attribute::Core;
        settings.majorVersion = 4;
        settings.minorVersion = 1;

        window = new sf::Window (
                                 sf::VideoMode({window_width, window_height}),
                                 "S6393212 - 09.cpp",
                                 sf::Style::Default,
                                 sf::State::Windowed,
                                 settings
                                 );
        window->setVerticalSyncEnabled (true);

        if (!window->setActive (true)) {
            std::cerr << "Failure: error during SFML OpenGL Activation." << std::endl;
            exit (1);
        }
        sf::ContextSettings gotten = window->getSettings ();

        std::cout << "depth bits: " << gotten.depthBits << std::endl;
        std::cout << "stencil bits: " << gotten.stencilBits << std::endl;
        std::cout << "antialiasing level: " << gotten.antiAliasingLevel << std::endl;
        std::cout << "SFML GL version: " << gotten.majorVersion << "." << gotten.minorVersion << std::endl;

        int version = gladLoadGL (sf::Context::getFunction);
        if (!version) {
            std::cerr << "Failure: error during glad loading." << std::endl;
            exit (1);
        }
        std::cout << "GLAD GL version: " << GLAD_VERSION_MAJOR(version) << "." << GLAD_VERSION_MINOR(version) << std::endl;
    }

    ~Setup ()
    {
        delete window;
    }
};

////////////////////
// Camera + World //
////////////////////

class Lights
{
public:
    glm::vec3 sun_pos = {1000.0, 2000.0, 0.0};     // sky high light
    glm::vec3 light_direct_pos = {0.0, 0.0, 0.0};   // xyz (absolute, in world coordinates)
    glm::vec3 light_direct_val = {0.75, 0.73, 0.70};   // rgb
    glm::vec3 light_ambient_val = {0.15, 0.15, 0.15};  // rgb

    // not used
    glm::vec3 material_diffuse = {1.0, 1.0, 1.0};   // rgb
    glm::vec3 material_ambient = {1.0, 1.0, 1.0};   // rgb

    glm::vec3 material_specular = {0.15, 0.15, 0.15};  // rgb
    float material_shininess = 32.0; // scalar

public:
    Lights ()
    {
        light_direct_pos = sun_pos;
    }

    void send_parameters (const fcg::Shaders& current_shader)
    {
        glUniform3fv (glGetUniformLocation(current_shader.program, "light.direct_val"), 1, &light_direct_val[0]);
        glUniform3fv (glGetUniformLocation(current_shader.program, "light.ambient_val"), 1, &light_ambient_val[0]);
        glUniform3fv (glGetUniformLocation(current_shader.program, "material.diffuse"), 1, &material_diffuse[0]);
        glUniform3fv (glGetUniformLocation(current_shader.program, "material.ambient"), 1, &material_ambient[0]);
        glUniform3fv (glGetUniformLocation(current_shader.program, "material.specular"), 1, &material_specular[0]);
        glUniform1fv (glGetUniformLocation(current_shader.program, "material.shininess"), 1, &material_shininess);
    }

    void send_position (const fcg::Shaders& current_shader)
    {
        glUniform3fv (glGetUniformLocation(current_shader.program, "light.direct_pos"), 1, &light_direct_pos[0]);
    }
};

class Camera
{
public:
    glm::mat4 v; // view matrix
    glm::mat4 p; // projection matrix
    glm::mat4 inv_v;
    glm::mat4 vp; // final matrix
    
private:

    /** Intrinsic camera parameters **/
    const float fd = 50.0 / 18.0; // focal distance
    float ar; // aspect ratio

    /** Extrinsic camera parameters **/
    // xyz, starting point of dynamic camera position
    glm::vec3 camera_pos = {0.0, 0.0, 0.0}; // xyz
    glm::quat camera_dir = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // orientation
    glm::vec3 camera_offset = glm::vec3(0.0, 0.5, 5.0); // camera offset

    const glm::vec3 OFFSET_VAR = glm::vec3 (0.0, 0.5, 5.0);

public:
    Camera ()
    {
        set_window_size (Setup::window_width, Setup::window_height);
    }

    void send_position (const fcg::Shaders& shaders)
    {   
        GLint camera_pos_loc = glGetUniformLocation (shaders.program, "camera_pos");
        glUniform3fv(camera_pos_loc, 1, &camera_pos[0]);
    }

    void set_window_size (const int w, const int h)
    {
        ar = ((float) w) / (float) h;
        update_projection ();
    }

    void update_projection ()
    {
        float ncp = 0.1f; float fcp = 2000.0f; // fixed

        // prepare projection matrix
        float a = (fcp + ncp) / (ncp - fcp);       // coefficient 3rd col
        float b = 2.0 * fcp * ncp / (ncp - fcp);   // coefficient 4th col

        // compute projecton matrix
        p = glm::mat4(
                fd,  0.0,     0.0,  0.0,    // 1st column
                0.0, fd * ar, 0.0,  0.0,    // 2nd column
                0.0, 0.0,       a, -1.0,    // 3rd column
                0.0, 0.0,       b,  0.0     // 4th column
        );
    }

    // now the camera is attached to the rigid body, this method converts the physics engine quat to the cameras euler angles
    void attach_to(const glm::vec3& target_pos, const glm::quat& target_dir, const bool show_hud, const float dt) {
        if (show_hud) { // like before, fixed camera
            camera_pos = target_pos;
            camera_dir = target_dir;
        } else {
            const float ADJ_SPEED = 5.0;
            glm::vec3 offset = camera_offset; // camera offset
            glm::vec3 rot_offset = target_dir * offset; // applies rotation

            // dynamic camera adjustments, same as physics_adv.hh
            glm::vec3 pos_diff = (target_pos + rot_offset) - camera_pos;
            camera_pos += (pos_diff * ADJ_SPEED * dt);

            glm::quat rot_diff =  target_dir - camera_dir;
            camera_dir += (rot_diff * ADJ_SPEED * dt); 
            camera_dir = glm::normalize(camera_dir); // normalize
        }

        // computes view matrix
        glm::mat4 rot_matrix = glm::mat4_cast(camera_dir); // uses a quaternion instead of euler angles
        glm::mat4 t = fcg::translation(-camera_pos.x, -camera_pos.y, -camera_pos.z);

        v = glm::transpose(rot_matrix) * t;
        
        vp = p * v; // joins view and projection matrices
        inv_v = glm::inverse(v);
    }

    void inc_offset () {if(camera_offset.z < 10.0f) {camera_offset += OFFSET_VAR;}}
    void dec_offset () {if(camera_offset.z > 0.5f) {camera_offset -= OFFSET_VAR;}}
};

class GPUMesh
{
public:
    glm::vec3 min_bounds;
    glm::vec3 max_bounds;
    glm::vec3 center;
    glm::vec3 extent;
    float span;
    glm::mat4 to_unit_extent; // normalization model matrix
    glm::vec3 unit_center;
    glm::vec3 unit_extent;
    float unit_span;

private:
    std::vector<float> points = {};
    std::vector<unsigned int> indices = {};

    GLuint vbo;
    GLuint ebo;
    GLuint vao;
    bool initialized = false;

public:
    GPUMesh (std::string filename){ load (filename); }

    ~GPUMesh () { clean (); }

    void load (std::string filename)
    {
        fcg::Mesh mesh (filename);
        mesh.pack4gpu (points, indices);
        send_arrays_2a3f ();

        min_bounds = mesh.min_bounds;
        max_bounds = mesh.max_bounds;
        center = (min_bounds + max_bounds) * 0.5f;
        span = glm::distance (max_bounds, min_bounds);
        extent = max_bounds - min_bounds;

        std::cout <<"MESH: "<< filename << "\n";
        std::cout <<"(original) center, extent, span:" << "\n";
        std::cout << center.x <<" "<< center.y <<" "<< center.z << "\n";
        std::cout << extent.x <<" "<< extent.y <<" "<< extent.z << "\n";
        std::cout << span << "\n";

        to_unit_extent =
            fcg::scaling (1.0 / glm::compMax (extent)) *
            fcg::translation (-center);

        unit_center = {0.0, 0.0, 0.0};
        unit_span = glm::distance (extent, {0.0, 0.0, 0.0});
        unit_extent = extent / glm::compMax (extent);

        std::cout <<"(unit normalized) center, extent, span:" << "\n";
        std::cout << unit_center.x <<" "<< unit_center.y <<" "<< unit_center.z << "\n";
        std::cout << unit_extent.x <<" "<< unit_extent.y <<" "<< unit_extent.z << "\n";
        std::cout << unit_span << "\n\n";

        initialized = true;
    }

    void clean ()
    {
        if (initialized) {
            glDeleteVertexArrays (1, &vao);
            glDeleteBuffers (1, &vbo);
        }
    }

    void draw ()
    {
        glBindVertexArray (vao);
        glDrawElements(GL_TRIANGLES, indices.size (), GL_UNSIGNED_INT, 0);
    }

protected:
    // send to the gpu the mesh arrays:
    // - the mesh vertices, 2 attributes, 3 floats each
    // - the mesh indices
    void send_arrays_2a3f ()
    {
        // we want just one buffer, and we retrieve the name OpenGL assigns to it.
        glGenBuffers (1, &vbo);
        // bind it as the current VBO
        glBindBuffer (GL_ARRAY_BUFFER, vbo);
        // transfer data from CPU RAM to GPU RAM.
        glBufferData (GL_ARRAY_BUFFER,
                      points.size () * sizeof (float),
                      points.data (),
                      GL_STATIC_DRAW);

        // we want just one buffer container, and we retrieve the name OpenGL assigns to it.
        glGenVertexArrays (1, &vao);
        // bind it as the current vao.
        glBindVertexArray (vao);

        // Attribute 0: position (x, y, z)
        glVertexAttribPointer (0,
                               3,
                               GL_FLOAT,
                               GL_FALSE,
                               6 * sizeof(float),
                               (void*)0);
        glEnableVertexAttribArray (0);

        // Attribute 1: 3 generic floats (u, v, w)
        glVertexAttribPointer (1,
                               3,
                               GL_FLOAT,
                               GL_FALSE,
                               6 * sizeof(float),
                               (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray (1);

        glGenBuffers(1, &ebo); 
        // MUST be bound after the VAO's binding!
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     indices.size () * sizeof (unsigned int),
                     indices.data (),
                     GL_STATIC_DRAW);
    }
};

class Skybox {
    public:
        Skybox (const std::vector<std::string> path) {
            glGenTextures(1, &id); // creates 1 texture and memorizes its id
            glBindTexture(GL_TEXTURE_CUBE_MAP, id); // its a cubemap!

            for (int i = 0; i < 6; ++i) {
                sf::Image cf; // current face of the cubeMap (six in total)

                if(!cf.loadFromFile(path.at(i))) {
                    std::cerr << "Failure: error during SFML Skybox load." << std::endl;
                    exit (1);
                }

                sf::Vector2u dimensions = cf.getSize(); // width, height
                const std::uint8_t* pixelData = cf.getPixelsPtr(); // its a pointer to the first image byte in ram

                glTexImage2D( // gpu loading
                    GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, // X, -X, Y, -Y, Z, -Z
                    0,
                    GL_RGBA,
                    dimensions.x,
                    dimensions.y,
                    0,
                    GL_RGBA,
                    GL_UNSIGNED_BYTE,
                    pixelData
                );

                // disable mipmap
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

                // no edge wrapping
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
            }
        }
        ~Skybox() {glDeleteTextures(1, &id);}

        void Bind() {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_CUBE_MAP, id);
        }

        void Unbind() {glBindTexture(GL_TEXTURE_CUBE_MAP, 0);}
    private:
        GLuint id;
};

class Texture2D {
    public:
        Texture2D(const std::string& path, const bool repeat) {
            sf::Image img;

                if(!img.loadFromFile(path)) {
                    std::cerr << "Failure: error during SFML Texture load." << std::endl;
                    exit (1);
                }
            
                glGenTextures(1, &id);
                glBindTexture(GL_TEXTURE_2D, id);

                sf::Vector2u dimensions = img.getSize(); // width, height
                const std::uint8_t* pixelData = img.getPixelsPtr(); // its a pointer to the first image byte in ram

                glTexImage2D( // gpu loading
                    GL_TEXTURE_2D,
                    0,
                    GL_RGBA,
                    dimensions.x,
                    dimensions.y,
                    0,
                    GL_RGBA,
                    GL_UNSIGNED_BYTE,
                    pixelData
                );

                if (repeat) {
                    // repeat ground texture
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

                    // what happens to the texture the closer (further) the camera is 
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

                    // creates the mipmap
                    glGenerateMipmap(GL_TEXTURE_2D);
                } else { // HUD, no mipmapping
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                }
        }
        ~Texture2D() {glDeleteTextures(1, &id);}
        
        void Bind(GLuint unit = 0) { // it can bind more than one texture
            glActiveTexture(GL_TEXTURE0 + unit);
            glBindTexture(GL_TEXTURE_2D, id);
        }

        void Unbind() {glBindTexture(GL_TEXTURE_2D, 0);}
    private:
        GLuint id;
    };

class Audio {
    private:
        sf::SoundBuffer engineBuffer;
        sf::SoundBuffer clickBuffer;
        sf::SoundBuffer crashBuffer;
        sf::SoundBuffer terrainAlarmBuffer;
        sf::SoundBuffer stallAlarmBuffer;

        float current_pitch = 1.0f;
        float target_pitch = current_pitch;
    public:
        sf::Sound engine;
        sf::Sound click;
        sf::Sound crash;
        sf::Sound alarm_terrain;
        sf::Sound alarm_stall;

        sf::Music ambient;
    public:
        Audio (const std::string dirname) : 
            engine(engineBuffer), 
            click(clickBuffer), 
            crash(crashBuffer), 
            alarm_terrain(terrainAlarmBuffer),
            alarm_stall(stallAlarmBuffer)
        {
            if (!ambient.openFromFile(dirname + "sound/ambient.mp3")) {
                std::cerr << "Failure: error during SFML Audio Loading." << std::endl;
            }

            if (!engineBuffer.loadFromFile(dirname + "sound/engine.wav")) {
                std::cerr << "Failure: error during SFML Audio Loading." << std::endl;
            }

            if (!clickBuffer.loadFromFile(dirname + "sound/click.wav")) {
                std::cerr << "Failure: error during SFML Audio Loading." << std::endl;
            }

            if (!crashBuffer.loadFromFile(dirname + "sound/crash.wav")) {
                std::cerr << "Failure: error during SFML Audio Loading." << std::endl;
            }

            if (!terrainAlarmBuffer.loadFromFile(dirname + "sound/terrain.wav")) {
                std::cerr << "Failure: error during SFML Audio Loading." << std::endl;
            }

            if (!stallAlarmBuffer.loadFromFile(dirname + "sound/stall.wav")) {
                std::cerr << "Failure: error during SFML Audio Loading." << std::endl;
            }

            // sound properties
            engine.setLooping(true); // loops
            engine.setPitch(current_pitch);
            // ambient noise
            ambient.setLooping(true);
            // alarm sounds
            alarm_terrain.setLooping(true); 
            alarm_stall.setLooping(true); 

            attenuation (true); // starts inside the cockpit
        }

        void set_speed (const float current_speed) { // set sound pitch based on aircraft speed, not throttle level
            target_pitch = current_speed * 0.17f;
        }

        void set_pitch (const float dt) { // dynamic pitch
            float diff = target_pitch - current_pitch;
            current_pitch += diff * dt;
            engine.setPitch(current_pitch);
        }

        void stop () { // stops all sounds
            engine.stop();
            click.stop();
            crash.stop();
            ambient.stop();
            alarm_stall.stop();
            alarm_terrain.stop();
        }

        void t_up () { // throttle level up
            click.setPitch(1.1f);
            click.play();
        }

        void t_down () { // throttle level down
            click.setPitch(0.9f);
            click.play();
        }

        void stuck () { // stuck throttle
            click.setPitch(0.3f);
            click.play();
        }

        void attenuation (const bool isInside) { // inside/outside difference
            if (isInside) {
                engine.setVolume(32.5f);
                click.setVolume(40.0f);
                ambient.setVolume(20.0f);
                alarm_stall.setVolume(100.0f);
                alarm_terrain.setVolume(100.0f);
            } else {
                engine.setVolume(75.0f);
                click.setVolume(20.0f);
                ambient.setVolume(70.0f);
                alarm_stall.setVolume(50.0f);
                alarm_terrain.setVolume(50.0f);
            }
        }
};

struct building { // shared struct
    glm::mat4 model;
    Texture2D* texture;
};

class Scene_aux 
{
    public:
        fcg::Airplane& airplane;
        Audio audio;
    public:
        Scene_aux (const std::string dirname, fcg::Airplane& airplane_ref) :
        airplane(airplane_ref),
        audio(dirname) 
        {
            // audio
            audio.engine.play();
            audio.ambient.play();
        }
        ~Scene_aux() {}

        // method to check ground and building collision
        bool check_collision (const std::vector<building>& buildings) {
            check_bounds(); // checks for altitude and speed
            if (airplane.position.y <= 0.5f) {
                std::cout << "ground check" << std::endl;
                return true;
            } else if (airplane.position.y >= 50.f) {
                std::cout << "you flew too high" << std::endl;
                return true;
            }
            for (building b : buildings) {
                // size
                float b_sx = b.model[0][0];
                float b_sy = b.model[1][1];
                float b_sz = b.model[2][2];
                // position
                float b_x = b.model[3][0];
                // float b_y = b.model[3][1];
                float b_z = b.model[3][2];

                if (airplane.position.x >= (b_x - b_sx/2) - 0.5 &&
                    airplane.position.x <= (b_x + b_sx/2) + 0.5 &&
                    airplane.position.y <= (b_sy) + 0.3 &&
                    airplane.position.z >= (b_z - b_sz/2 - 0.5) &&
                    airplane.position.z <= (b_z + b_sz/2) + 0.5) {
                    std::cout <<"building hit" << std::endl;
                    return true;
                }
            }
            return false;
        }

        // checks for altitude and low speed
        void check_bounds () {
            if (airplane.position.y < 3.0f) {
                if (audio.alarm_terrain.getStatus() != sf::Sound::Status::Playing) {audio.alarm_terrain.play();}
            } else if (airplane.get_speed() < 3.0f) {
                if (audio.alarm_stall.getStatus() != sf::Sound::Status::Playing) {audio.alarm_stall.play();}
            } else { // stops
                audio.alarm_terrain.stop();
                audio.alarm_stall.stop();
            }
        }
};

class Scene
{
public:
    Camera camera;
    fcg::Airplane airplane; // logic airplane model
    Scene_aux scene_aux; // audio and logic helper

    // drawable elements
    std::vector<building> buildings;
    struct hud_element {
        glm::vec3 size;
        glm::vec3 pos;
        Texture2D* texture;
    };
    std::vector<hud_element> hud;
    bool show_hud = true;
    
private:
    Lights lights;
    Skybox skybox; // skybox

    GPUMesh cube;
    GPUMesh aircraft;

    // Textures
    Texture2D* blendmap = nullptr;
    std::vector<Texture2D*> ground;
    std::vector<Texture2D*> buildings_t;
    Texture2D* aircraft_t = nullptr;
    std::vector<Texture2D*> hud_t; 

    // Shaders
    fcg::Shaders& main_shaders;
    fcg::Shaders& skybox_shaders;
    fcg::Shaders& building_shaders;
    fcg::Shaders& hud_shaders;
    fcg::Shaders& aircraft_shaders;

    // gpu loc
    GLint blendMap_loc;
    std::vector<GLint> texture_loc;
    GLint tiling_loc;
    GLint building_loc;
    GLint hud_loc;
    GLint aircraft_loc;
public:
    Scene (const std::string dirname, fcg::Shaders& main_sh, fcg::Shaders& skybox_sh, fcg::Shaders& building_sh, fcg::Shaders& hud_sh, fcg::Shaders& aircraft_sh) :
        camera (), 
        scene_aux(dirname, airplane),
        lights (),
        skybox({
            dirname + "texture/skybox/clear/right.png", dirname + "texture/skybox/clear/left.png", 
            dirname + "texture/skybox/clear/up.png", dirname + "texture/skybox/clear/down.png", 
            dirname + "texture/skybox/clear/front.png", dirname + "texture/skybox/clear/back.png"
        }),
        cube (dirname + "off/cube.off"),
        aircraft (dirname + "off/cessna.off"),
        main_shaders(main_sh),
        skybox_shaders(skybox_sh),
        building_shaders(building_sh),
        hud_shaders(hud_sh),
        aircraft_shaders(aircraft_sh)
    {  
        // scene initialize
        init_scene (dirname);
        locations ();
    }
    ~Scene() {
        if (blendmap != nullptr) {delete blendmap;}
        if(!ground.empty()) {
            for (int i = 0; i < ground.size(); ++i) {
                delete ground.at(i);
            }
            ground.clear();
        }
        if(!buildings.empty()) {buildings.clear();}
        if(!buildings_t.empty()) {
            for (int i = 0; i < buildings_t.size(); ++i) {
                delete buildings_t.at(i);
            }
            buildings_t.clear();
        }
        if(!hud.empty()) {hud.clear();}
        if(!hud_t.empty()) {
            for (int i = 0; i < hud_t.size(); ++i) {
                delete hud_t.at(i);
            }
            hud_t.clear();
        }
        if (aircraft_t != nullptr) {delete aircraft_t;}
    }

    void draw ()
    {   
        // clear the buffers
        glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // draw skybox
        draw_skybox(skybox_shaders);

        glm::mat4 root = fcg::identity ();

        if (!show_hud) {draw_aircraft(root, aircraft_shaders);}
        draw_buildings(root, building_shaders);
        draw_floor (root, main_shaders);
        if (show_hud) {draw_hud(camera, hud_shaders, airplane);}
    }

private:
   void locations ()
    {   // ground
        main_shaders.use();
        blendMap_loc = glGetUniformLocation(main_shaders.program, "blend_map");
        for (int i = 0; i < 4; ++i) { // needs all textures at the same time
            texture_loc.push_back(glGetUniformLocation(main_shaders.program, (std::string("texture") + std::to_string(i)).c_str()));
        }
        tiling_loc = glGetUniformLocation(main_shaders.program, "tiling_factor");
        glUniform1i(blendMap_loc, 0);
        for (int i = 0; i < ground.size(); ++i) {
            glUniform1i(texture_loc.at(i), i+1);
        } 
        glUniform1f(tiling_loc, 300.0f);

        // buildings
        building_shaders.use();
        building_loc = glGetUniformLocation(building_shaders.program, "buildingTexture"); // overload
        glUniform1i(building_loc, 0);

        // hud
        hud_shaders.use();
        hud_loc = glGetUniformLocation(hud_shaders.program, "hudTexture");
        glUniform1i(hud_loc, 0);

        // aircraft model
        aircraft_shaders.use();
        aircraft_loc = glGetUniformLocation(aircraft_shaders.program, "aircraftTexture");
        glUniform1i(aircraft_loc, 0);
    }

    void init_scene (const std::string dirname) {
        init_ground(dirname);
        init_buildings(dirname);
        init_airplane(dirname);
        init_hud(dirname);
    }

    void init_ground (const std::string dirname) {
        blendmap = new Texture2D(dirname + "texture/ground/BlendMap.png", false);

        // ground textures
        ground.push_back(new Texture2D(dirname + "texture/ground/terrain/city_grass.png", true));
        ground.push_back(new Texture2D(dirname + "texture/ground/terrain/road_tarmac5.png", true));
        ground.push_back(new Texture2D(dirname + "texture/ground/terrain/city_roofs.png", true));
        ground.push_back(new Texture2D(dirname + "texture/ground/terrain/dark_water.jpg", true));
    }
    // returns a vec3 containing three random floats in [MIN, MAX]
    glm::vec3 random_vec3 (const glm::vec3 MIN, const glm::vec3 MAX) {
        const glm::vec3 RANGE = MAX - MIN;
        float x = RANGE.x * ((((float) rand()) / (float) RAND_MAX)) + MIN.x; 
        float y = RANGE.y * ((((float) rand()) / (float) RAND_MAX)) + MIN.y; 
        float z = RANGE.z * ((((float) rand()) / (float) RAND_MAX)) + MIN.z; 
        return glm::vec3 (x, y, z);
    }

    bool check_overlap (const glm::mat4& current) { // similar to airplane check_collision
        // size
        float c_sx = current[0][0]/2;
        float c_sz = current[2][2]/2;
        // position
        float c_x = current[3][0];
        float c_z = current[3][2];
        for (building& b : buildings) {
                // size
                float b_sx = b.model[0][0]/2;
                float b_sz = b.model[2][2]/2;
                // position
                float b_x = b.model[3][0];
                float b_z = b.model[3][2];

                bool overlap_x = (c_x - c_sx) <= (b_x + b_sx) && (c_x + c_sx) >= (b_x - b_sx);
                bool overlap_z = (c_z - c_sz) <= (b_z + b_sz) && (c_z + c_sz) >= (b_z - b_sz);

                if (overlap_x && overlap_z) {return true;}
        }
        return false;
    }

    void building_cluster (const glm::vec3 center, const int n) {
        for (unsigned int i = 0; i < n; ++i) {
            building b;
            b.texture = buildings_t.at(i % buildings_t.size());
            glm::mat4 size, pos;
            do {
                glm::vec3 rs = random_vec3(glm::vec3(2.0, 5.0, 2.0), glm::vec3(3.0, 18.0, 3.0));
                size = fcg::scaling(rs.x, rs.y, rs.z);

                glm::vec3 rp = random_vec3(glm::vec3(-10.0, 0.0, -10.0), glm::vec3(10.0, 0.0, 10.0));
                pos = fcg::translation(rp.x + center.x, (rs.y/2.0) + center.y, rp.z + center.z); // rs.y/2.0 so it stays on the ground

                b.model = pos * size;
            } while (check_overlap(b.model));
            buildings.push_back(b);
        }
    }

    void init_buildings (const std::string dirname) {

        // buildings textures
        std::string tex_dir = dirname + "texture/building/";
        int n_tex = 15;
        for (int i = 0; i <= n_tex; ++i) {
            std::string filepath = tex_dir + std::to_string(i) + ".jpg";
            buildings_t.push_back(new Texture2D(filepath, true));
        }

        building_cluster(glm::vec3(0.0, 0.0, 20.0), 10);
        building_cluster(glm::vec3(-120.0, 0.0, -50.0), 5);
        building_cluster(glm::vec3(20.0, 0.0, -60.0), 10);
        building_cluster(glm::vec3(-15.0, 0.0, -75.0), 15);
        building_cluster(glm::vec3(5.0, 0.0, -120.0), 10);
        building_cluster(glm::vec3(65.0, 0.0, -120.0), 10);
        building_cluster(glm::vec3(-45.0, 0.0, 25.0), 5);
        building_cluster(glm::vec3(40.0, 0.0, 30.0), 10);
        building_cluster(glm::vec3(-150.0, 0.0, 0.0), 15);
        building_cluster(glm::vec3(-130, 0.0, -150.0), 10);
    }

    void init_airplane (const std::string dirname) {
        airplane.position = { 0.0f, 10.0f, 0.0f };
        airplane.orientation = {1.0f, 0.0f, 0.0f, 0.0f};
        aircraft_t = new Texture2D(dirname + "texture/aircraft/alloy.png", true);
    }

    void init_hud (const std::string dirname) {
        hud_t.push_back(new Texture2D(dirname + "texture/aircraft/cockpit.png", false)); // 0
        hud_t.push_back(new Texture2D(dirname + "texture/aircraft/yaw.png", false)); // 1
        hud_t.push_back(new Texture2D(dirname + "texture/aircraft/roll.png", false)); // 2
        hud_t.push_back(new Texture2D(dirname + "texture/aircraft/speed.png", false)); // 3
        hud_t.push_back(new Texture2D(dirname + "texture/aircraft/yoke.png",false)); // 4

        unsigned int i = 0;
        {
            hud_element h;
            h.texture = hud_t.at(i);
            i++;
            h.size = glm::vec3(1.6, 1.3, 0.001);;
            h.pos = glm::vec3(0.0, -0.4, -2.02);
            hud.push_back(h);
        }
        {
            hud_element h;
            h.texture = hud_t.at(i);
            i++;
            h.size = glm::vec3(0.9, 0.9, 0.001);
            h.pos = glm::vec3(0.202, -0.485, -2.01);
            hud.push_back(h);
        }
        {
            hud_element h;
            h.texture = hud_t.at(i);
            i++;
            h.size = glm::vec3(0.85, 0.85, 0.001);;
            h.pos = glm::vec3(-0.05, -0.407, -2.00);
            hud.push_back(h);
        }
        {
            hud_element h;
            h.texture = hud_t.at(i);
            i++;
            h.size = glm::vec3(1.0, 1.0, 0.001);;
            h.pos = glm::vec3(-0.30, -0.40, -1.99);
            hud.push_back(h);
        }
        {
            hud_element h;
            h.texture = hud_t.at(i);
            i++;
            h.size = glm::vec3(1.0, 1.0, 0.001);;
            h.pos = glm::vec3(-0.06, -0.6, -1.96); // further to make pitch translation possible
            hud.push_back(h);
        }
    }

    void draw_cube (const glm::mat4 parent_mm, fcg::Shaders& current_shader)
    {
        glm::mat4 mm;
        glm::mat3 ti_mm;
        mm = parent_mm * cube.to_unit_extent;
        ti_mm = glm::transpose (glm::inverse (glm::mat3 (mm)));

        GLint m_loc = glGetUniformLocation(current_shader.program, "model");
        GLint tim_loc = glGetUniformLocation(current_shader.program, "tr_inv_model");

        glUniformMatrix4fv(m_loc, 1, GL_FALSE, &mm[0][0]);
        glUniformMatrix3fv (tim_loc, 1, GL_FALSE, &ti_mm[0][0]);
        cube.draw ();
    }

    float draw_floor (const glm::mat4 parent_mm, fcg::Shaders& ground_shader)
    {   
        ground_shader.use();

        GLint vp_loc_current = glGetUniformLocation(ground_shader.program, "vp");
        glUniformMatrix4fv(vp_loc_current, 1, GL_FALSE, &camera.vp[0][0]);

        camera.send_position(ground_shader);
        lights.send_position(ground_shader);
        lights.send_parameters(ground_shader);

        glm::mat4 scale, translate, mm;
        float depth = 0.04;
        float h_depth = depth * 0.5;
        float height = cube.extent.y * 0.8;
        float h_height = height * 0.05;

        // draw floor
        scale = fcg::scaling (1500, depth, 1500); // flatten the cube!
        translate = fcg::translation (0, -h_height - h_depth, 0); // lower it down
        mm = parent_mm * translate * scale;

        if (blendmap != nullptr) {blendmap->Bind(0);}
        if (!ground.empty()) {
            for (int i = 0; i < ground.size(); ++i) {
                ground.at(i)->Bind(i+1);
            }
        }
        draw_cube (mm, ground_shader);

        return -h_height;
    }

    void draw_buildings (const glm::mat4 parent_mm, fcg::Shaders& building_shaders) {
        building_shaders.use();

        GLint vp_loc_current = glGetUniformLocation(building_shaders.program, "vp");
        glUniformMatrix4fv(vp_loc_current, 1, GL_FALSE, &camera.vp[0][0]);

        camera.send_position(building_shaders);
        lights.send_position(building_shaders);
        lights.send_parameters(building_shaders);

        for (building b : buildings) {
            b.texture->Bind(0); 
            draw_cube(parent_mm * b.model, building_shaders);
        }
    }

    void draw_hud (const Camera camera, fcg::Shaders& hud_shaders, fcg::Airplane airplane) {
        hud_shaders.use();

        GLint vp_loc_current = glGetUniformLocation(hud_shaders.program, "vp");
        glUniformMatrix4fv(vp_loc_current, 1, GL_FALSE, &camera.vp[0][0]);

        glEnable(GL_BLEND); // this makes png background see-through
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        unsigned int i = 0;
        for (hud_element h : hud) {
            h.texture->Bind(0); 

            float angle = 0.0f; 
            float offset = 0.0f;
            glm::mat4 scale = fcg::scaling(h.size.x, h.size.y, h.size.z);
            glm::mat4 rot = fcg::rotation_z(0.0f);
            glm::mat4 trans = fcg::translation(h.pos.x, h.pos.y, h.pos.z);

            // animations
            if (i == 1) { // yaw thingy
                offset = airplane.get_yaw() * 0.03f;
                trans = fcg::translation(h.pos.x + offset, h.pos.y, h.pos.z);
            }
            if (i == 2) { // virtual horizon
                angle = - airplane.get_horizon() * 50.0f;
                rot = fcg::rotation_z(angle);
            }
            if (i == 3) { // air speed
                angle = - airplane.get_speed() * 25.0f;
                rot = fcg::rotation_z(angle);
            }
            if (i == 4) { // yoke
                angle = airplane.get_roll() * 10.0f;
                rot = fcg::rotation_z(angle);
                offset = (airplane.get_pitch() * 0.05f) + h.pos.z;
                offset = glm::clamp(offset, -1.98f, -1.5f);
                trans = fcg::translation(h.pos.x, h.pos.y, offset);
            }

            glm::mat4 model = camera.inv_v * trans * rot * scale;

            draw_cube(model, hud_shaders);
            i++;
        }

        glDisable(GL_BLEND);
    }

    void draw_skybox (fcg::Shaders& skybox_shaders) 
    {   
        skybox_shaders.use();

        glm::mat4 viewnt = glm::mat4(glm::mat3(camera.v)); // removing translation
        glm::mat4 vp = camera.p * viewnt;

        GLint vp_loc_current = glGetUniformLocation(skybox_shaders.program, "vp");
        glUniformMatrix4fv(vp_loc_current, 1, GL_FALSE, &vp[0][0]);

        GLint model_loc = glGetUniformLocation(skybox_shaders.program, "model");
        glUniformMatrix4fv(model_loc, 1, GL_FALSE, &cube.to_unit_extent[0][0]);

        GLint tex_loc = glGetUniformLocation(skybox_shaders.program, "gCubemapTexture");
        glUniform1i(tex_loc, 0);

        glDepthFunc(GL_LEQUAL);
        skybox.Bind();
        glDisable(GL_CULL_FACE); 

        cube.draw(); // skyBOX

        glEnable(GL_CULL_FACE);
        glDepthFunc(GL_LESS);
        skybox.Unbind();
    }

    void draw_aircraft (const glm::mat4 parent_mm, fcg::Shaders& aircraft_shaders) {
        aircraft_shaders.use();

        GLint vp_loc_current = glGetUniformLocation(aircraft_shaders.program, "vp");
        glUniformMatrix4fv(vp_loc_current, 1, GL_FALSE, &camera.vp[0][0]);

        camera.send_position(aircraft_shaders);
        lights.send_position(aircraft_shaders);
        lights.send_parameters(aircraft_shaders);

        glm::mat4 scale, rotate, translate, mm, ti_mm, adapt;
        scale = fcg::scaling(1.0f, 1.0f, 1.0f); // possibly increase aircraft size
        adapt = fcg::rotation_y(90.0f); // model heading correction        
        rotate = glm::mat4_cast(airplane.orientation); // rotates as the physics model
        translate = fcg::translation(airplane.position.x, airplane.position.y, airplane.position.z); // moves as well

        mm = parent_mm * translate * rotate * adapt * scale * aircraft.to_unit_extent;
        ti_mm = glm::transpose (glm::inverse (glm::mat3 (mm)));

        GLint m_loc = glGetUniformLocation(aircraft_shaders.program, "model");
        GLint tim_loc = glGetUniformLocation(aircraft_shaders.program, "tr_inv_model");

        glUniformMatrix4fv(m_loc, 1, GL_FALSE, &mm[0][0]);
        glUniformMatrix3fv (tim_loc, 1, GL_FALSE, &ti_mm[0][0]);

        // texture bind
        if (aircraft_t != nullptr) {aircraft_t->Bind(0);}

        aircraft.draw ();
    }
};

////////////////////
// SFML Callbacks //
////////////////////

void handle (const sf::Event::Resized& resized, Camera& camera)
{
    glViewport (0, 0, resized.size.x, resized.size.y);
    camera.set_window_size (resized.size.x, resized.size.y);
}

void handle (const sf::Event::KeyPressed& key, Scene& scene)
{   
    switch (key.scancode) {
    case sf::Keyboard::Scancode::Escape:
        exit (0);
    case sf::Keyboard::Scancode::LShift:
        if(scene.airplane.get_throttle_level() != 5) {
            scene.scene_aux.audio.t_up();
            scene.airplane.throttle_up();
        } else {
            scene.scene_aux.audio.stuck();
        }
        break;
    case sf::Keyboard::Scancode::LControl:
        if(scene.airplane.get_throttle_level() != 0) {
            scene.scene_aux.audio.t_down();
            scene.airplane.throttle_down();
        } else {
            scene.scene_aux.audio.stuck();
        }
        break;
    case sf::Keyboard::Scancode::W:
        scene.airplane.set_pitch_dir(-1.0);
        break;
    case sf::Keyboard::Scancode::A:
        scene.airplane.set_yaw_dir(1.0);
        break;
    case sf::Keyboard::Scancode::S:
        scene.airplane.set_pitch_dir(1.0);
        break;
    case sf::Keyboard::Scancode::D:
        scene.airplane.set_yaw_dir(-1.0);
        break;
    case sf::Keyboard::Scancode::E:
        scene.airplane.set_roll_dir(-1.0);
        break;
    case sf::Keyboard::Scancode::Q:
        scene.airplane.set_roll_dir(1.0);
        break;
    case sf::Keyboard::Scancode::C:
        scene.show_hud = !scene.show_hud; // disables (enables) hud
        scene.scene_aux.audio.attenuation(scene.show_hud);
        break;
    case sf::Keyboard::Scancode::I:
        if(!scene.show_hud){scene.camera.inc_offset();}
        break;
    case sf::Keyboard::Scancode::K:
        if(!scene.show_hud){scene.camera.dec_offset();}
        break;
    default:
        return;
    }
}

void handle (const sf::Event::KeyReleased& key, Scene& scene)
{
    switch (key.scancode) {
    case sf::Keyboard::Scancode::W:
        scene.airplane.set_pitch_dir(0.0);
        break;
    case sf::Keyboard::Scancode::A:
        scene.airplane.set_yaw_dir(0.0);
        break;
    case sf::Keyboard::Scancode::S:
        scene.airplane.set_pitch_dir(0.0);
        break;
    case sf::Keyboard::Scancode::D:
        scene.airplane.set_yaw_dir(0.0);
        break;
    case sf::Keyboard::Scancode::E:
        scene.airplane.set_roll_dir(0.0);
        break;
    case sf::Keyboard::Scancode::Q:
        scene.airplane.set_roll_dir(0.0);
        break;
    default:
        return;
    }
}

//////////
// Main //
//////////

int main ()
{
    //// Startup ////
    Setup setup;
    sf::Window& window = *setup.window;

    std::string dirname = "resources/";
    fcg::Shaders main_shaders (dirname + "vert/main_shader.vert", dirname + "frag/main_shader.frag");
    fcg::Shaders skybox_shaders (dirname + "vert/skybox.vert", dirname + "frag/skybox.frag");
    fcg::Shaders building_shaders (dirname + "vert/main_shader.vert", dirname + "frag/building.frag");
    fcg::Shaders hud_shaders (dirname + "vert/hud.vert", dirname + "frag/hud.frag");
    fcg::Shaders aircraft_shaders (dirname + "vert/aircraft.vert", dirname + "frag/aircraft.frag");

    main_shaders.use ();
    srand(time(NULL)); // random buildings
    Scene scene (dirname, main_shaders, skybox_shaders, building_shaders, hud_shaders, aircraft_shaders);

    glEnable (GL_CULL_FACE);
    glCullFace (GL_BACK);

    glEnable (GL_DEPTH_TEST);

    // hide mouse
    window.setMouseCursorGrabbed(true);
    window.setMouseCursorVisible(false);

    //// Main Loop ////

    sf::Clock clock;
    bool running = true;

    while (running)
    {
        while (const std::optional event = window.pollEvent ())
        {
            if (event->is<sf::Event::Closed> ())
                running = false;
            else if (const auto* resized = event->getIf<sf::Event::Resized> ())
                handle (*resized, scene.camera);
            else if (const auto* key_pressed = event->getIf<sf::Event::KeyPressed> ())
                handle (*key_pressed, scene);
            else if (const auto* key_released = event->getIf<sf::Event::KeyReleased> ())
                handle (*key_released, scene);
        }

        float elapsed = clock.restart().asSeconds();

        scene.airplane.update (elapsed);
        scene.camera.attach_to(scene.airplane.position, scene.airplane.orientation, scene.show_hud, elapsed);
        scene.scene_aux.audio.set_speed(scene.airplane.get_speed());
        scene.scene_aux.audio.set_pitch(elapsed);
        
        if (scene.scene_aux.check_collision(scene.buildings)) { // checks for collisions
            running = false;
            
            scene.scene_aux.audio.stop(); // stops all audio
            scene.scene_aux.audio.crash.play(); // sound plays
            glClear(GL_COLOR_BUFFER_BIT); // black screen
            window.display();

            sf::sleep(sf::seconds(0.5f)); // pauses the game, this allows the sound to play correctly
        } else {
            scene.draw ();
            window.display ();
        }
    }
    return 0;
}
