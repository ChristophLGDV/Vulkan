/*
* Vulkan Example - Omni directional shadows using a dynamic cube map
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <vector>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vulkan/vulkan.h>
#include "vulkanexamplebase.h"

#define VERTEX_BUFFER_BIND_ID 0
#define ENABLE_VALIDATION false

// Texture properties
#define TEX_DIM 1024
#define TEX_FILTER VK_FILTER_LINEAR

// Offscreen frame buffer properties
#define FB_DIM TEX_DIM
#define FB_COLOR_FORMAT VK_FORMAT_R32_SFLOAT 


// 16 bits of depth is enough for such a small scene
#define DEPTH_FORMAT VK_FORMAT_D32_SFLOAT_S8_UINT //VK_FORMAT_D16_UNORM
#define SHADOWMAP_FILTER VK_FILTER_LINEAR
#define SHADOWMAP_DIM 2048

// Vertex layout for this example
std::vector<vkMeshLoader::VertexLayout> vertexLayout = 
{
	vkMeshLoader::VERTEX_LAYOUT_POSITION,
	vkMeshLoader::VERTEX_LAYOUT_UV,
	vkMeshLoader::VERTEX_LAYOUT_COLOR,
	vkMeshLoader::VERTEX_LAYOUT_NORMAL
};

class VulkanExample : public VulkanExampleBase
{
public:
	bool displayCubeMap = false;

	float zNear = 0.1f;
	float zFar = 1024.0f;


	// Depth bias (and slope) are used to avoid shadowing artefacts
	// Constant depth bias factor (always applied)
	float depthBiasConstant = 1.25f;
	// Slope depth bias factor, applied depending on polygon's slope
	float depthBiasSlope = 1.75f;


	struct {
		VkPipelineVertexInputStateCreateInfo inputState;
		std::vector<VkVertexInputBindingDescription> bindingDescriptions;
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
	} vertices;

	struct {
		vkMeshLoader::MeshBuffer skybox;
		vkMeshLoader::MeshBuffer scene;
	} meshes;

	struct {
		vkTools::UniformData scene;
		vkTools::UniformData offscreen;
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
		VkPipeline scene;
		VkPipeline offscreen;
		VkPipeline cubeMap;
	} pipelines;

	struct {
		VkPipelineLayout scene; 
		VkPipelineLayout offscreen;
	} pipelineLayouts;

	struct {
		VkDescriptorSet scene;
		VkDescriptorSet offscreen;
	} descriptorSets;
	 

	struct {
		VkDescriptorSetLayout scene;
		VkDescriptorSetLayout offscreen;
	} descriptorSetLayouts;

//	vkTools::VulkanTexture shadowCubeMap;

	// Framebuffer for offscreen rendering
	struct FrameBufferAttachment {
		VkImage image;
		VkDeviceMemory mem;
		VkImageView view;
	};
	struct OffscreenPass {
		int32_t width, height;
		VkFramebuffer frameBuffer;
		FrameBufferAttachment color, depth;
		VkRenderPass renderPass;
		VkSampler depthSampler;
		VkDescriptorImageInfo descriptor;
		VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
		// Semaphore used to synchronize between offscreen and final scene render pass
		VkSemaphore semaphore = VK_NULL_HANDLE;
	} offscreenPass;

	VkFormat fbDepthFormat;

	// Device features to be enabled for this example 
	static VkPhysicalDeviceFeatures getEnabledFeatures()
	{
		VkPhysicalDeviceFeatures enabledFeatures = {};
		enabledFeatures.geometryShader = VK_TRUE;
		enabledFeatures.shaderClipDistance = VK_TRUE;
		enabledFeatures.shaderCullDistance = VK_TRUE;
		enabledFeatures.shaderTessellationAndGeometryPointSize = VK_TRUE;
		return enabledFeatures;
	}

	VulkanExample() : VulkanExampleBase(ENABLE_VALIDATION, getEnabledFeatures)
	{
		zoom = -175.0f;
		zoomSpeed = 10.0f;
		timerSpeed *= 0.25f;
		rotation = { -20.5f, -673.0f, 0.0f };
		enableTextOverlay = true;
		title = "Vulkan Example - Point light shadows";
	}

	~VulkanExample()
	{
		// Clean up used Vulkan resources 
		// Note : Inherited destructor cleans up resources stored in base class
		 
		// Depth attachment
		vkDestroyImageView(device, offscreenPass.depth.view, nullptr);
		vkDestroyImage(device, offscreenPass.depth.image, nullptr);
		vkDestroySampler(device, offscreenPass.depthSampler, nullptr);
		vkFreeMemory(device, offscreenPass.depth.mem, nullptr);

		vkDestroyFramebuffer(device, offscreenPass.frameBuffer, nullptr);

		vkDestroyRenderPass(device, offscreenPass.renderPass, nullptr);

		// Pipelibes
		vkDestroyPipeline(device, pipelines.scene, nullptr);
		vkDestroyPipeline(device, pipelines.offscreen, nullptr);
		vkDestroyPipeline(device, pipelines.cubeMap, nullptr);

		vkDestroyPipelineLayout(device, pipelineLayouts.scene, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayouts.offscreen, nullptr);

		vkDestroyDescriptorSetLayout(device, descriptorSetLayouts.scene, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayouts.offscreen, nullptr);

		// Meshes
		vkMeshLoader::freeMeshBufferResources(device, &meshes.scene);
		vkMeshLoader::freeMeshBufferResources(device, &meshes.skybox);

		// Uniform buffers
		vkTools::destroyUniformData(device, &uniformData.offscreen);
		vkTools::destroyUniformData(device, &uniformData.scene);

		vkFreeCommandBuffers(device, cmdPool, 1, &offscreenPass.commandBuffer);
		vkDestroySemaphore(device, offscreenPass.semaphore, nullptr);
	}


	void reBuildCommandBuffers()
	{
		if (!checkCommandBuffers())
		{
			destroyCommandBuffers();
			createCommandBuffers();
		}
		buildCommandBuffers();
	}

	void buildCommandBuffers()
	{
		VkCommandBufferBeginInfo cmdBufInfo = vkTools::initializers::commandBufferBeginInfo();

		VkClearValue clearValues[2];
		clearValues[0].color = defaultClearColor;
		clearValues[1].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo renderPassBeginInfo = vkTools::initializers::renderPassBeginInfo();
		renderPassBeginInfo.renderPass = renderPass;
		renderPassBeginInfo.renderArea.offset.x = 0;
		renderPassBeginInfo.renderArea.offset.y = 0;
		renderPassBeginInfo.renderArea.extent.width = width;
		renderPassBeginInfo.renderArea.extent.height = height;
		renderPassBeginInfo.clearValueCount = 2;
		renderPassBeginInfo.pClearValues = clearValues;

		for (int32_t i = 0; i < drawCmdBuffers.size(); ++i)
		{
			renderPassBeginInfo.framebuffer = frameBuffers[i];

			VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));

			vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport viewport = vkTools::initializers::viewport(
				(float)width,
				(float)height,
				0.0f,
				1.0f);
			vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

			VkRect2D scissor = vkTools::initializers::rect2D(
				width,
				height,
				0,
				0);
			vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

			VkDeviceSize offsets[1] = { 0 };

			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts.scene, 0, 1, &descriptorSets.scene, 0, NULL);

			if (displayCubeMap)
			{
				vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.cubeMap);
				vkCmdBindVertexBuffers(drawCmdBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &meshes.skybox.vertices.buf, offsets);
				vkCmdBindIndexBuffer(drawCmdBuffers[i], meshes.skybox.indices.buf, 0, VK_INDEX_TYPE_UINT32);
				vkCmdDrawIndexed(drawCmdBuffers[i], meshes.skybox.indexCount, 1, 0, 0, 0);
			}
			else
			{
				vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.scene);
				vkCmdBindVertexBuffers(drawCmdBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &meshes.scene.vertices.buf, offsets);
				vkCmdBindIndexBuffer(drawCmdBuffers[i], meshes.scene.indices.buf, 0, VK_INDEX_TYPE_UINT32);
				vkCmdDrawIndexed(drawCmdBuffers[i], meshes.scene.indexCount, 1, 0, 0, 0);
			}

			vkCmdEndRenderPass(drawCmdBuffers[i]);

			VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
		}
	}	

	void loadAssets()
	{
		loadMesh(getAssetPath() + "models/cube.obj", &meshes.skybox, vertexLayout, 2.0f);
		loadMesh(getAssetPath() + "models/shadowscene_fire.dae", &meshes.scene, vertexLayout, 2.0f);
	}

	void setupVertexDescriptions()
	{
		// Binding description
		vertices.bindingDescriptions.resize(1);
		vertices.bindingDescriptions[0] =
			vkTools::initializers::vertexInputBindingDescription(
				VERTEX_BUFFER_BIND_ID,
				vkMeshLoader::vertexSize(vertexLayout),
				VK_VERTEX_INPUT_RATE_VERTEX);

		// Attribute descriptions
		vertices.attributeDescriptions.resize(4);
		// Location 0 : Position
		vertices.attributeDescriptions[0] =
			vkTools::initializers::vertexInputAttributeDescription(
				VERTEX_BUFFER_BIND_ID,
				0,
				VK_FORMAT_R32G32B32_SFLOAT,
				0);
		// Location 1 : Texture coordinates
		vertices.attributeDescriptions[1] =
			vkTools::initializers::vertexInputAttributeDescription(
				VERTEX_BUFFER_BIND_ID,
				1,
				VK_FORMAT_R32G32_SFLOAT,
				sizeof(float) * 3);
		// Location 2 : Color
		vertices.attributeDescriptions[2] =
			vkTools::initializers::vertexInputAttributeDescription(
				VERTEX_BUFFER_BIND_ID,
				2,
				VK_FORMAT_R32G32B32_SFLOAT,
				sizeof(float) * 5);
		// Location 3 : Normal
		vertices.attributeDescriptions[3] =
			vkTools::initializers::vertexInputAttributeDescription(
				VERTEX_BUFFER_BIND_ID,
				3,
				VK_FORMAT_R32G32B32_SFLOAT,
				sizeof(float) * 8);

		vertices.inputState = vkTools::initializers::pipelineVertexInputStateCreateInfo();
		vertices.inputState.vertexBindingDescriptionCount = vertices.bindingDescriptions.size();
		vertices.inputState.pVertexBindingDescriptions = vertices.bindingDescriptions.data();
		vertices.inputState.vertexAttributeDescriptionCount = vertices.attributeDescriptions.size();
		vertices.inputState.pVertexAttributeDescriptions = vertices.attributeDescriptions.data();
	}

	void setupDescriptorPool()
	{
		// Example uses three ubos and two image samplers
		std::vector<VkDescriptorPoolSize> poolSizes =
		{
			vkTools::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3),
			vkTools::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2)
		};

		VkDescriptorPoolCreateInfo descriptorPoolInfo =
			vkTools::initializers::descriptorPoolCreateInfo(
				poolSizes.size(),
				poolSizes.data(),
				3);

		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));
	}

	void setupDescriptorSetLayout()
	{
			// Scene pipeline layout
		{
			std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings =
			{
				// Binding 0 : Vertex shader uniform buffer
				vkTools::initializers::descriptorSetLayoutBinding(
					VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					VK_SHADER_STAGE_VERTEX_BIT,
					0),
				// Binding 1 : Fragment shader image sampler (cube map)
				vkTools::initializers::descriptorSetLayoutBinding(
					VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					VK_SHADER_STAGE_FRAGMENT_BIT,
					1)
			};

			VkDescriptorSetLayoutCreateInfo descriptorLayout =
				vkTools::initializers::descriptorSetLayoutCreateInfo(
					setLayoutBindings.data(),
					setLayoutBindings.size());

			VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &descriptorSetLayouts.scene));

			// 3D scene pipeline layout
			VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo =
				vkTools::initializers::pipelineLayoutCreateInfo(
					&descriptorSetLayouts.scene,
					1);

			VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pPipelineLayoutCreateInfo, nullptr, &pipelineLayouts.scene));
		}
		{
			// Offscreen pipeline layout

			VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo =
				vkTools::initializers::pipelineLayoutCreateInfo(
					&descriptorSetLayouts.offscreen,
					1);

			// Scene pipeline layout
			std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings =
			{
				// Binding 0 : Vertex shader uniform buffer
				vkTools::initializers::descriptorSetLayoutBinding(
					VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					VK_SHADER_STAGE_GEOMETRY_BIT,
					0),
				 
			};

			VkDescriptorSetLayoutCreateInfo descriptorLayout =
				vkTools::initializers::descriptorSetLayoutCreateInfo(
					setLayoutBindings.data(),
					setLayoutBindings.size());

			VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &descriptorSetLayouts.offscreen));


			VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pPipelineLayoutCreateInfo, nullptr, &pipelineLayouts.offscreen));
		}
		
	}

	void setupDescriptorSets()
	{
		// 3D scene
		{
			VkDescriptorSetAllocateInfo allocInfo =
				vkTools::initializers::descriptorSetAllocateInfo(
					descriptorPool,
					&descriptorSetLayouts.scene,
					1);

			VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSets.scene));

			// Image descriptor for the cube map 
			VkDescriptorImageInfo texDescriptor =
				vkTools::initializers::descriptorImageInfo(
					framebuffer.depth.sampler,
					offscreenPass.depth.view,
					VK_IMAGE_LAYOUT_GENERAL);

			std::vector<VkWriteDescriptorSet> sceneDescriptorSets =
			{
				// Binding 0 : Vertex shader uniform buffer
				vkTools::initializers::writeDescriptorSet(
				descriptorSets.scene,
					VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					0,
					&uniformData.scene.descriptor),

				// Binding 1 : Fragment shader shadow sampler
				vkTools::initializers::writeDescriptorSet(
					descriptorSets.scene,
					VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					1,
					&texDescriptor)
			};
			vkUpdateDescriptorSets(device, sceneDescriptorSets.size(), sceneDescriptorSets.data(), 0, NULL);
		}

		// Offscreen
		{

			VkDescriptorSetAllocateInfo allocInfo =
				vkTools::initializers::descriptorSetAllocateInfo(
					descriptorPool,
					&descriptorSetLayouts.offscreen,
					1);

			VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSets.offscreen));

			std::vector<VkWriteDescriptorSet> offScreenWriteDescriptorSets =
			{
				// Binding 0 : Vertex shader uniform buffer
				vkTools::initializers::writeDescriptorSet(
					descriptorSets.offscreen,
					VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					0,
					&uniformData.offscreen.descriptor),
			};

			vkUpdateDescriptorSets(device, offScreenWriteDescriptorSets.size(), offScreenWriteDescriptorSets.data(), 0, NULL);
		}
	}

	// Set up a separate render pass for the offscreen frame buffer
	// This is necessary as the offscreen frame buffer attachments
	// use formats different to the ones from the visible frame buffer
	// and at least the depth one may not be compatible
	void prepareOffscreenRenderpass()
	{
		VkAttachmentDescription attchmentDescription{};
		attchmentDescription.format = DEPTH_FORMAT;
		attchmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
		attchmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;						// Clear depth at beginning of the render pass
		attchmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;					// We will read from depth, so it's important to store the depth attachment results
		attchmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attchmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attchmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;					// We don't care about initial layout of the attachment
		attchmentDescription.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;	// Attachment will be transitioned to shader read at render pass end
		
		VkAttachmentReference depthReference = {};
		depthReference.attachment = 0;
		depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;		// Attachment will be used as depth/stencil during render pass
		 
		std::vector<VkSubpassDescription> subpasses;


		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 0;												// No color attachments
		subpass.pDepthStencilAttachment = &depthReference;		
																						// Use subpass dependencies for layout transitions
		std::array<VkSubpassDependency, 2> dependencies;

		{
			dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
			
			dependencies[0].dstSubpass = 0;
			dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | 
											VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			
			dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
		}
		{
			dependencies[1].srcSubpass = 0;
			dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
											VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			
			dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
			
			dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
		}


		VkRenderPassCreateInfo renderPassCreateInfo = vkTools::initializers::renderPassCreateInfo();
		 renderPassCreateInfo.attachmentCount = 1;
		 renderPassCreateInfo.pAttachments = &attchmentDescription;
		   renderPassCreateInfo.subpassCount = 1;
		   renderPassCreateInfo.pSubpasses = &subpass;
	// renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
	// renderPassCreateInfo.pDependencies = dependencies.data();

		VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &offscreenPass.renderPass));
	}

	// Prepare a new framebuffer for offscreen rendering
	// The contents of this framebuffer are then
	// copied to the different cube map faces
	void prepareOffscreenFramebuffer()
	{
		offscreenPass.width = SHADOWMAP_DIM;
		offscreenPass.height = SHADOWMAP_DIM;

		 
		// For shadow mapping we only need a depth attachment
		VkImageCreateInfo image = vkTools::initializers::imageCreateInfo();
		image.imageType = VK_IMAGE_TYPE_2D;
		image.extent.width = offscreenPass.width;
		image.extent.height = offscreenPass.height;
		image.extent.depth = 1;
		image.mipLevels = 1;

		image.arrayLayers = 6;
		image.samples = VK_SAMPLE_COUNT_1_BIT;
		image.tiling = VK_IMAGE_TILING_OPTIMAL;
		image.format = DEPTH_FORMAT;																// Depth stencil attachment
		image.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;		// We will sample directly from the depth attachment for the shadow mapping
		VK_CHECK_RESULT(vkCreateImage(device, &image, nullptr, &offscreenPass.depth.image));

		VkMemoryAllocateInfo memAlloc = vkTools::initializers::memoryAllocateInfo();
		VkMemoryRequirements memReqs;
		vkGetImageMemoryRequirements(device, offscreenPass.depth.image, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &offscreenPass.depth.mem));
		VK_CHECK_RESULT(vkBindImageMemory(device, offscreenPass.depth.image, offscreenPass.depth.mem, 0));

		VkImageViewCreateInfo depthStencilView = vkTools::initializers::imageViewCreateInfo();
		depthStencilView.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
		depthStencilView.format = DEPTH_FORMAT;
		depthStencilView.subresourceRange = {};
		depthStencilView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		depthStencilView.subresourceRange.baseMipLevel = 0;
		depthStencilView.subresourceRange.levelCount = 1;
		depthStencilView.subresourceRange.baseArrayLayer = 0;
		depthStencilView.subresourceRange.layerCount = 6; //this works with 1;

		depthStencilView.image = offscreenPass.depth.image;
		VK_CHECK_RESULT(vkCreateImageView(device, &depthStencilView, nullptr, &offscreenPass.depth.view));

		// Create sampler to sample from to depth attachment 
		// Used to sample in the fragment shader for shadowed rendering
		// Create sampler
		VkSamplerCreateInfo sampler = vkTools::initializers::samplerCreateInfo();
		sampler.magFilter = SHADOWMAP_FILTER;
		sampler.minFilter = SHADOWMAP_FILTER;
		sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		sampler.addressModeV = sampler.addressModeU;
		sampler.addressModeW = sampler.addressModeU;
		sampler.mipLodBias = 0.0f;
		sampler.maxAnisotropy = 0; 
		sampler.minLod = 0.0f;
		sampler.maxLod = 1.0f;
		sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		sampler.compareEnable = VK_TRUE;
		sampler.compareOp = VK_COMPARE_OP_LESS;
		
		
		VK_CHECK_RESULT(vkCreateSampler(device, &sampler, nullptr, &offscreenPass.depthSampler));

		prepareOffscreenRenderpass();

		// Create frame buffer
		VkFramebufferCreateInfo fbufCreateInfo = vkTools::initializers::framebufferCreateInfo();
		fbufCreateInfo.renderPass = offscreenPass.renderPass;
		fbufCreateInfo.attachmentCount = 1;
		fbufCreateInfo.pAttachments = &offscreenPass.depth.view;
		fbufCreateInfo.width = offscreenPass.width;
		fbufCreateInfo.height = offscreenPass.height;
		fbufCreateInfo.layers = 6;

		VK_CHECK_RESULT(vkCreateFramebuffer(device, &fbufCreateInfo, nullptr, &offscreenPass.frameBuffer));
	}

	// Command buffer for rendering and copying all cube map faces
	void buildOffscreenCommandBuffer()
	{
		if (offscreenPass.commandBuffer == VK_NULL_HANDLE)
		{
			offscreenPass.commandBuffer = VulkanExampleBase::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);
		}
		if (offscreenPass.semaphore == VK_NULL_HANDLE)
		{
			// Create a semaphore used to synchronize offscreen rendering and usage
			VkSemaphoreCreateInfo semaphoreCreateInfo = vkTools::initializers::semaphoreCreateInfo();
			VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &offscreenPass.semaphore));
		}

		VkCommandBufferBeginInfo cmdBufInfo = vkTools::initializers::commandBufferBeginInfo();

	

		VK_CHECK_RESULT(vkBeginCommandBuffer(offscreenPass.commandBuffer, &cmdBufInfo));
		
		VkClearValue clearValues[1];
		clearValues[0].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo renderPassBeginInfo = vkTools::initializers::renderPassBeginInfo();
		// Reuse render pass from example pass 

		renderPassBeginInfo.renderPass = offscreenPass.renderPass;
		renderPassBeginInfo.framebuffer = offscreenPass.frameBuffer;

		renderPassBeginInfo.renderArea.extent.width = offscreenPass.width;
		renderPassBeginInfo.renderArea.extent.height = offscreenPass.height;
		renderPassBeginInfo.clearValueCount = 1;
		renderPassBeginInfo.pClearValues = clearValues;

		vkCmdSetDepthBias(
			offscreenPass.commandBuffer,
			depthBiasConstant,
			0.0f,
			depthBiasSlope);
		// Render scene from cube face's point of view
		vkCmdBeginRenderPass(offscreenPass.commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);



		VkViewport viewport = vkTools::initializers::viewport((float)offscreenPass.width, (float)offscreenPass.height, 0.0f, 1.0f);
		vkCmdSetViewport(offscreenPass.commandBuffer, 0, 1, &viewport);

		VkRect2D scissor = vkTools::initializers::rect2D(offscreenPass.width, offscreenPass.height, 0, 0);
		vkCmdSetScissor(offscreenPass.commandBuffer, 0, 1, &scissor);
		 


		vkCmdBindPipeline(offscreenPass.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.offscreen);
		vkCmdBindDescriptorSets(offscreenPass.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			pipelineLayouts.offscreen, 0, 1, &descriptorSets.offscreen, 0, NULL);

		VkDeviceSize offsets[1] = { 0 };
		vkCmdBindVertexBuffers(offscreenPass.commandBuffer, VERTEX_BUFFER_BIND_ID, 1, &meshes.scene.vertices.buf, offsets);
		vkCmdBindIndexBuffer(offscreenPass.commandBuffer, meshes.scene.indices.buf, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(offscreenPass.commandBuffer, meshes.scene.indexCount, 1, 0, 0, 0); // geometry shader uses invocation to multiply


		vkCmdEndRenderPass(offscreenPass.commandBuffer);

		VK_CHECK_RESULT(vkEndCommandBuffer(offscreenPass.commandBuffer));
	}

	void preparePipelines()
	{
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
			vkTools::initializers::pipelineInputAssemblyStateCreateInfo(
				VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
				0,
				VK_FALSE);

		VkPipelineRasterizationStateCreateInfo rasterizationState =
			vkTools::initializers::pipelineRasterizationStateCreateInfo(
				VK_POLYGON_MODE_FILL,
				VK_CULL_MODE_FRONT_BIT,
				VK_FRONT_FACE_COUNTER_CLOCKWISE,
				0);

		VkPipelineColorBlendAttachmentState blendAttachmentState =
			vkTools::initializers::pipelineColorBlendAttachmentState(
				0xf,
				VK_FALSE);

		VkPipelineColorBlendStateCreateInfo colorBlendState =
			vkTools::initializers::pipelineColorBlendStateCreateInfo(
				1,
				&blendAttachmentState);

		VkPipelineDepthStencilStateCreateInfo depthStencilState =
			vkTools::initializers::pipelineDepthStencilStateCreateInfo(
				VK_TRUE,
				VK_TRUE,
				VK_COMPARE_OP_LESS_OR_EQUAL);

		VkPipelineViewportStateCreateInfo viewportState =
			vkTools::initializers::pipelineViewportStateCreateInfo(1, 1, 0);

		VkPipelineMultisampleStateCreateInfo multisampleState =
			vkTools::initializers::pipelineMultisampleStateCreateInfo(
				VK_SAMPLE_COUNT_1_BIT,
				0);

		std::vector<VkDynamicState> dynamicStateEnables = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};
		VkPipelineDynamicStateCreateInfo dynamicState =
			vkTools::initializers::pipelineDynamicStateCreateInfo(
				dynamicStateEnables.data(),
				dynamicStateEnables.size(),
				0);

		// 3D scene pipeline
		// Load shaders
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

		shaderStages[0] = loadShader(getAssetPath() + "shaders/shadowmappingomniLayered/scene.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getAssetPath() + "shaders/shadowmappingomniLayered/scene.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

		VkGraphicsPipelineCreateInfo pipelineCreateInfo =
			vkTools::initializers::pipelineCreateInfo(
				pipelineLayouts.scene,
				renderPass,
				0);

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

		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.scene));

		// Cube map display pipeline
		shaderStages[0] = loadShader(getAssetPath() + "shaders/shadowmappingomniLayered/cubemapdisplay.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getAssetPath() + "shaders/shadowmappingomniLayered/cubemapdisplay.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		rasterizationState.cullMode =  VK_CULL_MODE_BACK_BIT;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.cubeMap));

		std::array<VkPipelineShaderStageCreateInfo, 2> shadowStages;
		shadowStages[0] = loadShader(getAssetPath() + "shaders/shadowmappingomniLayered/shadow.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shadowStages[1] = loadShader(getAssetPath() + "shaders/shadowmappingomniLayered/shadow.geom.spv", VK_SHADER_STAGE_GEOMETRY_BIT);
		

		pipelineCreateInfo.pStages = shadowStages.data();
		pipelineCreateInfo.stageCount = static_cast<uint32_t>(shadowStages.size());
		
		// No blend attachment states (no color attachments used)
		colorBlendState.attachmentCount = 0;
		depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
		// Cull front faces
		rasterizationState.cullMode = VK_CULL_MODE_NONE; 
		// Enable depth bias
		rasterizationState.depthBiasEnable = VK_TRUE;
		// Add depth bias to dynamic state, so we can change it at runtime
		dynamicStateEnables.push_back(VK_DYNAMIC_STATE_DEPTH_BIAS);
		dynamicState =
			vkTools::initializers::pipelineDynamicStateCreateInfo(
				dynamicStateEnables.data(),
				dynamicStateEnables.size(),
				0);

		pipelineCreateInfo.layout = pipelineLayouts.offscreen;
		pipelineCreateInfo.renderPass = offscreenPass.renderPass;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.offscreen));
	}

	// Prepare and initialize uniform buffer containing shader uniforms
	void prepareUniformBuffers()
	{
		// Offscreen vertex shader uniform buffer block 
		createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			sizeof(uboOffscreenVS),
			&uboOffscreenVS,
			&uniformData.offscreen.buffer,
			&uniformData.offscreen.memory,
			&uniformData.offscreen.descriptor);

		// 3D scene
		createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			sizeof(uboVSscene),
			&uboVSscene,
			&uniformData.scene.buffer,
			&uniformData.scene.memory,
			&uniformData.scene.descriptor);

		updateUniformBufferOffscreen();
		updateUniformBuffers();
	}

	void updateUniformBuffers()
	{
		// 3D scene
		uboVSscene.projection = glm::perspective(glm::radians(45.0f), (float)width / (float)height, zNear, zFar);
		uboVSscene.view = glm::translate(glm::mat4(), glm::vec3(0.0f, 0.0f, displayCubeMap ? 0.0f : zoom));

		uboVSscene.model = glm::mat4();
		uboVSscene.model = glm::rotate(uboVSscene.model, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
		uboVSscene.model = glm::rotate(uboVSscene.model, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
		uboVSscene.model = glm::rotate(uboVSscene.model, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

		uboVSscene.lightPos = lightPos;

		uint8_t *pData;
		VK_CHECK_RESULT(vkMapMemory(device, uniformData.scene.memory, 0, sizeof(uboVSscene), 0, (void **)&pData));
		memcpy(pData, &uboVSscene, sizeof(uboVSscene));
		vkUnmapMemory(device, uniformData.scene.memory);
	}

	void updateUniformBufferOffscreen()
	{
		lightPos.x = sin(glm::radians(timer * 360.0f)) * 1.0f;
		lightPos.z = cos(glm::radians(timer * 360.0f)) * 1.0f;

		glm::mat4 P = glm::perspective((float)(M_PI / 2.0), 1.0f, zNear, zFar);
		 
		glm::mat4 M = glm::translate(glm::mat4(), glm::vec3(-lightPos.x, -lightPos.y, -lightPos.z));

		for (uint32_t face = 0; face < 6; ++face)
		{
			glm::mat4 V;
			switch (face)
			{
				case 0: // POSITIVE_X
					V = glm::rotate(V, glm::radians(90.0f),  glm::vec3(0.0f, 1.0f, 0.0f));
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
					V = glm::rotate(V, glm::radians(90.0f),  glm::vec3(1.0f, 0.0f, 0.0f));
					break;									 
				case 4:	// POSITIVE_Z						 
					V = glm::rotate(V, glm::radians(180.0f),  glm::vec3(1.0f, 0.0f, 0.0f));
					break;									 
				case 5:	// NEGATIVE_Z						 
					V = glm::rotate(V, glm::radians(180.0f),  glm::vec3(0.0f, 0.0f, 1.0f));
					break;
			}
			uboOffscreenVS.mvp[face] = P  * V *M;
		}

		uint8_t *pData;
		VK_CHECK_RESULT(vkMapMemory(device, uniformData.offscreen.memory, 0, sizeof(uboOffscreenVS), 0, (void **)&pData));
		memcpy(pData, &uboOffscreenVS, sizeof(uboOffscreenVS));
		vkUnmapMemory(device, uniformData.offscreen.memory);
	}

	void draw()
	{
		VulkanExampleBase::prepareFrame();

		// Offscreen rendering

		// Wait for swap chain presentation to finish
			submitInfo.pWaitSemaphores = &semaphores.presentComplete;
			// Signal ready with offscreen semaphore
			submitInfo.pSignalSemaphores = &offscreenPass.semaphore;
		
			// Submit work
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &offscreenPass.commandBuffer;
			VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));

		// Scene rendering

			// Wait for offscreen semaphore
			submitInfo.pWaitSemaphores = &offscreenPass.semaphore;
			// Signal ready with render complete semaphpre
			submitInfo.pSignalSemaphores = &semaphores.renderComplete;

		// Submit work
			submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];
			VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));

		VulkanExampleBase::submitFrame();
	}

	void prepare()
	{
		VulkanExampleBase::prepare();
		loadAssets();
		setupVertexDescriptions();
		prepareUniformBuffers();



		prepareOffscreenFramebuffer();
		 
		setupDescriptorSetLayout();
		preparePipelines();
		setupDescriptorPool();
		setupDescriptorSets();

		buildCommandBuffers();
		buildOffscreenCommandBuffer();
		prepared = true;
	}

	virtual void render()
	{
		if (!prepared)
			return;
		draw();
		if (!paused)
		{
			updateUniformBufferOffscreen();
			updateUniformBuffers();
		}
	}

	virtual void viewChanged()
	{
		updateUniformBufferOffscreen();
		updateUniformBuffers();
	}

	virtual void keyPressed(uint32_t keyCode)
	{
		switch (keyCode)
		{
		case KEY_D:
		case GAMEPAD_BUTTON_A:
			toggleCubeMapDisplay();
			break;
		}
	}

	virtual void getOverlayText(VulkanTextOverlay *textOverlay)
	{
#if defined(__ANDROID__)
		textOverlay->addText("Press \"Button A\" to display depth cubemap", 5.0f, 85.0f, VulkanTextOverlay::alignLeft);
#else
		textOverlay->addText("Press \"d\" to display depth cubemap", 5.0f, 85.0f, VulkanTextOverlay::alignLeft);
#endif
	}

	void toggleCubeMapDisplay()
	{
		displayCubeMap = !displayCubeMap;
		reBuildCommandBuffers();
	}
};


VulkanExample *vulkanExample;

#if defined(_WIN32)
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (vulkanExample != NULL)
	{
		vulkanExample->handleMessages(hWnd, uMsg, wParam, lParam);
	}
	return (DefWindowProc(hWnd, uMsg, wParam, lParam));
}
#elif defined(__linux__) && !defined(__ANDROID__)
static void handleEvent(const xcb_generic_event_t *event)
{
	if (vulkanExample != NULL)
	{
		vulkanExample->handleEvent(event);
	}
}
#endif

// Main entry point
#if defined(_WIN32)
// Windows entry point
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
#elif defined(__ANDROID__)
// Android entry point
void android_main(android_app* state)
#elif defined(__linux__)
// Linux entry point
int main(const int argc, const char *argv[])
#endif
{
#if defined(__ANDROID__)
	// Removing this may cause the compiler to omit the main entry point 
	// which would make the application crash at start
	app_dummy();
#endif
	vulkanExample = new VulkanExample();
#if defined(_WIN32)
	vulkanExample->setupWindow(hInstance, WndProc);
#elif defined(__ANDROID__)
	// Attach vulkan example to global android application state
	state->userData = vulkanExample;
	state->onAppCmd = VulkanExample::handleAppCommand;
	state->onInputEvent = VulkanExample::handleAppInput;
	vulkanExample->androidApp = state;
#elif defined(__linux__)
	vulkanExample->setupWindow();
#endif
#if !defined(__ANDROID__)
	vulkanExample->initSwapchain();
	vulkanExample->prepare();
#endif
	vulkanExample->renderLoop();
	delete(vulkanExample);
#if !defined(__ANDROID__)
	return 0;
#endif
}
