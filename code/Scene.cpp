#include "Scene.hpp"
#include "SkyBox.hpp"
#include <SOIL2.h>
#include <vector>
#include <cassert>
#include <cstdlib>
#include <iostream>

#include <glad/gl.h>
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>


using namespace glm;
using namespace std;

namespace udit {

    // ==========================================================
    // Shaders de cuerpo principal (color plano)
    // ==========================================================
    const string Scene::vertex_shader_code =
        R"(#version 330 core
layout (location = 0) in vec3 vertex_coordinates;
uniform mat4 model_view_matrix;
uniform mat4 projection_matrix;
void main()
{
    gl_Position = projection_matrix * model_view_matrix * vec4(vertex_coordinates, 1.0);
})";

    const string Scene::fragment_shader_code =
        R"(#version 330 core
uniform vec3 body_color;
out vec4 fragment_color;
void main()
{
    fragment_color = vec4(body_color, 1.0);
})";

    // ==========================================================
    // Constructor / Destructor
    // ==========================================================
    Scene::Scene(int width, int height)
    {
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glClearColor(0.02f, 0.02f, 0.05f, 1.0f);

        program_id = compile_shaders();
        if (program_id == 0) {
            cerr << "Scene: shader program compilation failed\n";
            return;
        }

        glUseProgram(program_id);

        model_view_matrix_id = glGetUniformLocation(program_id, "model_view_matrix");
        projection_matrix_id = glGetUniformLocation(program_id, "projection_matrix");
        body_color_id = glGetUniformLocation(program_id, "body_color");

        if (model_view_matrix_id == -1) cerr << "Warning: model_view_matrix uniform not found\n";
        if (projection_matrix_id == -1) cerr << "Warning: projection_matrix uniform not found\n";
        if (body_color_id == -1)        cerr << "Warning: body_color uniform not found\n";

        // Crear geometría
        create_sphere_geometry(32, 32);
        create_orbit_geometry(); // <--- IMPORTANTE: Crear geometria de orbita una sola vez
        init_solar_system();

        // Crear skybox: ajusta la ruta a tu textura si es necesario
        skybox = new SkyBox("assets/Fondo.png");

        // Configurar viewport y proyección
        resize(width, height);
    }

    Scene::~Scene() {
        if (vao_id != 0) { glDeleteVertexArrays(1, &vao_id); vao_id = 0; }
        if (vbo_ids[0] != 0 || vbo_ids[1] != 0) { glDeleteBuffers(2, vbo_ids); vbo_ids[0] = vbo_ids[1] = 0; }

        // Limpieza de orbita
        if (orbit_vao_id != 0) { glDeleteVertexArrays(1, &orbit_vao_id); orbit_vao_id = 0; }
        if (orbit_vbo_id != 0) { glDeleteBuffers(1, &orbit_vbo_id); orbit_vbo_id = 0; }

        if (program_id != 0) { glDeleteProgram(program_id); program_id = 0; }

        if (skybox) { delete skybox; skybox = nullptr; }
    }

    // ==========================================================
    // Inicialización del sistema solar
    // ==========================================================
    void Scene::init_solar_system() {
        sun = { 0.0f, 1.2f, 0.0f, 0.0f, 0.01f };

        const float mercury_period = 88.0f;
        const float venus_period = 225.0f;
        const float earth_period = 365.0f;
        const float mars_period = 687.0f;
        const float jupiter_period = 4333.0f;
        const float saturn_period = 10759.0f;
        const float uranus_period = 30687.0f;
        const float neptune_period = 60190.0f;
        const float moon_period = 27.3f;
        const float phobos_period = 0.32f;

        const float orbit_speed_scale = 0.05f;

        mercury = { 3.0f, 0.12f, orbit_speed_scale * (365.0f / mercury_period), 0.0f, 0.08f };
        venus = { 4.2f, 0.18f, orbit_speed_scale * (365.0f / venus_period),   0.0f, 0.06f };
        earth = { 6.0f, 0.22f, orbit_speed_scale * (365.0f / earth_period),   0.0f, 0.05f };
        mars = { 8.0f, 0.18f, orbit_speed_scale * (365.0f / mars_period),    0.0f, 0.04f };
        jupiter = { 12.0f, 0.65f, orbit_speed_scale * (365.0f / jupiter_period), 0.0f, 0.02f };
        saturn = { 15.5f, 0.52f, orbit_speed_scale * (365.0f / saturn_period),  0.0f, 0.02f };
        uranus = { 19.0f, 0.42f, orbit_speed_scale * (365.0f / uranus_period),  0.0f, 0.015f };
        neptune = { 22.5f, 0.40f, orbit_speed_scale * (365.0f / neptune_period), 0.0f, 0.012f };

        moon = { 0.9f, 0.06f, orbit_speed_scale * (365.0f / moon_period),    0.0f, 0.12f };
        phobos = { 0.7f, 0.04f, orbit_speed_scale * (365.0f / phobos_period), 0.0f, 0.15f };
    }

    // ==========================================================
    // Actualización de cuerpos celestes
    // ==========================================================
    void Scene::update() {
        const float dt = 0.025f; // Podrias usar delta time real aqui
        auto update_body = [&](Body& b) {
            b.angle += b.orbit_speed * dt;
            b.self_rotation += 0.02f * dt;
            };

        update_body(sun);
        update_body(mercury);
        update_body(venus);
        update_body(earth);
        update_body(moon);
        update_body(mars);
        update_body(phobos);
        update_body(jupiter);
        update_body(saturn);
        update_body(uranus);
        update_body(neptune);
    }

    // ==========================================================
    // Colores de cuerpos (Helper estatico)
    // ==========================================================
    static vec3 color_for_body(const Body& b, const Body& sun,
        const Body& mercury, const Body& venus,
        const Body& earth, const Body& moon,
        const Body& mars, const Body& phobos,
        const Body& jupiter, const Body& saturn,
        const Body& uranus, const Body& neptune)
    {
        if (&b == &sun) return vec3(0.85f, 0.75f, 0.00f);
        if (&b == &mercury) return vec3(0.50f, 0.50f, 0.50f);
        if (&b == &venus)   return vec3(0.85f, 0.70f, 0.20f);
        if (&b == &earth)   return vec3(0.15f, 0.45f, 0.80f);
        if (&b == &moon)    return vec3(0.60f, 0.60f, 0.60f);
        if (&b == &mars)    return vec3(0.85f, 0.25f, 0.15f);
        if (&b == &phobos)  return vec3(0.45f, 0.45f, 0.45f);
        if (&b == &jupiter) return vec3(0.85f, 0.55f, 0.25f);
        if (&b == &saturn)  return vec3(0.80f, 0.70f, 0.45f);
        if (&b == &uranus)  return vec3(0.25f, 0.75f, 0.90f);
        if (&b == &neptune) return vec3(0.08f, 0.25f, 0.75f);
        return vec3(1.0f);
    }

    // ==========================================================
    // Renderizado de un cuerpo
    // ==========================================================
    void Scene::draw_body(const mat4& parent, const Body& body) {
        if (vao_id == 0 || number_of_indices == 0 || program_id == 0) return;

        // Dibujar Orbita primero (opcional, pero ayuda a ver la trayectoria)
        // Nota: El sol no tiene orbita visible en este contexto local (0,0,0)
        if (body.distance > 0.1f) {
            draw_orbit(parent, body.distance);
        }

        mat4 model = parent;
        model = rotate(model, body.angle, vec3(0, 1, 0));
        model = translate(model, vec3(body.distance, 0, 0));
        model = rotate(model, body.self_rotation, vec3(0, 1, 0));
        model = scale(model, vec3(body.size));

        vec3 c = color_for_body(body, sun, mercury, venus, earth, moon,
            mars, phobos, jupiter, saturn, uranus, neptune);

        // Usar programa principal
        glUseProgram(program_id);
        if (body_color_id >= 0) glUniform3fv(body_color_id, 1, value_ptr(c));
        if (model_view_matrix_id >= 0) glUniformMatrix4fv(model_view_matrix_id, 1, GL_FALSE, value_ptr(model));

        glBindVertexArray(vao_id);
        glDrawElements(GL_TRIANGLES, number_of_indices, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

    // ==========================================================
    // Renderizado de toda la escena
    // ==========================================================
    void Scene::render() {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Matriz de vista (cámara fija)
        mat4 view = mat4(1.0f);
        view = translate(view, vec3(0, 2.0f, -33.0f));
        view = rotate(view, radians(-15.0f), vec3(1, 0, 0));

        // 1. Dibujar skybox primero (usa su propio shader y gestiona depth mask)
        if (skybox) {
            skybox->render(view, projection_matrix);
        }

        if (vao_id == 0 || number_of_indices == 0 || program_id == 0) return;

        // 2. Dibujar cuerpos opacos
        glUseProgram(program_id);

        // Sol (Manual porque no llama a draw_body igual que los planetas hijos)
        {
            mat4 sun_matrix = scale(view, vec3(sun.size * 1.3f));
            vec3 c = color_for_body(sun, sun, mercury, venus, earth, moon,
                mars, phobos, jupiter, saturn, uranus, neptune);
            if (body_color_id >= 0) glUniform3fv(body_color_id, 1, value_ptr(c));
            if (model_view_matrix_id >= 0) glUniformMatrix4fv(model_view_matrix_id, 1, GL_FALSE, value_ptr(sun_matrix));
            glBindVertexArray(vao_id);
            glDrawElements(GL_TRIANGLES, number_of_indices, GL_UNSIGNED_INT, 0);
            glBindVertexArray(0);
        }

        // Planetas (dibujaran sus orbitas internamente en draw_body)
        draw_body(view, mercury);
        draw_body(view, venus);
        draw_body(view, earth);
        draw_body(view, mars);
        draw_body(view, jupiter);
        draw_body(view, saturn);
        draw_body(view, uranus);
        draw_body(view, neptune);

        // Satélites
        {
            // Luna
            mat4 earth_parent = rotate(view, earth.angle, vec3(0, 1, 0));
            earth_parent = translate(earth_parent, vec3(earth.distance, 0, 0));
            draw_body(earth_parent, moon);

            // Phobos
            mat4 mars_parent = rotate(view, mars.angle, vec3(0, 1, 0));
            mars_parent = translate(mars_parent, vec3(mars.distance, 0, 0));
            draw_body(mars_parent, phobos);
        }
    }

    // ==========================================================
    // Redimensionamiento
    // ==========================================================
    void Scene::resize(int width, int height) {
        if (height == 0) height = 1;
        glViewport(0, 0, width, height);
        // CORRECCION CRÍTICA: Far plane aumentado de 100.0f a 1000.0f
        // El SkyBox escala a 500.0f, asi que con 100.0f se recortaba.
        projection_matrix = perspective(radians(45.0f), float(width) / float(height), 0.1f, 1000.0f);

        if (program_id != 0 && projection_matrix_id >= 0) {
            glUseProgram(program_id);
            glUniformMatrix4fv(projection_matrix_id, 1, GL_FALSE, value_ptr(projection_matrix));
        }
    }

    // ==========================================================
    // Generación de geometría de esfera
    // ==========================================================
    void Scene::create_sphere_geometry(int stacks, int slices) {
        // ... (Tu código original estaba bien, solo limpiamos los includes al inicio)
        vector<vec3> positions;
        vector<unsigned int> indices;

        positions.reserve((stacks + 1) * (slices + 1));
        for (int i = 0; i <= stacks; ++i) {
            float v = float(i) / float(stacks);
            float phi = v * pi<float>();
            float y = cos(phi);
            float r = sin(phi);

            for (int j = 0; j <= slices; ++j) {
                float u = float(j) / float(slices);
                float theta = u * two_pi<float>();
                float x = r * cos(theta);
                float z = r * sin(theta);

                positions.emplace_back(x, y, z);
            }
        }

        int verts_per_row = slices + 1;
        for (int i = 0; i < stacks; ++i) {
            for (int j = 0; j < slices; ++j) {
                unsigned int row1 = i * verts_per_row;
                unsigned int row2 = (i + 1) * verts_per_row;
                unsigned int i0 = row1 + j;
                unsigned int i1 = row2 + j;
                unsigned int i2 = row2 + j + 1;
                unsigned int i3 = row1 + j + 1;
                indices.insert(indices.end(), { i0, i1, i2, i0, i2, i3 });
            }
        }

        number_of_indices = static_cast<GLsizei>(indices.size());

        if (vao_id) { glDeleteVertexArrays(1, &vao_id); vao_id = 0; }
        glGenVertexArrays(1, &vao_id);

        if (vbo_ids[0] != 0 || vbo_ids[1] != 0) { glDeleteBuffers(2, vbo_ids); vbo_ids[0] = vbo_ids[1] = 0; }
        glGenBuffers(2, vbo_ids);

        glBindVertexArray(vao_id);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_ids[0]);
        glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(vec3), positions.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vec3), (void*)0);
        glEnableVertexAttribArray(0);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_ids[1]);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
        glBindVertexArray(0);
    }

    // ==========================================================
    // OPTIMIZACIÓN: Crear geometría de órbita (Init)
    // ==========================================================
    void Scene::create_orbit_geometry() {
        const int segments = 200;
        std::vector<glm::vec3> orbit_points;
        orbit_points.reserve(segments);

        // Crear un circulo unitario en XZ
        for (int i = 0; i < segments; ++i) {
            float angle = glm::two_pi<float>() * float(i) / float(segments);
            orbit_points.emplace_back(cos(angle), 0.0f, sin(angle));
        }

        if (orbit_vao_id) { glDeleteVertexArrays(1, &orbit_vao_id); }
        glGenVertexArrays(1, &orbit_vao_id);

        if (orbit_vbo_id) { glDeleteBuffers(1, &orbit_vbo_id); }
        glGenBuffers(1, &orbit_vbo_id);

        glBindVertexArray(orbit_vao_id);
        glBindBuffer(GL_ARRAY_BUFFER, orbit_vbo_id);
        glBufferData(GL_ARRAY_BUFFER, orbit_points.size() * sizeof(glm::vec3), orbit_points.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
        glBindVertexArray(0);

        orbit_vertex_count = segments;
    }

    // ==========================================================
    // Dibujar órbitas (Render eficiente)
    // ==========================================================
    void Scene::draw_orbit(const glm::mat4& view, float radius) {
        if (orbit_vao_id == 0) return;

        // Guardar estados
        GLboolean cull_enabled = glIsEnabled(GL_CULL_FACE);
        glDisable(GL_CULL_FACE);

        // Configurar shader
        glUseProgram(program_id);

        // Color gris tenue para la orbita
        if (body_color_id >= 0) glUniform3fv(body_color_id, 1, glm::value_ptr(glm::vec3(0.3f, 0.3f, 0.3f)));

        // Matriz: Usamos la 'view' (que es el padre, ej: posición del sol)
        // y escalamos el circulo unitario por el radio de la órbita.
        glm::mat4 model = glm::scale(view, glm::vec3(radius, 1.0f, radius));

        if (model_view_matrix_id >= 0) glUniformMatrix4fv(model_view_matrix_id, 1, GL_FALSE, glm::value_ptr(model));

        glBindVertexArray(orbit_vao_id);
        glDrawArrays(GL_LINE_LOOP, 0, orbit_vertex_count);
        glBindVertexArray(0);

        // Restaurar estados
        if (cull_enabled) glEnable(GL_CULL_FACE);
    }

    // ==========================================================
    // Compilación de shaders (Igual que antes)
    // ==========================================================
    GLuint Scene::compile_shaders() {
        // (Tu codigo de compilacion original estaba bien)
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
        const char* vsrc = vertex_shader_code.c_str();
        glShaderSource(vs, 1, &vsrc, nullptr);
        glCompileShader(vs);
        if (!check_shader(vs)) { glDeleteShader(vs); return 0; }

        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
        const char* fsrc = fragment_shader_code.c_str();
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

    vec3 Scene::random_color() {
        static bool seeded = false;
        if (!seeded) { srand(42); seeded = true; }
        return vec3(rand() / float(RAND_MAX), rand() / float(RAND_MAX), rand() / float(RAND_MAX));
    }

} // namespace udit