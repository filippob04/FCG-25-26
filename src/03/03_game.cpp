#define GLAD_GL_IMPLEMENTATION
#include "../../resources/glad/gl.h"
#include <SFML/Graphics.hpp>
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
                                 "S6393212 - 03.cpp",
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
    glm::vec3 camera_pos = {0.0, 5.0, 2}; // xyz

    // Angles defining the in-place camera rotation
    float yaw_deg = 0.0; // phi
    float pitch_deg = 0.0; // theta
    float roll_deg = 0.0;

    /** Camera movement **/
    float move_speed = 1.0;
    float rot_speed = 45.0;

    float move_dir = 0.0;
    float yaw_dir = 0.0;
    float pitch_dir = 0.0;
    float roll_dir = 0.0;

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

    // camera movement
    void move (float delta_time)
    {   
        // Pitch and Yaw
        yaw_deg += (yaw_dir * rot_speed * delta_time);

        // Gimbal Lock prevention
        pitch_deg += (pitch_dir * rot_speed * delta_time);
        pitch_deg = pitch_deg > 90.0? 90.0 : pitch_deg;
        pitch_deg = pitch_deg < -90.0? -90.0 : pitch_deg;

        // Roll
        roll_deg += (roll_dir * rot_speed * delta_time);

        // Complete movement
        float yaw_rad = glm::radians (yaw_deg);
        float pitch_rad = glm::radians (pitch_deg);

        float ps = glm::sin (yaw_rad);
        float pc = glm::cos (yaw_rad);
        
        float ts = glm::sin (pitch_rad);
        float tc = glm::cos (pitch_rad);
        
        // this vector contains where the camera is pointing
        glm::vec3 front_vector(
            tc * -ps,
            ts,
            tc * pc
        );

        float delta_s = -move_dir * move_speed * delta_time;
        // newPos = oldPos + front_vector * delta_s 
        camera_pos += front_vector * delta_s;

        view_projection ();
    }

    void set_move_dir (float dir) {
        move_dir = dir;
    }

    void set_yaw_dir (float dir) {
        yaw_dir = dir;
    }

    void set_pitch_dir (float dir) {
        pitch_dir = dir;
    }

    void set_roll_dir (float dir) {
        roll_dir = dir;
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

private:
    fcg::Shaders& main_shaders;
    fcg::Shaders& skybox_shaders;
    fcg::Shaders& building_shaders;

    // ground
    GLint blendMap_loc;
    std::vector<GLint> texture_loc;
    GLint tiling_loc;

    GLint building_loc;

public:
    Scene (std::string dirname, fcg::Shaders& main_sh, fcg::Shaders& skybox_sh, fcg::Shaders& building_sh) :
        camera (), lights (),
        cube (dirname + "off/cube.off"),
        skybox({
            dirname + "texture/skybox/clear/right.png", dirname + "texture/skybox/clear/left.png", 
            dirname + "texture/skybox/clear/up.png", dirname + "texture/skybox/clear/down.png", 
            dirname + "texture/skybox/clear/front.png", dirname + "texture/skybox/clear/back.png"
        }), 
        main_shaders(main_sh),
        skybox_shaders(skybox_sh),
        building_shaders(building_sh)
    {  
        // ground
        init_ground (dirname);
        
        // buildings
        init_buildings (dirname);

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

        building_loc = glGetUniformLocation(building_shaders.program, "building");

        glUniform1i(building_loc, 0);
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
        buildings_t.push_back(new Texture2D(dirname + "texture/building/BlueishWindowsBlackSpaces_S.jpg"));
        buildings_t.push_back(new Texture2D(dirname + "texture/building/DarkGreyWindowsPaleBlocks_S.jpg"));
        buildings_t.push_back(new Texture2D(dirname + "texture/building/GreyRectangles_S.jpg"));
        buildings_t.push_back(new Texture2D(dirname + "texture/building/VerticleBrownBricks_S.jpg"));
        buildings_t.push_back(new Texture2D(dirname + "texture/building/GreyWindowsWithBlinds_S.jpg"));
        buildings_t.push_back(new Texture2D(dirname + "texture/building/VerticalStrips_S.jpg"));
        buildings_t.push_back(new Texture2D(dirname + "texture/building/OfficeWindows_S.jpg"));
        buildings_t.push_back(new Texture2D(dirname + "texture/building/XSupport_S.jpg"));

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

    void draw ()
    {
        // clear the buffers
        glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // draw skybox
        draw_skybox(skybox_shaders);

        glm::mat4 root = fcg::identity ();

        draw_buildings(root, building_shaders);
        draw_floor (root, main_shaders);
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

    void draw_skybox (fcg::Shaders& skybox_shaders) 
    {   
        skybox_shaders.use();

        glm::mat4 viewnt = glm::mat4(glm::mat3(camera.v)); // removing translation
        glm::mat4 gWVP = camera.p * viewnt;

        GLint wvp_loc = glGetUniformLocation(skybox_shaders.program, "gWVP");
        glUniformMatrix4fv(wvp_loc, 1, GL_FALSE, &gWVP[0][0]);

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
        scene.camera.set_move_dir(1.0);
        break;
    case sf::Keyboard::Scancode::LControl:
        scene.camera.set_move_dir(-1.0);
        break;
    case sf::Keyboard::Scancode::W:
        scene.camera.set_pitch_dir(-1.0);
        break;
    case sf::Keyboard::Scancode::A:
        scene.camera.set_yaw_dir(-1.0);
        break;
    case sf::Keyboard::Scancode::S:
        scene.camera.set_pitch_dir(1.0);
        break;
    case sf::Keyboard::Scancode::D:
        scene.camera.set_yaw_dir(1.0);
        break;
    case sf::Keyboard::Scancode::E:
        scene.camera.set_roll_dir(1.0);
        break;
    case sf::Keyboard::Scancode::Q:
        scene.camera.set_roll_dir(-1.0);
        break;
    default:
        return;
    }
}

void handle (const sf::Event::KeyReleased& key, Scene& scene)
{
    switch (key.scancode) {
    case sf::Keyboard::Scancode::LShift:
        scene.camera.set_move_dir(0.0);
        break;
    case sf::Keyboard::Scancode::LControl:
        scene.camera.set_move_dir(0.0);
        break;
    case sf::Keyboard::Scancode::W:
        scene.camera.set_pitch_dir(0.0);
        break;
    case sf::Keyboard::Scancode::A:
        scene.camera.set_yaw_dir(0.0);
        break;
    case sf::Keyboard::Scancode::S:
        scene.camera.set_pitch_dir(0.0);
        break;
    case sf::Keyboard::Scancode::D:
        scene.camera.set_yaw_dir(0.0);
        break;
    case sf::Keyboard::Scancode::E:
        scene.camera.set_roll_dir(0.0);
        break;
    case sf::Keyboard::Scancode::Q:
        scene.camera.set_roll_dir(0.0);
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

    main_shaders.use ();

    Scene scene (dirname, main_shaders, skybox_shaders, building_shaders);

    glEnable (GL_CULL_FACE);
    glCullFace (GL_BACK);

    glEnable (GL_DEPTH_TEST);


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

        scene.camera.move (elapsed);

        scene.draw ();
        window.display ();
    }

    return 0;
}
