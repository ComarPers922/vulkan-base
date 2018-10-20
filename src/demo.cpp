#include "common.h"
#include "demo.h"
#include "geometry.h"
#include "matrix.h"
#include "vk.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/impl/imgui_impl_vulkan.h"
#include "imgui/impl/imgui_impl_sdl.h"

#include "sdl/SDL_scancode.h"

#include <chrono>

static Vk_Image load_texture(const std::string& texture_file) {
    int w, h;
    int component_count;

    std::string abs_path = get_resource_path(texture_file);

    auto rgba_pixels = stbi_load(abs_path.c_str(), &w, &h, &component_count,STBI_rgb_alpha);
    if (rgba_pixels == nullptr)
        error("failed to load image file: " + abs_path);

    Vk_Image texture = vk_create_texture(w, h, VK_FORMAT_R8G8B8A8_SRGB, true, rgba_pixels, 4, texture_file.c_str());
    stbi_image_free(rgba_pixels);
    return texture;
};

static const VkDescriptorPoolSize descriptor_pool_sizes[] = {
    {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             16},
    {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,              16},
    {VK_DESCRIPTOR_TYPE_SAMPLER,                    16},
    {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              16},
    {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NVX, 16},
};

constexpr uint32_t max_descriptor_sets = 64;

void Vk_Demo::initialize(Vk_Create_Info vk_create_info, SDL_Window* sdl_window) {
    this->sdl_window = sdl_window;

    vk_create_info.descriptor_pool_sizes        = descriptor_pool_sizes;
    vk_create_info.descriptor_pool_size_count   = (uint32_t)std::size(descriptor_pool_sizes);
    vk_create_info.max_descriptor_sets          = max_descriptor_sets;

    vk_initialize(vk_create_info);

    // Device properties.
    {
        VkPhysicalDeviceRaytracingPropertiesNVX raytracing_properties { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAYTRACING_PROPERTIES_NVX };
        VkPhysicalDeviceProperties2 physical_device_properties { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
        physical_device_properties.pNext = &raytracing_properties;
        vkGetPhysicalDeviceProperties2(vk.physical_device, &physical_device_properties);

        rt.shader_header_size = raytracing_properties.shaderHeaderSize;

        printf("Device: %s\n", physical_device_properties.properties.deviceName);
        printf("Vulkan API version: %d.%d.%d\n",
            VK_VERSION_MAJOR(physical_device_properties.properties.apiVersion),
            VK_VERSION_MINOR(physical_device_properties.properties.apiVersion),
            VK_VERSION_PATCH(physical_device_properties.properties.apiVersion)
        );

        printf("\n");
        printf("VkPhysicalDeviceRaytracingPropertiesNVX:\n");
        printf("  shaderHeaderSize = %u\n", raytracing_properties.shaderHeaderSize);
        printf("  maxRecursionDepth = %u\n", raytracing_properties.maxRecursionDepth);
        printf("  maxGeometryCount = %u\n", raytracing_properties.maxGeometryCount);
    }

    // Geometry buffers.
    {
        Model model = load_obj_model(get_resource_path("iron-man/model.obj"));
        model_vertex_count = static_cast<uint32_t>(model.vertices.size());
        model_index_count = static_cast<uint32_t>(model.indices.size());
        {
            const VkDeviceSize size = model.vertices.size() * sizeof(model.vertices[0]);
            vertex_buffer = vk_create_buffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, "vertex_buffer");
            vk_ensure_staging_buffer_allocation(size);
            memcpy(vk.staging_buffer_ptr, model.vertices.data(), size);

            vk_record_and_run_commands(vk.command_pool, vk.queue, [&size, this](VkCommandBuffer command_buffer) {
                VkBufferCopy region;
                region.srcOffset = 0;
                region.dstOffset = 0;
                region.size = size;
                vkCmdCopyBuffer(command_buffer, vk.staging_buffer, vertex_buffer.handle, 1, &region);
            });
        }
        {
            const VkDeviceSize size = model.indices.size() * sizeof(model.indices[0]);
            index_buffer = vk_create_buffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, "index_buffer");
            vk_ensure_staging_buffer_allocation(size);
            memcpy(vk.staging_buffer_ptr, model.indices.data(), size);

            vk_record_and_run_commands(vk.command_pool, vk.queue, [&size, this](VkCommandBuffer command_buffer) {
                VkBufferCopy region;
                region.srcOffset = 0;
                region.dstOffset = 0;
                region.size = size;
                vkCmdCopyBuffer(command_buffer, vk.staging_buffer, index_buffer.handle, 1, &region);
            });
        }
    }

    // Texture.
    {
        texture = load_texture("iron-man/model.jpg");

        VkSamplerCreateInfo create_info { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
        create_info.magFilter           = VK_FILTER_LINEAR;
        create_info.minFilter           = VK_FILTER_LINEAR;
        create_info.mipmapMode          = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        create_info.addressModeU        = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        create_info.addressModeV        = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        create_info.addressModeW        = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        create_info.mipLodBias          = 0.0f;
        create_info.anisotropyEnable    = VK_FALSE;
        create_info.maxAnisotropy       = 1;
        create_info.minLod              = 0.0f;
        create_info.maxLod              = 12.0f;

        VK_CHECK(vkCreateSampler(vk.device, &create_info, nullptr, &sampler));
        vk_set_debug_name(sampler, "diffuse_texture_sampler");
    }

    // UI render pass.
    {
        VkAttachmentDescription attachments[1] = {};
        attachments[0].format           = VK_FORMAT_R16G16B16A16_SFLOAT;
        attachments[0].samples          = VK_SAMPLE_COUNT_1_BIT;
        attachments[0].loadOp           = VK_ATTACHMENT_LOAD_OP_LOAD;
        attachments[0].storeOp          = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[0].stencilLoadOp    = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[0].stencilStoreOp   = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[0].initialLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        attachments[0].finalLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkAttachmentReference color_attachment_ref;
        color_attachment_ref.attachment = 0;
        color_attachment_ref.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount    = 1;
        subpass.pColorAttachments       = &color_attachment_ref;

        VkRenderPassCreateInfo create_info{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
        create_info.attachmentCount = (uint32_t)std::size(attachments);
        create_info.pAttachments = attachments;
        create_info.subpassCount = 1;
        create_info.pSubpasses = &subpass;

        VK_CHECK(vkCreateRenderPass(vk.device, &create_info, nullptr, &ui_render_pass));
        vk_set_debug_name(ui_render_pass, "ui_render_pass");
    }

    create_output_image();
    create_ui_framebuffer();

    VkGeometryTrianglesNVX model_triangles { VK_STRUCTURE_TYPE_GEOMETRY_TRIANGLES_NVX };
    model_triangles.vertexData    = vertex_buffer.handle;
    model_triangles.vertexOffset  = 0;
    model_triangles.vertexCount   = model_vertex_count;
    model_triangles.vertexStride  = sizeof(Vertex);
    model_triangles.vertexFormat  = VK_FORMAT_R32G32B32_SFLOAT;
    model_triangles.indexData     = index_buffer.handle;
    model_triangles.indexOffset   = 0;
    model_triangles.indexCount    = model_index_count;
    model_triangles.indexType     = VK_INDEX_TYPE_UINT16;

    raster.create(texture.view, sampler, output_image.view);

    if (vk.raytracing_supported)
        rt.create(model_triangles, texture.view, sampler, output_image.view);

    copy_to_swapchain.create(output_image.view);

    setup_imgui();

    last_frame_time = Clock::now();
}

void Vk_Demo::shutdown() {
    VK_CHECK(vkDeviceWaitIdle(vk.device));
    release_imgui();

    vertex_buffer.destroy();
    index_buffer.destroy();

    texture.destroy();
    vkDestroySampler(vk.device, sampler, nullptr);

    output_image.destroy();
    destroy_ui_framebuffer();

    vkDestroyRenderPass(vk.device, ui_render_pass, nullptr);
    vkDestroyFramebuffer(vk.device, ui_framebuffer, nullptr);

    raster.destroy();

    if (vk.raytracing_supported)
        rt.destroy();

    copy_to_swapchain.destroy();;

    vk_shutdown();
}

void Vk_Demo::release_resolution_dependent_resources() {
    VK_CHECK(vkDeviceWaitIdle(vk.device));
    destroy_ui_framebuffer();
    raster.destroy_framebuffer();
    output_image.destroy();
    vk_release_resolution_dependent_resources();
}

void Vk_Demo::restore_resolution_dependent_resources() {
    vk_restore_resolution_dependent_resources(vsync);
    create_output_image();
    create_ui_framebuffer();

    raster.create_framebuffer(output_image.view);

    if (vk.raytracing_supported)
        rt.update_output_image_descriptor(output_image.view);

    copy_to_swapchain.update_resolution_dependent_descriptors(output_image.view);
    last_frame_time = Clock::now();
}

void Vk_Demo::create_ui_framebuffer() {
    VkFramebufferCreateInfo create_info { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
    create_info.renderPass      = ui_render_pass;
    create_info.attachmentCount = 1;
    create_info.pAttachments    = &output_image.view;
    create_info.width           = vk.surface_size.width;
    create_info.height          = vk.surface_size.height;
    create_info.layers          = 1;

    VK_CHECK(vkCreateFramebuffer(vk.device, &create_info, nullptr, &ui_framebuffer));
}

void Vk_Demo::destroy_ui_framebuffer() {
    vkDestroyFramebuffer(vk.device, ui_framebuffer, nullptr);
    ui_framebuffer = VK_NULL_HANDLE;
}

void Vk_Demo::create_output_image() {
    output_image = vk_create_image(vk.surface_size.width, vk.surface_size.height, VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, "output_image");

    if (raytracing) {
        vk_record_and_run_commands(vk.command_pool, vk.queue, [this](VkCommandBuffer command_buffer) {
            vk_record_image_layout_transition(command_buffer, output_image.handle, VK_IMAGE_ASPECT_COLOR_BIT,
                0, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
        });
    }
}

void Vk_Demo::setup_imgui() {
    ImGui::CreateContext();
    ImGui_ImplSDL2_InitForVulkan(sdl_window);

    // Setup Vulkan binding
    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.Instance          = vk.instance;
    init_info.PhysicalDevice    = vk.physical_device;
    init_info.Device            = vk.device;
    init_info.QueueFamily       = vk.queue_family_index;
    init_info.Queue             = vk.queue;
    init_info.DescriptorPool    = vk.descriptor_pool;

    ImGui_ImplVulkan_Init(&init_info, ui_render_pass);
    ImGui::StyleColorsDark();

    vk_record_and_run_commands(vk.command_pool, vk.queue, [](VkCommandBuffer cb) {
        ImGui_ImplVulkan_CreateFontsTexture(cb);
    });
    ImGui_ImplVulkan_InvalidateFontUploadObjects();
}

void Vk_Demo::release_imgui() {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
}

void Vk_Demo::do_imgui() {
    ImGuiIO& io = ImGui::GetIO();

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame(sdl_window);
    ImGui::NewFrame();

    if (!io.WantCaptureKeyboard) {
        if (ImGui::IsKeyPressed(SDL_SCANCODE_F10)) {
            show_ui = !show_ui;
        }
    }

    if (show_ui) {
        const float DISTANCE = 10.0f;
        static int corner = 0;

        ImVec2 window_pos = ImVec2((corner & 1) ? ImGui::GetIO().DisplaySize.x - DISTANCE : DISTANCE,
                                   (corner & 2) ? ImGui::GetIO().DisplaySize.y - DISTANCE : DISTANCE);

        ImVec2 window_pos_pivot = ImVec2((corner & 1) ? 1.0f : 0.0f, (corner & 2) ? 1.0f : 0.0f);

        if (corner != -1)
            ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
        ImGui::SetNextWindowBgAlpha(0.3f);

        if (ImGui::Begin("UI", &show_ui, 
            (corner != -1 ? ImGuiWindowFlags_NoMove : 0) | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav))
        {
            ImGui::Text("%.1f FPS (%.3f ms/frame)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::Checkbox("Vertical sync", &vsync);
            ImGui::Checkbox("Animate", &animate);

            if (!vk.raytracing_supported) {
                ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
            }
            ImGui::Checkbox("Raytracing", &raytracing);
            if (!vk.raytracing_supported) {
                ImGui::PopItemFlag();
                ImGui::PopStyleVar();
            }

            if (ImGui::BeginPopupContextWindow()) {
                if (ImGui::MenuItem("Custom",       NULL, corner == -1)) corner = -1;
                if (ImGui::MenuItem("Top-left",     NULL, corner == 0)) corner = 0;
                if (ImGui::MenuItem("Top-right",    NULL, corner == 1)) corner = 1;
                if (ImGui::MenuItem("Bottom-left",  NULL, corner == 2)) corner = 2;
                if (ImGui::MenuItem("Bottom-right", NULL, corner == 3)) corner = 3;
                if (ImGui::MenuItem("Close")) show_ui = false;
                ImGui::EndPopup();
            }
        }
        ImGui::End();
    }
}

void Vk_Demo::run_frame() {
    // Update time.
    Time current_time = Clock::now();
    if (animate) {
        double time_delta = std::chrono::duration_cast<std::chrono::microseconds>(current_time - last_frame_time).count() / 1e6;
        sim_time += time_delta;
    }
    last_frame_time = current_time;

    // Update resources.
    model_transform = rotate_y(Matrix3x4::identity, (float)sim_time * radians(30.0f));
    view_transform = look_at_transform(Vector(0, 0.5, 3.0), Vector(0), Vector(0, 1, 0));
    raster.update(model_transform, view_transform);

    if (vk.raytracing_supported)
        rt.update_instance(model_transform);

    do_imgui();

    vk_begin_frame();
    bool old_vsync = vsync;

    // Draw image.
    if (raytracing)
        draw_raytraced_image();
    else
       draw_rasterized_image();

    // Draw ImGui.
    {
        ImGui::Render();
        
        VkRenderPassBeginInfo render_pass_begin_info{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
        render_pass_begin_info.renderPass           = ui_render_pass;
        render_pass_begin_info.framebuffer          = ui_framebuffer;
        render_pass_begin_info.renderArea.extent    = vk.surface_size;

        vkCmdBeginRenderPass(vk.command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), vk.command_buffer);
        vkCmdEndRenderPass(vk.command_buffer);
    }

    // Copy output image to swapchain.
    {
        const uint32_t group_size_x = 32; // according to shader
        const uint32_t group_size_y = 32;

        uint32_t group_count_x = (vk.surface_size.width + group_size_x - 1) / group_size_x;
        uint32_t group_count_y = (vk.surface_size.height + group_size_y - 1) / group_size_y;

        VkImageMemoryBarrier output_image_barrier { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        output_image_barrier.srcAccessMask = raytracing ? VK_ACCESS_SHADER_WRITE_BIT : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        output_image_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        output_image_barrier.oldLayout = raytracing ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        output_image_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        output_image_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        output_image_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        output_image_barrier.image = output_image.handle;
        output_image_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        output_image_barrier.subresourceRange.levelCount = 1;
        output_image_barrier.subresourceRange.layerCount = 1;

        vkCmdPipelineBarrier(vk.command_buffer,
            raytracing ? VK_PIPELINE_STAGE_RAYTRACING_BIT_NVX : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &output_image_barrier);

        vk_record_image_layout_transition(vk.command_buffer, vk.swapchain_info.images[vk.swapchain_image_index], VK_IMAGE_ASPECT_COLOR_BIT,
            0, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

        uint32_t push_constants[] = { vk.surface_size.width, vk.surface_size.height };

        vkCmdPushConstants(vk.command_buffer, copy_to_swapchain.pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
            0, sizeof(push_constants), push_constants);

        vkCmdBindDescriptorSets(vk.command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, copy_to_swapchain.pipeline_layout,
            0, 1, &copy_to_swapchain.sets[vk.swapchain_image_index], 0, nullptr);

        vkCmdBindPipeline(vk.command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, copy_to_swapchain.pipeline);
        vkCmdDispatch(vk.command_buffer, group_count_x, group_count_y, 1);

        vk_record_image_layout_transition(vk.command_buffer, vk.swapchain_info.images[vk.swapchain_image_index], VK_IMAGE_ASPECT_COLOR_BIT,
            VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    }

    vk_end_frame();

    if (vsync != old_vsync) {
        release_resolution_dependent_resources();
        restore_resolution_dependent_resources();
    }
}

void Vk_Demo::draw_rasterized_image() {
    VkViewport viewport{};
    viewport.width = static_cast<float>(vk.surface_size.width);
    viewport.height = static_cast<float>(vk.surface_size.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.extent = vk.surface_size;

    vkCmdSetViewport(vk.command_buffer, 0, 1, &viewport);
    vkCmdSetScissor(vk.command_buffer, 0, 1, &scissor);

    VkClearValue clear_values[2];
    clear_values[0].color = {srgb_encode(0.32f), srgb_encode(0.32f), srgb_encode(0.4f), 0.0f};
    clear_values[1].depthStencil.depth = 1.0;
    clear_values[1].depthStencil.stencil = 0;

    VkRenderPassBeginInfo render_pass_begin_info { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    render_pass_begin_info.renderPass        = raster.render_pass;
    render_pass_begin_info.framebuffer       = raster.framebuffer;
    render_pass_begin_info.renderArea.extent = vk.surface_size;
    render_pass_begin_info.clearValueCount   = (uint32_t)std::size(clear_values);
    render_pass_begin_info.pClearValues      = clear_values;

    vkCmdBeginRenderPass(vk.command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
    const VkDeviceSize zero_offset = 0;
    vkCmdBindVertexBuffers(vk.command_buffer, 0, 1, &vertex_buffer.handle, &zero_offset);
    vkCmdBindIndexBuffer(vk.command_buffer, index_buffer.handle, 0, VK_INDEX_TYPE_UINT16);
    vkCmdBindDescriptorSets(vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, raster.pipeline_layout, 0, 1, &raster.descriptor_set, 0, nullptr);
    vkCmdBindPipeline(vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, raster.pipeline);
    vkCmdDrawIndexed(vk.command_buffer, model_index_count, 1, 0, 0, 0);
    vkCmdEndRenderPass(vk.command_buffer);
}

void Vk_Demo::draw_raytraced_image() {
    vkCmdBuildAccelerationStructureNVX(vk.command_buffer,
        VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NVX,
        1, rt.instance_buffer, 0,
        0, nullptr,
        VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_NVX,
        VK_TRUE, rt.top_level_accel, VK_NULL_HANDLE,
        rt.scratch_buffer, 0);

    VkMemoryBarrier barrier { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
    barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NVX | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NVX;
    barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NVX | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NVX;

    vkCmdPipelineBarrier(vk.command_buffer, VK_PIPELINE_STAGE_RAYTRACING_BIT_NVX, VK_PIPELINE_STAGE_RAYTRACING_BIT_NVX,
        0, 1, &barrier, 0, nullptr, 0, nullptr);

    const VkBuffer sbt = rt.shader_binding_table.handle;
    const uint32_t sbt_slot_size = rt.shader_header_size;

    vkCmdBindDescriptorSets(vk.command_buffer, VK_PIPELINE_BIND_POINT_RAYTRACING_NVX, rt.pipeline_layout, 0, 1, &rt.descriptor_set, 0, nullptr);
    vkCmdBindPipeline(vk.command_buffer, VK_PIPELINE_BIND_POINT_RAYTRACING_NVX, rt.pipeline);

    vkCmdTraceRaysNVX(vk.command_buffer,
        sbt, 0, // raygen shader
        sbt, 1 * sbt_slot_size, sbt_slot_size, // miss shader
        sbt, 2 * sbt_slot_size, sbt_slot_size, // chit shader
        vk.surface_size.width, vk.surface_size.height);
}

void Copy_To_Swapchain::create(VkImageView output_image_view) {
    // Descriptor set layout.
    {
        VkDescriptorSetLayoutBinding layout_bindings[3] {};
        layout_bindings[0].binding          = 0;
        layout_bindings[0].descriptorType   = VK_DESCRIPTOR_TYPE_SAMPLER;
        layout_bindings[0].descriptorCount  = 1;
        layout_bindings[0].stageFlags       = VK_SHADER_STAGE_COMPUTE_BIT;

        layout_bindings[1].binding          = 1;
        layout_bindings[1].descriptorType   = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        layout_bindings[1].descriptorCount  = 1;
        layout_bindings[1].stageFlags       = VK_SHADER_STAGE_COMPUTE_BIT;

        layout_bindings[2].binding          = 2;
        layout_bindings[2].descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        layout_bindings[2].descriptorCount  = 1;
        layout_bindings[2].stageFlags       = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutCreateInfo create_info { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        create_info.bindingCount = (uint32_t)std::size(layout_bindings);
        create_info.pBindings = layout_bindings;
        VK_CHECK(vkCreateDescriptorSetLayout(vk.device, &create_info, nullptr, &set_layout));
    }

    // Pipeline layout.
    {
        VkPushConstantRange range;
        range.stageFlags    = VK_SHADER_STAGE_COMPUTE_BIT;
        range.offset        = 0;
        range.size          = 8; // uint32 width + uint32 height

        VkPipelineLayoutCreateInfo create_info { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        create_info.setLayoutCount          = 1;
        create_info.pSetLayouts             = &set_layout;
        create_info.pushConstantRangeCount  = 1;
        create_info.pPushConstantRanges     = &range;

        VK_CHECK(vkCreatePipelineLayout(vk.device, &create_info, nullptr, &pipeline_layout));
    }

    // Pipeline.
    {
        VkShaderModule copy_shader = vk_load_spirv("spirv/copy_to_swapchain.comp.spv");

        VkPipelineShaderStageCreateInfo compute_stage { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        compute_stage.stage    = VK_SHADER_STAGE_COMPUTE_BIT;
        compute_stage.module   = copy_shader;
        compute_stage.pName    = "main";

        VkComputePipelineCreateInfo create_info{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
        create_info.stage = compute_stage;
        create_info.layout = pipeline_layout;
        VK_CHECK(vkCreateComputePipelines(vk.device, VK_NULL_HANDLE, 1, &create_info, nullptr, &pipeline));

        vkDestroyShaderModule(vk.device, copy_shader, nullptr);
    }

    // Point sampler.
    {
        VkSamplerCreateInfo create_info { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
        VK_CHECK(vkCreateSampler(vk.device, &create_info, nullptr, &point_sampler));
        vk_set_debug_name(point_sampler, "point_sampler");
    }

    update_resolution_dependent_descriptors(output_image_view);
}

void Copy_To_Swapchain::destroy() {
    vkDestroyDescriptorSetLayout(vk.device, set_layout, nullptr);
    vkDestroyPipelineLayout(vk.device, pipeline_layout, nullptr);
    vkDestroyPipeline(vk.device, pipeline, nullptr);
    vkDestroySampler(vk.device, point_sampler, nullptr);
    sets.clear();
}

void Copy_To_Swapchain::update_resolution_dependent_descriptors(VkImageView output_image_view) {
    if (sets.size() < vk.swapchain_info.images.size())
    {
        size_t n = vk.swapchain_info.images.size() - sets.size();
        for (size_t i = 0; i < n; i++)
        {
            VkDescriptorSetAllocateInfo alloc_info { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
            alloc_info.descriptorPool     = vk.descriptor_pool;
            alloc_info.descriptorSetCount = 1;
            alloc_info.pSetLayouts        = &set_layout;

            VkDescriptorSet set;
            VK_CHECK(vkAllocateDescriptorSets(vk.device, &alloc_info, &set));
            sets.push_back(set);

            VkDescriptorImageInfo sampler_info{};
            sampler_info.sampler = point_sampler;

            VkWriteDescriptorSet descriptor_writes[1] = {};
            descriptor_writes[0].sType              = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptor_writes[0].dstSet             = set;
            descriptor_writes[0].dstBinding         = 0;
            descriptor_writes[0].descriptorCount    = 1;
            descriptor_writes[0].descriptorType     = VK_DESCRIPTOR_TYPE_SAMPLER;
            descriptor_writes[0].pImageInfo         = &sampler_info;

            vkUpdateDescriptorSets(vk.device, (uint32_t)std::size(descriptor_writes), descriptor_writes, 0, nullptr);
        }
    }

    VkDescriptorImageInfo src_image_info{};
    src_image_info.imageView    = output_image_view;
    src_image_info.imageLayout  = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    for (size_t i = 0; i < vk.swapchain_info.images.size(); i++) {
        VkDescriptorImageInfo swapchain_image_info{};
        swapchain_image_info.imageView      = vk.swapchain_info.image_views[i];
        swapchain_image_info.imageLayout    = VK_IMAGE_LAYOUT_GENERAL;

        VkWriteDescriptorSet descriptor_writes[2] = {};
        descriptor_writes[0].sType              = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_writes[0].dstSet             = sets[i];
        descriptor_writes[0].dstBinding         = 1;
        descriptor_writes[0].descriptorCount    = 1;
        descriptor_writes[0].descriptorType     = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        descriptor_writes[0].pImageInfo         = &src_image_info;

        descriptor_writes[1].sType              = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_writes[1].dstSet             = sets[i];
        descriptor_writes[1].dstBinding         = 2;
        descriptor_writes[1].descriptorCount    = 1;
        descriptor_writes[1].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        descriptor_writes[1].pImageInfo         = &swapchain_image_info;

        vkUpdateDescriptorSets(vk.device, (uint32_t)std::size(descriptor_writes), descriptor_writes, 0, nullptr);
    }
}
