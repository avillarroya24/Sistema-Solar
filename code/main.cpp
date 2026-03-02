#include "Scene.hpp"
#include "Window.hpp"
#include <SDL3/SDL_main.h>

using udit::Scene;
using udit::Window;

int main(int , char ** )
{
    constexpr unsigned viewport_width = 1024;
    constexpr unsigned viewport_height = 576;


    Window window("Sistema Solar", viewport_width, viewport_height, { 3,3 });
    Scene scene(viewport_width, viewport_height);

    bool exit = false;
    SDL_Event event;

    while (!exit)
    {
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT) exit = true;
        }

        scene.update();
        scene.render();
        window.swap_buffers();
    }

    SDL_Quit();
    return 0;
}
