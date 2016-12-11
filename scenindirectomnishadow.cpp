/*
* Vulkan Example -  Rendering a scene with multiple meshes and materials
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
#include <glm/gtc/type_ptr.hpp>

#include <vulkan/vulkan.h>
#include "vulkanexamplebase.h"

#define VERTEX_BUFFER_BIND_ID 0
#define VERTEX_DIVISOR 1
#define ENABLE_VALIDATION false

// Texture properties
#define TEX_DIM 1024
#define TEX_FILTER VK_FILTER_LINEAR

// Offscreen frame buffer properties
#define FB_DIM TEX_DIM
#define FB_COLOR_FORMAT VK_FORMAT_R32_SFLOAT 

#define SAMPLE_COUNT VK_SAMPLE_COUNT_4_BIT

struct {
	struct {
		VkImage image;
		VkImageView view;
		VkDeviceMemory memory;
	} color;
	struct {
		VkImage image;
		VkImageView view;
		VkDeviceMemory memory;
	} depth;
} multisampleTarget;


// Vertex layout used in this example
struct Vertex {
	glm::vec3 pos;
	glm::vec3 normal;
	glm::vec2 uv;
	glm::vec3 color;
};

// Scene related structs

// Shader properites for a material
// Will be passed to the shaders using push constant
struct Material
{
	glm::vec4 ambient;
	//float opacity;
	glm::vec4 diffuse;
//float padding;
	glm::vec4 specular;
	//float shininess;
};

// Stores info on the materials used in the scene
struct SceneMaterial
{
	std::string name;
	std::string textureName;
	// Material properties
	Material properties;
	// The example only uses a diffuse channel
	vkTools::VulkanTexture diffuse ;
	// The material's descriptor contains the material descriptors
	VkDescriptorSet descriptorSet;
	// Pointer to the pipeline used by this material
	VkPipeline *pipeline;
};

// Stores per-mesh Vulkan resources
struct SceneMesh
{
	VkBuffer vertexBuffer;
	VkDeviceMemory vertexMemory;

	VkBuffer indexBuffer;
	VkDeviceMemory indexMemory;
	uint32_t vertexCount;
	uint32_t indexCount;

	//VkDescriptorSet descriptorSet;

	// Pointer to the material used by this mesh
	SceneMaterial *material;
};

// Class for loading the scene and generating all Vulkan resources
class Scene
{
private:
	VkDevice device;
	VkQueue queue;


public:
	vkTools::VulkanTexture diffuseArray;
	uint32_t layerCount;

	VkDescriptorPool descriptorPool;
	// We will be using separate descriptor sets (and bindings)
	// for material and scene related uniforms
	struct
	{
		VkDescriptorSetLayout Material;
		VkDescriptorSetLayout scene;
		VkDescriptorSetLayout shadow;
	} descriptorSetLayouts;


	VkDescriptorSet descriptorSetShadow;
private:
	VkDescriptorSet descriptorSetScene;
	VkDescriptorSet descriptorSetMaterial;
	VkDescriptorSet descriptorSetOffscreen;

	vkTools::VulkanTextureLoader *textureLoader;

	const aiScene* aScene;

	SceneMesh sceneMesh;
	std::vector<VkDrawIndexedIndirectCommand> indirectDrawCommands;
	VkBuffer indirectDrawCommandsBuffer;
	VkDeviceMemory indirectDrawCommandsMemory;

	VkPhysicalDeviceMemoryProperties deviceMemProps;
	uint32_t getMemoryTypeIndex(uint32_t typeBits, VkFlags properties)
	{
		for (int i = 0; i < 32; i++)
		{
			if ((typeBits & 1) == 1)
			{
				if ((deviceMemProps.memoryTypes[i].propertyFlags & properties) == properties)
				{
					return i;
				}
			}
			typeBits >>= 1;
		}
		return 0;
	}
public:




	void init_sampler(  VkSampler &sampler) {

		VkSamplerCreateInfo samplerCreateInfo = {};
		samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
		samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
		samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCreateInfo.mipLodBias = 0.0;
		samplerCreateInfo.anisotropyEnable = VK_FALSE,
			samplerCreateInfo.maxAnisotropy = 0;
		samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
		samplerCreateInfo.minLod = 0.0;
		samplerCreateInfo.maxLod = 0.0;
		samplerCreateInfo.compareEnable = VK_FALSE;
		samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

		/* create sampler */
		vkCreateSampler(device, &samplerCreateInfo, NULL, &sampler);
		
	}


	void uploadMaterials(VkCommandBuffer copyCmd)
	{



		VkMemoryAllocateInfo memAlloc = {
			VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			NULL,
			0,
			0 };
		VkMemoryRequirements memReqs;
		int materialSize = materialBufferData.size() * sizeof(Material);

		if (true)
		{
			VkMemoryAllocateInfo memAlloc = {
				VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
				NULL,
				0,
				0 };
			VkMemoryRequirements memReqs;
			VkBufferCreateInfo bufferCreateInfo  	 {
				VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
				NULL,
				0,
				materialSize,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
			};
			
			VK_CHECK_RESULT(vkCreateBuffer(device, &bufferCreateInfo, nullptr, &uniformBuffer.material.buffer));
			vkGetBufferMemoryRequirements(device, uniformBuffer.material.buffer, &memReqs);
			memAlloc.allocationSize = memReqs.size;
			memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &uniformBuffer.material.memory));
			
			VK_CHECK_RESULT(vkMapMemory(device, uniformBuffer.material.memory, 0, materialSize, 0, (void **)&uniformBuffer.material.mapped));			 
			memcpy(uniformBuffer.material.mapped, materialBufferData.data(), materialSize);		 
			vkUnmapMemory(device, uniformBuffer.material.memory);
			
			VK_CHECK_RESULT(vkBindBufferMemory(device, uniformBuffer.material.buffer, uniformBuffer.material.memory, 0));
			uniformBuffer.material.descriptor.offset = 0;
			uniformBuffer.material.descriptor.buffer = uniformBuffer.material.buffer;
			uniformBuffer.material.descriptor.range = materialSize;

		}
		else
		{
			struct
			{
				struct {
					VkDeviceMemory memory;
					VkBuffer buffer;
				} mBuffer;
			} staging;

			// Generate vertex buffer
			VkBufferCreateInfo mBufferInfo;
			void* data;
			 
			// Staging buffer
			mBufferInfo  = VkBufferCreateInfo{
				VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
				NULL,
				0,
				(VkDeviceSize) materialSize,
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_SHARING_MODE_EXCLUSIVE,
				0,
				NULL
			};

			VK_CHECK_RESULT(vkCreateBuffer(device, &mBufferInfo, nullptr, &staging.mBuffer.buffer));
			vkGetBufferMemoryRequirements(device, staging.mBuffer.buffer, &memReqs);
			memAlloc.allocationSize = memReqs.size;
			memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT );
			VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &staging.mBuffer.memory));
			VK_CHECK_RESULT(vkMapMemory(device, staging.mBuffer.memory, 0, materialSize, 0, &data));
			memcpy(data, materialBufferData.data(), materialSize);
			vkUnmapMemory(device, staging.mBuffer.memory);
			VK_CHECK_RESULT(vkBindBufferMemory(device, staging.mBuffer.buffer, staging.mBuffer.memory, 0));
		 
			// Target
			mBufferInfo  = VkBufferCreateInfo{
				VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
				NULL,
				0,
				(VkDeviceSize) materialSize,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_SHARING_MODE_EXCLUSIVE,
				0,
				NULL
			};
			VK_CHECK_RESULT(vkCreateBuffer(device, &mBufferInfo, nullptr, &uniformBuffer.material.buffer));
			vkGetBufferMemoryRequirements(device, uniformBuffer.material.buffer, &memReqs);
			memAlloc.allocationSize = memReqs.size;
			memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);  
			VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &uniformBuffer.material.memory));
			VK_CHECK_RESULT(vkBindBufferMemory(device, uniformBuffer.material.buffer, uniformBuffer.material.memory, 0));
		 

			// Copy
			VkCommandBufferBeginInfo cmdBufInfo{
				VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				NULL,
				0,
				NULL
			};
			VK_CHECK_RESULT(vkBeginCommandBuffer(copyCmd, &cmdBufInfo));

			VkBufferCopy copyRegion = {};

			copyRegion.size = materialSize;
			vkCmdCopyBuffer(
				copyCmd,
				staging.mBuffer.buffer,
				uniformBuffer.material.buffer,
				1,
				&copyRegion);

			uniformBuffer.material.descriptor.offset = 0;
			uniformBuffer.material.descriptor.buffer = uniformBuffer.material.buffer;
			uniformBuffer.material.descriptor.range = materialSize;

			VkSubmitInfo submitInfo = {};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &copyCmd;

			VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
			VK_CHECK_RESULT(vkQueueWaitIdle(queue));

			vkDestroyBuffer(device, staging.mBuffer.buffer, nullptr);
			vkFreeMemory(device, staging.mBuffer.memory, nullptr);

		}

		// Generate descriptor sets for the materials

		// Descriptor pool
		//  twice the ubo size bc of offscreen rendering

		std::vector<VkDescriptorPoolSize> poolSizes;
		poolSizes.push_back(VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3});
		poolSizes.push_back(VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,  static_cast<uint32_t>(materials.size()) });
		poolSizes.push_back(VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLER, 1 });
		poolSizes.push_back(VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 });

		VkDescriptorPoolCreateInfo descriptorPoolInfo = {
			VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			NULL,
			0,
			static_cast<uint32_t>(materials.size()) + 5,
			static_cast<uint32_t>(poolSizes.size()),
			poolSizes.data()
		}; // additional ubos(scene, offscreen) + shadowmap

		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));

		// descriptorSetLayouts.scene
		{
			// Descriptor set and pipeline layouts
			std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings;
			VkDescriptorSetLayoutCreateInfo descriptorLayout;

			// Set 0: Scene matrices
			setLayoutBindings.push_back(VkDescriptorSetLayoutBinding{
				0,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				1,
				VK_SHADER_STAGE_VERTEX_BIT,
				NULL
			});

			descriptorLayout = VkDescriptorSetLayoutCreateInfo{
				VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
				NULL,
				0,
				static_cast<uint32_t>(setLayoutBindings.size()),
				setLayoutBindings.data()
			}; 
			 


			VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &descriptorSetLayouts.scene));
		}

		//descriptorSetLayouts.Material
		{
			// Descriptor set and pipeline layouts
			std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings;

			// Set 1: Material data
			setLayoutBindings.push_back(VkDescriptorSetLayoutBinding{
				0,												//binding;
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,				//descriptorType;
				1,												//descriptorCount;
				VK_SHADER_STAGE_FRAGMENT_BIT,					//stageFlags;
				0 												//pImmutableSamplers;
			});

		 	// Set 1: Sampler			 
		 	setLayoutBindings.push_back(VkDescriptorSetLayoutBinding{
		 		1,												//binding;			
		 		VK_DESCRIPTOR_TYPE_SAMPLER,						//descriptorType;
		 		1,												//descriptorCount;
		 		VK_SHADER_STAGE_FRAGMENT_BIT,					//stageFlags;
		 		0 												//pImmutableSamplers;
		 	});
		 	
		 	// Set 1: Texture	
		 	setLayoutBindings.push_back(VkDescriptorSetLayoutBinding{
		 		2,												//binding;
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,				//descriptorType;
		 		static_cast<uint32_t>(materials.size()),		//descriptorCount;
		 		VK_SHADER_STAGE_FRAGMENT_BIT,					//stageFlags;
				0												//pImmutableSamplers;
		 	});
			
			const VkDescriptorSetLayoutCreateInfo descriptorLayout =
			{
				VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,	//sType;
				NULL,													//pNext;
				0,														//flags;
				static_cast<uint32_t>(setLayoutBindings.size()),		//bindingCount;
				setLayoutBindings.data(),								//pBindings;
			};

			VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &descriptorSetLayouts.Material));
		}
		
		//descriptorSetLayouts.shadow
		{ 	
			////////////////////////////////////////////////////////////////////////
			//add shadow to scene, similat to ubo data

			// Descriptor set and pipeline layouts
			std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings;
			VkDescriptorSetLayoutCreateInfo descriptorLayout;

			setLayoutBindings.push_back(VkDescriptorSetLayoutBinding{
				0,
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				1,
				VK_SHADER_STAGE_FRAGMENT_BIT,
				NULL
			});

			descriptorLayout = VkDescriptorSetLayoutCreateInfo{
				VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
				NULL,
				0,
				static_cast<uint32_t>(setLayoutBindings.size()),
				setLayoutBindings.data()
			};

			VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &descriptorSetLayouts.shadow));

		}
		////////////////////////////////////////////////////////////////////////////


		// Setup pipeline layout scene
		{
			// Setup pipeline layout
			std::array<VkDescriptorSetLayout, 3> setLayouts = {
				descriptorSetLayouts.scene,
				descriptorSetLayouts.Material,
				descriptorSetLayouts.shadow };

			VkPushConstantRange pushConstantRange{
				VK_SHADER_STAGE_FRAGMENT_BIT,
				0,
				sizeof(int)
			};

			VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = VkPipelineLayoutCreateInfo{
				VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
				NULL,
				NULL,
				static_cast<uint32_t>(setLayouts.size()),
				setLayouts.data(),
				1,
				&pushConstantRange
			};

			// We will be using a push constant block to pass material properties to the fragment shaders

			VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayouts.scene));
		}

		// Setup pipeline layout offscreen
		{
			std::array<VkDescriptorSetLayout, 1 > setLayouts = { descriptorSetLayouts.scene };
			VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = VkPipelineLayoutCreateInfo{
				VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
				NULL,
				NULL,
				static_cast<uint32_t>(setLayouts.size()),
				setLayouts.data(),
				0,
				NULL
			};

			VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayouts.offscreen));

		}


		

		// Scene descriptor set
		{
			 
			VkDescriptorSetAllocateInfo allocInfo{
				VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				NULL,
				descriptorPool,
				1,
				&descriptorSetLayouts.scene
			};

			VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSetScene));

			std::vector<VkWriteDescriptorSet> writeDescriptorSets;
			// Binding 0 : Vertex shader uniform buffer

			writeDescriptorSets.push_back(VkWriteDescriptorSet{
				VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				NULL,
				descriptorSetScene,
				0,
				0,
				1,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				NULL,
				&uniformBuffer.scene.descriptor,
				NULL
			});

			vkUpdateDescriptorSets(device, writeDescriptorSets.size(), writeDescriptorSets.data(), 0, NULL);
		}

		// Offscreen descriptor set
		{
			VkDescriptorSetAllocateInfo allocInfo{
				VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				NULL,
				descriptorPool,
				1,
				&descriptorSetLayouts.scene
			};

			VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSetOffscreen));

			std::vector<VkWriteDescriptorSet> writeDescriptorSets;
			// Binding 0 : Vertex shader uniform buffer
			writeDescriptorSets.push_back(VkWriteDescriptorSet{
				VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				NULL,
				descriptorSetOffscreen,
				0,
				0,
				1,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				NULL,
				&uniformBuffer.offscreen.descriptor,
				NULL
			});



			vkUpdateDescriptorSets(device, writeDescriptorSets.size(), writeDescriptorSets.data(), 0, NULL);
		}

		// Material Descriptor set
		{ 

			VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {
				VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,	//sType;
				NULL,											//pNext;
				descriptorPool,									//descriptorPool;
				1,												//descriptorSetCount;
				&descriptorSetLayouts.Material					//pSetLayouts;
			};
			
			VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSetMaterial));

			std::vector<VkWriteDescriptorSet> writeDescriptorSets;
			  
			 
		 	writeDescriptorSets.push_back(VkWriteDescriptorSet{
		 		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, // sType;
		 		NULL,									// pNext;				
		 		descriptorSetMaterial,					// dstSet;
		 		0,										// dstBinding;
		 		0, 										// dstArrayElement;
		 		1,										// descriptorCount;
		 		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,		// descriptorType;   //VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		 		0,										// pImageInfo;
		 		&uniformBuffer.material.descriptor, 	// pBufferInfo;
		 		0										// pTexelBufferView;
		 	});
			 
		 	writeDescriptorSets.push_back(VkWriteDescriptorSet{
		 	  	VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, // sType;
		 	  	NULL,									// pNext;				
		 	  	descriptorSetMaterial,					// dstSet;
		 	  	1,										// dstBinding;
		 	  	0, 										// dstArrayElement;
		 	  	1,										// descriptorCount;
		 	  	VK_DESCRIPTOR_TYPE_SAMPLER,				// descriptorType;
				&materials[0].diffuse.descriptor,		// pImageInfo;
		 	  	0, 										// pBufferInfo;
		 	  	0										// pTexelBufferView;
		 	  	});

			 		 
		 	// concat textures of all materials
		 	for (int i = 0; i < materials.size(); i++)
		 	{  
		 		 writeDescriptorSets.push_back( VkWriteDescriptorSet{
		 			 VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,	// sType;
		 			 NULL,										// pNext;				
		 			 descriptorSetMaterial,						// dstSet;
		 			 2,											// dstBinding;
		 			 static_cast<uint32_t>(i), 					// dstArrayElement;
		 			 1,											// descriptorCount;
					 VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,			// descriptorType; //VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,//VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
					 &materials[i].diffuse.descriptor,			// pImageInfo;
		 			 0, 										// pBufferInfo;
		 			 0											// pTexelBufferView;
		 			});
		 
		 	}

			vkUpdateDescriptorSets(device, writeDescriptorSets.size(), writeDescriptorSets.data(), 0, NULL);
		}

	}

	std::vector<Material> materialBufferData ;
	// Get materials from the assimp scene and map to our scene structures
	void loadMaterials()
	{
		materials.resize(aScene->mNumMaterials);

		// add materials in material buffer
		// add textures to texture array

		 materialBufferData.resize(materials.size());

		for (size_t i = 0; i < materials.size(); i++)
		{
			materials[i] = {};

			aiString name;
			aScene->mMaterials[i]->Get(AI_MATKEY_NAME, name);

			// Properties
			aiColor4D color;
			aScene->mMaterials[i]->Get(AI_MATKEY_COLOR_AMBIENT, color);
			materials[i].properties.ambient = glm::make_vec4(&color.r) + glm::vec4(0.1f);
			aScene->mMaterials[i]->Get(AI_MATKEY_COLOR_DIFFUSE, color);
			materials[i].properties.diffuse = glm::make_vec4(&color.r);
			aScene->mMaterials[i]->Get(AI_MATKEY_COLOR_SPECULAR, color);
			materials[i].properties.specular = glm::make_vec4(&color.r);
			aScene->mMaterials[i]->Get(AI_MATKEY_OPACITY, materials[i].properties.ambient.w);

			if ((materials[i].properties.ambient.w) > 0.0f)
				materials[i].properties.specular = glm::vec4(0.0f);

			materials[i].name = name.C_Str();
			std::cout << "Material \"" << materials[i].name << "\"" << std::endl;

			// Textures
			aiString texturefile;
			// Diffuse
			aScene->mMaterials[i]->GetTexture(aiTextureType_DIFFUSE, 0, &texturefile);
			if (aScene->mMaterials[i]->GetTextureCount(aiTextureType_DIFFUSE) > 0)
			{
				std::cout << "  Diffuse: \"" << texturefile.C_Str() << "\"" << std::endl;
				std::string fileName = std::string(texturefile.C_Str());
				std::replace(fileName.begin(), fileName.end(), '\\', '/');
				textureLoader->loadTexture(assetPath + fileName, VK_FORMAT_BC3_UNORM_BLOCK, &materials[i].diffuse);
			  
	 			materials[i].textureName = std::string(assetPath + fileName);
			}
			else
			{
				std::cout << "  Material has no diffuse, using dummy texture!" << std::endl;
				// todo : separate pipeline and layout
				textureLoader->loadTexture(assetPath + "dummy.ktx", VK_FORMAT_BC2_UNORM_BLOCK, &materials[i].diffuse);

				materials[i].textureName = std::string(assetPath + "dummy.ktx");
			}
			materials[i].diffuse.descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			// For scenes with multiple textures per material we would need to check for additional texture types, e.g.:
			// aiTextureType_HEIGHT, aiTextureType_OPACITY, aiTextureType_SPECULAR, etc.

			// Assign pipeline
			materials[i].pipeline = (materials[i].properties.ambient.w == 0.0f) ? &pipelines.solid : &pipelines.blending;

			materialBufferData[i].ambient = materials[i].properties.ambient;
			materialBufferData[i].diffuse = materials[i].properties.diffuse;
			materialBufferData[i].specular = materials[i].properties.specular; 
			materialBufferData[i].specular.w =	aScene->mMaterials[i]->Get(AI_MATKEY_SHININESS, color);

		}



	}

	// Load all meshes from the scene and generate the Vulkan resources
	// for rendering them
	void loadMeshes(VkCommandBuffer copyCmd)
	{


		std::vector<Vertex> allVertices;
		std::vector<uint32_t> allIndices;

		int firstIndex = 0;
		int vertexOffset = 0;

		meshes.resize(aScene->mNumMeshes);
		for (uint32_t i = 0; i < meshes.size(); i++)
		{
			aiMesh *aMesh = aScene->mMeshes[i];

			std::cout << "Mesh \"" << aMesh->mName.C_Str() << "\"" << std::endl;
			std::cout << "	Material: \"" << materials[aMesh->mMaterialIndex].name << "\"" << std::endl;
			std::cout << "	Faces: " << aMesh->mNumFaces << std::endl;

			meshes[i].material = &materials[aMesh->mMaterialIndex];

			// Vertices
			std::vector<Vertex> vertices;
			vertices.resize(aMesh->mNumVertices);

			bool hasUV = aMesh->HasTextureCoords(0);
			bool hasColor = aMesh->HasVertexColors(0);
			bool hasNormals = aMesh->HasNormals();

			meshes[i].vertexCount = aMesh->mNumVertices;
			for (uint32_t v = 0; v < aMesh->mNumVertices; v++)
			{
				vertices[v].pos = glm::make_vec3(&aMesh->mVertices[v].x);
				vertices[v].pos.y = -vertices[v].pos.y;
				vertices[v].uv = hasUV ? glm::make_vec2(&aMesh->mTextureCoords[0][v].x) : glm::vec2(0.0f);
				vertices[v].normal = hasNormals ? glm::make_vec3(&aMesh->mNormals[v].x) : glm::vec3(0.0f);
				vertices[v].normal.y = -vertices[v].normal.y;
				vertices[v].color = hasColor ? glm::make_vec3(&aMesh->mColors[0][v].r) : glm::vec3(1.0f);

				allVertices.push_back(vertices[v]);
			}

			// Indices
			std::vector<uint32_t> indices;
			meshes[i].indexCount = aMesh->mNumFaces * 3;
			indices.resize(aMesh->mNumFaces * 3);
			for (uint32_t f = 0; f < aMesh->mNumFaces; f++)
			{
				memcpy(&indices[f*3], &aMesh->mFaces[f].mIndices[0], sizeof(uint32_t) * 3);

				allIndices.push_back(indices[f * 3]);
				allIndices.push_back(indices[f * 3+1]);
				allIndices.push_back(indices[f * 3+2]);
			}

			// Create buffers
			// todo : only one memory allocation

			uint32_t vertexDataSize = vertices.size() * sizeof(Vertex);
			uint32_t indexDataSize = indices.size() * sizeof(uint32_t);

			VkMemoryAllocateInfo memAlloc {
				VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
				NULL,
				0,
				0
			}; 
			
			VkMemoryRequirements memReqs;

			struct
			{
				struct {
					VkDeviceMemory memory;
					VkBuffer buffer;
				} vBuffer;
				struct {
					VkDeviceMemory memory;
					VkBuffer buffer;
				} iBuffer;
			} staging;

			// Generate vertex buffer
			VkBufferCreateInfo vBufferInfo;
			void* data;

			// Staging buffer
			vBufferInfo = VkBufferCreateInfo{
				VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
				NULL,
				0,
				vertexDataSize,
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_SHARING_MODE_EXCLUSIVE,
				0,
				NULL
			}; 
			
			VK_CHECK_RESULT(vkCreateBuffer(device, &vBufferInfo, nullptr, &staging.vBuffer.buffer));
			vkGetBufferMemoryRequirements(device, staging.vBuffer.buffer, &memReqs);
			memAlloc.allocationSize = memReqs.size;
			memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
			VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &staging.vBuffer.memory));
			VK_CHECK_RESULT(vkMapMemory(device, staging.vBuffer.memory, 0, VK_WHOLE_SIZE, 0, &data));
			memcpy(data, vertices.data(), vertexDataSize);
			vkUnmapMemory(device, staging.vBuffer.memory);
			VK_CHECK_RESULT(vkBindBufferMemory(device, staging.vBuffer.buffer, staging.vBuffer.memory, 0));

			// Target
			vBufferInfo = VkBufferCreateInfo{
				VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
				NULL,
				0,
				vertexDataSize,
				VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_SHARING_MODE_EXCLUSIVE,
				0,
				NULL
			};

			VK_CHECK_RESULT(vkCreateBuffer(device, &vBufferInfo, nullptr, &meshes[i].vertexBuffer));
			vkGetBufferMemoryRequirements(device, meshes[i].vertexBuffer, &memReqs);
			memAlloc.allocationSize = memReqs.size;
			memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &meshes[i].vertexMemory));
			VK_CHECK_RESULT(vkBindBufferMemory(device, meshes[i].vertexBuffer, meshes[i].vertexMemory, 0));

			// Generate index buffer
			VkBufferCreateInfo iBufferInfo;

			// Staging buffer
			iBufferInfo = VkBufferCreateInfo{
				VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
				NULL,
				0,
				indexDataSize,
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_SHARING_MODE_EXCLUSIVE,
				0,
				NULL
			};
			
			VK_CHECK_RESULT(vkCreateBuffer(device, &iBufferInfo, nullptr, &staging.iBuffer.buffer));
			vkGetBufferMemoryRequirements(device, staging.iBuffer.buffer, &memReqs);
			memAlloc.allocationSize = memReqs.size;
			memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
			VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &staging.iBuffer.memory));
			VK_CHECK_RESULT(vkMapMemory(device, staging.iBuffer.memory, 0, VK_WHOLE_SIZE, 0, &data));
			memcpy(data, indices.data(), indexDataSize);
			vkUnmapMemory(device, staging.iBuffer.memory);
			VK_CHECK_RESULT(vkBindBufferMemory(device, staging.iBuffer.buffer, staging.iBuffer.memory, 0));

			// Target
			iBufferInfo = VkBufferCreateInfo{
				VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
				NULL,
				0,
				indexDataSize,
				VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_SHARING_MODE_EXCLUSIVE,
				0,
				NULL
			};

			VK_CHECK_RESULT(vkCreateBuffer(device, &iBufferInfo, nullptr, &meshes[i].indexBuffer));
			vkGetBufferMemoryRequirements(device, meshes[i].indexBuffer, &memReqs);
			memAlloc.allocationSize = memReqs.size;
			memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &meshes[i].indexMemory));
			VK_CHECK_RESULT(vkBindBufferMemory(device, meshes[i].indexBuffer, meshes[i].indexMemory, 0));

			// Copy
			VkCommandBufferBeginInfo cmdBufInfo{
				VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				NULL,
				0,
				NULL
			};
			VK_CHECK_RESULT(vkBeginCommandBuffer(copyCmd, &cmdBufInfo));

			VkBufferCopy copyRegion = {};

			copyRegion.size = vertexDataSize;
			vkCmdCopyBuffer(
				copyCmd,
				staging.vBuffer.buffer,
				meshes[i].vertexBuffer,
				1,
				&copyRegion);

			copyRegion.size = indexDataSize;
			vkCmdCopyBuffer(
				copyCmd,
				staging.iBuffer.buffer,
				meshes[i].indexBuffer,
				1,
				&copyRegion);

			VK_CHECK_RESULT(vkEndCommandBuffer(copyCmd));

			VkSubmitInfo submitInfo = {};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &copyCmd;

			VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
			VK_CHECK_RESULT(vkQueueWaitIdle(queue));

			vkDestroyBuffer(device, staging.vBuffer.buffer, nullptr);
			vkFreeMemory(device, staging.vBuffer.memory, nullptr);
			vkDestroyBuffer(device, staging.iBuffer.buffer, nullptr);
			vkFreeMemory(device, staging.iBuffer.memory, nullptr);

			 
			indirectDrawCommands.push_back(
				VkDrawIndexedIndirectCommand{
				static_cast<uint32_t>(meshes[i].indexCount),
				1,
				static_cast<uint32_t>(firstIndex),
				static_cast<int32_t>(vertexOffset),
				0
			});

			firstIndex += meshes[i].vertexCount;
			vertexOffset += meshes[i].indexCount;
				  
			 

		}// end for meshes


		void* commandBufferMapped;
		VkMemoryAllocateInfo memAlloc = {
			VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			NULL,
			0,
			0 };
		VkMemoryRequirements memReqs;
		int commandSize = sizeof(VkDrawIndexedIndirectCommand)*indirectDrawCommands.size();
		if(true)
		{ 

			VkBufferCreateInfo bufferCreateInfo {
				VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
					NULL,
					0,
					(VkDeviceSize)commandSize,
					VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT
			};

			
			VK_CHECK_RESULT(vkCreateBuffer(device, &bufferCreateInfo, nullptr, &indirectDrawCommandsBuffer));
			vkGetBufferMemoryRequirements(device, indirectDrawCommandsBuffer, &memReqs);
			memAlloc.allocationSize = memReqs.size;
			memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT| VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &indirectDrawCommandsMemory));

			VK_CHECK_RESULT(vkMapMemory(device, indirectDrawCommandsMemory, 0, commandSize, 0, &commandBufferMapped));
			memcpy(commandBufferMapped, indirectDrawCommands.data(), commandSize);
			vkUnmapMemory(device, indirectDrawCommandsMemory);

			VK_CHECK_RESULT(vkBindBufferMemory(device, indirectDrawCommandsBuffer, indirectDrawCommandsMemory, 0));
			 
		}
		else
		{			
			struct
			{
				struct {
					VkDeviceMemory memory;
					VkBuffer buffer;
				} cBuffer;
			} staging;

			// Generate vertex buffer
			VkBufferCreateInfo cBufferInfo; 
			 
			// Staging buffer
			cBufferInfo = VkBufferCreateInfo{
				VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
				NULL,
				0,
				(VkDeviceSize)commandSize,
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_SHARING_MODE_EXCLUSIVE,
				0,
				NULL
			};


			VK_CHECK_RESULT(vkCreateBuffer(device, &cBufferInfo, nullptr, &staging.cBuffer.buffer));
			vkGetBufferMemoryRequirements(device, staging.cBuffer.buffer, &memReqs);
			memAlloc.allocationSize = memReqs.size;
			memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT );
			VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &staging.cBuffer.memory));
			VK_CHECK_RESULT(vkMapMemory(device, staging.cBuffer.memory, 0, commandSize, 0, &commandBufferMapped));
			memcpy(commandBufferMapped, indirectDrawCommands.data(), commandSize);
			vkUnmapMemory(device, staging.cBuffer.memory);
			VK_CHECK_RESULT(vkBindBufferMemory(device, staging.cBuffer.buffer, staging.cBuffer.memory, 0));
		 
			// Target
			cBufferInfo = VkBufferCreateInfo{
				VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
				NULL,
				0,
				(VkDeviceSize)commandSize,
				VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_SHARING_MODE_EXCLUSIVE,
				0,
				NULL
			};
			 
			
			VK_CHECK_RESULT(vkCreateBuffer(device, &cBufferInfo, nullptr, &indirectDrawCommandsBuffer));
			vkGetBufferMemoryRequirements(device, indirectDrawCommandsBuffer, &memReqs);
			memAlloc.allocationSize = memReqs.size;
			memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &indirectDrawCommandsMemory));
			VK_CHECK_RESULT(vkBindBufferMemory(device, indirectDrawCommandsBuffer, indirectDrawCommandsMemory, 0));
		 

			// Copy
			VkCommandBufferBeginInfo cmdBufInfo{
				VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				NULL,
				0,
				NULL
			};
			VK_CHECK_RESULT(vkBeginCommandBuffer(copyCmd, &cmdBufInfo));

			VkBufferCopy copyRegion = {};

			copyRegion.size = commandSize;
			vkCmdCopyBuffer(
				 copyCmd,
				 staging.cBuffer.buffer,
				 indirectDrawCommandsBuffer,
				 1,
				 &copyRegion);
			 

			VkSubmitInfo submitInfo = {};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &copyCmd;

		 	VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
		 	VK_CHECK_RESULT(vkQueueWaitIdle(queue));

			vkDestroyBuffer(device, staging.cBuffer.buffer, nullptr);
			vkFreeMemory(device, staging.cBuffer.memory, nullptr);

		}



		{
			// Create buffers
			// todo : only one memory allocation


			uint32_t vertexDataSize = allVertices.size() * sizeof(Vertex);
			uint32_t indexDataSize = allIndices.size() * sizeof(uint32_t);

			VkMemoryAllocateInfo memAlloc = {
				VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
				NULL,
				0,
				0 };
			VkMemoryRequirements memReqs;

			struct
			{
				struct {
					VkDeviceMemory memory;
					VkBuffer buffer;
				} vBuffer;
				struct {
					VkDeviceMemory memory;
					VkBuffer buffer;
				} iBuffer;
			} staging;

			// Generate vertex buffer
			VkBufferCreateInfo vBufferInfo;
			void* data;  

			// Staging buffer
			
			vBufferInfo = VkBufferCreateInfo{
				VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
				NULL,
				0,
				vertexDataSize,
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_SHARING_MODE_EXCLUSIVE,
				0,
				NULL
			};


			VK_CHECK_RESULT(vkCreateBuffer(device, &vBufferInfo, nullptr, &staging.vBuffer.buffer));
			vkGetBufferMemoryRequirements(device, staging.vBuffer.buffer, &memReqs);
			memAlloc.allocationSize = memReqs.size;
			memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
			VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &staging.vBuffer.memory));
			VK_CHECK_RESULT(vkMapMemory(device, staging.vBuffer.memory, 0, VK_WHOLE_SIZE, 0, &data));
			memcpy(data, allVertices.data(), vertexDataSize);
			vkUnmapMemory(device, staging.vBuffer.memory);
			VK_CHECK_RESULT(vkBindBufferMemory(device, staging.vBuffer.buffer, staging.vBuffer.memory, 0));

			// Target
			 
			vBufferInfo = VkBufferCreateInfo{
				VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
				NULL,
				0,
				vertexDataSize,
				VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_SHARING_MODE_EXCLUSIVE,
				0,
				NULL
			};
			
			
			VK_CHECK_RESULT(vkCreateBuffer(device, &vBufferInfo, nullptr, &sceneMesh.vertexBuffer));
			vkGetBufferMemoryRequirements(device, sceneMesh.vertexBuffer, &memReqs);
			memAlloc.allocationSize = memReqs.size;
			memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &sceneMesh.vertexMemory));
			VK_CHECK_RESULT(vkBindBufferMemory(device, sceneMesh.vertexBuffer, sceneMesh.vertexMemory, 0));

			// Generate index buffer
			VkBufferCreateInfo iBufferInfo;

			// Staging buffer
			
			iBufferInfo = VkBufferCreateInfo{
				VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
				NULL,
				0,
				indexDataSize,
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_SHARING_MODE_EXCLUSIVE,
				0,
				NULL
			};
			
			
			VK_CHECK_RESULT(vkCreateBuffer(device, &iBufferInfo, nullptr, &staging.iBuffer.buffer));
			vkGetBufferMemoryRequirements(device, staging.iBuffer.buffer, &memReqs);
			memAlloc.allocationSize = memReqs.size;
			memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
			VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &staging.iBuffer.memory));
			VK_CHECK_RESULT(vkMapMemory(device, staging.iBuffer.memory, 0, VK_WHOLE_SIZE, 0, &data));
			memcpy(data, allIndices.data(), indexDataSize);
			vkUnmapMemory(device, staging.iBuffer.memory);
			VK_CHECK_RESULT(vkBindBufferMemory(device, staging.iBuffer.buffer, staging.iBuffer.memory, 0));

			// Target
			
			iBufferInfo = VkBufferCreateInfo{
				VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
				NULL,
				0,
				indexDataSize,
				VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_SHARING_MODE_EXCLUSIVE,
				0,
				NULL
			};
			
			VK_CHECK_RESULT(vkCreateBuffer(device, &iBufferInfo, nullptr, &sceneMesh.indexBuffer));
			vkGetBufferMemoryRequirements(device, sceneMesh.indexBuffer, &memReqs);
			memAlloc.allocationSize = memReqs.size;
			memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &sceneMesh.indexMemory));
			VK_CHECK_RESULT(vkBindBufferMemory(device, sceneMesh.indexBuffer, sceneMesh.indexMemory, 0));

			// Copy
			VkCommandBufferBeginInfo cmdBufInfo{
				VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				NULL,
				0,
				NULL
			};
			VK_CHECK_RESULT(vkBeginCommandBuffer(copyCmd, &cmdBufInfo));

			VkBufferCopy copyRegion = {};

			copyRegion.size = vertexDataSize;
			vkCmdCopyBuffer(
				copyCmd,
				staging.vBuffer.buffer,
				sceneMesh.vertexBuffer,
				1,
				&copyRegion);

			copyRegion.size = indexDataSize;
			vkCmdCopyBuffer(
				copyCmd,
				staging.iBuffer.buffer,
				sceneMesh.indexBuffer,
				1,
				&copyRegion);

			VK_CHECK_RESULT(vkEndCommandBuffer(copyCmd));

			VkSubmitInfo submitInfo = {};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &copyCmd;

			VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
			VK_CHECK_RESULT(vkQueueWaitIdle(queue));

			vkDestroyBuffer(device, staging.vBuffer.buffer, nullptr);
			vkFreeMemory(device, staging.vBuffer.memory, nullptr);
			vkDestroyBuffer(device, staging.iBuffer.buffer, nullptr);
			vkFreeMemory(device, staging.iBuffer.memory, nullptr);


}

	}

public:
#if defined(__ANDROID__)
	AAssetManager* assetManager = nullptr;
#endif

	std::string assetPath = "";

	std::vector<SceneMaterial> materials;
	std::vector<SceneMesh> meshes;

	// Shared ubo containing matrices used by all
	// materials and meshes
	 
	struct {
		vkTools::UniformData scene;
		vkTools::UniformData offscreen;
		vkTools::UniformData material;
	} uniformBuffer;


	struct {
		glm::mat4 projection;
		glm::mat4 view;
		glm::mat4 model;
		glm::vec4 lightPos = glm::vec4(1.25f, 28.35f, 0.0f, 0.0f);
	} uniformDataScene, uniformDataOffscreen;

	// Scene uses multiple pipelines
	struct {
		VkPipeline solid;
		VkPipeline blending;
		VkPipeline wireframe;
		VkPipeline offscreen;
	} pipelines;

	// Shared pipeline layout 
	struct {
		VkPipelineLayout scene;
		VkPipelineLayout offscreen;
	} pipelineLayouts;

	// For displaying only a single part of the scene
	bool renderSingleScenePart = false;
	uint32_t scenePartIndex = 0;

	Scene(VkDevice device, VkQueue queue, VkPhysicalDeviceMemoryProperties memprops, vkTools::VulkanTextureLoader *textureloader)
	{
		this->device = device;
		this->queue = queue;
		this->deviceMemProps = memprops;
		this->textureLoader = textureloader;

		// Prepare uniform buffer for global matrices
		VkMemoryRequirements memReqs;


		VkMemoryAllocateInfo memAlloc = {
			VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			NULL,
			0,
			0 };
		VkBufferCreateInfo bufferCreateInfo = {
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			NULL,
			0,
			sizeof(uniformDataScene),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
		};

		VK_CHECK_RESULT(vkCreateBuffer(device, &bufferCreateInfo, nullptr, &uniformBuffer.scene.buffer));
		vkGetBufferMemoryRequirements(device, uniformBuffer.scene.buffer, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &uniformBuffer.scene.memory));
		VK_CHECK_RESULT(vkBindBufferMemory(device, uniformBuffer.scene.buffer, uniformBuffer.scene.memory, 0));
		VK_CHECK_RESULT(vkMapMemory(device, uniformBuffer.scene.memory, 0, sizeof(uniformDataScene), 0, (void **)&uniformBuffer.scene.mapped));
		uniformBuffer.scene.descriptor.offset = 0;
		uniformBuffer.scene.descriptor.buffer = uniformBuffer.scene.buffer;
		uniformBuffer.scene.descriptor.range = sizeof(uniformDataScene);



		bufferCreateInfo = VkBufferCreateInfo{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			NULL,
			0,
			sizeof(uniformDataOffscreen),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
		}; 
		
		VK_CHECK_RESULT(vkCreateBuffer(device, &bufferCreateInfo, nullptr, &uniformBuffer.offscreen.buffer));
		vkGetBufferMemoryRequirements(device, uniformBuffer.offscreen.buffer, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &uniformBuffer.offscreen.memory));
		VK_CHECK_RESULT(vkBindBufferMemory(device, uniformBuffer.offscreen.buffer, uniformBuffer.offscreen.memory, 0));
		VK_CHECK_RESULT(vkMapMemory(device, uniformBuffer.offscreen.memory, 0, sizeof(uniformDataOffscreen), 0, (void **)&uniformBuffer.offscreen.mapped));
		uniformBuffer.offscreen.descriptor.offset = 0;
		uniformBuffer.offscreen.descriptor.buffer = uniformBuffer.offscreen.buffer;
		uniformBuffer.offscreen.descriptor.range = sizeof(uniformDataOffscreen);

	}

	~Scene()
	{
		for (auto mesh : meshes)
		{
			vkDestroyBuffer(device, mesh.vertexBuffer, nullptr);
			vkFreeMemory(device, mesh.vertexMemory, nullptr);
			vkDestroyBuffer(device, mesh.indexBuffer, nullptr);
			vkFreeMemory(device, mesh.indexMemory, nullptr);
		}
		for (auto material : materials)
		{
			textureLoader->destroyTexture(material.diffuse);
		}
		vkDestroyPipelineLayout(device, pipelineLayouts.scene, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayouts.offscreen, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayouts.Material, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayouts.scene, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayouts.shadow, nullptr);
		vkDestroyDescriptorPool(device, descriptorPool, nullptr);
		vkDestroyPipeline(device, pipelines.solid, nullptr);
		vkDestroyPipeline(device, pipelines.blending, nullptr);
		vkDestroyPipeline(device, pipelines.wireframe, nullptr);
		vkDestroyPipeline(device, pipelines.offscreen, nullptr);
		vkTools::destroyUniformData(device, &uniformBuffer.scene);
		vkTools::destroyUniformData(device, &uniformBuffer.offscreen);
		vkTools::destroyUniformData(device, &uniformBuffer.material);
	}

	void load(std::string filename, VkCommandBuffer copyCmd)
	{
		Assimp::Importer Importer;

		int flags = aiProcess_PreTransformVertices | aiProcess_Triangulate | aiProcess_GenNormals;

#if defined(__ANDROID__)
		AAsset* asset = AAssetManager_open(assetManager, filename.c_str(), AASSET_MODE_STREAMING);
		assert(asset);
		size_t size = AAsset_getLength(asset);
		assert(size > 0);
		void *meshData = malloc(size);
		AAsset_read(asset, meshData, size);
		AAsset_close(asset);
		aScene = Importer.ReadFileFromMemory(meshData, size, flags);
		free(meshData);
#else
		aScene = Importer.ReadFile(filename.c_str(), flags);
#endif
		if (aScene)
		{
			loadMaterials();

			loadMeshes(copyCmd);
		}
		else
		{
			printf("Error parsing '%s': '%s'\n", filename.c_str(), Importer.GetErrorString());
#if defined(__ANDROID__)
			LOGE("Error parsing '%s': '%s'", filename.c_str(), Importer.GetErrorString());
#endif
		}

	}



	// Renders the scene into an active command buffer
	// In a real world application we would do some visibility culling in here
	void render(VkCommandBuffer cmdBuffer, bool wireframe, bool offscreen)
	{


		std::vector<VkDescriptorSet> descriptorSets(3);
		if (!offscreen)
		{
			descriptorSets.resize(3);
			// Set 0: Scene descriptor set containing global matrices
			descriptorSets[0] = descriptorSetScene;
			// Set 1: Per-Material descriptor set containing bound images
			descriptorSets[1] = descriptorSetMaterial;

			//Set 2: Shadow Descriptor
			descriptorSets[2] = descriptorSetShadow;
		}
		else
		{
			descriptorSets.resize(1);
			// Set 0: Scene descriptor set containing global matrices
			descriptorSets[0] = descriptorSetOffscreen;
		}
		vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, offscreen ? pipelines.offscreen : (wireframe ? pipelines.wireframe : pipelines.solid));
		vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, offscreen ? pipelineLayouts.offscreen : pipelineLayouts.scene, 0,
			static_cast<uint32_t>(descriptorSets.size()), descriptorSets.data(), 0, NULL);

		VkDeviceSize offsets[1] = { 0 };

		vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &sceneMesh.vertexBuffer, offsets);
		vkCmdBindIndexBuffer(cmdBuffer, sceneMesh.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

		int firstIndex = 0;
		int vertexOffset = 0;

		bool multiDraw = false;

		if (multiDraw)
		{
			int i = 0;
			if (!offscreen)
			{
				// Pass material properies via push constants
				vkCmdPushConstants(
					cmdBuffer,
					pipelineLayouts.scene,
					VK_SHADER_STAGE_FRAGMENT_BIT,
					0,
					sizeof(int),
					&i);
			}
			vkCmdDrawIndexedIndirect(cmdBuffer,
				indirectDrawCommandsBuffer,
				0,
				indirectDrawCommands.size(),
				sizeof(VkDrawIndexedIndirectCommand)
			);
		} 
		else
		{
			for (int i = 0; i < meshes.size(); i++)
			{
				if ((renderSingleScenePart) && (i != scenePartIndex))
					continue;

				//if (meshes[i].material->opacity == 0.0f)
				//	continue;

				if (!offscreen)
				{
					// Pass material properies via push constants
					vkCmdPushConstants(
						cmdBuffer,
						pipelineLayouts.scene,
						VK_SHADER_STAGE_FRAGMENT_BIT,
						0,
						sizeof(int),
						&i);
				}
				//vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &meshes[i].vertexBuffer, offsets);
				//vkCmdBindIndexBuffer(cmdBuffer, meshes[i].indexBuffer, 0, VK_INDEX_TYPE_UINT32);
				bool drawIndirect = true;
				if (drawIndirect)
				{
					vkCmdDrawIndexedIndirect(cmdBuffer,
						indirectDrawCommandsBuffer,
						sizeof(VkDrawIndexedIndirectCommand)*i,
						1,
						sizeof(VkDrawIndexedIndirectCommand)
						);
				}
				else 
				{
					//vkCmdDrawIndexed(cmdBuffer, meshes[i].indexCount, 1, firstIndex, vertexOffset, 0);
					vkCmdDrawIndexed(cmdBuffer,
						indirectDrawCommands[i].indexCount,
						indirectDrawCommands[i].instanceCount,
						indirectDrawCommands[i].firstIndex,
						indirectDrawCommands[i].vertexOffset,
						indirectDrawCommands[i].firstInstance);

				}
				
				firstIndex += meshes[i].vertexCount;
				vertexOffset += meshes[i].indexCount;


			}
		}
		

		// Render transparent objects last

	}
};

class VulkanExample : public VulkanExampleBase
{
public:
	bool wireframe = false;
	bool attachLight = true;

	bool displayCubeMap = false;

	float zNear = 0.1f;
	float zFar = 1024.0f;

	Scene *scene = nullptr;

	glm::vec4 lightPos = glm::vec4(0.0f, 15.0f, 0.0f, 1.0);


	struct {
		VkPipelineVertexInputStateCreateInfo inputState;
		std::vector<VkVertexInputBindingDescription> bindingDescriptions;
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
	} vertices;


	struct {
		glm::mat4 projection;
		glm::mat4 view;
		glm::mat4 model;
		glm::vec4 lightPos;
	} uboOffscreenVS;


	vkTools::VulkanTexture shadowCubeMap;

	// Framebuffer for offscreen rendering
	struct FrameBufferAttachment {
		VkImage image;
		VkDeviceMemory mem;
		VkImageView view;
	};
	struct FrameBuffer {
		int32_t width, height;
		VkFramebuffer frameBuffer;
		FrameBufferAttachment color, depth;
		VkRenderPass renderPass;
	} offScreenFrameBuf;


	VkCommandBuffer offScreenCmdBuffer = VK_NULL_HANDLE;
	VkFormat fbDepthFormat;

	// Semaphore used to synchronize offscreen rendering before using it's texture target for sampling
	VkSemaphore offscreenSemaphore = VK_NULL_HANDLE;





	VulkanExample() : VulkanExampleBase(ENABLE_VALIDATION)
	{
		rotation = { -20.5f, -673.0f, 0.0f };
		rotationSpeed = 0.5f;
		enableTextOverlay = false;
		camera.type = Camera::CameraType::firstperson;
		camera.movementSpeed = 7.5f;
		camera.position = { 15.0f, -13.5f, 0.0f };
		camera.setRotation(glm::vec3(5.0f, 90.0f, 0.0f));
		camera.setPerspective(60.0f, (float)width / (float)height, 0.1f, 256.0f);
		title = "Vulkan Example - Scene rendering";
	}

	~VulkanExample()
	{

		// Cube map
		vkDestroyImageView(device, shadowCubeMap.view, nullptr);
		vkDestroyImage(device, shadowCubeMap.image, nullptr);
		vkDestroySampler(device, shadowCubeMap.sampler, nullptr);
		vkFreeMemory(device, shadowCubeMap.deviceMemory, nullptr);

		// Frame buffer

		// Color attachment
		vkDestroyImageView(device, offScreenFrameBuf.color.view, nullptr);
		vkDestroyImage(device, offScreenFrameBuf.color.image, nullptr);
		vkFreeMemory(device, offScreenFrameBuf.color.mem, nullptr);

		// Depth attachment
		vkDestroyImageView(device, offScreenFrameBuf.depth.view, nullptr);
		vkDestroyImage(device, offScreenFrameBuf.depth.image, nullptr);
		vkFreeMemory(device, offScreenFrameBuf.depth.mem, nullptr);

		vkDestroyFramebuffer(device, offScreenFrameBuf.frameBuffer, nullptr);

		vkDestroyRenderPass(device, offScreenFrameBuf.renderPass, nullptr);
		 
		// Destroy MSAA target
		vkDestroyImage(device, multisampleTarget.color.image, nullptr);
		vkDestroyImageView(device, multisampleTarget.color.view, nullptr);
		vkFreeMemory(device, multisampleTarget.color.memory, nullptr);
		vkDestroyImage(device, multisampleTarget.depth.image, nullptr);
		vkDestroyImageView(device, multisampleTarget.depth.view, nullptr);
		vkFreeMemory(device, multisampleTarget.depth.memory, nullptr);

		 

		delete(scene);



		vkFreeCommandBuffers(device, cmdPool, 1, &offScreenCmdBuffer);
		vkDestroySemaphore(device, offscreenSemaphore, nullptr);


	}


	void loadTextureArray(  VkFormat format)
	{
#if defined(__ANDROID__)
		// Textures are stored inside the apk on Android (compressed)
		// So they need to be loaded via the asset manager
		AAsset* asset = AAssetManager_open(androidApp->activity->assetManager, filename.c_str(), AASSET_MODE_STREAMING);
		assert(asset);
		size_t size = AAsset_getLength(asset);
		assert(size > 0);

		void *textureData = malloc(size);
		AAsset_read(asset, textureData, size);
		AAsset_close(asset);

		gli::texture2DArray tex2DArray(gli::load((const char*)textureData, size));
#else
	//	gli::texture2DArray tex2DArray(gli::load(filename));
#endif

	//	assert(!tex2DArray.empty());

	// Check if all array layers have the same dimesions

		std::vector<gli::texture2D*> textures(scene->materials.size());

		// Setup buffer copy regions for array layers
		std::vector<VkBufferImageCopy> bufferCopyRegions;
		uint32_t offset = 0;


		// If dimensions differ, copy layer by layer and pass offsets
		for (auto materialID = 0; materialID < scene->materials.size(); materialID++)
		{
			textures[materialID] = new gli::texture2D(gli::load(scene->materials[materialID].textureName));

			VkBufferImageCopy bufferCopyRegion = {};
			bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			bufferCopyRegion.imageSubresource.mipLevel = 0;
			bufferCopyRegion.imageSubresource.baseArrayLayer = materialID;
			bufferCopyRegion.imageSubresource.layerCount = 1;
			bufferCopyRegion.imageExtent.width = textures[materialID]->dimensions().x;
			bufferCopyRegion.imageExtent.height = textures[materialID]->dimensions().y;
			bufferCopyRegion.imageExtent.depth = 1;
			bufferCopyRegion.bufferOffset = offset;

			bufferCopyRegions.push_back(bufferCopyRegion);


			scene->diffuseArray.width	 = std::max((unsigned int)textures[materialID]->dimensions().x, scene->diffuseArray.width);
			scene->diffuseArray.height = std::max((unsigned int)textures[materialID]->dimensions().y, scene->diffuseArray.height);

			offset += textures[materialID]->size();		 
		}

		int bufferSize = offset;
		 
		int singleTextureSize =	scene->diffuseArray.width + scene->diffuseArray.height;
		 

		scene->layerCount = scene->materials.size();// tex2DArray.layers();

		VkMemoryAllocateInfo memAllocInfo = {
			VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			NULL,
			0,
			0 };

		VkMemoryRequirements memReqs;

		// Create a host-visible staging buffer that contains the raw image data
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingMemory;

		VkBufferCreateInfo bufferCreateInfo =  
		VkBufferCreateInfo{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			NULL,
			0,
			scene->diffuseArray.width + scene->diffuseArray.height * scene->materials.size(),
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_SHARING_MODE_EXCLUSIVE
		};



		VK_CHECK_RESULT(vkCreateBuffer(device, &bufferCreateInfo, nullptr, &stagingBuffer));

		// Get memory requirements for the staging buffer (alignment, memory type bits)
		vkGetBufferMemoryRequirements(device, stagingBuffer, &memReqs);

		memAllocInfo.allocationSize = memReqs.size;
		// Get memory type index for a host visible buffer
		memAllocInfo.memoryTypeIndex = getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

		VK_CHECK_RESULT(vkAllocateMemory(device, &memAllocInfo, nullptr, &stagingMemory));
		VK_CHECK_RESULT(vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0));

		// Copy texture data into staging buffer
		uint8_t *data;
		VK_CHECK_RESULT(vkMapMemory(device, stagingMemory, 0, memReqs.size, 0, (void **)&data));

		for (auto materialID = 0; materialID < scene->materials.size(); materialID++)
		{
			textures[materialID] = new gli::texture2D(gli::load(scene->materials[materialID].textureName));
			memcpy(data+ singleTextureSize * materialID, textures[materialID]->data(), textures[materialID]->size());
		}

		vkUnmapMemory(device, stagingMemory);

		
		

		for (auto t : textures)
			delete t;


		// Create optimal tiled target image
		 
		VkImageCreateInfo imageCreateInfo{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			NULL,
			VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
			VK_IMAGE_TYPE_2D,
			format,
			VkExtent3D{ scene->diffuseArray.width, scene->diffuseArray.height, 1 },
			1,
			scene->layerCount,
			VK_SAMPLE_COUNT_1_BIT,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_SHARING_MODE_EXCLUSIVE,
			0,
			NULL,
			VK_IMAGE_LAYOUT_UNDEFINED
		};


		VK_CHECK_RESULT(vkCreateImage(device, &imageCreateInfo, nullptr, &scene->diffuseArray.image));

		vkGetImageMemoryRequirements(device, scene->diffuseArray.image, &memReqs);

		memAllocInfo.allocationSize = memReqs.size;
		memAllocInfo.memoryTypeIndex = getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		VK_CHECK_RESULT(vkAllocateMemory(device, &memAllocInfo, nullptr, &scene->diffuseArray.deviceMemory));
		VK_CHECK_RESULT(vkBindImageMemory(device, scene->diffuseArray.image, scene->diffuseArray.deviceMemory, 0));

		VkCommandBuffer copyCmd = VulkanExampleBase::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		// Image barrier for optimal image (target)
		// Set initial layout for all array layers (faces) of the optimal (target) tiled texture
		 
		VkImageSubresourceRange subresourceRange{
			VK_IMAGE_ASPECT_COLOR_BIT,
			0,
			1,
			0,
			scene->layerCount
		};

		vkTools::setImageLayout(
			copyCmd,
			scene->diffuseArray.image,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			subresourceRange);

		// Copy the cube map faces from the staging buffer to the optimal tiled image
		vkCmdCopyBufferToImage(
			copyCmd,
			stagingBuffer,
			scene->diffuseArray.image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			bufferCopyRegions.size(),
			bufferCopyRegions.data()
			);

		// Change texture image layout to shader read after all faces have been copied
		scene->diffuseArray.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		vkTools::setImageLayout(
			copyCmd,
			scene->diffuseArray.image,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			scene->diffuseArray.imageLayout,
			subresourceRange);

		VulkanExampleBase::flushCommandBuffer(copyCmd, queue, true);

		// Create sampler 
		VkSamplerCreateInfo sampler{
			VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			NULL,
			0,
			VK_FILTER_LINEAR,
			VK_FILTER_LINEAR,
			VK_SAMPLER_MIPMAP_MODE_LINEAR,
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
			0.0f,
			VK_FALSE,
			8,
			VK_FALSE,
			VK_COMPARE_OP_NEVER,
			0.0f,
			1.0f,
			VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
			VK_FALSE
		};


		VK_CHECK_RESULT(vkCreateSampler(device, &sampler, nullptr, &scene->diffuseArray.sampler));
		 
		VkImageViewCreateInfo view{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			NULL,
			0,
			scene->diffuseArray.image,
			VK_IMAGE_VIEW_TYPE_2D_ARRAY,
			format,
			{ VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A }	,
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
		};


		VK_CHECK_RESULT(vkCreateImageView(device, &view, nullptr, &scene->diffuseArray.view));

		// Clean up staging resources
		vkFreeMemory(device, stagingMemory, nullptr);
		vkDestroyBuffer(device, stagingBuffer, nullptr);
	}


	void prepareCubeMap()
	{
		shadowCubeMap.width = TEX_DIM;
		shadowCubeMap.height = TEX_DIM;

		// 32 bit float format for higher precision
		VkFormat format = VK_FORMAT_R32_SFLOAT;

		// Cube map image description
		 
		VkImageCreateInfo imageCreateInfo{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			NULL,
			VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
			VK_IMAGE_TYPE_2D,
			format,
			VkExtent3D{ shadowCubeMap.width, shadowCubeMap.height, 1 },
			1,
			6,
			VK_SAMPLE_COUNT_1_BIT,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_SHARING_MODE_EXCLUSIVE,
			0,
			NULL,
			VK_IMAGE_LAYOUT_UNDEFINED
		};
		VkMemoryAllocateInfo memAllocInfo{
			VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			NULL,
			0,
			0,
		}; 
		
		VkMemoryRequirements memReqs;

		VkCommandBuffer layoutCmd = VulkanExampleBase::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		// Create cube map image
		VK_CHECK_RESULT(vkCreateImage(device, &imageCreateInfo, nullptr, &shadowCubeMap.image));

		vkGetImageMemoryRequirements(device, shadowCubeMap.image, &memReqs);

		memAllocInfo.allocationSize = memReqs.size;
		memAllocInfo.memoryTypeIndex = getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device, &memAllocInfo, nullptr, &shadowCubeMap.deviceMemory));
		VK_CHECK_RESULT(vkBindImageMemory(device, shadowCubeMap.image, shadowCubeMap.deviceMemory, 0));

		// Image barrier for optimal image (target)
		VkImageSubresourceRange subresourceRange{
			VK_IMAGE_ASPECT_COLOR_BIT,
			0,
			1,
			0,
			6
		};

		vkTools::setImageLayout(
			layoutCmd,
			shadowCubeMap.image,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			subresourceRange);

		VulkanExampleBase::flushCommandBuffer(layoutCmd, queue, true);

		// Create sampler
		VkSamplerCreateInfo sampler{
			VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			NULL,
			0,
			TEX_FILTER,
			TEX_FILTER,
			VK_SAMPLER_MIPMAP_MODE_LINEAR,
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
			0.0f,
			false,
			0,
			false,
			VK_COMPARE_OP_NEVER,
			0.0f,
			1.0f,
			VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
			false
		};

		VK_CHECK_RESULT(vkCreateSampler(device, &sampler, nullptr, &shadowCubeMap.sampler));


		// Create image view
		VkImageViewCreateInfo view{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			NULL,
			0,
			shadowCubeMap.image,
			VK_IMAGE_VIEW_TYPE_CUBE,
			format,
			{ VK_COMPONENT_SWIZZLE_R }	,
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6 }
		};



		VK_CHECK_RESULT(vkCreateImageView(device, &view, nullptr, &shadowCubeMap.view));
	}

	// Updates a single cube map face
	// Renders the scene with face's view and does 
	// a copy from framebuffer to cube face
	// Uses push constants for quick update of
	// view matrix for the current cube map face
	void updateCubeFace(uint32_t faceIndex)
	{
		VkClearValue clearValues[2];
		clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
		clearValues[1].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo renderPassBeginInfo{
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			NULL,
			offScreenFrameBuf.renderPass,
			offScreenFrameBuf.frameBuffer,
			{ { 0,0 },{ offScreenFrameBuf.width,offScreenFrameBuf.height } },
			2,
			clearValues
		};

		// Update view matrix via push constant

		glm::mat4 viewMatrix = glm::mat4();
		switch (faceIndex)
		{
		case 0: // POSITIVE_X
			viewMatrix = glm::rotate(viewMatrix, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
			viewMatrix = glm::rotate(viewMatrix, glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
			break;
		case 1:	// NEGATIVE_X
			viewMatrix = glm::rotate(viewMatrix, glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
			viewMatrix = glm::rotate(viewMatrix, glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
			break;
		case 2:	// POSITIVE_Y
			viewMatrix = glm::rotate(viewMatrix, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
			break;
		case 3:	// NEGATIVE_Y
			viewMatrix = glm::rotate(viewMatrix, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
			break;
		case 4:	// POSITIVE_Z
			viewMatrix = glm::rotate(viewMatrix, glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
			break;
		case 5:	// NEGATIVE_Z
			viewMatrix = glm::rotate(viewMatrix, glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f));
			break;
		}

		// Render scene from cube face's point of view
		vkCmdBeginRenderPass(offScreenCmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		// Update shader push constant block
		// Contains current face view matrix
		vkCmdPushConstants(
			offScreenCmdBuffer,
			scene->pipelineLayouts.offscreen,
			VK_SHADER_STAGE_VERTEX_BIT,
			0,
			sizeof(glm::mat4),
			&viewMatrix);
		 
		scene->render(offScreenCmdBuffer, wireframe, true);



		vkCmdEndRenderPass(offScreenCmdBuffer);
		// Make sure color writes to the framebuffer are finished before using it as transfer source
		vkTools::setImageLayout(
			offScreenCmdBuffer,
			offScreenFrameBuf.color.image,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

		// Copy region for transfer from framebuffer to cube face
		VkImageCopy copyRegion = {};

		copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegion.srcSubresource.baseArrayLayer = 0;
		copyRegion.srcSubresource.mipLevel = 0;
		copyRegion.srcSubresource.layerCount = 1;
		copyRegion.srcOffset = { 0, 0, 0 };

		copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegion.dstSubresource.baseArrayLayer = faceIndex;
		copyRegion.dstSubresource.mipLevel = 0;
		copyRegion.dstSubresource.layerCount = 1;
		copyRegion.dstOffset = { 0, 0, 0 };

		copyRegion.extent.width = shadowCubeMap.width;
		copyRegion.extent.height = shadowCubeMap.height;
		copyRegion.extent.depth = 1;

		// Put image copy into command buffer
		vkCmdCopyImage(
			offScreenCmdBuffer,
			offScreenFrameBuf.color.image,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			shadowCubeMap.image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&copyRegion);

		// Transform framebuffer color attachment back 
		vkTools::setImageLayout(
			offScreenCmdBuffer,
			offScreenFrameBuf.color.image,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	}


	// Command buffer for rendering and copying all cube map faces
	void buildOffscreenCommandBuffer()
	{
		if (offScreenCmdBuffer == VK_NULL_HANDLE)
		{
			offScreenCmdBuffer = VulkanExampleBase::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);
		}

		// Create a semaphore used to synchronize offscreen rendering and usage

		VkSemaphoreCreateInfo semaphoreCreateInfo{
			VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			NULL,
			0
		};

		VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &offscreenSemaphore));

		VkCommandBufferBeginInfo cmdBufInfo{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			NULL,
			0,
			NULL
		};
		VK_CHECK_RESULT(vkBeginCommandBuffer(offScreenCmdBuffer, &cmdBufInfo));

		VkViewport viewport{
			0,
			0,
			offScreenFrameBuf.width,
			offScreenFrameBuf.height,
			0,
			1,
		};
		vkCmdSetViewport(offScreenCmdBuffer, 0, 1, &viewport);

		VkRect2D scissor = {
			0,
			0,
			offScreenFrameBuf.width,
			offScreenFrameBuf.height,
		};

		vkCmdSetScissor(offScreenCmdBuffer, 0, 1, &scissor);

		VkImageSubresourceRange subresourceRange{
			VK_IMAGE_ASPECT_COLOR_BIT,
			0,
			1,
			0,
			6
		};

		// Change image layout for all cubemap faces to transfer destination
		vkTools::setImageLayout(
			offScreenCmdBuffer,
			shadowCubeMap.image,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			subresourceRange);

		for (uint32_t face = 0; face < 6; ++face)
		{
			updateCubeFace(face);
		}

		// Change image layout for all cubemap faces to shader read after they have been copied
		vkTools::setImageLayout(
			offScreenCmdBuffer,
			shadowCubeMap.image,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			subresourceRange);

		VK_CHECK_RESULT(vkEndCommandBuffer(offScreenCmdBuffer));
	}

	// Prepare a new framebuffer for offscreen rendering
	// The contents of this framebuffer are then
	// copied to the different cube map faces
	void prepareOffscreenFramebuffer()
	{
		offScreenFrameBuf.width = FB_DIM;
		offScreenFrameBuf.height = FB_DIM;

		VkFormat fbColorFormat = FB_COLOR_FORMAT;

		 
		VkImageCreateInfo imageCreateInfo{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			NULL,
			VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
			VK_IMAGE_TYPE_2D,
			fbColorFormat,
			VkExtent3D{ (unsigned int)offScreenFrameBuf.width, (unsigned int)offScreenFrameBuf.height, 1 },
			1,
			1,
			VK_SAMPLE_COUNT_1_BIT,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
			VK_SHARING_MODE_EXCLUSIVE,
			0,
			NULL,
			VK_IMAGE_LAYOUT_UNDEFINED
		};

		VkMemoryAllocateInfo memAlloc{
			VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			NULL,
			0,
			0
		};

		// Create image view
		VkImageViewCreateInfo colorImageView{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			NULL,
			0,
			NULL,
			VK_IMAGE_VIEW_TYPE_2D,
			fbColorFormat,
			{}	,
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
		};

		VkMemoryRequirements memReqs;

		VK_CHECK_RESULT(vkCreateImage(device, &imageCreateInfo, nullptr, &offScreenFrameBuf.color.image));
		vkGetImageMemoryRequirements(device, offScreenFrameBuf.color.image, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &offScreenFrameBuf.color.mem));
		VK_CHECK_RESULT(vkBindImageMemory(device, offScreenFrameBuf.color.image, offScreenFrameBuf.color.mem, 0));

		VkCommandBuffer layoutCmd = VulkanExampleBase::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		vkTools::setImageLayout(
			layoutCmd,
			offScreenFrameBuf.color.image,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

		colorImageView.image = offScreenFrameBuf.color.image;
		VK_CHECK_RESULT(vkCreateImageView(device, &colorImageView, nullptr, &offScreenFrameBuf.color.view));

		// Depth stencil attachment
		imageCreateInfo.format = fbDepthFormat;
		imageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
 
		VkImageViewCreateInfo depthStencilView{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			NULL,
			0,
			NULL,
			VK_IMAGE_VIEW_TYPE_2D,
			fbDepthFormat,
			{}	,
			{ VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1 }
		};

		VK_CHECK_RESULT(vkCreateImage(device, &imageCreateInfo, nullptr, &offScreenFrameBuf.depth.image));
		vkGetImageMemoryRequirements(device, offScreenFrameBuf.depth.image, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &offScreenFrameBuf.depth.mem));
		VK_CHECK_RESULT(vkBindImageMemory(device, offScreenFrameBuf.depth.image, offScreenFrameBuf.depth.mem, 0));

		vkTools::setImageLayout(
			layoutCmd,
			offScreenFrameBuf.depth.image,
			VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

		VulkanExampleBase::flushCommandBuffer(layoutCmd, queue, true);

		depthStencilView.image = offScreenFrameBuf.depth.image;
		VK_CHECK_RESULT(vkCreateImageView(device, &depthStencilView, nullptr, &offScreenFrameBuf.depth.view));

		VkImageView attachments[2];
		attachments[0] = offScreenFrameBuf.color.view;
		attachments[1] = offScreenFrameBuf.depth.view;
 
		VkFramebufferCreateInfo fbufCreateInfo{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			NULL,
			0,
			offScreenFrameBuf.renderPass,
			2,
			attachments,
			offScreenFrameBuf.width,
			offScreenFrameBuf.height,
			1
		};

		VK_CHECK_RESULT(vkCreateFramebuffer(device, &fbufCreateInfo, nullptr, &offScreenFrameBuf.frameBuffer));
	}

	// Set up a separate render pass for the offscreen frame buffer
	// This is necessary as the offscreen frame buffer attachments
	// use formats different to the ones from the visible frame buffer
	// and at least the depth one may not be compatible
	void prepareOffscreenRenderpass()
	{
		VkAttachmentDescription osAttachments[2] = {};

		// Find a suitable depth format
		VkBool32 validDepthFormat = vkTools::getSupportedDepthFormat(physicalDevice, &fbDepthFormat);
		assert(validDepthFormat);

		osAttachments[0].format = FB_COLOR_FORMAT;
		osAttachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		osAttachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		osAttachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		osAttachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		osAttachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		osAttachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		osAttachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		// Depth attachment
		osAttachments[1].format = fbDepthFormat;
		osAttachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
		osAttachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		osAttachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		osAttachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		osAttachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		osAttachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		osAttachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference colorReference = {};
		colorReference.attachment = 0;
		colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depthReference = {};
		depthReference.attachment = 1;
		depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorReference;
		subpass.pDepthStencilAttachment = &depthReference;

	 

		VkRenderPassCreateInfo renderPassCreateInfo{
			VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			NULL,
			0,
			2,
			osAttachments,
			1,
			&subpass,
			0,
			NULL
		};


		VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &offScreenFrameBuf.renderPass));
	}
	 
#if 1
	// Creates a multi sample render target (image and view) that is used to resolve 
	// into the visible frame buffer target in the render pass
	void setupMultisampleTarget()
	{
		// Check if device supports requested sample count for color and depth frame buffer
		assert((deviceProperties.limits.framebufferColorSampleCounts >= SAMPLE_COUNT) && (deviceProperties.limits.framebufferDepthSampleCounts >= SAMPLE_COUNT));

		// Color target
		VkImageCreateInfo info = vkTools::initializers::imageCreateInfo();
		info.imageType = VK_IMAGE_TYPE_2D;
		info.format = colorformat;
		info.extent.width = width;
		info.extent.height = height;
		info.extent.depth = 1;
		info.mipLevels = 1;
		info.arrayLayers = 1;
		info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		info.tiling = VK_IMAGE_TILING_OPTIMAL;
		info.samples = SAMPLE_COUNT;
		// Image will only be used as a transient target
		info.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VK_CHECK_RESULT(vkCreateImage(device, &info, nullptr, &multisampleTarget.color.image));

		VkMemoryRequirements memReqs;
		vkGetImageMemoryRequirements(device, multisampleTarget.color.image, &memReqs);
		VkMemoryAllocateInfo memAlloc = vkTools::initializers::memoryAllocateInfo();
		memAlloc.allocationSize = memReqs.size;
		// We prefer a lazily allocated memory type
		// This means that the memory gets allocated when the implementation sees fit, e.g. when first using the images
		VkBool32 lazyMemType = getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT, &memAlloc.memoryTypeIndex);
		if (!lazyMemType)
		{
			// If this is not available, fall back to device local memory
			getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memAlloc.memoryTypeIndex);
		}
		VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &multisampleTarget.color.memory));
		vkBindImageMemory(device, multisampleTarget.color.image, multisampleTarget.color.memory, 0);

		// Create image view for the MSAA target
		VkImageViewCreateInfo viewInfo = vkTools::initializers::imageViewCreateInfo();
		viewInfo.image = multisampleTarget.color.image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = colorformat;
		viewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
		viewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
		viewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
		viewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.layerCount = 1;

		VK_CHECK_RESULT(vkCreateImageView(device, &viewInfo, nullptr, &multisampleTarget.color.view));

		// Depth target
		info.imageType = VK_IMAGE_TYPE_2D;
		info.format = depthFormat;
		info.extent.width = width;
		info.extent.height = height;
		info.extent.depth = 1;
		info.mipLevels = 1;
		info.arrayLayers = 1;
		info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		info.tiling = VK_IMAGE_TILING_OPTIMAL;
		info.samples = SAMPLE_COUNT;
		// Image will only be used as a transient target
		info.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VK_CHECK_RESULT(vkCreateImage(device, &info, nullptr, &multisampleTarget.depth.image));

		vkGetImageMemoryRequirements(device, multisampleTarget.depth.image, &memReqs);
		memAlloc = vkTools::initializers::memoryAllocateInfo();
		memAlloc.allocationSize = memReqs.size;
		lazyMemType = getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT, &memAlloc.memoryTypeIndex);
		if (!lazyMemType)
		{
			getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memAlloc.memoryTypeIndex);
		}

		VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &multisampleTarget.depth.memory));
		vkBindImageMemory(device, multisampleTarget.depth.image, multisampleTarget.depth.memory, 0);

		// Create image view for the MSAA target
		viewInfo.image = multisampleTarget.depth.image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = depthFormat;
		viewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
		viewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
		viewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
		viewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.layerCount = 1;

		VK_CHECK_RESULT(vkCreateImageView(device, &viewInfo, nullptr, &multisampleTarget.depth.view));
	}

	// Setup a render pass for using a multi sampled attachment 
	// and a resolve attachment that the msaa image is resolved 
	// to at the end of the render pass
	void setupRenderPass()
	{
		// Overrides the virtual function of the base class

		std::array<VkAttachmentDescription, 4> attachments = {};

		// Multisampled attachment that we render to
		attachments[0].format = colorformat;
		attachments[0].samples = SAMPLE_COUNT;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		// No longer required after resolve, this may save some bandwidth on certain GPUs
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		// This is the frame buffer attachment to where the multisampled image
		// will be resolved to and which will be presented to the swapchain
		attachments[1].format = colorformat;
		attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		attachments[1].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		// Multisampled depth attachment we render to
		attachments[2].format = depthFormat;
		attachments[2].samples = SAMPLE_COUNT;
		attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[2].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		attachments[2].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		// Depth resolve attachment
		attachments[3].format = depthFormat;
		attachments[3].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[3].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[3].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[3].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[3].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[3].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		attachments[3].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference colorReference = {};
		colorReference.attachment = 0;
		colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depthReference = {};
		depthReference.attachment = 2;
		depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		// Two resolve attachment references for color and depth
		std::array<VkAttachmentReference, 2> resolveReferences = {};
		resolveReferences[0].attachment = 1;
		resolveReferences[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		resolveReferences[1].attachment = 3;
		resolveReferences[1].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorReference;
		// Pass our resolve attachments to the sub pass
		subpass.pResolveAttachments = resolveReferences.data();
		subpass.pDepthStencilAttachment = &depthReference;

		VkRenderPassCreateInfo renderPassInfo = vkTools::initializers::renderPassCreateInfo();
		renderPassInfo.attachmentCount = attachments.size();
		renderPassInfo.pAttachments = attachments.data();
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;

		VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass));
	}


	// Frame buffer attachments must match with render pass setup, 
	// so we need to adjust frame buffer creation to cover our 
	// multisample target
	void setupFrameBuffer()
	{
		// Overrides the virtual function of the base class

		std::array<VkImageView, 4> attachments;

		setupMultisampleTarget();

		attachments[0] = multisampleTarget.color.view;
		// attachment[1] = swapchain image
		attachments[2] = multisampleTarget.depth.view;
		attachments[3] = depthStencil.view;

		VkFramebufferCreateInfo frameBufferCreateInfo = {};
		frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		frameBufferCreateInfo.pNext = NULL;
		frameBufferCreateInfo.renderPass = renderPass;
		frameBufferCreateInfo.attachmentCount = attachments.size();
		frameBufferCreateInfo.pAttachments = attachments.data();
		frameBufferCreateInfo.width = width;
		frameBufferCreateInfo.height = height;
		frameBufferCreateInfo.layers = 1;

		// Create frame buffers for every swap chain image
		frameBuffers.resize(swapChain.imageCount);
		for (uint32_t i = 0; i < frameBuffers.size(); i++)
		{
			attachments[1] = swapChain.buffers[i].view;
			VK_CHECK_RESULT(vkCreateFramebuffer(device, &frameBufferCreateInfo, nullptr, &frameBuffers[i]));
		}
	}

#endif

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

		// Initial image layout transitions
		// We need to transform the MSAA target layouts before using them

		createSetupCommandBuffer();
		
		// Tansform MSAA color target
		vkTools::setImageLayout(
			setupCmdBuffer,
			multisampleTarget.color.image,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		
		// Tansform MSAA depth target
		vkTools::setImageLayout(
			setupCmdBuffer,
			multisampleTarget.depth.image,
			VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
		
		flushSetupCommandBuffer();


		VkCommandBufferBeginInfo cmdBufInfo{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			NULL,
			0,
			NULL
		}; 

		VkClearValue clearValues[3];
		// Clear to a white background for higher contrast
		clearValues[0].color = { { 1.0f, 1.0f, 1.0f, 1.0f } };
		clearValues[1].color = { { 1.0f, 1.0f, 1.0f, 1.0f } };
		clearValues[2].depthStencil = { 1.0f, 0 };


		VkRenderPassBeginInfo renderPassBeginInfo{
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			NULL,
			renderPass,
			NULL,
			{ { 0,0 },{ width, height } },
			3,
			clearValues
		};

		for (int32_t i = 0; i < drawCmdBuffers.size(); ++i)
		{
			renderPassBeginInfo.framebuffer = frameBuffers[i];

			VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));

			vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			 
			VkViewport viewport{
				0,
				0,
				 width,
				 height,
				0,
				1,
			}; 
			vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

			VkRect2D scissor = { { 0,0 },{ width , height } };
			vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

		
			scene->render(drawCmdBuffers[i], wireframe, false);

			vkCmdEndRenderPass(drawCmdBuffers[i]);

			VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
		}
	}

	void setupVertexDescriptions()
	{ 
 
		 
 

		 
	// Binding description
		vertices.bindingDescriptions.push_back(
			VkVertexInputBindingDescription{
			VERTEX_BUFFER_BIND_ID,
			sizeof(Vertex),
			VK_VERTEX_INPUT_RATE_VERTEX
		});
		   

		// Attribute descriptions
		// Describes memory layout and shader positions

		// Location 0 : Position
		vertices.attributeDescriptions.push_back(
			VkVertexInputAttributeDescription{
			0,
			VERTEX_BUFFER_BIND_ID,
			VK_FORMAT_R32G32B32_SFLOAT,
			offsetof(Vertex, Vertex::pos) });//

		// Location 1 : Normal
		vertices.attributeDescriptions.push_back(
			VkVertexInputAttributeDescription{
			1,
			VERTEX_BUFFER_BIND_ID,
			VK_FORMAT_R32G32B32_SFLOAT,
			offsetof(Vertex,Vertex::normal) }); //

		// Location 2 : Texture coordinates
		vertices.attributeDescriptions.push_back(
			VkVertexInputAttributeDescription{
			2,
			VERTEX_BUFFER_BIND_ID,
			VK_FORMAT_R32G32_SFLOAT,
			offsetof(Vertex,Vertex::uv) });//

		// Location 3 : Color
		vertices.attributeDescriptions.push_back(
			VkVertexInputAttributeDescription{
			3,
			VERTEX_BUFFER_BIND_ID,
			VK_FORMAT_R32G32B32_SFLOAT,
			offsetof(Vertex,Vertex::color) });//

	//	// Location 4 : Divisor
	//	vertices.attributeDescriptions.push_back(
	//		VkVertexInputAttributeDescription{
	//		4,
	//		VERTEX_DIVISOR,
	//		VK_FORMAT_R32_UINT,
	//		sizeof(float) * 11 });

		vertices.inputState =
			VkPipelineVertexInputStateCreateInfo{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			NULL,
			0,
			static_cast<uint32_t>(vertices.bindingDescriptions.size()),
			vertices.bindingDescriptions.data(),
			static_cast<uint32_t>(vertices.attributeDescriptions.size()),
			vertices.attributeDescriptions.data()
		};

		 


	}


	void preparePipelines()
	{
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState{
			VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			NULL,
			0,
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			VK_FALSE
		};
		VkPipelineRasterizationStateCreateInfo rasterizationState{
			VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			NULL,
			0,
			VK_TRUE,
			VK_FALSE,
			VK_POLYGON_MODE_FILL,
			VK_CULL_MODE_BACK_BIT,
			VK_FRONT_FACE_COUNTER_CLOCKWISE,
			VK_FALSE,
			0,
			0,
			0,
			1
		};

		VkPipelineColorBlendAttachmentState blendAttachmentState{
			VK_FALSE,
			VK_BLEND_FACTOR_ZERO,
			VK_BLEND_FACTOR_ZERO,
			VK_BLEND_OP_ADD,
			VK_BLEND_FACTOR_ZERO,
			VK_BLEND_FACTOR_ZERO,
			VK_BLEND_OP_ADD,
			0xf
		};


		VkPipelineColorBlendStateCreateInfo colorBlendState{
			VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			NULL,
			0,
			VK_FALSE,
			VK_LOGIC_OP_CLEAR,
			1,
			&blendAttachmentState,
			{ 0,0,0,0 }
		};


		VkStencilOpState front;
		VkStencilOpState back;
		back.compareOp = VK_COMPARE_OP_ALWAYS;

		VkPipelineDepthStencilStateCreateInfo depthStencilState{
			VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			NULL,
			0,
			VK_TRUE,
			VK_TRUE,
			VK_COMPARE_OP_LESS_OR_EQUAL,
			VK_FALSE,
			VK_FALSE,
			front,
			back,
			0,
			0
		};

		VkPipelineViewportStateCreateInfo viewportState{
			VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			NULL,
			0,
			1,
			NULL,
			1,
			NULL
		};

		VkPipelineMultisampleStateCreateInfo multisampleState{
			VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			NULL,
			0,
			SAMPLE_COUNT,
			VK_FALSE,
			0,
			NULL,
			VK_FALSE,
			VK_FALSE
		};


		std::vector<VkDynamicState> dynamicStateEnables = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};
		VkPipelineDynamicStateCreateInfo dynamicState{
			VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			NULL,
			0,
			dynamicStateEnables.size(),
			dynamicStateEnables.data()
		};

		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

		// Solid rendering pipeline
		shaderStages[0] = loadShader(getAssetPath() + "shaders/sceneRenderingIndirectOmniShadow/scene.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getAssetPath() + "shaders/sceneRenderingIndirectOmniShadow/scene.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		//	shaderStages[0] = loadShader(getAssetPath() + "shaders/shadowmapomni/scene.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		//	shaderStages[1] = loadShader(getAssetPath() + "shaders/shadowmapomni/scene.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

		VkGraphicsPipelineCreateInfo  pipelineCreateInfo{
			VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			NULL,
			0,
			shaderStages.size(),
			shaderStages.data(),
			&vertices.inputState,
			&inputAssemblyState,
			NULL,
			&viewportState,
			&rasterizationState,
			&multisampleState,
			&depthStencilState,
			&colorBlendState,
			&dynamicState,
			scene->pipelineLayouts.scene,
			renderPass,
			0,
			NULL,
			0
		};

		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &scene->pipelines.solid));

		// Alpha blended pipeline
		rasterizationState.cullMode = VK_CULL_MODE_NONE;
		blendAttachmentState.blendEnable = VK_TRUE;
		blendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
		blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR;
		blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;

		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &scene->pipelines.blending));

		// Wire frame rendering pipeline
		rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
		blendAttachmentState.blendEnable = VK_FALSE;
		rasterizationState.polygonMode = VK_POLYGON_MODE_LINE;
		rasterizationState.lineWidth = 1.0f;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &scene->pipelines.wireframe));



		// Offscreen pipeline
		shaderStages[0] = loadShader(getAssetPath() + "shaders/sceneRenderingIndirectOmniShadow/offscreen.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getAssetPath() + "shaders/sceneRenderingIndirectOmniShadow/offscreen.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
		pipelineCreateInfo.layout = scene->pipelineLayouts.offscreen;
		pipelineCreateInfo.renderPass = offScreenFrameBuf.renderPass;


		rasterizationState.cullMode = VK_CULL_MODE_NONE;
		blendAttachmentState.blendEnable = VK_FALSE;
		rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;

		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &scene->pipelines.offscreen));

	}
 
	void updateUniformBufferOffscreen()
	{
	//scene->uniformData.lightPos.x = sin(glm::radians(timer * 360.0f)) * 1.0f;
	//scene->uniformData.lightPos.z = cos(glm::radians(timer * 360.0f)) * 1.0f;
	//
	//uboOffscreenVS.projection 
	//
	//uboOffscreenVS.view = glm::mat4();
	//uboOffscreenVS.model = glm::translate(glm::mat4(), glm::vec3(-scene->uniformData.lightPos));
	//
	//uboOffscreenVS.lightPos = scene->uniformData.lightPos;
	//
	//uint8_t *pData;
	//VK_CHECK_RESULT(vkMapMemory(device, uniformData.offscreen.memory, 0, sizeof(uboOffscreenVS), 0, (void **)&pData));
	//memcpy(pData, &uboOffscreenVS, sizeof(uboOffscreenVS));
	//vkUnmapMemory(device, uniformData.offscreen.memory);


		lightPos.x = sin(glm::radians(timer * 360.0f)) * 1.0f;
		lightPos.z = cos(glm::radians(timer * 360.0f)) * 1.0f;

		scene->uniformDataOffscreen.lightPos = lightPos;
		if (attachLight)
		{
			scene->uniformDataOffscreen.lightPos = glm::vec4(-camera.position + glm::vec3(1,0,0), 1.0f);
		}

		scene->uniformDataOffscreen.projection = glm::perspective((float)(M_PI / 2.0), 1.0f, zNear, zFar);
		scene->uniformDataOffscreen.view = glm::mat4();
		scene->uniformDataOffscreen.model = glm::translate(glm::mat4(), glm::vec3(-scene->uniformDataOffscreen.lightPos));


	 	memcpy(scene->uniformBuffer.offscreen.mapped, &scene->uniformDataOffscreen, sizeof(scene->uniformDataOffscreen));

	}

	void updateUniformBuffers()
	{
		scene->uniformDataScene.lightPos = lightPos;
		if (attachLight)
		{
			scene->uniformDataScene.lightPos = glm::vec4(-camera.position + glm::vec3(1, 0, 0), 1.0f);
		}

		scene->uniformDataScene.projection = camera.matrices.perspective;
		scene->uniformDataScene.view = camera.matrices.view;
		scene->uniformDataScene.model = glm::mat4();

		memcpy(scene->uniformBuffer.scene.mapped, &scene->uniformDataScene, sizeof(scene->uniformDataScene));
	} 



	void draw()
	{
		VulkanExampleBase::prepareFrame();

		// The scene render command buffer has to wait for the offscreen
		// rendering (and transfer) to be finished before using
		// the shadow map, so we need to synchronize
		// We use an additional semaphore for this

		// Offscreen rendering

		// Wait for swap chain presentation to finish
 	   submitInfo.pWaitSemaphores = &semaphores.presentComplete;
 	//// Signal ready with offscreen semaphore
 	   submitInfo.pSignalSemaphores = &offscreenSemaphore;
 	//
 	//// Submit work
 	 submitInfo.commandBufferCount = 1;
 	 submitInfo.pCommandBuffers = &offScreenCmdBuffer;
 	 	VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
 
 
 
 		// Scene rendering
 
 	//	// Wait for offscreen semaphore
 	   	submitInfo.pWaitSemaphores = &offscreenSemaphore;
 	//	// Signal ready with render complete semaphpre
 	   	submitInfo.pSignalSemaphores = &semaphores.renderComplete;

		submitInfo.commandBufferCount = 1;
		// Submit work
	 submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];
	 VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));

		VulkanExampleBase::submitFrame();
	}

	void loadScene()
	{
		VkCommandBuffer copyCmd = VulkanExampleBase::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);
		scene = new Scene(device, queue, deviceMemoryProperties, textureLoader);

#if defined(__ANDROID__)
		scene->assetManager = androidApp->activity->assetManager;
#endif
		scene->assetPath = getAssetPath() + "models/sibenik/";
		scene->load(getAssetPath() + "models/sibenik/sibenik.dae", copyCmd);

		//loadTextureArray(VK_FORMAT_BC3_UNORM_BLOCK);
		scene->uploadMaterials(copyCmd);
		vkFreeCommandBuffers(device, cmdPool, 1, &copyCmd);



		updateUniformBuffers();
	}



	void setupDescriptor_Pool_Layout_AND_Sets()
	{ 


		//////////////////////////////////////////////////////////////

		//////////////////////////////////////////////////////////////

		// Descriptor set
		VkDescriptorSetAllocateInfo allocInfo{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			NULL,
			scene->descriptorPool,
			1,
			&scene->descriptorSetLayouts.shadow
		};

		// 3D scene
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &scene->descriptorSetShadow));

		// Image descriptor for the cube map 

		VkDescriptorImageInfo  shadowTexDescriptor{
			shadowCubeMap.sampler,
			shadowCubeMap.view,
			VK_IMAGE_LAYOUT_GENERAL
		};

		std::vector<VkWriteDescriptorSet> shadowDescriptorSets =
		{		 

			// Binding 1 : Fragment shader shadow sampler	
			VkWriteDescriptorSet {
				VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				NULL,
				scene->descriptorSetShadow,
				0,
				0,
				1,
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				&shadowTexDescriptor,
				NULL,
				NULL,
			}
 
		};
		vkUpdateDescriptorSets(device, shadowDescriptorSets.size(), shadowDescriptorSets.data(), 0, NULL);
 




	}



	void prepare()
	{
		VulkanExampleBase::prepare();
		setupVertexDescriptions();
		loadScene();

		setupVertexDescriptions(); 
		prepareCubeMap();
		
		setupDescriptor_Pool_Layout_AND_Sets();
		prepareOffscreenRenderpass();


		prepareOffscreenFramebuffer();

		preparePipelines();
		buildCommandBuffers();

		buildOffscreenCommandBuffer();
 

		prepared = true;
	}

	virtual void render()
	{
		if (!prepared)
			return;
		draw();

		updateUniformBuffers();
		updateUniformBufferOffscreen();
	}

	virtual void viewChanged()
	{
		updateUniformBuffers();
	}

	virtual void keyPressed(uint32_t keyCode)
	{
		std::cout << "keyCode. " << keyCode << std::endl;
		switch (keyCode)
		{
		case 0x20:
		case GAMEPAD_BUTTON_A:
			wireframe = !wireframe;
			reBuildCommandBuffers();
			break;
		case 0x50:
			scene->renderSingleScenePart = !scene->renderSingleScenePart;
			reBuildCommandBuffers();
			updateTextOverlay();
			break;
		case 0x6B:
			scene->scenePartIndex = (scene->scenePartIndex < static_cast<uint32_t>(scene->meshes.size())) ? scene->scenePartIndex + 1 : 0;
			reBuildCommandBuffers();
			updateTextOverlay();
			break;
		case 0x6D:
			scene->scenePartIndex = (scene->scenePartIndex > 0) ? scene->scenePartIndex - 1 : static_cast<uint32_t>(scene->meshes.size()) - 1;
			updateTextOverlay();
			reBuildCommandBuffers();
			break;
		case 0x46:
			attachLight = !attachLight;
			updateUniformBuffers();
			break;
		}
	}

	virtual void getOverlayText(VulkanTextOverlay *textOverlay)
	{
#if defined(__ANDROID__)
		textOverlay->addText("Press \"Button A\" to toggle wireframe", 5.0f, 85.0f, VulkanTextOverlay::alignLeft);
#else
		textOverlay->addText("Press \"w\" to toggle wireframe", 5.0f, 85.0f, VulkanTextOverlay::alignLeft);
		if ((scene) && (scene->renderSingleScenePart))
		{
			textOverlay->addText("Rendering mesh " + std::to_string(scene->scenePartIndex + 1) + " of " + std::to_string(static_cast<uint32_t>(scene->meshes.size())) + "(\"p\" to toggle)", 5.0f, 100.0f, VulkanTextOverlay::alignLeft);
		}
		else
		{
			textOverlay->addText("Rendering whole scene (\"p\" to toggle)", 5.0f, 100.0f, VulkanTextOverlay::alignLeft);
		}
#endif
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