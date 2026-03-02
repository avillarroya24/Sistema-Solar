#include "SkyBox.hpp"
#include <SOIL2.h>
#include <iostream>
#include <vector>
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>

namespace udit {

    // Helper interno para shaders (mismo que en Scene, podria estar en un util)
    static GLuint compile_shader_program(const char* vsrc, const char* fsrc) {
        auto check_shader = [](GLuint shader) {
            GLint ok = GL_FALSE;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
            if (ok == GL_FALSE) {
                GLint len = 0;
                glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
                std::string log(len > 0 ? len : 1, ' ');
                glGetShaderInfoLog(shader, len, nullptr, &log[0]);
                std::cerr << "Shader compile error:\n" << log << "\n";
                return false;
            }
            return true;
            };

        auto check_program = [](GLuint program) {
            GLint ok = GL_FALSE;
            glGetProgramiv(program, GL_LINK_STATUS, &ok);
            if (ok == GL_FALSE) {
                GLint len = 0;
                glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
                std::string log(len > 0 ? len : 1, ' ');
                glGetProgramInfoLog(program, len, nullptr, &log[0]);
                std::cerr << "Program link error:\n" << log << "\n";
                return false;
            }
            return true;
            };

        GLuint vs = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vs, 1, &vsrc, nullptr);
        glCompileShader(vs);
        if (!check_shader(vs)) { glDeleteShader(vs); return 0; }

        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fs, 1, &fsrc, nullptr);
        glCompileShader(fs);
        if (!check_shader(fs)) { glDeleteShader(vs); glDeleteShader(fs); return 0; }

        GLuint program = glCreateProgram();
        glAttachShader(program, vs);
        glAttachShader(program, fs);
        glLinkProgram(program);

        if (!check_program(program)) {
            glDeleteShader(vs);
            glDeleteShader(fs);
            glDeleteProgram(program);
            return 0;
        }

        glDeleteShader(vs);
        glDeleteShader(fs);
        return program;
    }

    SkyBox::SkyBox(const std::string& path) : texture_path(path) {
        create_cube_geometry();
        load_texture();

        const char* vertex_shader_code = R"(#version 330 core
layout(location=0) in vec3 vertex_coordinates;
layout(location=1) in vec2 vertex_uv;
uniform mat4 model_view_matrix;
uniform mat4 projection_matrix;
out vec2 texture_uv;
void main() {
    // Truco comun: Z = W asegura profundidad maxima (detras de todo)
    // Pero tu metodo de deshabilitar depth write tambien funciona.
    gl_Position = projection_matrix * model_view_matrix * vec4(vertex_coordinates, 1.0);
    texture_uv = vertex_uv;
})";

        const char* fragment_shader_code = R"(#version 330 core
in vec2 texture_uv;
out vec4 fragment_color;
uniform sampler2D sampler2d;
void main() {
    fragment_color = texture(sampler2d, texture_uv);
})";

        shader_program = compile_shader_program(vertex_shader_code, fragment_shader_code);
        if (shader_program == 0) {
            std::cerr << "SkyBox: failed to compile/link shaders\n";
            return;
        }

        model_view_matrix_id = glGetUniformLocation(shader_program, "model_view_matrix");
        projection_matrix_id = glGetUniformLocation(shader_program, "projection_matrix");
        sampler_id = glGetUniformLocation(shader_program, "sampler2d");

        glUseProgram(shader_program);
        if (sampler_id >= 0) glUniform1i(sampler_id, 0);
        glUseProgram(0);
    }

    SkyBox::~SkyBox() {
        if (vao_id) { glDeleteVertexArrays(1, &vao_id); vao_id = 0; }
        if (vbo_id) { glDeleteBuffers(1, &vbo_id); vbo_id = 0; }
        if (ebo_id) { glDeleteBuffers(1, &ebo_id); ebo_id = 0; }
        if (texture_id) { glDeleteTextures(1, &texture_id); texture_id = 0; }
        if (shader_program) { glDeleteProgram(shader_program); shader_program = 0; }
    }

    void SkyBox::create_cube_geometry() {
        struct Vertex { glm::vec3 pos; glm::vec2 uv; };
        // Geometria de cubo basico
        std::vector<Vertex> vertices = {
            {{-1,-1, 1},{0,0}}, {{1,-1,1},{1,0}}, {{1,1,1},{1,1}}, {{-1,1,1},{0,1}},
            {{-1,-1,-1},{1,0}}, {{-1,1,-1},{1,1}}, {{1,1,-1},{0,1}}, {{1,-1,-1},{0,0}},
            {{-1,-1,-1},{0,0}}, {{-1,-1,1},{1,0}}, {{-1,1,1},{1,1}}, {{-1,1,-1},{0,1}},
            {{1,-1,-1},{1,0}}, {{1,1,-1},{1,1}}, {{1,1,1},{0,1}}, {{1,-1,1},{0,0}},
            {{-1,1,-1},{0,1}}, {{-1,1,1},{0,0}}, {{1,1,1},{1,0}}, {{1,1,-1},{1,1}},
            {{-1,-1,-1},{0,0}}, {{1,-1,-1},{1,0}}, {{1,-1,1},{1,1}}, {{-1,-1,1},{0,1}}
        };

        std::vector<unsigned int> indices = {
            0,1,2,0,2,3, 4,5,6,4,6,7,
            8,9,10,8,10,11, 12,13,14,12,14,15,
            16,17,18,16,18,19, 20,21,22,20,22,23
        };

        number_of_indices = static_cast<GLsizei>(indices.size());

        glGenVertexArrays(1, &vao_id);
        glGenBuffers(1, &vbo_id);
        glGenBuffers(1, &ebo_id);

        glBindVertexArray(vao_id);

        glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_id);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));
        glEnableVertexAttribArray(1);

        glBindVertexArray(0);
    }

    void SkyBox::load_texture() {
        int w = 0, h = 0, c = 0;
        unsigned char* data = SOIL_load_image(texture_path.c_str(), &w, &h, &c, SOIL_LOAD_AUTO);
        if (!data) {
            std::cerr << "SkyBox: Failed to load texture: " << texture_path << "\n";
            texture_id = 0;
            return;
        }

        GLenum format = (c == 4) ? GL_RGBA : GL_RGB;

        glGenTextures(1, &texture_id);
        glBindTexture(GL_TEXTURE_2D, texture_id);
        glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        SOIL_free_image_data(data);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void SkyBox::render(const glm::mat4& view, const glm::mat4& projection) {
        if (shader_program == 0 || vao_id == 0 || texture_id == 0) return;

        // Guardar estados simples
        GLboolean depth_enabled = glIsEnabled(GL_DEPTH_TEST);
        GLboolean cull_enabled = glIsEnabled(GL_CULL_FACE);
        GLint prev_cull_face = 0;
        glGetIntegerv(GL_CULL_FACE_MODE, &prev_cull_face);

        // Deshabilitar escritura en depth para que el skybox quede "atrás"
        // y no tape lo que se dibuje después.
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);

        glUseProgram(shader_program);

        // Eliminar traslación del view
        glm::mat4 view_no_translation = glm::mat4(glm::mat3(view));

        // Escala del Skybox. IMPORTANTE: El far plane de la camara debe ser mayor a 500.0f
        glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3(500.0f));
        glm::mat4 mv = view_no_translation * model;

        if (model_view_matrix_id >= 0) glUniformMatrix4fv(model_view_matrix_id, 1, GL_FALSE, glm::value_ptr(mv));
        if (projection_matrix_id >= 0) glUniformMatrix4fv(projection_matrix_id, 1, GL_FALSE, glm::value_ptr(projection));

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture_id);

        glBindVertexArray(vao_id);

        // Asegurar que dibujamos las caras frontales (el cubo esta definido normal)
        // Como estamos DENTRO del cubo, si el winding order es estandar CCW, 
        // veriamos las caras traseras (back).
        // Si tienes culling activado, el cubo desaparece si no lo ajustas.
        if (!cull_enabled) glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT); // Invertimos culling para ver el interior

        glDrawElements(GL_TRIANGLES, number_of_indices, GL_UNSIGNED_INT, 0);

        // Restaurar culling
        glCullFace(prev_cull_face);
        if (!cull_enabled) glDisable(GL_CULL_FACE);

        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glUseProgram(0);

        // Restaurar depth
        glDepthMask(GL_TRUE);
        if (depth_enabled) glEnable(GL_DEPTH_TEST);
    }

} // namespace udit