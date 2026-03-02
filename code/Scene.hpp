#pragma once

#include <glad/gl.h>
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <string>



namespace udit
{
    struct Body
    {
        float distance;
        float size;
        float orbit_speed;
        float angle = 0.0f;
        float self_rotation = 0.0f;
    };

    class SkyBox; // forward declaration

    class Scene
    {
    private:
        // GL objects para los planetas
        GLuint program_id = 0;
        GLuint vao_id = 0;
        GLuint vbo_ids[2] = { 0, 0 }; // 0: positions, 1: indices
        GLsizei number_of_indices = 0;

        // GL objects para las orbitas (OPTIMIZACIÓN)
        GLuint orbit_vao_id = 0;
        GLuint orbit_vbo_id = 0;
        GLsizei orbit_vertex_count = 0;

        // shader uniforms
        GLint model_view_matrix_id = -1;
        GLint projection_matrix_id = -1;
        GLint body_color_id = -1;

        // projection and skybox
        glm::mat4 projection_matrix = glm::mat4(1.0f);
        SkyBox* skybox = nullptr;

        // solar system bodies
        Body sun, mercury, venus, earth, moon, mars, phobos;
        Body jupiter, saturn, uranus, neptune;

        // helpers
        glm::vec3 random_color();
        void draw_body(const glm::mat4& parent, const Body& body);
        void draw_orbit(const glm::mat4& view, float radius);
        void init_solar_system();
        void create_sphere_geometry(int stacks = 32, int slices = 32);
        void create_orbit_geometry(); // Nueva funcion para inicializar orbitas

    public:
        static const std::string vertex_shader_code;
        static const std::string fragment_shader_code;

        Scene(int width, int height);
        ~Scene();

        void update();
        void render();
        void resize(int width, int height);
        GLuint compile_shaders();
    };
} // namespace udit