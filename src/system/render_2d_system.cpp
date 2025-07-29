﻿#include "render_2d_system.h"

// Project includes
#include "src/engine/frame_info.h"
#include "src/vulkan/device.h"
#include "src/vulkan/renderer.h"

// Standard includes
#include <ranges>
#include <stdexcept>

// GLM includes
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

namespace dae
{
    struct push_constant_data_2d
    {
        glm::mat4 transform{1.0f};
        bool use_texture;
    };
    
    render_2d_system::render_2d_system(VkDescriptorSetLayout global_set_layout)
    {
        create_pipeline_layout(global_set_layout);
        create_pipeline(renderer::instance().swap_chain_render_pass());
    }

    void render_2d_system::render()
    {
        auto &frame_info = frame_info::instance();
        pipeline_->bind(frame_info.command_buffer);

        vkCmdBindDescriptorSets(
            frame_info.command_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipeline_layout_,
            0,
            1,
            &frame_info.global_descriptor_set,
            0,
            nullptr
        );
        
        for (auto const &obj : frame_info.game_objects)
        {
            push_constant_data_2d push{};
            push.transform = obj->transform.mat4();
            push.use_texture = obj->use_texture;

            vkCmdPushConstants(
                frame_info.command_buffer,
                pipeline_layout_,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0,
                sizeof(push_constant_data_2d),
                &push);
            
            obj->model->bind(frame_info.command_buffer);
            obj->model->draw(frame_info.command_buffer);
        }
    }

    void render_2d_system::create_pipeline_layout(VkDescriptorSetLayout global_set_layout)
    {
        VkPushConstantRange push_constant_range{};
        push_constant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        push_constant_range.offset     = 0;
        push_constant_range.size       = sizeof(push_constant_data_2d);

        std::vector<VkDescriptorSetLayout> descriptor_set_layouts{global_set_layout};
        
        VkPipelineLayoutCreateInfo pipeline_layout_info{};
        pipeline_layout_info.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_info.setLayoutCount         = static_cast<uint32_t>(descriptor_set_layouts.size());
        pipeline_layout_info.pSetLayouts            = descriptor_set_layouts.data();
        pipeline_layout_info.pushConstantRangeCount = 1;
        pipeline_layout_info.pPushConstantRanges    = &push_constant_range;

        if (vkCreatePipelineLayout(device_ptr_->logical_device(), &pipeline_layout_info, nullptr, &pipeline_layout_) != VK_SUCCESS)
        {
            throw std::runtime_error{"Failed to create pipeline layout!"};
        }
    }

    void render_2d_system::create_pipeline(VkRenderPass render_pass)
    {
        assert(pipeline_layout_ != nullptr and "Cannot create pipeline before pipeline layout");
        
        pipeline_config_info pipeline_config{};
        pipeline::default_pipeline_config_info(pipeline_config);
        pipeline_config.render_pass = render_pass;
        pipeline_config.pipeline_layout = pipeline_layout_;
        pipeline_ = std::make_unique<pipeline>(
            "shaders/2d.vert.spv",
            "shaders/2d.frag.spv",
            pipeline_config);
    }
}
