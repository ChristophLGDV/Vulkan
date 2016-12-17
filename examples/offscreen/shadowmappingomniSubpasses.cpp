/*
* Vulkan Example - Omni directional shadows using a dynamic cube map
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/



#include "../offscreen/vulkanOffscreenExampleBase.hpp"


// Texture properties
#define TEX_DIM 1024
#define TEX_FILTER vk::Filter::eLinear
// Offscreen frame buffer properties
#define FB_DIM TEX_DIM
#define FB_COLOR_FORMAT   

// Vertex layout for this example
std::vector<vkx::VertexLayout> vertexLayout =
{
    vkx::VertexLayout::VERTEX_LAYOUT_POSITION,
    vkx::VertexLayout::VERTEX_LAYOUT_UV,
    vkx::VertexLayout::VERTEX_LAYOUT_COLOR,
    vkx::VertexLayout::VERTEX_LAYOUT_NORMAL
};

class VulkanExample : public vkx::OffscreenExampleBase {
public:
    bool displayCubeMap = false;

    float zNear = 0.1f;
    float zFar = 1024.0f;

    struct {
        vk::PipelineVertexInputStateCreateInfo inputState;
        std::vector<vk::VertexInputBindingDescription> bindingDescriptions;
        std::vector<vk::VertexInputAttributeDescription> attributeDescriptions;
    } vertices;

    struct {
        vkx::MeshBuffer skybox;
        vkx::MeshBuffer scene;
    } meshes;

    struct {
        vkx::UniformData scene;
        vkx::UniformData offscreen;
    } uniformData;

    struct {
        glm::mat4 projection;
        glm::mat4 model;
    } uboVSquad;

    glm::vec4 lightPos = glm::vec4(0.0f, -25.0f, 0.0f, 1.0);

    struct {
        glm::mat4 projection;
        glm::mat4 view;
        glm::mat4 model;
        glm::vec4 lightPos;
    } uboVSscene;

    struct {
		glm::mat4 mvp[6];
    } uboOffscreenVS;

    struct {
        vk::Pipeline scene;
        vk::Pipeline offscreen;
        vk::Pipeline cubeMap;
    } pipelines;

    struct {
        vk::PipelineLayout scene;
        vk::PipelineLayout offscreen;
    } pipelineLayouts;

    struct {
        vk::DescriptorSet scene;
        vk::DescriptorSet offscreen;
    } descriptorSets;

	struct {
		vk::DescriptorSetLayout	scene;
		vk::DescriptorSetLayout	offscreen;
	} descriptorSetLayouts;
	 
//	std::array<vk::ImageView,FACE_COUNT>depthWriteViews;




    //// vk::Framebuffer for offscreen rendering
    //using FrameBufferAttachment = vkx::CreateImageResult;
    //struct FrameBuffer {
    //    int32_t width, height;
    //    vk::Framebuffer framebuffer;
    //    FrameBufferAttachment color, depth;
    //} offscreenFrameBuf;

    //vk::CommandBuffer offscreen.cmdBuffer;

    VulkanExample() : vkx::OffscreenExampleBase(ENABLE_VALIDATION) {
		enableVsync = true;
		enableTextOverlay = true;
        zoomSpeed = 10.0f;
        timerSpeed *= 0.25f;
       // camera.setRotation({ -20.5f, -673.0f, 0.0f });
		camera.type = Camera::CameraType::firstperson;
		camera.movementSpeed = 2.5f;
		camera.setTranslation({ -15.0f, -43.5f, -55.0f });
		camera.setRotation(glm::vec3(5.0f, 0.0f, 0.0f));
		camera.setPerspective(90.0f, size, 0.1f, 256.0f);
		camera.setZoom(-75.0f);
        title = "Vulkan Example - Point light shadows - Layered Rendering";
    }

    ~VulkanExample() {
        // Clean up used Vulkan resources 
        // Note : Inherited destructor cleans up resources stored in base class

        // Cube map 
		
        // Pipelibes
        device.destroyPipeline(pipelines.scene);
        device.destroyPipeline(pipelines.offscreen);
        device.destroyPipeline(pipelines.cubeMap);

        device.destroyPipelineLayout(pipelineLayouts.scene);
        device.destroyPipelineLayout(pipelineLayouts.offscreen);

		device.destroyDescriptorSetLayout(descriptorSetLayouts.scene);
		device.destroyDescriptorSetLayout(descriptorSetLayouts.offscreen);

        // Meshes
        meshes.scene.destroy();
        meshes.skybox.destroy();

        // Uniform buffers
        uniformData.offscreen.destroy();
        uniformData.scene.destroy();
    }

    

    // Command buffer for rendering and copying all cube map faces
    void buildOffscreenCommandBuffer() {
        // Create separate command buffer for offscreen 
        // rendering
        if (!offscreen.cmdBuffer) {
            vk::CommandBufferAllocateInfo cmd = vkx::commandBufferAllocateInfo(cmdPool, vk::CommandBufferLevel::ePrimary, 1);
            offscreen.cmdBuffer = device.allocateCommandBuffers(cmd)[0];
        }

        vk::CommandBufferBeginInfo cmdBufInfo;
        cmdBufInfo.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse;
        offscreen.cmdBuffer.begin(cmdBufInfo);
        offscreen.cmdBuffer.setViewport(0, vkx::viewport(offscreen.size));
        offscreen.cmdBuffer.setScissor(0, vkx::rect2D(offscreen.size));

		 

		std::array<vk::ClearValue, FACE_COUNT> clearValues;

		for (auto& clearValue : clearValues)
		{
			clearValue.depthStencil = { 1.0f, 0 };
		} 

		vk::RenderPassBeginInfo renderPassBeginInfo;
		// Reuse render pass from example pass
		renderPassBeginInfo.renderPass = offscreen.renderPass;
		renderPassBeginInfo.framebuffer = offscreen.framebuffers[0].framebuffer;
		renderPassBeginInfo.renderArea.extent.width = offscreen.size.x;
		renderPassBeginInfo.renderArea.extent.height = offscreen.size.y; 
		renderPassBeginInfo.clearValueCount = 0; clearValues.size();
		renderPassBeginInfo.pClearValues = clearValues.data();


		offscreen.cmdBuffer.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
		
		offscreen.cmdBuffer.bindPipeline(
			vk::PipelineBindPoint::eGraphics, 
			pipelines.offscreen);

		offscreen.cmdBuffer.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			pipelineLayouts.offscreen,
			0,
			descriptorSets.offscreen,
			nullptr);

		offscreen.cmdBuffer.bindVertexBuffers(VERTEX_BUFFER_BIND_ID, meshes.scene.vertices.buffer, { 0 });
		offscreen.cmdBuffer.bindIndexBuffer(meshes.scene.indices.buffer, 0, vk::IndexType::eUint32);

	 for (int face = 0; face < FACE_COUNT; face++)
	 { 
	 
		offscreen.cmdBuffer.pushConstants(
			pipelineLayouts.offscreen,
			vk::ShaderStageFlagBits::eVertex,
			0,
			sizeof(unsigned int),
			&face);
	 
	 
	 	offscreen.cmdBuffer.drawIndexed(meshes.scene.indexCount, 1, 0, 0, 0);
	 
	 
	 	if (face < (FACE_COUNT - 1))
	 		offscreen.cmdBuffer.nextSubpass(vk::SubpassContents::eInline);
	 }
		offscreen.cmdBuffer.endRenderPass();

		 
        offscreen.cmdBuffer.end();
    }
 

    void updateDrawCommandBuffer(const vk::CommandBuffer& cmdBuffer) {
        cmdBuffer.setViewport(0, vkx::viewport(size));
        cmdBuffer.setScissor(0, vkx::rect2D(size));

        cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, 
			pipelineLayouts.scene,
			0,
			descriptorSets.scene, nullptr);

        if (displayCubeMap) {
            cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipelines.cubeMap);
            cmdBuffer.bindVertexBuffers(VERTEX_BUFFER_BIND_ID, meshes.skybox.vertices.buffer, { 0 });
            cmdBuffer.bindIndexBuffer(meshes.skybox.indices.buffer, 0, vk::IndexType::eUint32);
            cmdBuffer.drawIndexed(meshes.skybox.indexCount, 1, 0, 0, 0);
        } else {
            cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipelines.scene);
            cmdBuffer.bindVertexBuffers(VERTEX_BUFFER_BIND_ID, meshes.scene.vertices.buffer, { 0 });
            cmdBuffer.bindIndexBuffer(meshes.scene.indices.buffer, 0, vk::IndexType::eUint32);
            cmdBuffer.drawIndexed(meshes.scene.indexCount, 1, 0, 0, 0);
        }

    }

    void loadMeshes() {
        meshes.skybox = loadMesh(getAssetPath() + "models/cube.obj", vertexLayout, 2.0f);
        meshes.scene = loadMesh(getAssetPath() + "models/shadowscene_fire.dae", vertexLayout, 2.0f);
    }

    void setupVertexDescriptions() {
        // Binding description
        vertices.bindingDescriptions.resize(1);
        vertices.bindingDescriptions[0] =
            vkx::vertexInputBindingDescription(VERTEX_BUFFER_BIND_ID, vkx::vertexSize(vertexLayout), vk::VertexInputRate::eVertex);

        // Attribute descriptions
        vertices.attributeDescriptions.resize(4);
        // Location 0 : Position
        vertices.attributeDescriptions[0] =
            vkx::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 0, vk::Format::eR32G32B32Sfloat, 0);
        // Location 1 : Texture coordinates
        vertices.attributeDescriptions[1] =
            vkx::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 1, vk::Format::eR32G32Sfloat, sizeof(float) * 3);
        // Location 2 : Color
        vertices.attributeDescriptions[2] =
            vkx::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 2, vk::Format::eR32G32B32Sfloat, sizeof(float) * 5);
        // Location 3 : Normal
        vertices.attributeDescriptions[3] =
            vkx::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 3, vk::Format::eR32G32B32Sfloat, sizeof(float) * 8);

        vertices.inputState = vk::PipelineVertexInputStateCreateInfo();
        vertices.inputState.vertexBindingDescriptionCount = vertices.bindingDescriptions.size();
        vertices.inputState.pVertexBindingDescriptions = vertices.bindingDescriptions.data();
        vertices.inputState.vertexAttributeDescriptionCount = vertices.attributeDescriptions.size();
        vertices.inputState.pVertexAttributeDescriptions = vertices.attributeDescriptions.data();
    }

    void setupDescriptorPool() {
        // Example uses three ubos and two image samplers
        std::vector<vk::DescriptorPoolSize> poolSizes =
        {
            vkx::descriptorPoolSize(vk::DescriptorType::eUniformBuffer, 3),
			vkx::descriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, 2)
        };

        vk::DescriptorPoolCreateInfo descriptorPoolInfo =
            vkx::descriptorPoolCreateInfo(poolSizes.size(), poolSizes.data(), 3);

        descriptorPool = device.createDescriptorPool(descriptorPoolInfo);
    }

    void setupDescriptorSetLayout() {
      


		// Scene pipeline layout
		{
			std::vector<vk::DescriptorSetLayoutBinding> setLayoutBindings =
			{
				// Binding 0 : Vertex shader uniform buffer
				vkx::descriptorSetLayoutBinding(
					vk::DescriptorType::eUniformBuffer,
					vk::ShaderStageFlagBits::eVertex,
					0),
				// Binding 1 : Fragment shader image sampler (cube map)
				vkx::descriptorSetLayoutBinding(
					vk::DescriptorType::eCombinedImageSampler,
					vk::ShaderStageFlagBits::eFragment,
					1)
			};


			vk::DescriptorSetLayoutCreateInfo descriptorLayoutInfo =
				vkx::descriptorSetLayoutCreateInfo(setLayoutBindings.data(), setLayoutBindings.size());

			descriptorSetLayouts.scene = device.createDescriptorSetLayout(descriptorLayoutInfo);



			vk::PipelineLayoutCreateInfo pPipelineLayoutCreateInfo =
				vkx::pipelineLayoutCreateInfo(&descriptorSetLayouts.scene, 1);

			pipelineLayouts.scene = device.createPipelineLayout(pPipelineLayoutCreateInfo);

			  
		}


		// Offscreen pipeline layout
		{


			std::vector<vk::DescriptorSetLayoutBinding> setLayoutBindings =
			{
				// Binding 0 : Vertex shader uniform buffer
				vkx::descriptorSetLayoutBinding(
					vk::DescriptorType::eUniformBuffer,
					vk::ShaderStageFlagBits::eVertex,
					0), 
			};




			vk::DescriptorSetLayoutCreateInfo descriptorLayoutInfo =
				vkx::descriptorSetLayoutCreateInfo(setLayoutBindings.data(), setLayoutBindings.size());

			descriptorSetLayouts.offscreen = device.createDescriptorSetLayout(descriptorLayoutInfo);




			vk::PushConstantRange pushConstantRange(
					vk::ShaderStageFlagBits::eVertex,
					0,
					sizeof(unsigned int));

			// Push constant ranges are part of the pipeline layout


			vk::PipelineLayoutCreateInfo pPipelineLayoutCreateInfo =
				vkx::pipelineLayoutCreateInfo(&descriptorSetLayouts.offscreen, 1);

			pPipelineLayoutCreateInfo.pushConstantRangeCount = 1;
			pPipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;

			pipelineLayouts.offscreen = device.createPipelineLayout(pPipelineLayoutCreateInfo);
			 
		}





		//no push constant range for offscreen rendering required
		// Offscreen pipeline layout
        // Push constants for cube map face view matrices
        //vk::PushConstantRange pushConstantRange(vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::mat4));
		//
        //// Push constant ranges are part of the pipeline layout
        //pPipelineLayoutCreateInfo.pushConstantRangeCount = 1;
        //pPipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
		//
        //pipelineLayouts.offscreen = device.createPipelineLayout(pPipelineLayoutCreateInfo);

    }

    void setupDescriptorSets() {
      
		// 3D scene
		{  
			vk::DescriptorSetAllocateInfo allocInfoScene =
			vkx::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayouts.scene, 1);

			descriptorSets.scene = device.allocateDescriptorSets(allocInfoScene)[0];

			// vk::Image descriptor for the cube map 
			vk::DescriptorImageInfo texDescriptor =
				vkx::descriptorImageInfo(offscreen.framebuffers[0].depth.sampler, 
					offscreen.framebuffers[0].depth.view, vk::ImageLayout::eGeneral);

			std::vector<vk::WriteDescriptorSet> sceneDescriptorSets =
			{
				// Binding 0 : Vertex shader uniform buffer
				vkx::writeDescriptorSet(
				descriptorSets.scene,
					vk::DescriptorType::eUniformBuffer,
					0,
					&uniformData.scene.descriptor),
				// Binding 1 : Fragment shader shadow sampler
				vkx::writeDescriptorSet(
					descriptorSets.scene,
					vk::DescriptorType::eCombinedImageSampler,
					1,
					&texDescriptor)
			};
			device.updateDescriptorSets(sceneDescriptorSets.size(), sceneDescriptorSets.data(), 0, NULL);
		}
		// Offscreen
		{
			vk::DescriptorSetAllocateInfo allocInfoOffscreen =
				vkx::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayouts.offscreen, 1);

			descriptorSets.offscreen = device.allocateDescriptorSets(allocInfoOffscreen)[0];

			std::vector<vk::WriteDescriptorSet> offscreenWriteDescriptorSets =
			{
				// Binding 0 : Vertex shader uniform buffer
				vkx::writeDescriptorSet(
					descriptorSets.offscreen,
					vk::DescriptorType::eUniformBuffer,
					0,
					&uniformData.offscreen.descriptor),
			};
			device.updateDescriptorSets(offscreenWriteDescriptorSets.size(), offscreenWriteDescriptorSets.data(), 0, NULL);
		}
    }

    void preparePipelines() {
        vk::PipelineInputAssemblyStateCreateInfo inputAssemblyState =
            vkx::pipelineInputAssemblyStateCreateInfo(vk::PrimitiveTopology::eTriangleList, vk::PipelineInputAssemblyStateCreateFlags(), VK_FALSE);

        vk::PipelineRasterizationStateCreateInfo rasterizationState =
            vkx::pipelineRasterizationStateCreateInfo(vk::PolygonMode::eFill, vk::CullModeFlagBits::eBack, vk::FrontFace::eClockwise);

        vk::PipelineColorBlendAttachmentState blendAttachmentState =
            vkx::pipelineColorBlendAttachmentState();

        vk::PipelineColorBlendStateCreateInfo colorBlendState =
            vkx::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);

        vk::PipelineDepthStencilStateCreateInfo depthStencilState =
            vkx::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, vk::CompareOp::eLessOrEqual);

        vk::PipelineViewportStateCreateInfo viewportState =
            vkx::pipelineViewportStateCreateInfo(1, 1);

        vk::PipelineMultisampleStateCreateInfo multisampleState =
            vkx::pipelineMultisampleStateCreateInfo(vk::SampleCountFlagBits::e1);

        std::vector<vk::DynamicState> dynamicStateEnables = {
            vk::DynamicState::eViewport,
            vk::DynamicState::eScissor
        };
        vk::PipelineDynamicStateCreateInfo dynamicState =
            vkx::pipelineDynamicStateCreateInfo(dynamicStateEnables.data(), dynamicStateEnables.size());

        // 3D scene pipeline
        // Load shaders
        std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages;

        shaderStages[0] = loadShader(getAssetPath() + "shaders/shadowmappingomniSubpasses/scene.vert.spv", vk::ShaderStageFlagBits::eVertex);
        shaderStages[1] = loadShader(getAssetPath() + "shaders/shadowmappingomniSubpasses/scene.frag.spv", vk::ShaderStageFlagBits::eFragment);

        vk::GraphicsPipelineCreateInfo pipelineCreateInfo =
            vkx::pipelineCreateInfo(pipelineLayouts.scene, renderPass);

        pipelineCreateInfo.pVertexInputState = &vertices.inputState;
        pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
        pipelineCreateInfo.pRasterizationState = &rasterizationState;
        pipelineCreateInfo.pColorBlendState = &colorBlendState;
        pipelineCreateInfo.pMultisampleState = &multisampleState;
        pipelineCreateInfo.pViewportState = &viewportState;
        pipelineCreateInfo.pDepthStencilState = &depthStencilState;
        pipelineCreateInfo.pDynamicState = &dynamicState;
        pipelineCreateInfo.stageCount = shaderStages.size();
        pipelineCreateInfo.pStages = shaderStages.data();

        pipelines.scene = device.createGraphicsPipelines(pipelineCache, pipelineCreateInfo, nullptr)[0];


        // Cube map display pipeline
        shaderStages[0] = loadShader(getAssetPath() + "shaders/shadowmappingomniSubpasses/cubemapdisplay.vert.spv", vk::ShaderStageFlagBits::eVertex);
        shaderStages[1] = loadShader(getAssetPath() + "shaders/shadowmappingomniSubpasses/cubemapdisplay.frag.spv", vk::ShaderStageFlagBits::eFragment);
        rasterizationState.cullMode = vk::CullModeFlagBits::eFront;
        pipelines.cubeMap = device.createGraphicsPipelines(pipelineCache, pipelineCreateInfo, nullptr)[0];


		// Offscreen pipeline
	//	shaderStages[0] = loadShader(getAssetPath() + "shaders/shadowmappingomni/offscreen.vert.spv", vk::ShaderStageFlagBits::eVertex);
	//	shaderStages[1] = loadShader(getAssetPath() + "shaders/shadowmappingomni/offscreen.frag.spv", vk::ShaderStageFlagBits::eFragment);
		// Offscreen pipeline
		shaderStages[0] = loadShader(getAssetPath() + "shaders/shadowmappingomniSubpasses/shadow.vert.spv", vk::ShaderStageFlagBits::eVertex);
		pipelineCreateInfo.stageCount = 1;
		//shaderStages[1] = loadShader(getAssetPath() + "shaders/shadowmappingomniLayered/shadow.geom.spv", vk::ShaderStageFlagBits::eGeometry);
        rasterizationState.cullMode = vk::CullModeFlagBits::eBack;
        pipelineCreateInfo.layout = pipelineLayouts.offscreen;
        pipelines.offscreen = device.createGraphicsPipelines(pipelineCache, pipelineCreateInfo, nullptr)[0];

    }

    // Prepare and initialize uniform buffer containing shader uniforms
    void prepareUniformBuffers() {
        // Offscreen vertex shader uniform buffer block
        uniformData.offscreen = createUniformBuffer(uboOffscreenVS);
        uniformData.offscreen.map();

        // 3D scene
        uniformData.scene = createUniformBuffer(uboVSscene);
        uniformData.scene.map();

        updateUniformBufferOffscreen();
        updateUniformBuffers();
    }

    void updateUniformBuffers() {
        // 3D scene
        uboVSscene.projection = glm::perspective(glm::radians(45.0f), (float)size.width / (float)size.height, zNear, zFar);
        uboVSscene.view = glm::translate(glm::mat4(), glm::vec3(0.0f, 0.0f, displayCubeMap ? 0.0f : camera.position.z));
        uboVSscene.model = glm::mat4_cast(camera.orientation);
        uboVSscene.lightPos = lightPos;
        uniformData.scene.copy(uboVSscene);
    }

    void updateUniformBufferOffscreen() 
	{
		lightPos.x = sin(glm::radians(timer * 360.0f)) * 1.0f;
		lightPos.z = cos(glm::radians(timer * 360.0f)) * 1.0f;

		glm::mat4 P = camera.vulkanCorrection * glm::perspective((float)(M_PI / 2.0), 1.0f, zNear, zFar);

		glm::mat4 M = glm::translate(glm::mat4(), glm::vec3(-lightPos.x, -lightPos.y, -lightPos.z));

		for (uint32_t face = 0; face < FACE_COUNT; ++face)
		{
			glm::mat4 V;
			switch (face)
			{
			case 0: // POSITIVE_X
				V = glm::rotate(V, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
				V = glm::rotate(V, glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
				break;
			case 1:	// NEGATIVE_X						  
				V = glm::rotate(V, glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
				V = glm::rotate(V, glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
				break;
			case 2:	// POSITIVE_Y						  
				V = glm::rotate(V, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
				break;
			case 3:	// NEGATIVE_Y						 
				V = glm::rotate(V, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
				break;
			case 4:	// POSITIVE_Z						 
				V = glm::rotate(V, glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
				break;
			case 5:	// NEGATIVE_Z						 
				V = glm::rotate(V, glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f));
				break;
			}
			uboOffscreenVS.mvp[face] = P  * V *M;
		}

        uniformData.offscreen.copy(uboOffscreenVS);
    }

    void prepare() {

		bool defaultFB = false;
		offscreen.defaultFramebuffer = defaultFB;
		offscreen.size = glm::uvec2(TEX_DIM);



		if(defaultFB)
		{
			offscreen.colorFormats = { { vk::Format::eR32Sfloat } };
			offscreen.depthFormat = vk::Format::eUndefined;
			offscreen.attachmentUsage = vk::ImageUsageFlagBits::eTransferSrc;
			offscreen.colorFinalLayout = vk::ImageLayout::eTransferSrcOptimal;
		}
		else
		{
			//no color Attachments
			offscreen.colorFormats = { };
			offscreen.colorFinalLayout = vk::ImageLayout::eUndefined;
			offscreen.depthFinalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
			// Depth attachment 
			
			// Image

			offscreen.depthStencilImage.imageType = vk::ImageType::e2D;
			offscreen.depthStencilImage.extent.width = TEX_DIM;
			offscreen.depthStencilImage.extent.height = TEX_DIM;
			offscreen.depthStencilImage.extent.depth = 1;
			offscreen.depthStencilImage.mipLevels = 1;
			offscreen.depthStencilImage.arrayLayers = 6; // Cubemap
			offscreen.depthStencilImage.format = vkx::getSupportedDepthFormat(offscreen.context.physicalDevice);
			offscreen.depthStencilImage.samples = vk::SampleCountFlagBits::e1;
			offscreen.depthStencilImage.tiling = vk::ImageTiling::eOptimal;
			offscreen.depthStencilImage.flags = vk::ImageCreateFlagBits::eCubeCompatible;
			// vk::Image of the framebuffer is  RT and Texture 
			offscreen.depthStencilImage.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled;

			// View

			offscreen.depthStencilView.viewType = vk::ImageViewType::eCube; // Cubemap			
			offscreen.depthStencilView.format		=  vkx::getSupportedDepthFormat(offscreen.context.physicalDevice);
			offscreen.depthStencilView.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
			offscreen.depthStencilView.subresourceRange.levelCount = 1;
			offscreen.depthStencilView.subresourceRange.layerCount = FACE_COUNT; // view for cube sampler

			// Framebuffer 
			offscreen.fbufCreateInfo.width = TEX_DIM;
			offscreen.fbufCreateInfo.height = TEX_DIM;
			offscreen.fbufCreateInfo.layers = 1;

				
			//Shadow Sampler
			offscreen.shadowSampler = true;


		}
		 

		for (int face = 0; face < FACE_COUNT; face++)
		{
			vk::ImageViewCreateInfo depthStencilView;
			depthStencilView.viewType = vk::ImageViewType::e2D; //_ARRAY
			depthStencilView.format = vkx::getSupportedDepthFormat(offscreen.context.physicalDevice);
			depthStencilView.subresourceRange = {};
			depthStencilView.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
			depthStencilView.subresourceRange.baseMipLevel = 0;
			depthStencilView.subresourceRange.levelCount = 1;
			depthStencilView.subresourceRange.baseArrayLayer = face;
			depthStencilView.subresourceRange.layerCount = 1;// FACE_COUNT - 1;

			offscreen.depthStencilViews.push_back(depthStencilView);
		}



	  


		OffscreenExampleBase::prepare();


        loadMeshes();
        setupVertexDescriptions();
        prepareUniformBuffers(); 
        setupDescriptorSetLayout();
        preparePipelines();
        setupDescriptorPool();
        setupDescriptorSets();
        updateDrawCommandBuffers();
        buildOffscreenCommandBuffer();
        prepared = true;
    }


	

    virtual void render() {
        if (!prepared)
            return;
       draw();
        if (!paused) {
            updateUniformBufferOffscreen();
            updateUniformBuffers();
        }
    }

    virtual void viewChanged() {
        updateUniformBufferOffscreen();
        updateUniformBuffers();
    }

    void toggleCubeMapDisplay() {
        displayCubeMap = !displayCubeMap;
        updateDrawCommandBuffers();
    }

    void keyPressed(uint32_t key) override {

		ExampleBase::keyPressed(key);
        switch (key) {
        case GLFW_KEY_D:
            toggleCubeMapDisplay();
            break;
        }
    }
};

RUN_EXAMPLE(VulkanExample)
