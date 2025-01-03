#include "demo.h"

#include "glfw/glfw3.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_vulkan.h"
#include "imgui/imgui_impl_glfw.h"

#include <array>

#include "Constants.h"
#include "TransformComponent.h"

static VkFormat get_depth_image_format() {
    VkFormat candidates[2] = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_X8_D24_UNORM_PACK32 };
    for (auto format : candidates) {
        VkFormatProperties props{};
        vkGetPhysicalDeviceFormatProperties(vk.physical_device, format, &props);
        if ((props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0) {
            return format;
        }
    }
    error("failed to select depth attachment format");
    return VK_FORMAT_UNDEFINED;
}

void Vk_Demo::initialize(GLFWwindow* window) {
    Vk_Init_Params vk_init_params;
    vk_init_params.error_reporter = &error;

    std::array instance_extensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#ifdef VK_USE_PLATFORM_WIN32_KHR
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#endif
#ifdef VK_USE_PLATFORM_XCB_KHR
        VK_KHR_XCB_SURFACE_EXTENSION_NAME,
#endif
    };
    std::array device_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME,
        VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
    };
    vk_init_params.instance_extensions = std::span{ instance_extensions };
    vk_init_params.device_extensions = std::span{ device_extensions };

    // Specify required features.
    VkPhysicalDeviceFeatures2 features2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
    Vk_PNexer pnexer(features2);
    features2.features.samplerAnisotropy = VK_TRUE;
    vk_init_params.device_create_info_pnext = (const VkBaseInStructure*)&features2;

    VkPhysicalDeviceBufferDeviceAddressFeatures buffer_device_address_features{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES };
    buffer_device_address_features.bufferDeviceAddress = VK_TRUE;
    pnexer.next(buffer_device_address_features);

    VkPhysicalDeviceDynamicRenderingFeatures dynamic_rendering_features{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES };
    dynamic_rendering_features.dynamicRendering = VK_TRUE;
    pnexer.next(dynamic_rendering_features);

    VkPhysicalDeviceSynchronization2Features synchronization2_features{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES };
    synchronization2_features.synchronization2 = VK_TRUE;
    pnexer.next(synchronization2_features);

    VkPhysicalDeviceDescriptorBufferFeaturesEXT descriptor_buffer_features{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT };
    descriptor_buffer_features.descriptorBuffer = VK_TRUE;
    pnexer.next(descriptor_buffer_features);

    // Surface formats
    std::array surface_formats = {
        VK_FORMAT_B8G8R8A8_SRGB,
        VK_FORMAT_R8G8B8A8_SRGB,
    };
    vk_init_params.supported_surface_formats = std::span{ surface_formats };
    vk_init_params.surface_usage_flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    vk_initialize(window, vk_init_params);

    // Device properties.
    {
        VkPhysicalDeviceProperties2 physical_device_properties { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
        vkGetPhysicalDeviceProperties2(vk.physical_device, &physical_device_properties);
        printf("Device: %s\n", physical_device_properties.properties.deviceName);
        printf("Vulkan API version: %d.%d.%d\n",
            VK_VERSION_MAJOR(physical_device_properties.properties.apiVersion),
            VK_VERSION_MINOR(physical_device_properties.properties.apiVersion),
            VK_VERSION_PATCH(physical_device_properties.properties.apiVersion)
        );
    }
    auto& gpu_mesh = *castleModel.GetRenderable()->GetGPUMesh();
    // Geometry buffers.
    {
        // Triangle_Mesh mesh = load_obj_model(get_resource_path("model/mesh.obj"), 1.25f);
        // Triangle_Mesh mesh = load_obj_model(get_resource_path("model/Baloo.obj"), 1.f);
        Triangle_Mesh mesh = load_obj_model(get_resource_path("model/mine_craft_castle.obj"), 1.f);
        {
            const VkDeviceSize size = mesh.vertices.size() * sizeof(mesh.vertices[0]);
            VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            gpu_mesh.vertex_buffer = vk_create_buffer(size, usage, mesh.vertices.data(), "vertex_buffer");
            gpu_mesh.vertex_count = uint32_t(mesh.vertices.size());
        }
        {
            const VkDeviceSize size = mesh.indices.size() * sizeof(mesh.indices[0]);
            VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            gpu_mesh.index_buffer = vk_create_buffer(size, usage, mesh.indices.data(), "index_buffer");
            gpu_mesh.index_count = uint32_t(mesh.indices.size());
        }

        {
            const VkDeviceSize size = quad.vertices.size() * sizeof(quad.vertices[0]);
            VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            quad_mesh.vertex_buffer = vk_create_buffer(size, usage, quad.vertices.data(), "post-process-vertex_buffer");
            quad_mesh.vertex_count = static_cast<uint32_t>(quad.vertices.size());
        }
        {
            const VkDeviceSize size = quad.indices.size() * sizeof(quad.indices[0]);
            VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            quad_mesh.index_buffer = vk_create_buffer(size, usage, quad.indices.data(), "post-process-index_buffer");
            quad_mesh.index_count = static_cast<uint32_t>(quad.indices.size());
        }
    }

    auto& tankMesh = *tankModel.GetRenderable()->GetGPUMesh();
    {
        // Triangle_Mesh mesh = load_obj_model(get_resource_path("model/mesh.obj"), 1.25f);
        // Triangle_Mesh mesh = load_obj_model(get_resource_path("model/Baloo.obj"), 1.f);
        Triangle_Mesh mesh = load_obj_model(get_resource_path("model/Tank.obj"), 1.f);
        {
            const VkDeviceSize size = mesh.vertices.size() * sizeof(mesh.vertices[0]);
            VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            tankMesh.vertex_buffer = vk_create_buffer(size, usage, mesh.vertices.data(), "tank_vertex_buffer");
            tankMesh.vertex_count = uint32_t(mesh.vertices.size());
        }
        {
            const VkDeviceSize size = mesh.indices.size() * sizeof(mesh.indices[0]);
            VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            tankMesh.index_buffer = vk_create_buffer(size, usage, mesh.indices.data(), "tank_index_buffer");
            tankMesh.index_count = uint32_t(mesh.indices.size());
        }
    }

    auto& balooMesh = *balooModel.GetRenderable()->GetGPUMesh();
    {
        // Triangle_Mesh mesh = load_obj_model(get_resource_path("model/mesh.obj"), 1.25f);
        // Triangle_Mesh mesh = load_obj_model(get_resource_path("model/Baloo.obj"), 1.f);
        Triangle_Mesh mesh = load_obj_model(get_resource_path("model/Baloo.obj"), 1.f);
        {
            const VkDeviceSize size = mesh.vertices.size() * sizeof(mesh.vertices[0]);
            VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            balooMesh.vertex_buffer = vk_create_buffer(size, usage, mesh.vertices.data(), "baloo_vertex_buffer");
            balooMesh.vertex_count = uint32_t(mesh.vertices.size());
        }
        {
            const VkDeviceSize size = mesh.indices.size() * sizeof(mesh.indices[0]);
            VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            balooMesh.index_buffer = vk_create_buffer(size, usage, mesh.indices.data(), "baloo_index_buffer");
            balooMesh.index_count = uint32_t(mesh.indices.size());
        }
    }

    // Texture.
    {
        // texture = vk_load_texture(get_resource_path("model/diffuse.jpg"));
        // texture = vk_load_texture(get_resource_path("model/baloo_diff.png"));
        texture = vk_load_texture(get_resource_path("model/mine_craft_castle.jpg"));

        auto& modelTexture = castleModel.GetRenderable()->GetTexture();
        auto tempImage = vk_load_texture(get_resource_path("model/mine_craft_castle.jpg"));
        modelTexture = std::make_unique<Vk_Image>(std::move(tempImage));
        castleModel.GetTransform()->Transform.position.x = -.5f;


        auto& tankModelTexture = tankModel.GetRenderable()->GetTexture();
        auto tankTempImage = vk_load_texture(get_resource_path("model/Tank_Base_color.png"));
        tankModelTexture = std::make_unique<Vk_Image>(std::move(tankTempImage));
        tankModel.GetTransform()->Transform.position.x = .5f;

        auto& balooModelTexture = balooModel.GetRenderable()->GetTexture();
        auto balooTempImage = vk_load_texture(get_resource_path("model/baloo_diff.png"));
        balooModelTexture = std::make_unique<Vk_Image>(std::move(balooTempImage));
        balooModel.GetTransform()->Transform.position.z = -2.f;

        VkSamplerCreateInfo create_info { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
        create_info.magFilter = VK_FILTER_LINEAR;
        create_info.minFilter = VK_FILTER_LINEAR;
        create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        create_info.mipLodBias = 0.0f;
        create_info.anisotropyEnable = VK_TRUE;
        create_info.maxAnisotropy = 16;
        create_info.minLod = 0.0f;
        create_info.maxLod = 12.0f;

        VK_CHECK(vkCreateSampler(vk.device, &create_info, nullptr, &linear_sampler));
        vk_set_debug_name(linear_sampler, "diffuse_linear_texture_sampler");
    }

    {
        VkSamplerCreateInfo create_info{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
        create_info.magFilter = VK_FILTER_NEAREST;
        create_info.minFilter = VK_FILTER_NEAREST;
        create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        create_info.mipLodBias = 0.0f;
        create_info.anisotropyEnable = VK_TRUE;
        create_info.maxAnisotropy = 16;
        create_info.minLod = 0.0f;
        create_info.maxLod = 12.0f;

        VK_CHECK(vkCreateSampler(vk.device, &create_info, nullptr, &nearest_sampler));
        vk_set_debug_name(nearest_sampler, "diffuse_nearest_texture_sampler");
    }

    uniform_buffer = vk_create_mapped_buffer(static_cast<VkDeviceSize>(sizeof(TAATransform)),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &mapped_uniform_buffer, "uniform_buffer");

    descriptor_set_layout = Vk_Descriptor_Set_Layout()
        .uniform_buffer(0, VK_SHADER_STAGE_VERTEX_BIT)
        .sampled_image(1, VK_SHADER_STAGE_FRAGMENT_BIT)
        .sampler(2, VK_SHADER_STAGE_FRAGMENT_BIT)
        .create("set_layout");

    main_texture_descriptor_set_layout = Vk_Descriptor_Set_Layout()
        .sampled_image(0, VK_SHADER_STAGE_FRAGMENT_BIT)
		.uniform_buffer(1, VK_SHADER_STAGE_VERTEX_BIT)
        .create("main_texture_set_layout", true);

    post_process_descriptor_set_layout = Vk_Descriptor_Set_Layout()
        .default_post_process()
		.sampled_image(3, VK_SHADER_STAGE_FRAGMENT_BIT)
		.sampled_image(5, VK_SHADER_STAGE_FRAGMENT_BIT)
		.sampler(4, VK_SHADER_STAGE_FRAGMENT_BIT)
        .create("post_process_set_layout");

    auto pushConstant = VkPushConstantRange{ };
    pushConstant.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstant.offset = 0;
    pushConstant.size = sizeof(Post_Process_Push_Constatnts);

    pipeline_layout = vk_create_pipeline_layout({ descriptor_set_layout, main_texture_descriptor_set_layout }, {}, "pipeline_layout");
    post_process_pipeline_layout = vk_create_pipeline_layout({ post_process_descriptor_set_layout }, { pushConstant }, "post_process_pipeline_layout");
    // Pipeline.
    Vk_Graphics_Pipeline_State state = get_default_graphics_pipeline_state();
    {
        Vk_Shader_Module vertex_shader(get_resource_path("spirv/mesh.vert.spv"));
        Vk_Shader_Module fragment_shader(get_resource_path("spirv/mesh.frag.spv"));

        // VkVertexInputBindingDescription
        state.vertex_bindings[0].binding = 0;
        state.vertex_bindings[0].stride = sizeof(Vertex);
        state.vertex_bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        state.vertex_binding_count = 1;

        // VkVertexInputAttributeDescription
        state.vertex_attributes[0].location = 0; // position
        state.vertex_attributes[0].binding = 0;
        state.vertex_attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        state.vertex_attributes[0].offset = 0;

        state.vertex_attributes[1].location = 1; // uv
        state.vertex_attributes[1].binding = 0;
        state.vertex_attributes[1].format = VK_FORMAT_R32G32_SFLOAT;
        state.vertex_attributes[1].offset = 12;

        state.vertex_attribute_count = 2;

        state.color_attachment_formats[0] = vk.surface_format.format;
        state.color_attachment_formats[1] = vk.surface_format.format;
        state.color_attachment_count = 2;
        state.depth_attachment_format = get_depth_image_format();

        state.attachment_blend_state_count = 2;

        auto& attachment_blend_state = state.attachment_blend_state[1];
        attachment_blend_state = VkPipelineColorBlendAttachmentState{};
        attachment_blend_state.blendEnable = VK_FALSE;
        attachment_blend_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        pipeline = vk_create_graphics_pipeline(state, vertex_shader.handle, fragment_shader.handle, pipeline_layout, "draw_mesh_pipeline");
    }

    Vk_Graphics_Pipeline_State post_process_state = get_default_graphics_pipeline_state();
    {
        Vk_Shader_Module post_process_vertex_shader(get_resource_path("spirv/postprocess.vert.spv"));
        Vk_Shader_Module post_process_fragment_shader(get_resource_path("spirv/postprocess.frag.spv"));

        // VkVertexInputBindingDescription
        post_process_state.vertex_bindings[0].binding = 0;
        post_process_state.vertex_bindings[0].stride = sizeof(Vertex);
        post_process_state.vertex_bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        post_process_state.vertex_binding_count = 1;

        // VkVertexInputAttributeDescription
        post_process_state.vertex_attributes[0].location = 0; // position
        post_process_state.vertex_attributes[0].binding = 0;
        post_process_state.vertex_attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        post_process_state.vertex_attributes[0].offset = 0;

        post_process_state.vertex_attributes[1].location = 1; // uv
        post_process_state.vertex_attributes[1].binding = 0;
        post_process_state.vertex_attributes[1].format = VK_FORMAT_R32G32_SFLOAT;
        post_process_state.vertex_attributes[1].offset = 12;

        post_process_state.vertex_attribute_count = 2;

        post_process_state.color_attachment_formats[0] = vk.surface_format.format;
        post_process_state.color_attachment_count = 1;
        post_process_state.depth_attachment_format = get_depth_image_format();

        post_process_pipeline = vk_create_graphics_pipeline(
            post_process_state,
            post_process_vertex_shader.handle, post_process_fragment_shader.handle,
            post_process_pipeline_layout,
            "post_process_draw_mesh_pipeline");
    }

    // Descriptor buffer.
    {
        VkPhysicalDeviceDescriptorBufferPropertiesEXT descriptor_buffer_properties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT };
        VkPhysicalDeviceProperties2 physical_device_properties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
        physical_device_properties.pNext = &descriptor_buffer_properties;
        vkGetPhysicalDeviceProperties2(vk.physical_device, &physical_device_properties);

        VkDeviceSize layout_size_in_bytes = 0;
        vkGetDescriptorSetLayoutSizeEXT(vk.device, descriptor_set_layout, &layout_size_in_bytes);

        descriptor_buffer = vk_create_mapped_buffer(
            layout_size_in_bytes,
            VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT | VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT,
            &mapped_descriptor_buffer_ptr, "descriptor_buffer"
        );
        assert(descriptor_buffer.device_address % descriptor_buffer_properties.descriptorBufferOffsetAlignment == 0);

        // Write descriptor 0 (uniform buffer)
        {
            VkDescriptorAddressInfoEXT address_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT };
            address_info.address = uniform_buffer.device_address;
            address_info.range = sizeof(TAATransform);

            VkDescriptorGetInfoEXT descriptor_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT };
            descriptor_info.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptor_info.data.pUniformBuffer = &address_info;

            VkDeviceSize offset;
            vkGetDescriptorSetLayoutBindingOffsetEXT(vk.device, descriptor_set_layout, 0, &offset);
            vkGetDescriptorEXT(vk.device, &descriptor_info, descriptor_buffer_properties.uniformBufferDescriptorSize,
                (uint8_t*)mapped_descriptor_buffer_ptr + offset);
        }

        // Write descriptor 1 (sampled image)
        {
            VkDescriptorImageInfo image_info;
            image_info.imageView = texture.view;
            image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkDescriptorGetInfoEXT descriptor_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT };
            descriptor_info.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            descriptor_info.data.pSampledImage = &image_info;

            VkDeviceSize offset;
            vkGetDescriptorSetLayoutBindingOffsetEXT(vk.device, descriptor_set_layout, 1, &offset);
            vkGetDescriptorEXT(vk.device, &descriptor_info, descriptor_buffer_properties.sampledImageDescriptorSize,
                (uint8_t*)mapped_descriptor_buffer_ptr + offset);
        }

        // Write descriptor 2 (sampler)
        {
            VkDescriptorGetInfoEXT descriptor_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT };
            descriptor_info.type = VK_DESCRIPTOR_TYPE_SAMPLER;
            descriptor_info.data.pSampler = &linear_sampler;

            VkDeviceSize offset;
            vkGetDescriptorSetLayoutBindingOffsetEXT(vk.device, descriptor_set_layout, 2, &offset);
            vkGetDescriptorEXT(vk.device, &descriptor_info, descriptor_buffer_properties.samplerDescriptorSize,
                (uint8_t*)mapped_descriptor_buffer_ptr + offset);
        }
    }

    // Descriptor buffer.
    {
        VkPhysicalDeviceDescriptorBufferPropertiesEXT descriptor_buffer_properties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT };
        VkPhysicalDeviceProperties2 physical_device_properties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
        physical_device_properties.pNext = &descriptor_buffer_properties;
        vkGetPhysicalDeviceProperties2(vk.physical_device, &physical_device_properties);

        VkDeviceSize layout_size_in_bytes = 0;
        vkGetDescriptorSetLayoutSizeEXT(vk.device, post_process_descriptor_set_layout, &layout_size_in_bytes);

        post_process_descriptor_buffer = vk_create_mapped_buffer(
            layout_size_in_bytes,
            VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT | VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT,
            &post_process_mapped_descriptor_buffer_ptr, "post_process_descriptor_buffer"
        );
        assert(post_process_descriptor_buffer.device_address % descriptor_buffer_properties.descriptorBufferOffsetAlignment == 0);

        //post_process_image = vk_create_image(vk.surface_size.width, vk.surface_size.height, VK_FORMAT_B8G8R8A8_SRGB,
        //    VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, "post_process");

        // Write descriptor 1 (sampled image)
        //{
        //    VkDescriptorImageInfo image_info;
        //    image_info.imageView = post_process_image.view;
        //    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        //    VkDescriptorGetInfoEXT descriptor_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT };
        //    descriptor_info.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        //    descriptor_info.data.pSampledImage = &image_info;

        //    VkDeviceSize offset;
        //    vkGetDescriptorSetLayoutBindingOffsetEXT(vk.device, post_process_descriptor_set_layout, 1, &offset);
        //    vkGetDescriptorEXT(vk.device, &descriptor_info, descriptor_buffer_properties.sampledImageDescriptorSize,
        //        static_cast<uint8_t*>(post_process_mapped_descriptor_buffer_ptr) + offset);
        //}

        // Write descriptor 2 (sampler)
        {
            VkDescriptorGetInfoEXT descriptor_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT };
            descriptor_info.type = VK_DESCRIPTOR_TYPE_SAMPLER;
            descriptor_info.data.pSampler = &linear_sampler;

            VkDeviceSize offset;
            vkGetDescriptorSetLayoutBindingOffsetEXT(vk.device, post_process_descriptor_set_layout, 2, &offset);
            vkGetDescriptorEXT(vk.device, &descriptor_info, descriptor_buffer_properties.samplerDescriptorSize,
                static_cast<uint8_t*>(post_process_mapped_descriptor_buffer_ptr) + offset);
        }

        {
            VkDescriptorGetInfoEXT descriptor_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT };
            descriptor_info.type = VK_DESCRIPTOR_TYPE_SAMPLER;
            descriptor_info.data.pSampler = &nearest_sampler;

            VkDeviceSize offset;
            vkGetDescriptorSetLayoutBindingOffsetEXT(vk.device, post_process_descriptor_set_layout, 4, &offset);
            vkGetDescriptorEXT(vk.device, &descriptor_info, descriptor_buffer_properties.samplerDescriptorSize,
                static_cast<uint8_t*>(post_process_mapped_descriptor_buffer_ptr) + offset);
        }
    }

    // ImGui setup.
    {
        ImGui::CreateContext();
        ImGui_ImplGlfw_InitForVulkan(window, true);

        state.color_attachment_count = 1;

        ImGui_ImplVulkan_InitInfo init_info{};
        init_info.Instance = vk.instance;
        init_info.PhysicalDevice = vk.physical_device;
        init_info.Device = vk.device;
        init_info.QueueFamily = vk.queue_family_index;
        init_info.Queue = vk.queue;
        init_info.DescriptorPool = vk.imgui_descriptor_pool;
		init_info.MinImageCount = 2;
		init_info.ImageCount = (uint32_t)vk.swapchain_info.images.size();
        init_info.UseDynamicRendering = true;
        init_info.PipelineRenderingCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
        init_info.PipelineRenderingCreateInfo.colorAttachmentCount = state.color_attachment_count;
        init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = state.color_attachment_formats;
        init_info.PipelineRenderingCreateInfo.depthAttachmentFormat = state.depth_attachment_format;

        ImGui_ImplVulkan_Init(& init_info);
        ImGui::StyleColorsDark();
        ImGui_ImplVulkan_CreateFontsTexture();
    }

    restore_resolution_dependent_resources();
    gpu_times.frame = time_keeper.allocate_time_interval();
    time_keeper.initialize_time_intervals();
}

void Vk_Demo::shutdown() {
    VK_CHECK(vkDeviceWaitIdle(vk.device));

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    motion_vec_image.destroy();
    prev_frame_image.destroy();
    post_process_image.destroy();
    release_resolution_dependent_resources();
    quad_mesh.destroy();
    // castleModel.GetRenderable()->GetTexture()->destroy();
    tankModel.Destroy();
    castleModel.Destroy();
    balooModel.Destroy();
    // secondaryTexture.destroy();
    texture.destroy();
    post_process_descriptor_buffer.destroy();
    descriptor_buffer.destroy();
    uniform_buffer.destroy();

    vkDestroySampler(vk.device, nearest_sampler, nullptr);
    vkDestroySampler(vk.device, linear_sampler, nullptr);
    vkDestroyDescriptorSetLayout(vk.device, post_process_descriptor_set_layout, nullptr);
    vkDestroyDescriptorSetLayout(vk.device, main_texture_descriptor_set_layout, nullptr);
	vkDestroyDescriptorSetLayout(vk.device, descriptor_set_layout, nullptr);
    vkDestroyPipelineLayout(vk.device, post_process_pipeline_layout, nullptr);
    vkDestroyPipelineLayout(vk.device, pipeline_layout, nullptr);
    vkDestroyPipeline(vk.device, post_process_pipeline, nullptr);
    vkDestroyPipeline(vk.device, pipeline, nullptr);

    vk_shutdown();
}

void Vk_Demo::release_resolution_dependent_resources() {
    motion_vec_image.destroy();
    prev_frame_image.destroy();
    post_process_image.destroy();
    depth_buffer_image.destroy();
}

void Vk_Demo::restore_resolution_dependent_resources() {
    // create depth buffer
    VkFormat depth_format = get_depth_image_format();
    // if (depth_buffer_image.handle == nullptr)
    {
        post_process_image = vk_create_image(vk.surface_size.width, vk.surface_size.height, VK_FORMAT_B8G8R8A8_SRGB,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, "post_process");

        prev_frame_image = vk_create_image(vk.surface_size.width, vk.surface_size.height, VK_FORMAT_B8G8R8A8_SRGB,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, "prev_frame");

        motion_vec_image = vk_create_image(vk.surface_size.width, vk.surface_size.height, VK_FORMAT_B8G8R8A8_SRGB,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, "motion_vec");
    }
    VkDescriptorImageInfo image_info;
    image_info.imageView = post_process_image.view;
    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorGetInfoEXT descriptor_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT };
    descriptor_info.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;

    descriptor_info.data.pSampledImage = &image_info;
    VkPhysicalDeviceDescriptorBufferPropertiesEXT descriptor_buffer_properties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT };
    VkPhysicalDeviceProperties2 physical_device_properties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
    physical_device_properties.pNext = &descriptor_buffer_properties;
    vkGetPhysicalDeviceProperties2(vk.physical_device, &physical_device_properties);

    VkDeviceSize offset;
    vkGetDescriptorSetLayoutBindingOffsetEXT(vk.device, post_process_descriptor_set_layout, 1, &offset);
    vkGetDescriptorEXT(vk.device, &descriptor_info, descriptor_buffer_properties.sampledImageDescriptorSize,
        static_cast<uint8_t*>(post_process_mapped_descriptor_buffer_ptr) + offset);

    image_info.imageView = prev_frame_image.view;
    vkGetDescriptorSetLayoutBindingOffsetEXT(vk.device, post_process_descriptor_set_layout, 3, &offset);
    vkGetDescriptorEXT(vk.device, &descriptor_info, descriptor_buffer_properties.sampledImageDescriptorSize,
        static_cast<uint8_t*>(post_process_mapped_descriptor_buffer_ptr) + offset);

    image_info.imageView = motion_vec_image.view;
    vkGetDescriptorSetLayoutBindingOffsetEXT(vk.device, post_process_descriptor_set_layout, 5, &offset);
    vkGetDescriptorEXT(vk.device, &descriptor_info, descriptor_buffer_properties.sampledImageDescriptorSize,
        static_cast<uint8_t*>(post_process_mapped_descriptor_buffer_ptr) + offset);

    depth_buffer_image = vk_create_image(vk.surface_size.width, vk.surface_size.height, depth_format,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, "depth_buffer");

    vk_execute(vk.command_pools[0], vk.queue, [this](VkCommandBuffer command_buffer) {
        const VkImageSubresourceRange subresource_range{ VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };
        vk_cmd_image_barrier_for_subresource(command_buffer, depth_buffer_image.handle, subresource_range,
            VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    });

    vk_execute(vk.command_pools[0], vk.queue, [this](VkCommandBuffer command_buffer) {
        const VkImageSubresourceRange subresource_range{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        vk_cmd_image_barrier_for_subresource(command_buffer, prev_frame_image.handle, subresource_range,
            VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        });

    vk_execute(vk.command_pools[0], vk.queue, [this](VkCommandBuffer command_buffer) {
        const VkImageSubresourceRange subresource_range{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        vk_cmd_image_barrier_for_subresource(command_buffer, motion_vec_image.handle, subresource_range,
            VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        });

    last_frame_time = Clock::now();
}

void Vk_Demo::update_scale_by_delta(const float& delta)
{
    scale = std::clamp(scale + delta * scroll_sensitivity * static_cast<float>(time_delta), 0.1f, 5.f);
}

void Vk_Demo::run_frame() {
    Time current_time = Clock::now();
    time_delta = std::chrono::duration_cast<std::chrono::microseconds>(current_time - last_frame_time).count() / 1e6;
    if (animate) {
        sim_time += time_delta;
    }
    last_frame_time = current_time;

	float aspect_ratio = (float)vk.surface_size.width / (float)vk.surface_size.height;
	Matrix4x4 projection_transform = perspective_transform_opengl_z01(radians(45.0f), aspect_ratio, 0.1f, 50.0f);
	Matrix3x4 view_transform = look_at_transform(camera_pos, Vector3(0), Vector3(0, 1, 0));
    //auto model_transform = Matrix4x4::identity;
    //model_transform[0][3] = 1.f;
    //model_transform = rotate_y(model_transform, (float)sim_time * radians(20.0f));
    //model_transform[0][0] *= scale;
    //model_transform[1][1] *= scale;
    //model_transform[2][2] *= scale;
    auto castleTransform = castleModel.GetTransform();
    auto tankTransform = tankModel.GetTransform();
    auto balooTransform = balooModel.GetTransform();
    if (animate)
    {
		castleTransform->Transform.yaw += static_cast<float>(time_delta) * 20.f;
        tankTransform->Transform.yaw -= static_cast<float>(time_delta) * 20.f;
        balooTransform->Transform.yaw = sin(static_cast<float>(sim_time)) * 20.f;
    }
    castleTransform->Transform.scale = scale;
    tankTransform->Transform.scale = scale;

    auto model_transform = castleTransform->ProduceModelTransform();
    // Matrix4x4 model_view_proj = projection_transform * view_transform * model_transform;
    Matrix4x4 model_view_proj = projection_transform * view_transform;// *model_transform;

    main_frame_uniform.cur = model_view_proj;

    memcpy(mapped_uniform_buffer, &main_frame_uniform, sizeof(TAATransform));

    do_imgui();
    draw_frame();

    main_frame_uniform.prev = main_frame_uniform.cur;
}

void Vk_Demo::draw_frame() {
    vk_begin_frame();
    vk_begin_gpu_marker_scope(vk.command_buffer, "draw_frame");
    time_keeper.next_frame();
    gpu_times.frame->begin();

    VkDescriptorBufferBindingInfoEXT descriptor_buffer_binding_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT };
    descriptor_buffer_binding_info.address = descriptor_buffer.device_address;
    descriptor_buffer_binding_info.usage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT | VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT;
    vkCmdBindDescriptorBuffersEXT(vk.command_buffer, 1, &descriptor_buffer_binding_info);

    vk_cmd_image_barrier(vk.command_buffer, vk.swapchain_info.images[vk.swapchain_image_index],
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT /* execution dependency with acquire semaphore wait */, VK_ACCESS_2_NONE,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    VkViewport viewport{};
    viewport.width = float(vk.surface_size.width);
    viewport.height = float(vk.surface_size.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(vk.command_buffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.extent = vk.surface_size;
    vkCmdSetScissor(vk.command_buffer, 0, 1, &scissor);

    VkRenderingAttachmentInfo color_attachment{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
    color_attachment.imageView = vk.swapchain_info.image_views[vk.swapchain_image_index];
    color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.clearValue.color = { 0.32f, 0.32f, 0.4f, 0.0f };

    VkRenderingAttachmentInfo motion_vec_attachment{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
    motion_vec_attachment.imageView = motion_vec_image.view;
    motion_vec_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    motion_vec_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    motion_vec_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    motion_vec_attachment.clearValue.color = { 0.f, 0.f, 0.f, 0.0f };

    VkRenderingAttachmentInfo depth_attachment{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
    depth_attachment.imageView = depth_buffer_image.view;
    depth_attachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.clearValue.depthStencil = { 1.f, 0 };

    std::array colorAttachments{ color_attachment, motion_vec_attachment };

    VkRenderingInfo rendering_info{ VK_STRUCTURE_TYPE_RENDERING_INFO };
    rendering_info.renderArea.extent = vk.surface_size;
    rendering_info.layerCount = 1;
    rendering_info.colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size());
    rendering_info.pColorAttachments = colorAttachments.data();
    rendering_info.pDepthAttachment = &depth_attachment;


    vk_begin_gpu_marker_scope(vk.command_buffer, "Draw main frame");

    // auto& gpu_mesh = *castleModel.GetRenderable()->GetGPUMesh();

	color_attachment_transition_for_copy_src();

    post_process_transition_for_copy_dst(post_process_image.handle);

    vk_cmd_image_barrier(vk.command_buffer, motion_vec_image.handle,
        VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_ACCESS_2_NONE,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_NONE,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    vkCmdBeginRendering(vk.command_buffer, &rendering_info);
    const VkDeviceSize zero_offset = 0;

    const uint32_t buffer_index = 0;
    const VkDeviceSize set_offset = 0;
    vkCmdSetDescriptorBufferOffsetsEXT(vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &buffer_index, &set_offset);
    vkCmdBindPipeline(vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    //vkCmdBindVertexBuffers(vk.command_buffer, 0, 1, &gpu_mesh.vertex_buffer.handle, &zero_offset);
    //vkCmdBindIndexBuffer(vk.command_buffer, gpu_mesh.index_buffer.handle, 0, VK_INDEX_TYPE_UINT32);
    //vkCmdDrawIndexed(vk.command_buffer, gpu_mesh.index_count, 1, 0, 0, 0);

    //auto imageInfo = VkDescriptorImageInfo{ VK_NULL_HANDLE,
    //    castleModel.GetRenderable()->GetTexture()->view,
    //    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    //auto write = VkWriteDescriptorSet{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };

    //write.descriptorCount = 1;
    //write.dstBinding = 0;
    //write.pImageInfo = &imageInfo;
    //write.dstSet = VK_NULL_HANDLE;
    //write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;

    //vkCmdPushDescriptorSetKHR(vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
    //    pipeline_layout, 1, 1, &write);

    auto renderable = castleModel.GetRenderable();
    if (renderable)
    {
        // renderable->DrawWithTextures(vk.command_buffer, pipeline_layout);
        castleModel.DrawGameObject(vk.command_buffer, pipeline_layout);
    }

    balooModel.DrawGameObject(vk.command_buffer, pipeline_layout);

    // tankModel.GetRenderable()->DrawWithTextures(vk.command_buffer, pipeline_layout);
    tankModel.DrawGameObject(vk.command_buffer, pipeline_layout);


    vkCmdEndRendering(vk.command_buffer);
    vk_end_gpu_marker_scope(vk.command_buffer);

    vk_begin_gpu_marker_scope(vk.command_buffer, "Begin post processing");
    simple_image_copy(vk.swapchain_info.images[vk.swapchain_image_index],
        post_process_image.handle,
        { vk.surface_size.width, vk.surface_size.height });

    post_process_transition_for_rendering(post_process_image.handle);
    color_attachment_transition_for_rendering();

    vk_cmd_image_barrier(vk.command_buffer, motion_vec_image.handle,
        VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_ACCESS_2_NONE,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_NONE,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VkDescriptorBufferBindingInfoEXT post_process_descriptor_buffer_binding_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT };
    post_process_descriptor_buffer_binding_info.address = post_process_descriptor_buffer.device_address;
    post_process_descriptor_buffer_binding_info.usage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT | VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT;
    vkCmdBindDescriptorBuffersEXT(vk.command_buffer, 1, &post_process_descriptor_buffer_binding_info);

	rendering_info.colorAttachmentCount = 1;
    rendering_info.pColorAttachments = &color_attachment;

	vkCmdBeginRendering(vk.command_buffer, &rendering_info);
    vkCmdBindVertexBuffers(vk.command_buffer, 0, 1, &quad_mesh.vertex_buffer.handle, &zero_offset);
    vkCmdBindIndexBuffer(vk.command_buffer, quad_mesh.index_buffer.handle, 0, VK_INDEX_TYPE_UINT32);

    //vkCmdBindVertexBuffers(vk.command_buffer, 0, 1, &quad_mesh.vertex_buffer.handle, &zero_offset);
    //vkCmdBindIndexBuffer(vk.command_buffer, quad_mesh.index_buffer.handle, 0, VK_INDEX_TYPE_UINT32);

    auto pushConstants = Post_Process_Push_Constatnts{ static_cast<uint32_t>(aliasingOption), threshold, frameIndex };

    vkCmdSetDescriptorBufferOffsetsEXT(vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, post_process_pipeline_layout, 0, 1, &buffer_index, &set_offset);

    vkCmdBindPipeline(vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, post_process_pipeline);
    vkCmdPushConstants(vk.command_buffer, post_process_pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT,
        0, sizeof(Post_Process_Push_Constatnts), &pushConstants);
    vkCmdDrawIndexed(vk.command_buffer, quad_mesh.index_count, 1, 0, 0, 0);
    vkCmdEndRendering(vk.command_buffer);
    vk_end_gpu_marker_scope(vk.command_buffer);

    vk_begin_gpu_marker_scope(vk.command_buffer, "Save current frame as history frame");
    color_attachment_transition_for_copy_src();
    post_process_transition_for_copy_dst(prev_frame_image.handle);
    simple_image_copy(vk.swapchain_info.images[vk.swapchain_image_index],
        prev_frame_image.handle,
        { vk.surface_size.width, vk.surface_size.height });
    post_process_transition_for_rendering(prev_frame_image.handle);

    color_attachment_transition_for_rendering();
    vk_end_gpu_marker_scope(vk.command_buffer);

    //vk_begin_gpu_marker_scope(vk.command_buffer, "Begin sharpening post processing");
    //simple_image_copy(vk.swapchain_info.images[vk.swapchain_image_index],
    //    post_process_image.handle,
    //    { vk.surface_size.width, vk.surface_size.height });

    //post_process_transition_for_rendering(post_process_image.handle);
    //color_attachment_transition_for_rendering();

    //vk_cmd_image_barrier(vk.command_buffer, motion_vec_image.handle,
    //    VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_ACCESS_2_NONE,
    //    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    //    VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_NONE,
    //    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    //rendering_info.colorAttachmentCount = 1;
    //rendering_info.pColorAttachments = &color_attachment;

    //vkCmdBeginRendering(vk.command_buffer, &rendering_info);
    //vkCmdBindVertexBuffers(vk.command_buffer, 0, 1, &quad_mesh.vertex_buffer.handle, &zero_offset);
    //vkCmdBindIndexBuffer(vk.command_buffer, quad_mesh.index_buffer.handle, 0, VK_INDEX_TYPE_UINT32);

    ////vkCmdBindVertexBuffers(vk.command_buffer, 0, 1, &quad_mesh.vertex_buffer.handle, &zero_offset);
    ////vkCmdBindIndexBuffer(vk.command_buffer, quad_mesh.index_buffer.handle, 0, VK_INDEX_TYPE_UINT32);

    //auto sharpening = Post_Process_Push_Constatnts{ pushConstants.AAType == 2 ? 4u : 0u, threshold, frameIndex };

    //vkCmdSetDescriptorBufferOffsetsEXT(vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, post_process_pipeline_layout, 0, 1, &buffer_index, &set_offset);

    //vkCmdBindPipeline(vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, post_process_pipeline);
    //vkCmdPushConstants(vk.command_buffer, post_process_pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT,
    //    0, sizeof(Post_Process_Push_Constatnts), &sharpening);
    //vkCmdDrawIndexed(vk.command_buffer, quad_mesh.index_count, 1, 0, 0, 0);
    //vkCmdEndRendering(vk.command_buffer);
    //vk_end_gpu_marker_scope(vk.command_buffer);

    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;

    vk_begin_gpu_marker_scope(vk.command_buffer, "Drawing GUI");
    vkCmdBeginRendering(vk.command_buffer, &rendering_info);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), vk.command_buffer);
    vkCmdEndRendering(vk.command_buffer);
    vk_end_gpu_marker_scope(vk.command_buffer);

    color_attachment_transition_for_present();

    gpu_times.frame->end();
    vk_end_gpu_marker_scope(vk.command_buffer);
    vk_end_frame();
    frameIndex++;
}

void Vk_Demo::color_attachment_transition_for_copy_src()
{
    vk_cmd_image_barrier(vk.command_buffer, vk.swapchain_info.images[vk.swapchain_image_index],
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_2_COPY_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
}

void Vk_Demo::post_process_transition_for_copy_dst(const VkImage& targetImg)
{
    vk_cmd_image_barrier(vk.command_buffer, targetImg,
        VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_2_COPY_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
}

void Vk_Demo::simple_image_copy(const VkImage& src, const VkImage& dst, const VkExtent2D& imgExtent)
{
    auto copyInfo = VkCopyImageInfo2{ VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2 };
    copyInfo.srcImage = src;
    copyInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    copyInfo.dstImage = dst;
    copyInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

    auto copyRegion = VkImageCopy2{ VK_STRUCTURE_TYPE_IMAGE_COPY_2 };
    copyRegion.srcOffset = { 0, 0, 0 };
    copyRegion.dstOffset = { 0, 0, 0 };
    copyRegion.extent = { imgExtent.width, imgExtent.height, 1 };
    copyRegion.srcSubresource = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .mipLevel = 0,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };
    copyRegion.dstSubresource = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .mipLevel = 0,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };

    copyInfo.regionCount = 1;
    copyInfo.pRegions = &copyRegion;

    vkCmdCopyImage2(vk.command_buffer, &copyInfo);
}

void Vk_Demo::color_attachment_transition_for_rendering()
{
    vk_cmd_image_barrier(vk.command_buffer, vk.swapchain_info.images[vk.swapchain_image_index],
        VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL);
}

void Vk_Demo::post_process_transition_for_rendering(const VkImage& targetImg)
{
    vk_cmd_image_barrier(vk.command_buffer, targetImg,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void Vk_Demo::color_attachment_transition_for_present()
{
    vk_cmd_image_barrier(vk.command_buffer, vk.swapchain_info.images[vk.swapchain_image_index],
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
}

void Vk_Demo::do_imgui() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    if (!ImGui::GetIO().WantCaptureKeyboard) {
        if (ImGui::IsKeyPressed(ImGuiKey_F10)) {
            show_ui = !show_ui;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_W) || ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
            camera_pos.z -= 0.2f;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_S) || ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
            camera_pos.z += 0.2f;
        }
    }
    if (show_ui) {
        const float margin = 10.0f;
        static int corner = 0;

        ImVec2 window_pos = ImVec2((corner & 1) ? ImGui::GetIO().DisplaySize.x - margin : margin,
                                   (corner & 2) ? ImGui::GetIO().DisplaySize.y - margin : margin);
        ImVec2 window_pos_pivot = ImVec2((corner & 1) ? 1.0f : 0.0f, (corner & 2) ? 1.0f : 0.0f);

        if (corner != -1) {
            ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
        }
        ImGui::SetNextWindowBgAlpha(0.3f);

        if (ImGui::Begin("UI", &show_ui, 
            (corner != -1 ? ImGuiWindowFlags_NoMove : 0) | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav))
        {
            ImGui::Text("%.1f FPS (%.3f ms/frame)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
            ImGui::Text("Frame time: %.2f ms", gpu_times.frame->length_ms);
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::Checkbox("Vertical sync", &vsync);
            ImGui::SliderFloat("Scroll Sensitivity", &scroll_sensitivity, 0.1f, 10.f);
            ImGui::Checkbox("Animate", &animate);
            //bool isFXAAEnabled = aliasingOption != 0;
            //ImGui::Checkbox("Enable FXAA", &isFXAAEnabled);
            //aliasingOption = isFXAAEnabled ? 1 : 0;

        	const std::array options = { "None", "FXAA", "TAA" };
            const int32_t numOptions = static_cast<uint32_t>(options.size());

            ImGui::Combo("Antialiasing", &aliasingOption, options.data(), numOptions);
            switch (aliasingOption)
            {
				case 0: break;
				case 1:
                {
                    ImGui::SliderFloat("FXAA Threshold", &threshold, 0.01f, 1.f);
                    
                }
                break;
                case 2: break;
                default: break;
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
    ImGui::Render();
}
