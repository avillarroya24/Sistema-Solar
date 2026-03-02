#pragma once
#include <glad/gl.h>
#include <glm.hpp>
#include <string>

namespace udit {

    class SkyBox {
    private:
        GLuint vao_id = 0;
        GLuint vbo_id = 0;
        GLuint ebo_id = 0;
        GLuint shader_program = 0;
        GLint model_view_matrix_id = -1;
        GLint projection_matrix_id = -1;
        GLint sampler_id = -1;
        GLuint texture_id = 0;
        GLsizei number_of_indices = 0;
        std::string texture_path;

        void create_cube_geometry();
        void load_texture();

    public:
        SkyBox(const std::string& texture_path);
        ~SkyBox();
        void render(const glm::mat4& view, const glm::mat4& projection);
    };

} // namespace udit
