#define GLAD_GL_IMPLEMENTATION
#include "../../resources/glad/gl.h"
#include <SFML/Window.hpp>
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
    static const int window_width = 800;
    static const int window_height = 800;

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
                                 "S6393212 - 01.cpp",
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
    glm::vec3 light_direct_pos_relative = {1.0, 1.0, 1.0}; // xyz (relative to camera position)
    glm::vec3 light_direct_pos = {0.0, 0.0, 0.0};   // xyz (absolute, in world coordinates)
    glm::vec3 light_direct_val = {1.0, 1.0, 1.0};   // rgb
    glm::vec3 light_ambient_val = {0.1, 0.1, 0.1};  // rgb

    glm::vec3 material_diffuse = {0.8, 0.7, 0.6};   // rgb
    glm::vec3 material_ambient = {0.5, 0.5, 0.8};   // rgb
    glm::vec3 material_specular = {1.0, 1.0, 1.0};  // rgb
    float material_shininess = 1000.0; // scalar

private:
    // lights
    GLint light_direct_pos_loc;   // xyz
    GLint light_direct_val_loc;   // rgb
    GLint light_ambient_val_loc;  // rgb
    // materials
    GLint material_diffuse_loc;   // rgb
    GLint material_ambient_loc;   // rgb
    GLint material_specular_loc;  // rgb
    GLint material_shininess_loc; // scalar

public:
    Lights (fcg::Shaders& shaders)
    {
        locations (shaders);
    }

    void locations (fcg::Shaders& shaders)
    {
        light_direct_pos_loc = glGetUniformLocation (shaders.program, "light.direct_pos");
        light_direct_val_loc = glGetUniformLocation (shaders.program, "light.direct_val");
        light_ambient_val_loc = glGetUniformLocation (shaders.program, "light.ambient_val");
        material_diffuse_loc = glGetUniformLocation (shaders.program, "material.diffuse");
        material_ambient_loc = glGetUniformLocation (shaders.program, "material.ambient");
        material_specular_loc = glGetUniformLocation (shaders.program, "material.specular");
        material_shininess_loc = glGetUniformLocation (shaders.program, "material.shininess");
    }

    void send_parameters ()
    {
        glUniform3fv (light_direct_val_loc, 1, &light_direct_val[0]);
        glUniform3fv (light_ambient_val_loc, 1, &light_ambient_val[0]);
        glUniform3fv (material_diffuse_loc, 1, &material_diffuse[0]);
        glUniform3fv (material_ambient_loc, 1, &material_ambient[0]);
        glUniform3fv (material_specular_loc, 1, &material_specular[0]);
        glUniform1fv (material_shininess_loc, 1, &material_shininess);
    }

    void send_position_relative (const glm::mat4& inverse_view_matrix)
    {
        glm::vec4 p = glm::vec4 (light_direct_pos_relative, 1.0);
        p = inverse_view_matrix * p;
        light_direct_pos = {p.x, p.y, p.z};
        send_position ();
    }

    void send_position ()
    {
        glUniform3fv (light_direct_pos_loc, 1, &light_direct_pos[0]);
    }
};


class Camera
{
public:
    glm::mat4 v;
    glm::mat4 inv_v;
    glm::mat4 vp;

private:

    /** Intrinsic camera parameters **/
    const float fd = 50.0 / 18.0; // focal distance
    float ar; // aspect ratio

    /** Extrinsic camera parameters **/
    // xyz, starting point of dynamic camera position
    glm::vec3 camera_pos = {0.0, 0.0, 2}; // xyz
    GLint camera_pos_loc; // xyz

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
    Camera (fcg::Shaders& shaders)
    {
        locations (shaders);
        set_window_size (Setup::window_width, Setup::window_height);
        view_projection ();
    }

    void locations (fcg::Shaders& shaders)
    {
        camera_pos_loc = glGetUniformLocation (shaders.program, "camera_pos");
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
        float od = glm::distance ({0.0, 0.0, 0.0}, cp);
        float ncp = od - 4.0; // distance near clip plane
        if (ncp < 0.0001)
            ncp = 0.0001;
        float fcp = od + 4.0; // distance far clip plane

        // prepare rotations and translation matrices
        glm::mat4 rz = fcg::rotation_z (roll_deg); // new roll matrix
        glm::mat4 ry = fcg::rotation_y (yaw_deg);
        glm::mat4 rx = fcg::rotation_x (pitch_deg);
        glm::mat4 t = fcg::translation (-cp.x, -cp.y, -cp.z);

        // prepare projection matrix
        float a = (fcp + ncp) / (ncp - fcp);       // coefficient 3rd col
        float b = 2.0 * fcp * ncp / (ncp - fcp);   // coefficient 4th col

        glm::mat4 pr = glm::mat4(
                                 fd,  0.0,     0.0,  0.0,    // 1st column
                                 0.0, fd * ar, 0.0,  0.0,    // 2nd column
                                 0.0, 0.0,       a, -1.0,    // 3rd column
                                 0.0, 0.0,       b,  0.0     // 4th column
                                 );

        // Compute VP matrix and update it
        v = rz * rx * ry * t;
        vp = pr * v;
        inv_v = glm::inverse (v);

        glUniform3fv(camera_pos_loc, 1, &cp[0]);
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


class Scene
{
public:
    Camera camera;
    Lights lights;

    GPUMesh cube;
    GPUMesh sphere;

private:
    GLint model_loc;
    GLint vp_loc;
    GLint tr_inv_model_loc;

public:
    Scene (std::string dirname, fcg::Shaders& shaders) :
        camera (shaders), lights (shaders),
        cube (dirname + "cube.off"),
        sphere (dirname + "sphere.off")
    {
        locations (shaders);
        update_all ();
    }

    void locations (fcg::Shaders& shaders)
    {
        camera.locations (shaders);
        lights.locations (shaders);

        model_loc = glGetUniformLocation (shaders.program, "model");
        vp_loc = glGetUniformLocation (shaders.program, "vp");
        tr_inv_model_loc = glGetUniformLocation (shaders.program, "tr_inv_model");
    }

    void update_all ()
    {
        camera.view_projection ();
        lights.send_parameters ();
        lights.send_position_relative (camera.inv_v);
    }

    void draw ()
    {
        // clear the buffers
        glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // the view-projection matrix is the same for all the scene
        glUniformMatrix4fv(vp_loc, 1, GL_FALSE, &camera.vp[0][0]);

        glm::mat4 root = fcg::identity ();
        float floor = draw_floor (root);
        
        // Temporary Object
        glm::mat4 mm, translate, scale;
        scale = fcg::scaling (0.1, 0.1, 0.1);
        translate = fcg::translation (0, (cube.extent.y * 0.8) * 0.5 + floor, 0); // raise it upwards
        mm = root * translate * scale;
        draw_sphere(mm);
    }

private:
    void draw_cube (glm::mat4 parent_mm)
    {
        glm::mat4 mm;
        glm::mat3 ti_mm;
        mm = parent_mm * cube.to_unit_extent;
        ti_mm = glm::transpose (glm::inverse (glm::mat3 (mm)));
        glUniformMatrix4fv(model_loc, 1, GL_FALSE, &mm[0][0]);
        glUniformMatrix3fv (tr_inv_model_loc, 1, GL_FALSE, &ti_mm[0][0]);
        cube.draw ();
    }

    void draw_sphere (glm::mat4 parent_mm)
    {
        glm::mat4 mm;
        glm::mat3 ti_mm;
        mm = parent_mm * sphere.to_unit_extent;
        ti_mm = glm::transpose (glm::inverse (glm::mat3 (mm)));
        glUniformMatrix4fv(model_loc, 1, GL_FALSE, &mm[0][0]);
        glUniformMatrix3fv (tr_inv_model_loc, 1, GL_FALSE, &ti_mm[0][0]);
        sphere.draw ();
    }

    // A floor, drawn in the unitary cube
    // returns floor level in world coordinates
    float draw_floor (glm::mat4 parent_mm)
    {
        glm::mat4 scale, translate, mm;
        float depth = 0.04;
        float h_depth = depth * 0.5;
        float height = cube.extent.y * 0.8;
        float h_height = height * 0.5;

        // draw floor
        scale = fcg::scaling (2.0, depth, 2.0); // flatten the cube!
        translate = fcg::translation (0, -h_height - h_depth, 0); // lower it down
        mm = parent_mm * translate * scale;
        draw_cube (mm);

        return -h_height;
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

void handle (const sf::Event::KeyPressed& key, fcg::Shaders& shaders, Scene& scene)
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

void handle (const sf::Event::KeyReleased& key, fcg::Shaders& shaders, Scene& scene)
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
    // mandatory command line argument: dirname where meshes are found
    std::string dirname = "./resources/off/";
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

    fcg::Shaders shaders ("./resources/vert/shader_phong.vert", "./resources/frag/shader_phong.frag");
    shaders.use ();

    Scene scene (dirname, shaders);

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
                handle (*key_pressed, shaders, scene);
            else if (const auto* key_released = event->getIf<sf::Event::KeyReleased> ())
                handle (*key_released, shaders, scene);
        }

        float elapsed = clock.restart().asSeconds();

        scene.camera.move (elapsed);
        scene.lights.send_position_relative (scene.camera.inv_v);

        scene.draw ();
        window.display ();
    }

    return 0;
}
