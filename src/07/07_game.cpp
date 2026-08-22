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
                                 "S6393212 - 07.cpp",
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
    glm::vec3 light_ambient_val = {0.2, 0.2, 0.2};  // rgb

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

    void send_parameters (fcg::Shaders& current_shader)
    {
        glUniform3fv (glGetUniformLocation(current_shader.program, "light.direct_val"), 1, &light_direct_val[0]);
        glUniform3fv (glGetUniformLocation(current_shader.program, "light.ambient_val"), 1, &light_ambient_val[0]);
        glUniform3fv (glGetUniformLocation(current_shader.program, "material.diffuse"), 1, &material_diffuse[0]);
        glUniform3fv (glGetUniformLocation(current_shader.program, "material.ambient"), 1, &material_ambient[0]);
        glUniform3fv (glGetUniformLocation(current_shader.program, "material.specular"), 1, &material_specular[0]);
        glUniform1fv (glGetUniformLocation(current_shader.program, "material.shininess"), 1, &material_shininess);
    }

    void send_position (fcg::Shaders& current_shader)
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
    glm::mat4 vp;

private:

    /** Intrinsic camera parameters **/
    const float fd = 50.0 / 18.0; // focal distance
    float ar; // aspect ratio

    /** Extrinsic camera parameters **/
    // xyz, starting point of dynamic camera position
    glm::vec3 camera_pos = {0.0, 0.0, 0.0}; // xyz

    // Angles defining the in-place camera rotation
    float yaw_deg = 0.0; // phi
    float pitch_deg = 0.0; // theta
    float roll_deg = 0.0;

public:
    Camera ()
    {
        set_window_size (Setup::window_width, Setup::window_height);
        view_projection ();
    }

    void send_position (fcg::Shaders& shaders)
    {   
        GLint camera_pos_loc = glGetUniformLocation (shaders.program, "camera_pos");
        glUniform3fv(camera_pos_loc, 1, &camera_pos[0]);
    }

    void set_window_size (int w, int h)
    {
        ar = ((float) w) / (float) h;
        view_projection ();
    }

    void view_projection ()
    {
        const glm::vec3 cp = camera_pos;
        float ncp = 0.1f; float fcp = 2000.0f; // fixed

        // prepare rotations and translation matrices
        glm::mat4 rz = fcg::rotation_z (roll_deg); // new roll matrix
        glm::mat4 ry = fcg::rotation_y (yaw_deg);
        glm::mat4 rx = fcg::rotation_x (pitch_deg);
        glm::mat4 t = fcg::translation (-cp.x, -cp.y, -cp.z);

        // prepare projection matrix
        float a = (fcp + ncp) / (ncp - fcp);       // coefficient 3rd col
        float b = 2.0 * fcp * ncp / (ncp - fcp);   // coefficient 4th col

        p = glm::mat4(
                        fd,  0.0,     0.0,  0.0,    // 1st column
                        0.0, fd * ar, 0.0,  0.0,    // 2nd column
                        0.0, 0.0,       a, -1.0,    // 3rd column
                        0.0, 0.0,       b,  0.0     // 4th column
        );

        // Compute VP matrix and update it
        v = rz * rx * ry * t;
        vp = p * v;
        inv_v = glm::inverse (v);
    }

    // now the camera is attached to the rigid body, this method converts the physics engine quat to the cameras euler angles
    void attach_to(const glm::vec3& target_pos, const glm::quat& target_dir) {
        camera_pos = target_pos;

        glm::mat4 rot_matrix = glm::mat4_cast(target_dir); // uses a quaternion instead of euler angles
        glm::mat4 t = fcg::translation(-camera_pos.x, -camera_pos.y, -camera_pos.z);

        v = glm::transpose(rot_matrix) * t;
        
        vp = p * v;
        inv_v = glm::inverse(v);
    }
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
        Skybox (std::vector<std::string> path) {
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
        Texture2D(const std::string& path) {
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

                // repeat ground texture
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                
                // what happens to the texture the closer (further) the camera is 
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

                // creates the mipmap
                glGenerateMipmap(GL_TEXTURE_2D);
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

        float current_pitch = 1.0f;
        float target_pitch = current_pitch;
    public:
        sf::Sound engine;
        sf::Sound click;
        sf::Sound crash;

        sf::Music ambient;
    public:
        Audio (std::string dirname) : 
            engine(engineBuffer), 
            click(clickBuffer), 
            crash(crashBuffer) 
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

            // sound properties
            engine.setLooping(true); // loops
            engine.setPitch(current_pitch);
            engine.setVolume(75.0f);
            click.setVolume(60.0f);
            // ambient noise
            ambient.setLooping(true);
            ambient.setVolume(50.0f); 
        }

        void set_speed (float current_speed) { // set sound pitch based on aircraft speed, not throttle level
            target_pitch = current_speed * 0.17f;
        }

        void set_pitch (float dt) {
            float diff = target_pitch - current_pitch;
            current_pitch += diff * dt;
            engine.setPitch(current_pitch);
        }

        void stop () {
            engine.stop();
            click.stop();
            crash.stop();
            ambient.stop();
        }

        void t_up () {
            click.setPitch(1.1f);
            click.play();
        }

        void t_down () {
            click.setPitch(0.9f);
            click.play();
        }

        void stuck () {
            click.setPitch(0.3f);
            click.play();
        }
};

class Scene
{
public:
    Camera camera;
    Lights lights;

    GPUMesh cube;
    Skybox skybox;

    Texture2D* blendmap = nullptr;
    std::vector<Texture2D*> ground;

    struct building {
        glm::mat4 model;
        Texture2D* texture;
    };

    std::vector<building> buildings;
    std::vector<Texture2D*> buildings_t;

    fcg::Airplane airplane;

    // hud elements
    struct hud_element {
        glm::vec3 size;
        glm::vec3 pos;
        Texture2D* texture;
    };

    std::vector<hud_element> hud;
    std::vector<Texture2D*> hud_t; 

    Audio audio; // audio
private:
    fcg::Shaders& main_shaders;
    fcg::Shaders& skybox_shaders;
    fcg::Shaders& building_shaders;
    fcg::Shaders& hud_shaders;

    // ground
    GLint blendMap_loc;
    std::vector<GLint> texture_loc;
    GLint tiling_loc;

    GLint building_loc;

    GLint hud_loc;

public:
    Scene (std::string dirname, fcg::Shaders& main_sh, fcg::Shaders& skybox_sh, fcg::Shaders& building_sh, fcg::Shaders& hud_sh) :
        camera (), lights (),
        cube (dirname + "off/cube.off"),
        skybox({
            dirname + "texture/skybox/clear/right.png", dirname + "texture/skybox/clear/left.png", 
            dirname + "texture/skybox/clear/up.png", dirname + "texture/skybox/clear/down.png", 
            dirname + "texture/skybox/clear/front.png", dirname + "texture/skybox/clear/back.png"
        }), 
        audio(dirname),
        main_shaders(main_sh),
        skybox_shaders(skybox_sh),
        building_shaders(building_sh),
        hud_shaders(hud_sh)
    {  
        // ground
        init_ground (dirname);
        
        // buildings
        init_buildings (dirname);

        // player body
        init_airplane ();

        // hud
        init_hud (dirname);

        // audio
        audio.engine.play();
        audio.ambient.play();

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
    }

    void locations ()
    {   
        // ground
        main_shaders.use();

        blendMap_loc = glGetUniformLocation(main_shaders.program, "blend_map");
        for (int i = 0; i < 4; ++i) {
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

        building_loc = glGetUniformLocation(building_shaders.program, "buildingTexture");

        glUniform1i(building_loc, 0);

        // hud
        hud_shaders.use();

        hud_loc = glGetUniformLocation(hud_shaders.program, "hudTexture");

        glUniform1i(hud_loc, 0);
    }

    void init_ground ( std::string dirname) {
        blendmap = new Texture2D(dirname + "texture/ground/BlendMap.png");

        // ground textures
        ground.push_back(new Texture2D(dirname + "texture/ground/terrain/city_grass.png"));
        ground.push_back(new Texture2D(dirname + "texture/ground/terrain/road_tarmac5.png"));
        ground.push_back(new Texture2D(dirname + "texture/ground/terrain/city_roofs.png"));
        ground.push_back(new Texture2D(dirname + "texture/ground/terrain/dark_water.jpg"));
    }

    void init_buildings (std::string dirname) {

        // buildings textures
        buildings_t.push_back(new Texture2D(dirname + "texture/building/2.jpg"));
        buildings_t.push_back(new Texture2D(dirname + "texture/building/6.jpg"));
        buildings_t.push_back(new Texture2D(dirname + "texture/building/9.jpg"));
        buildings_t.push_back(new Texture2D(dirname + "texture/building/3.jpg"));
        buildings_t.push_back(new Texture2D(dirname + "texture/building/0.jpg"));
        buildings_t.push_back(new Texture2D(dirname + "texture/building/5.jpg"));
        buildings_t.push_back(new Texture2D(dirname + "texture/building/8.jpg"));
        buildings_t.push_back(new Texture2D(dirname + "texture/building/4.jpg"));

        unsigned int i = 0;
        // positioning
        {
            building b;
            b.texture = buildings_t.at(i);
            i++;
            glm::mat4 size = fcg::scaling(3.0, 12.0, 3.0);
            glm::mat4 pos = fcg::translation(-27.0, 6.0, -11.0);
            b.model = pos * size;
            buildings.push_back(b);
        }
        {
            building b;
            b.texture = buildings_t.at(i);
            i++;
            glm::mat4 size = fcg::scaling(2.0, 10.0, 3.0);
            glm::mat4 pos = fcg::translation(-23.0, 5.0, -15.0);
            b.model = pos * size;
            buildings.push_back(b);
        }
        {
            building b;
            b.texture = buildings_t.at(i);
            i++;
            glm::mat4 size = fcg::scaling(1.8, 8.0, 2.0);
            glm::mat4 pos = fcg::translation(-24.0, 4.0, -9.0);
            b.model = pos * size;
            buildings.push_back(b);
        }
        {
            building b;
            b.texture = buildings_t.at(i);
            i++;
            glm::mat4 size = fcg::scaling(3.0, 8.0, 2.0);
            glm::mat4 pos = fcg::translation(-20.0, 4.0, -12.0);
            b.model = pos * size;
            buildings.push_back(b);
        }
        {
            building b;
            b.texture = buildings_t.at(i);
            i++;
            glm::mat4 size = fcg::scaling(2.3, 10.0, 2.0);
            glm::mat4 pos = fcg::translation(-3.0, 5.0, -30.0);
            b.model = pos * size;
            buildings.push_back(b);
        }
        {
            building b;
            b.texture = buildings_t.at(i);
            i++;
            glm::mat4 size = fcg::scaling(2.5, 11.0, 2.0);
            glm::mat4 pos = fcg::translation(-5.0, 5.5, -26.0);
            b.model = pos * size;
            buildings.push_back(b);
        }
        {
            building b;
            b.texture = buildings_t.at(i);
            i++;
            glm::mat4 size = fcg::scaling(3.0, 14.0, 2.5);
            glm::mat4 pos = fcg::translation(-10.0, 7.0, -32.0);
            b.model = pos * size;
            buildings.push_back(b);
        }
        {
            building b;
            b.texture = buildings_t.at(i);
            i++;
            glm::mat4 size = fcg::scaling(3.0, 12.0, 2.5);
            glm::mat4 pos = fcg::translation(-11.0, 6.0, -28.0);
            b.model = pos * size;
            buildings.push_back(b);
        }
        i = 0;
        {
            building b;
            b.texture = buildings_t.at(i);
            i++;
            glm::mat4 size = fcg::scaling(3.0, 6.0, 3.0);
            glm::mat4 pos = fcg::translation(-3.0, 3.0, -80.0);
            b.model = pos * size;
            buildings.push_back(b);
        }
        {
            building b;
            b.texture = buildings_t.at(i);
            i++;
            glm::mat4 size = fcg::scaling(2.0, 10.0, 3.0);
            glm::mat4 pos = fcg::translation(1.0, 5.0, -84.0);
            b.model = pos * size;
            buildings.push_back(b);
        }
        {
            building b;
            b.texture = buildings_t.at(i);
            i++;
            glm::mat4 size = fcg::scaling(2.0, 11.0, 2.2);
            glm::mat4 pos = fcg::translation(0.0, 5.5, -82.0);
            b.model = pos * size;
            buildings.push_back(b);
        }
        {
            building b;
            b.texture = buildings_t.at(i);
            i++;
            glm::mat4 size = fcg::scaling(3.0, 8.0, 2.0);
            glm::mat4 pos = fcg::translation(4.0, 4.0, -79.0);
            b.model = pos * size;
            buildings.push_back(b);
        }
        {
            building b;
            b.texture = buildings_t.at(i);
            i++;
            glm::mat4 size = fcg::scaling(2.3, 10.0, 2.0);
            glm::mat4 pos = fcg::translation(-8.0, 5.0, 35.0);
            b.model = pos * size;
            buildings.push_back(b);
        }
        {
            building b;
            b.texture = buildings_t.at(i);
            i++;
            glm::mat4 size = fcg::scaling(2.5, 11.0, 2.0);
            glm::mat4 pos = fcg::translation(-10.0, 5.5, 39.0);
            b.model = pos * size;
            buildings.push_back(b);
        }
        {
            building b;
            b.texture = buildings_t.at(i);
            i++;
            glm::mat4 size = fcg::scaling(3.0, 14.0, 2.5);
            glm::mat4 pos = fcg::translation(-15.0, 7.0, 42.0);
            b.model = pos * size;
            buildings.push_back(b);
        }
        {
            building b;
            b.texture = buildings_t.at(i);
            i++;
            glm::mat4 size = fcg::scaling(3.0, 12.0, 2.5);
            glm::mat4 pos = fcg::translation(-16.0, 6.0, 37.0);
            b.model = pos * size;
            buildings.push_back(b);
        }
        i = 0;
        {
            building b;
            b.texture = buildings_t.at(i);
            i++;
            glm::mat4 size = fcg::scaling(3.0, 6.0, 3.0);
            glm::mat4 pos = fcg::translation(97.0, 3.0, -80.0);
            b.model = pos * size;
            buildings.push_back(b);
        }
        {
            building b;
            b.texture = buildings_t.at(i);
            i++;
            glm::mat4 size = fcg::scaling(2.0, 14.0, 3.0);
            glm::mat4 pos = fcg::translation(101.0, 7.0, -84.0);
            b.model = pos * size;
            buildings.push_back(b);
        }
        {
            building b;
            b.texture = buildings_t.at(i);
            i++;
            glm::mat4 size = fcg::scaling(2.0, 9.0, 2.2);
            glm::mat4 pos = fcg::translation(100.0, 4.5, -82.0);
            b.model = pos * size;
            buildings.push_back(b);
        }
        {
            building b;
            b.texture = buildings_t.at(i);
            i++;
            glm::mat4 size = fcg::scaling(3.0, 10.0, 2.0);
            glm::mat4 pos = fcg::translation(104.0, 5.0, -79.0);
            b.model = pos * size;
            buildings.push_back(b);
        }
        {
            building b;
            b.texture = buildings_t.at(i);
            i++;
            glm::mat4 size = fcg::scaling(2.3, 10.0, 2.0);
            glm::mat4 pos = fcg::translation(-8.0, 5.0, 35.0);
            b.model = pos * size;
            buildings.push_back(b);
        }
        {
            building b;
            b.texture = buildings_t.at(i);
            i++;
            glm::mat4 size = fcg::scaling(2.5, 12.0, 2.0);
            glm::mat4 pos = fcg::translation(-120.0, 6.0, 9.0);
            b.model = pos * size;
            buildings.push_back(b);
        }
        {
            building b;
            b.texture = buildings_t.at(i);
            i++;
            glm::mat4 size = fcg::scaling(3.0, 8.0, 2.5);
            glm::mat4 pos = fcg::translation(-125.0, 4.0, 12.0);
            b.model = pos * size;
            buildings.push_back(b);
        }
        {
            building b;
            b.texture = buildings_t.at(i);
            i++;
            glm::mat4 size = fcg::scaling(3.0, 10.0, 2.5);
            glm::mat4 pos = fcg::translation(-126.0, 5.0, 7.0);
            b.model = pos * size;
            buildings.push_back(b);
        }
    }

    void init_airplane () {

        airplane.position = { 0.0f, 10.0f, 0.0f };
        airplane.orientation = {1.0f, 0.0f, 0.0f, 0.0f};
    }

    void init_hud (std::string dirname) {
        hud_t.push_back(new Texture2D(dirname + "texture/aircraft/cockpit.png")); // 0
        hud_t.push_back(new Texture2D(dirname + "texture/aircraft/yaw.png")); // 1
        hud_t.push_back(new Texture2D(dirname + "texture/aircraft/roll.png")); // 2
        hud_t.push_back(new Texture2D(dirname + "texture/aircraft/speed.png")); // 3
        hud_t.push_back(new Texture2D(dirname + "texture/aircraft/yoke.png")); // 4

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

    void draw ()
    {   
        // clear the buffers
        glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // draw skybox
        draw_skybox(skybox_shaders);

        glm::mat4 root = fcg::identity ();

        draw_buildings(root, building_shaders);
        draw_floor (root, main_shaders);
        draw_hud(camera, hud_shaders, airplane);
    }

    // method to check ground and building collision
    bool check_collision () {
        if (airplane.position.y <= 0.5f) {
            std::cout << "ground check" << std::endl;
            return true;
        } else if (airplane.position.y >= 50.f) {
            std::cout << "its not kerbal space program" << std::endl;
            return true;
        }
        const float TOL = 1.0f; // tolerance (wingspan)
        for (building b : buildings) {
            // size
            float b_sx = b.model[0][0];
            float b_sy = b.model[1][1];
            float b_sz = b.model[2][2];
            // position
            float b_x = b.model[3][0];
            // float b_y = b.model[3][1];
            float b_z = b.model[3][2];

            if (airplane.position.x >= (b_x - b_sx/2) - TOL &&
                airplane.position.x <= (b_x + b_sx/2) + TOL &&
                airplane.position.y <= (b_sy) + TOL/3 &&
                airplane.position.z >= (b_z - b_sz/2 - TOL) &&
                airplane.position.z <= (b_z + b_sz/2) + TOL) {
                std::cout <<"building hit" << std::endl;
                return true;
            }
        }
        return false;
    }

private:
    void draw_cube (glm::mat4 parent_mm, fcg::Shaders& current_shader)
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

    // A floor, drawn in the unitary cube
    // returns floor level in world coordinates
    float draw_floor (glm::mat4 parent_mm, fcg::Shaders& ground_shader)
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

        if (blendmap != nullptr) {
            blendmap->Bind(0);
        }
        if (!ground.empty()) {
            for (int i = 0; i < ground.size(); ++i) {
                ground.at(i)->Bind(i+1);
            }
        }
        draw_cube (mm, ground_shader);

        return -h_height;
    }

    void draw_buildings (glm::mat4 parent_mm, fcg::Shaders& building_shaders) {
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

    void draw_hud (Camera camera, fcg::Shaders& hud_shaders, fcg::Airplane airplane) {
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
            scene.audio.t_up();
            scene.airplane.throttle_up();
        } else {
            scene.audio.stuck();
        }
        break;
    case sf::Keyboard::Scancode::LControl:
        if(scene.airplane.get_throttle_level() != 0) {
            scene.audio.t_down();
            scene.airplane.throttle_down();
        } else {
            scene.audio.stuck();
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

int main (int argc, char* argv[])
{
    std::string dirname = "resources/";
    if (argc == 2) {
        dirname = argv[1];
    } else if (argc > 2) {
        std::cout << "Usage: " << argv[0] << " [dirname]\n";
        exit (1);
    }
    if (dirname.empty() || dirname.back() != '/')
        dirname.push_back('/');

    //// Startup ////

    Setup setup;
    sf::Window& window = *setup.window;

    fcg::Shaders main_shaders ("./resources/vert/main_shader.vert", "./resources/frag/main_shader.frag");
    fcg::Shaders skybox_shaders ("./resources/vert/skybox.vert", "./resources/frag/skybox.frag");
    fcg::Shaders building_shaders ("./resources/vert/main_shader.vert", "./resources/frag/building.frag");
    fcg::Shaders hud_shaders ("./resources/vert/hud.vert", "./resources/frag/hud.frag");

    main_shaders.use ();

    Scene scene (dirname, main_shaders, skybox_shaders, building_shaders, hud_shaders);

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
        scene.camera.attach_to(scene.airplane.position, scene.airplane.orientation);
        scene.audio.set_speed(scene.airplane.get_speed());
        scene.audio.set_pitch(elapsed);
        
        if (scene.check_collision()) { // checks for collisions
            running = false;
            
            scene.audio.stop(); // stops all audio
            scene.audio.crash.play(); // sound plays
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
