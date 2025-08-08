
#include "engine/scene_config_manager.h"
#include "engine/scene_loader.h"
#include "src/engine/engine.h"
#include "src/utility/utils.h"

#include <cstdlib>
#include <iostream>


void load()
{
     dae::scene_config_manager::instance().load_scene_config("configs/scene_config.json");
     dae::scene_loader::instance().load_scenes();
}

int main()
{
    try
    {
         dae::engine engine{"data/"};
         engine.run(load);
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}