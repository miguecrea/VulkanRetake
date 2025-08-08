﻿#include "engine.h"

// Project includes
#include "src/core/factory.h"
#include "src/engine/camera.h"
#include "src/engine/frame_info.h"
#include "src/engine/game_time.h"
#include "src/engine/scene_manager.h"
#include "src/input/movement_controller.h"
#include "src/input/shading_mode_controller.h"
#include "src/system/point_light_system.h"
#include "src/system/render_2d_system.h"
#include "src/system/render_3d_system.h"
#include "src/system/texture_pbr_system.h"
#include "src/utility/texture.h"
#include "src/vulkan/buffer.h"
#include "src/vulkan/device.h"
#include "src/vulkan/renderer.h"

#include <chrono>
#include <thread>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include "scene_loader.h"
#include "src/system/material_pbr_system.h"

namespace dae
{
    std::string engine::data_path;

    engine::engine(std::string const& path)
    {
        data_path = path;


        window_ptr_ = &window::instance();
        window_ptr_->init(width, height, "Graphics Programming 2 ");

        device_ptr_ = &device::instance();
        renderer_ptr_ = &renderer::instance();

        global_pool_ = descriptor_pool::builder().set_max_sets(swap_chain::MAX_FRAMES_IN_FLIGHT).add_pool_size(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, swap_chain::MAX_FRAMES_IN_FLIGHT).add_pool_size(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, swap_chain::MAX_FRAMES_IN_FLIGHT)
            .build();
    }

    void engine::run(std::function<void()> const& load)
    {


        std::vector<std::unique_ptr<buffer>> ubo_buffers(swap_chain::MAX_FRAMES_IN_FLIGHT);

        //ubo 
        for (int i = 0; i < ubo_buffers.size(); ++i)
        {
            ubo_buffers[i] = std::make_unique<buffer>(
                sizeof(global_ubo),
                1,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
            );
            ubo_buffers[i]->map();
        }





        //build descriptor set layout 

        //and build it with 5 bindings 

        //  A binding refers to a slot or index in the descriptor set.

        descriptor_set_layout::builder Builder{};

        //first is a uniform buffer 
        //2 patrameters type of resorice bound and that spot and ,which sahder stages can acces this binding 
        Builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS);
        Builder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
        Builder.add_binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
        Builder.add_binding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
        Builder.add_binding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
        Builder.add_binding(5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

        std::unique_ptr<descriptor_set_layout> global_set_layout = Builder.build();




        // scenes
        auto & scene_manager = scene_manager::instance();


        scene_manager.create_scene("2d", std::make_unique<render_2d_system>(global_set_layout->get_descriptor_set_layout()));
        scene_manager.create_scene("3d", std::make_unique<render_3d_system>(global_set_layout->get_descriptor_set_layout()));
        scene_manager.create_scene("material_pbr", std::make_unique<material_pbr_system>(global_set_layout->get_descriptor_set_layout()));
        scene_manager.create_scene("texture_pbr", std::make_unique<texture_pbr_system>(global_set_layout->get_descriptor_set_layout()));
        scene_manager.create_scene("light", std::make_unique<point_light_system>(global_set_layout->get_descriptor_set_layout()));



        //create game objects //and create models load vertex and index buffers 
        load();

        // textures

        texture diffuse_texture{ scene_loader::instance().diffuse_texture_path(), VK_FORMAT_R8G8B8A8_SRGB };
        texture normal_texture{ scene_loader::instance().normal_texture_path(), VK_FORMAT_R8G8B8A8_UNORM };
        texture specular_texture{ scene_loader::instance().specular_texture_path(), VK_FORMAT_R8G8B8A8_SRGB };
        texture gloss_texture{ scene_loader::instance().glossiness_texture_path(), VK_FORMAT_R8G8B8A8_SRGB };
        texture texture{ scene_loader::instance().texture_path(), VK_FORMAT_R8G8B8A8_SRGB };





        //Which sampler to use(how to read the texture : filtering, wrapping, etc.)

        //Which image view to sample from(a "view" into the texture image)

        //In what layout the image is currently(e.g.VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)



        //descriptor set image of each texture 
        VkDescriptorImageInfo diffuse_image_info{};
        diffuse_image_info.sampler = diffuse_texture.sampler();
        diffuse_image_info.imageView = diffuse_texture.image_view();
        diffuse_image_info.imageLayout = diffuse_texture.image_layout();

        VkDescriptorImageInfo normal_image_info{};
        normal_image_info.sampler = normal_texture.sampler();
        normal_image_info.imageView = normal_texture.image_view();
        normal_image_info.imageLayout = normal_texture.image_layout();

        VkDescriptorImageInfo specular_image_info{};
        specular_image_info.sampler = specular_texture.sampler();
        specular_image_info.imageView = specular_texture.image_view();
        specular_image_info.imageLayout = specular_texture.image_layout();

        VkDescriptorImageInfo emission_image_info{};
        emission_image_info.sampler = gloss_texture.sampler();
        emission_image_info.imageView = gloss_texture.image_view();
        emission_image_info.imageLayout = gloss_texture.image_layout();

        VkDescriptorImageInfo texture_image_info{};
        texture_image_info.sampler = texture.sampler();
        texture_image_info.imageView = texture.image_view();
        texture_image_info.imageLayout = texture.image_layout();



        //Creating descriptor sets for each frame


        std::vector<VkDescriptorSet> global_descriptor_sets(swap_chain::MAX_FRAMES_IN_FLIGHT);

        for (int i = 0; i < global_descriptor_sets.size(); ++i)
        {

            //buffer info 
            auto buffer_info = ubo_buffers[i]->descriptor_info();


           // VkDescriptorBufferInfo

            /*The buffer handle

                Offset

                Size*/



            descriptor_writer(global_set_layout.get(), global_pool_.get())
                .write_buffer(0, &buffer_info)  //binds UBO 
                .write_image(1, &diffuse_image_info)
                .write_image(2, &normal_image_info)
                .write_image(3, &specular_image_info)
                .write_image(4, &emission_image_info)
                .write_image(5, &texture_image_info)
                .build(global_descriptor_sets[i]);
            // Actually allocates the descriptor set and updates it with all these bindings.
        }

        camera camera{};
        
        auto viewer_object = game_object("viewer");
        viewer_object.transform.translation = { 0.0f, -1.5f, -5.0f };
        viewer_object.transform.rotation = { -0.2f, 0.0f, 0.0f };
        movement_controller camera_controller = {};

        // register input callbacks
        glfwSetKeyCallback(window_ptr_->get_glfw_window(), shading_mode_controller::key_callback);

        // ubo and frame info
        global_ubo ubo{};


        auto & frame_info = frame_info::instance();

        // time
        using namespace std::chrono;
        using namespace std::chrono_literals;
        auto last_time = high_resolution_clock::now();
        float lag = 0.0f;

        while (not window_ptr_->should_close())
        {
            glfwPollEvents();

            auto current_time = high_resolution_clock::now();
            game_time::instance().set_delta_time(duration<float>(current_time - last_time).count()); // dt always has a 1 frame delay

            last_time = current_time;
            lag += game_time::instance().delta_time();




            // camera
            camera_controller.move(window_ptr_->get_glfw_window(), viewer_object);
            camera.set_view_yxz(viewer_object.transform.translation, viewer_object.transform.rotation);

            float aspect = renderer_ptr_->aspect_ratio();
            camera.set_orthographic_projection(-aspect, aspect, -1, 1, -1, 1);
            camera.set_perspective_projection(glm::radians(50.0f), aspect, 0.1f, 10.0f);



            if (auto command_buffer = renderer_ptr_->begin_frame())
            {
                int frame_index = renderer_ptr_->frame_index();

                // frame info
                frame_info.frame_index = frame_index;
                frame_info.command_buffer = command_buffer;
                frame_info.camera_ptr = &camera;
                frame_info.global_descriptor_set = global_descriptor_sets[frame_index];
                frame_info.ubo_ptr = &ubo;



                // ubo
                ubo.projection = camera.get_projection();
                ubo.view = camera.get_view();
                ubo.inverse_view = camera.get_inverse_view();

                // update all scenes 
                scene_manager.update();
                
                ubo_buffers[frame_index]->write_to_buffer(&ubo);
                ubo_buffers[frame_index]->flush();

                // render
                renderer_ptr_->begin_swap_chain_render_pass(command_buffer);
                scene_manager.render();
                renderer_ptr_->end_swap_chain_render_pass(command_buffer);
                renderer_ptr_->end_frame();

                auto const sleep_time = current_time + milliseconds(static_cast<long long>(game_time::instance().ms_per_frame())) - high_resolution_clock::now();
                std::this_thread::sleep_for(sleep_time);
            }
        }
        vkDeviceWaitIdle(device_ptr_->logical_device());
    }
}
