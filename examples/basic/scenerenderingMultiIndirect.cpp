/*
* Vulkan Example -  Rendering a scene with multiple meshes and materials
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "vulkanexamplebase.h"

#define VERTEX_BUFFER_BIND_ID 0

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
struct SceneMaterialProperites {
	glm::vec4 ambient;
	glm::vec3 diffuse;
	float opacity;
	glm::vec4 specular;
};

// Stores info on the materials used in the scene
struct SceneMaterial {
	std::string name;
	// Material properties
	SceneMaterialProperites properties;
	// The example only uses a diffuse channel
	vkx::Texture diffuse;
	// The material's descriptor contains the material descriptors
	vk::DescriptorSet descriptorSet;
	// Pointer to the pipeline used by this material
	vk::Pipeline *pipeline;
};

// Stores per-mesh Vulkan resources
struct SceneMesh {
	vkx::CreateBufferResult vertices;
	vkx::CreateBufferResult indices;
	uint32_t vertexCount;
	uint32_t indexCount;

	// Pointer to the material used by this mesh
	SceneMaterial *material;
};

// Class for loading the scene and generating all Vulkan resources
class Scene {
private:
	const vkx::Context& context;
	vk::Device device;
	vk::Queue queue;

	vk::DescriptorPool descriptorPool;

	// We will be using separate descriptor sets (and bindings)
	// for material and scene related uniforms
	struct {
		vk::DescriptorSetLayout material;
		vk::DescriptorSetLayout scene;
	} descriptorSetLayouts;

	vk::DescriptorSet descriptorSetScene;
	vk::DescriptorSet descriptorSetMaterial;


	SceneMesh sceneMesh;

	std::vector<vk::DrawIndexedIndirectCommand> indirectDrawCommandsData;
	vkx::CreateBufferResult indirectDrawCommands; 

	vkx::TextureLoader *textureLoader;

	const aiScene* aScene;
	std::vector<SceneMaterialProperites> materialBufferData;

	// Get materials from the assimp scene and map to our scene structures
	void loadMaterials() {

		 materialBufferData.resize(aScene->mNumMaterials);


		materials.resize(aScene->mNumMaterials);

		for (size_t i = 0; i < materials.size(); i++) {
			materials[i] = {};

			aiString name;
			aScene->mMaterials[i]->Get(AI_MATKEY_NAME, name);

			// Properties
			aiColor4D color;
			aScene->mMaterials[i]->Get(AI_MATKEY_COLOR_AMBIENT, color);
			materials[i].properties.ambient = glm::make_vec4(&color.r) + glm::vec4(0.1f);
			aScene->mMaterials[i]->Get(AI_MATKEY_COLOR_DIFFUSE, color);
			materials[i].properties.diffuse = glm::make_vec3(&color.r);
			aScene->mMaterials[i]->Get(AI_MATKEY_COLOR_SPECULAR, color);
			materials[i].properties.specular = glm::make_vec4(&color.r);
			aScene->mMaterials[i]->Get(AI_MATKEY_OPACITY, materials[i].properties.opacity);

			if ((materials[i].properties.opacity) > 0.0f)
				materials[i].properties.specular = glm::vec4(0.0f);

			materials[i].name = name.C_Str();
			std::cout << "Material \"" << materials[i].name << "\"" << std::endl;

			// Textures
			aiString texturefile;
			// Diffuse
			aScene->mMaterials[i]->GetTexture(aiTextureType_DIFFUSE, 0, &texturefile);
			if (aScene->mMaterials[i]->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
				std::cout << "  Diffuse: \"" << texturefile.C_Str() << "\"" << std::endl;
				std::string fileName = std::string(texturefile.C_Str());
				std::replace(fileName.begin(), fileName.end(), '\\', '/');
				materials[i].diffuse = textureLoader->loadTexture(assetPath + fileName, vk::Format::eBc3UnormBlock);
			}
			else {
				std::cout << "  Material has no diffuse, using dummy texture!" << std::endl;
				// todo : separate pipeline and layout
				materials[i].diffuse = textureLoader->loadTexture(assetPath + "dummy.ktx", vk::Format::eBc2UnormBlock);
			}

			// For scenes with multiple textures per material we would need to check for additional texture types, e.g.:
			// aiTextureType_HEIGHT, aiTextureType_OPACITY, aiTextureType_SPECULAR, etc.

			// Assign pipeline
			materials[i].pipeline = (materials[i].properties.opacity == 0.0f) ? &pipelines.solid : &pipelines.blending;
		
			materialBufferData[i].ambient = materials[i].properties.ambient;
			materialBufferData[i].diffuse = materials[i].properties.diffuse;
			materialBufferData[i].specular = materials[i].properties.specular;
			aScene->mMaterials[i]->Get(AI_MATKEY_SHININESS, color);
			materialBufferData[i].specular.w = color.r;
			aScene->mMaterials[i]->Get(AI_MATKEY_OPACITY, color);
			materialBufferData[i].opacity = color.r;

		
		}



		vk::DeviceSize materialSize = materialBufferData.size() * sizeof(SceneMaterialProperites);

		if (true)
		{

			vk::MemoryAllocateInfo memAlloc(
				0,
				0);
			vk::MemoryRequirements memReqs;

			vk::BufferCreateInfo bufferCreateInfo(
				vk::BufferCreateFlags(),
				materialSize,
				vk::BufferUsageFlagBits::eUniformBuffer,
				vk::SharingMode::eExclusive, 
				0, 
				nullptr
			); 
			device.createBuffer(&bufferCreateInfo, nullptr, &uniformBufferMaterial.buffer);
			device.getBufferMemoryRequirements(uniformBufferMaterial.buffer, &memReqs);
			memAlloc.allocationSize = memReqs.size;
			memAlloc.memoryTypeIndex = context.getMemoryType(
				memReqs.memoryTypeBits, 
				vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

			device.allocateMemory(&memAlloc, nullptr, &uniformBufferMaterial.memory);

			uniformBufferMaterial.mapped = device.mapMemory(uniformBufferMaterial.memory, 0, materialSize
				, vk::MemoryMapFlags());
			memcpy(uniformBufferMaterial.mapped, materialBufferData.data(), materialSize);
			device.unmapMemory( uniformBufferMaterial.memory);

			device.bindBufferMemory(uniformBufferMaterial.buffer, uniformBufferMaterial.memory, 0);
			uniformBufferMaterial.descriptor.offset = 0;
			uniformBufferMaterial.descriptor.buffer = uniformBufferMaterial.buffer;
			uniformBufferMaterial.descriptor.range = materialSize;

		}


		// Generate descriptor sets for the materials

		// Descriptor pool
		std::vector<vk::DescriptorPoolSize> poolSizes;
		poolSizes.push_back(vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 1)); //static_cast<uint32_t>(materials.size())
		poolSizes.push_back(vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, static_cast<uint32_t>(materials.size())));

		vk::DescriptorPoolCreateInfo descriptorPoolInfo(vk::DescriptorPoolCreateFlags(),
			static_cast<uint32_t>(materials.size()) + 1,
			static_cast<uint32_t>(poolSizes.size()),
			poolSizes.data());


		descriptorPool = device.createDescriptorPool(descriptorPoolInfo);

		// Descriptor set and pipeline layouts
		std::vector<vk::DescriptorSetLayoutBinding> setLayoutBindings;
		vk::DescriptorSetLayoutCreateInfo descriptorLayout;
		
		
		// Set 0: Scene matrices			
		{		
			setLayoutBindings.push_back(vk::DescriptorSetLayoutBinding(
				0,
				vk::DescriptorType::eUniformBuffer,
				1,
				vk::ShaderStageFlagBits::eVertex, nullptr));

			descriptorLayout = vk::DescriptorSetLayoutCreateInfo(vk::DescriptorSetLayoutCreateFlags(),
				static_cast<uint32_t>(setLayoutBindings.size()),
				setLayoutBindings.data());

			descriptorSetLayouts.scene = device.createDescriptorSetLayout(descriptorLayout);
		}

		// Set 1: Material data
		{
			setLayoutBindings.clear();
			// Set 1: Material data
			setLayoutBindings.push_back(vk::DescriptorSetLayoutBinding(
				0,
				vk::DescriptorType::eUniformBuffer,
				1,
				vk::ShaderStageFlagBits::eFragment,
				nullptr));

			// Set 1: Sampler	
			setLayoutBindings.push_back(vk::DescriptorSetLayoutBinding(
				1,
				vk::DescriptorType::eSampler,
				1,
				vk::ShaderStageFlagBits::eFragment,
				nullptr));

			// Set 1: Texture	
			setLayoutBindings.push_back(vk::DescriptorSetLayoutBinding(
				2,
				vk::DescriptorType::eCombinedImageSampler,
				static_cast<uint32_t>(materials.size()),
				vk::ShaderStageFlagBits::eFragment,
				nullptr));

			descriptorLayout = vk::DescriptorSetLayoutCreateInfo(vk::DescriptorSetLayoutCreateFlags(),
				static_cast<uint32_t>(setLayoutBindings.size()),
				setLayoutBindings.data());


			descriptorSetLayouts.material = device.createDescriptorSetLayout(descriptorLayout);
		}

		// Setup pipeline layout
		{
			std::array<vk::DescriptorSetLayout, 2> setLayouts = { descriptorSetLayouts.scene, descriptorSetLayouts.material };
			vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo = vk::PipelineLayoutCreateInfo(vk::PipelineLayoutCreateFlags(),
				static_cast<uint32_t>(setLayouts.size()),
				setLayouts.data());

			// We will be using a push constant block to pass material properties to the fragment shaders
			vk::PushConstantRange pushConstantRange(
				vk::ShaderStageFlagBits::eFragment,
				sizeof(int),
				0);

			pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
			pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;

			pipelineLayout = device.createPipelineLayout(pipelineLayoutCreateInfo);
		}

		// Material descriptor sets
		{
			vk::DescriptorSetAllocateInfo allocInfo(
					descriptorPool,
					1,
					&descriptorSetLayouts.material);
			descriptorSetScene = device.allocateDescriptorSets(allocInfo)[0];

			std::vector<vk::WriteDescriptorSet> writeDescriptorSets;


			writeDescriptorSets.push_back(vk::WriteDescriptorSet{			
				descriptorSetMaterial,					// dstSet;
				0,										// dstBinding;
				0, 										// dstArrayElement;
				1,										// descriptorCount;
				vk::DescriptorType::eUniformBuffer,		// descriptorType;   //VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				0,										// pImageInfo;
				&uniformBufferMaterial.descriptor,		// pBufferInfo;
				0										// pTexelBufferView;
			});

			writeDescriptorSets.push_back(vk::WriteDescriptorSet{  		
				descriptorSetMaterial,					// dstSet;
				1,										// dstBinding;
				0, 										// dstArrayElement;
				1,										// descriptorCount;
				vk::DescriptorType::eSampler,			// descriptorType;
				&materials[0].diffuse.descriptor,		// pImageInfo;
				0, 										// pBufferInfo;
				0										// pTexelBufferView;
			});



			for (size_t i = 0; i < materials.size(); i++) 
			{
				writeDescriptorSets.push_back(vk::WriteDescriptorSet{
					descriptorSetMaterial,						// dstSet;
					2,											// dstBinding;
					static_cast<uint32_t>(i), 					// dstArrayElement;
					1,											// descriptorCount;
					vk::DescriptorType::eCombinedImageSampler,	// descriptorType; //VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,//VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
					&materials[i].diffuse.descriptor,			// pImageInfo;
					0, 											// pBufferInfo;
					0											// pTexelBufferView;
				});


			}

			device.updateDescriptorSets(writeDescriptorSets, {});
		}

		// Scene descriptor set
		{
			vk::DescriptorSetAllocateInfo allocInfo =
				vk::DescriptorSetAllocateInfo(
					descriptorPool,
					1,
					&descriptorSetLayouts.scene);
			descriptorSetScene = device.allocateDescriptorSets(allocInfo)[0];

			std::vector<vk::WriteDescriptorSet> writeDescriptorSets;
			// Binding 0 : Vertex shader uniform buffer
			writeDescriptorSets.push_back(vk::WriteDescriptorSet(
				descriptorSetScene,
				0,
				0,
				1,
				vk::DescriptorType::eUniformBuffer,
				nullptr,
				&uniformBufferScene.descriptor,
				nullptr));

			device.updateDescriptorSets(writeDescriptorSets, {});
		}

	}

	// Load all meshes from the scene and generate the Vulkan resources
	// for rendering them
	void loadMeshes(vk::CommandBuffer copyCmd) 
	{
		meshes.resize(aScene->mNumMeshes);

		std::vector<Vertex> allVertices;
		std::vector<uint32_t> allIndices;

		int firstIndex = 0;
		int vertexOffset = 0;

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
			for (uint32_t v = 0; v < aMesh->mNumVertices; v++) {
				vertices[v].pos = glm::make_vec3(&aMesh->mVertices[v].x);
				vertices[v].pos.y = -vertices[v].pos.y;
				vertices[v].uv = hasUV ? glm::make_vec2(&aMesh->mTextureCoords[0][v].x) : glm::vec2(0.0f);
				vertices[v].normal = hasNormals ? glm::make_vec3(&aMesh->mNormals[v].x) : glm::vec3(0.0f);
				vertices[v].normal.y = -vertices[v].normal.y;
				vertices[v].color = hasColor ? glm::make_vec3(&aMesh->mColors[0][v].r) : glm::vec3(1.0f);


				allVertices.push_back(vertices[v]);
			}
			meshes[i].vertices = context.stageToDeviceBuffer(vk::BufferUsageFlagBits::eVertexBuffer, vertices);

			// Indices
			std::vector<uint32_t> indices;
			meshes[i].indexCount = aMesh->mNumFaces * 3;
			indices.resize(aMesh->mNumFaces * 3);
			for (uint32_t f = 0; f < aMesh->mNumFaces; f++) {
				memcpy(&indices[f * 3], &aMesh->mFaces[f].mIndices[0], sizeof(uint32_t) * 3);

				allIndices.push_back(indices[f * 3]);
				allIndices.push_back(indices[f * 3 + 1]);
				allIndices.push_back(indices[f * 3 + 2]);
			}
			meshes[i].indices = context.stageToDeviceBuffer(vk::BufferUsageFlagBits::eIndexBuffer, indices);


 
			indirectDrawCommandsData.push_back(
				vk::DrawIndexedIndirectCommand(
				static_cast<uint32_t>(meshes[i].indexCount),
				1,
				static_cast<uint32_t>(firstIndex),
				static_cast<int32_t>(vertexOffset),
				0
			));

			firstIndex += meshes[i].vertexCount;
			vertexOffset += meshes[i].indexCount;

		}// end for meshes


		sceneMesh.vertices = context.stageToDeviceBuffer(vk::BufferUsageFlagBits::eVertexBuffer, allVertices);
		sceneMesh.indices = context.stageToDeviceBuffer(vk::BufferUsageFlagBits::eIndexBuffer, allIndices);

		indirectDrawCommands = context.stageToDeviceBuffer(vk::BufferUsageFlagBits::eIndirectBuffer, indirectDrawCommands);

		  
	 
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
	vkx::UniformData uniformBufferScene;
	struct {
		glm::mat4 projection;
		glm::mat4 view;
		glm::mat4 model;
		glm::vec4 lightPos = glm::vec4(1.25f, 8.35f, 0.0f, 0.0f);
	} uniformData;


	vkx::UniformData uniformBufferMaterial;


	// Scene uses multiple pipelines
	struct {
		vk::Pipeline solid;
		vk::Pipeline blending;
		vk::Pipeline wireframe;
	} pipelines;

	// Shared pipeline layout
	vk::PipelineLayout pipelineLayout;

	// For displaying only a single part of the scene
	bool renderSingleScenePart = false;
	uint32_t scenePartIndex = 0;

	Scene(const vkx::Context& context, vkx::TextureLoader *textureloader) : context(context) {
		this->device = context.device;
		this->queue = context.queue;
		this->textureLoader = textureloader;
		uniformBufferScene = context.createUniformBuffer(uniformData);
	}

	~Scene() {
		for (auto mesh : meshes) {
			mesh.vertices.destroy();
			mesh.indices.destroy();
		}
		for (auto material : materials) {
			material.diffuse.destroy();
		}
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayouts.material, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayouts.scene, nullptr);
		vkDestroyDescriptorPool(device, descriptorPool, nullptr);
		vkDestroyPipeline(device, pipelines.solid, nullptr);
		vkDestroyPipeline(device, pipelines.blending, nullptr);
		vkDestroyPipeline(device, pipelines.wireframe, nullptr);
		uniformBufferScene.destroy();
	}

	void load(std::string filename, vk::CommandBuffer copyCmd) {
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
		if (aScene) {
			loadMaterials();
			loadMeshes(copyCmd);
		}
		else {
			printf("Error parsing '%s': '%s'\n", filename.c_str(), Importer.GetErrorString());
#if defined(__ANDROID__)
			LOGE("Error parsing '%s': '%s'", filename.c_str(), Importer.GetErrorString());
#endif
		}

	}

	// Renders the scene into an active command buffer
	// In a real world application we would do some visibility culling in here
	void render(vk::CommandBuffer cmdBuffer, bool wireframe) {


		std::array<vk::DescriptorSet, 2> descriptorSets;
		// Set 0: Scene descriptor set containing global matrices
		descriptorSets[0] = descriptorSetScene;
		// Set 1: Per-Material descriptor set containing bound images
		descriptorSets[1] = descriptorSetMaterial;

		int firstIndex = 0;
		int vertexOffset = 0;

		bool multiDraw = false;

		if (multiDraw)
		{
			int i = 0;
		
				// Pass material properies via push constants
				
			cmdBuffer.pushConstants(pipelineLayout,
				vk::ShaderStageFlagBits::eFragment,
				0,
				sizeof(int),
				&i);
			cmdBuffer.drawIndexedIndirect(
				indirectDrawCommands.buffer,
				0,
				indirectDrawCommandsData.size(),
				sizeof(vk::DrawIndexedIndirectCommand)
			);
	

		}
		else
		{
			vk::DeviceSize offsets[1] = { 0 };
			for (size_t i = 0; i < meshes.size(); i++) {
				if ((renderSingleScenePart) && (i != scenePartIndex))
					continue;

				//if (meshes[i].material->opacity == 0.0f)
				//	continue;

				// todo : per material pipelines
				//			vkCmdBindPipeline(cmdBuffer, vk::PipelineBindPoint::eGraphics, *mesh.material->pipeline);

				// We will be using multiple descriptor sets for rendering
				// In GLSL the selection is done via the set and binding keywords
				// VS: layout (set = 0, binding = 0) uniform UBO;
				// FS: layout (set = 1, binding = 0) uniform sampler2D samplerColorMap;


				cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, wireframe ? pipelines.wireframe : *meshes[i].material->pipeline);
				cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, descriptorSets, {});

				// Pass material properies via push constants
				vkCmdPushConstants(
					cmdBuffer,
					pipelineLayout,
					VK_SHADER_STAGE_FRAGMENT_BIT,
					0,
					sizeof(SceneMaterialProperites),
					&meshes[i].material->properties);

				cmdBuffer.bindVertexBuffers(0, meshes[i].vertices.buffer, { 0 });
				cmdBuffer.bindIndexBuffer(meshes[i].indices.buffer, 0, vk::IndexType::eUint32);
				cmdBuffer.drawIndexed(meshes[i].indexCount, 1, 0, 0, 0);
			}
		}

		// Render transparent objects last

	}
};

class VulkanExample : public vkx::ExampleBase {
	using Parent = ExampleBase;
public:
	bool wireframe = false;
	bool attachLight = false;

	Scene *scene = nullptr;

	struct {
		vk::PipelineVertexInputStateCreateInfo inputState;
		std::vector<vk::VertexInputBindingDescription> bindingDescriptions;
		std::vector<vk::VertexInputAttributeDescription> attributeDescriptions;
	} vertices;

	VulkanExample() : Parent(ENABLE_VALIDATION) {
		rotationSpeed = 0.5f;
		enableTextOverlay = true;
		camera.type = Camera::CameraType::firstperson;
		camera.movementSpeed = 7.5f;
		camera.setTranslation({ -15.0f, 13.5f, 0.0f });
		camera.setRotation(glm::vec3(5.0f, 90.0f, 0.0f));
		camera.setPerspective(60.0f, size, 0.1f, 256.0f);
		title = "Vulkan Example - Scene rendering";
	}

	~VulkanExample() {
		delete(scene);
	}

	void updateDrawCommandBuffer(const vk::CommandBuffer& cmdBuffer) override {
		cmdBuffer.setViewport(0, vkx::viewport(size));
		cmdBuffer.setScissor(0, vkx::rect2D(size));
		scene->render(cmdBuffer, wireframe);
	}

	void setupVertexDescriptions() {
		// Binding description
		vertices.bindingDescriptions.resize(1);
		vertices.bindingDescriptions[0] =
			vkx::vertexInputBindingDescription(
				VERTEX_BUFFER_BIND_ID,
				sizeof(Vertex),
				vk::VertexInputRate::eVertex);

		// Attribute descriptions
		// Describes memory layout and shader positions
		vertices.attributeDescriptions.resize(4);
		// Location 0 : Position
		vertices.attributeDescriptions[0] =
			vkx::vertexInputAttributeDescription(
				VERTEX_BUFFER_BIND_ID,
				0,
				vk::Format::eR32G32B32Sfloat,
				0);
		// Location 1 : Normal
		vertices.attributeDescriptions[1] =
			vkx::vertexInputAttributeDescription(
				VERTEX_BUFFER_BIND_ID,
				1,
				vk::Format::eR32G32B32Sfloat,
				sizeof(float) * 3);
		// Location 2 : Texture coordinates
		vertices.attributeDescriptions[2] =
			vkx::vertexInputAttributeDescription(
				VERTEX_BUFFER_BIND_ID,
				2,
				vk::Format::eR32G32Sfloat,
				sizeof(float) * 6);
		// Location 3 : Color
		vertices.attributeDescriptions[3] =
			vkx::vertexInputAttributeDescription(
				VERTEX_BUFFER_BIND_ID,
				3,
				vk::Format::eR32G32B32Sfloat,
				sizeof(float) * 8);

		vertices.inputState.vertexBindingDescriptionCount = vertices.bindingDescriptions.size();
		vertices.inputState.pVertexBindingDescriptions = vertices.bindingDescriptions.data();
		vertices.inputState.vertexAttributeDescriptionCount = vertices.attributeDescriptions.size();
		vertices.inputState.pVertexAttributeDescriptions = vertices.attributeDescriptions.data();
	}

	void preparePipelines() {
		vk::PipelineInputAssemblyStateCreateInfo inputAssemblyState =
			vkx::pipelineInputAssemblyStateCreateInfo(vk::PrimitiveTopology::eTriangleList);

		vk::PipelineRasterizationStateCreateInfo rasterizationState =
			vkx::pipelineRasterizationStateCreateInfo(
				vk::PolygonMode::eFill,
				vk::CullModeFlagBits::eBack,
				vk::FrontFace::eCounterClockwise);

		vk::PipelineColorBlendAttachmentState blendAttachmentState =
			vkx::pipelineColorBlendAttachmentState();

		vk::PipelineColorBlendStateCreateInfo colorBlendState =
			vkx::pipelineColorBlendStateCreateInfo(
				1,
				&blendAttachmentState);

		vk::PipelineDepthStencilStateCreateInfo depthStencilState =
			vkx::pipelineDepthStencilStateCreateInfo(
				VK_TRUE,
				VK_TRUE,
				vk::CompareOp::eLessOrEqual);

		vk::PipelineViewportStateCreateInfo viewportState =
			vkx::pipelineViewportStateCreateInfo(1, 1);

		vk::PipelineMultisampleStateCreateInfo multisampleState =
			vkx::pipelineMultisampleStateCreateInfo(vk::SampleCountFlagBits::e1);

		std::vector<vk::DynamicState> dynamicStateEnables = {
			vk::DynamicState::eViewport,
			vk::DynamicState::eScissor
		};
		vk::PipelineDynamicStateCreateInfo dynamicState =
			vkx::pipelineDynamicStateCreateInfo(
				dynamicStateEnables.data(),
				dynamicStateEnables.size());

		std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages;

		// Solid rendering pipeline
		shaderStages[0] = loadShader(getAssetPath() + "shaders/scenerendering/scene.vert.spv", vk::ShaderStageFlagBits::eVertex);
		shaderStages[1] = loadShader(getAssetPath() + "shaders/scenerendering/scene.frag.spv", vk::ShaderStageFlagBits::eFragment);

		vk::GraphicsPipelineCreateInfo pipelineCreateInfo =
			vkx::pipelineCreateInfo(
				scene->pipelineLayout,
				renderPass);

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

		scene->pipelines.solid = device.createGraphicsPipelines(pipelineCache, pipelineCreateInfo)[0];

		// Alpha blended pipeline
		rasterizationState.cullMode = vk::CullModeFlagBits::eNone;
		blendAttachmentState.blendEnable = VK_TRUE;
		blendAttachmentState.colorBlendOp = vk::BlendOp::eAdd;
		blendAttachmentState.srcColorBlendFactor = vk::BlendFactor::eSrcColor;
		blendAttachmentState.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcColor;

		scene->pipelines.blending = device.createGraphicsPipelines(pipelineCache, pipelineCreateInfo)[0];

		// Wire frame rendering pipeline
		rasterizationState.cullMode = vk::CullModeFlagBits::eBack;
		blendAttachmentState.blendEnable = VK_FALSE;
		rasterizationState.polygonMode = vk::PolygonMode::eLine;
		rasterizationState.lineWidth = 1.0f;
		scene->pipelines.wireframe = device.createGraphicsPipelines(pipelineCache, pipelineCreateInfo)[0];
	}

	void updateUniformBuffers() {
		if (attachLight) {
			scene->uniformData.lightPos = glm::vec4(-camera.position, 1.0f);
		}

		scene->uniformData.projection = camera.matrices.perspective;
		scene->uniformData.view = camera.matrices.view;
		scene->uniformData.model = glm::mat4();

		memcpy(scene->uniformBufferScene.mapped, &scene->uniformData, sizeof(scene->uniformData));
	}

	void loadScene() {
		withPrimaryCommandBuffer([&](const vk::CommandBuffer& cmdBuffer) {
			scene = new Scene(*this, textureLoader);
#if defined(__ANDROID__)
			scene->assetManager = androidApp->activity->assetManager;
#endif
			scene->assetPath = getAssetPath() + "models/sibenik/";
			scene->load(getAssetPath() + "models/sibenik/sibenik.dae", cmdBuffer);
		});

		updateUniformBuffers();
	}

	void prepare() {
		Parent::prepare();
		setupVertexDescriptions();
		loadScene();
		preparePipelines();
		updateDrawCommandBuffers();
		prepared = true;
	}

	virtual void render() {
		if (!prepared)
			return;
		draw();


	}

	virtual void viewChanged() {
		updateUniformBuffers();
	}


	void keyPressed(uint32_t keyCode) override {
		Parent::keyPressed(keyCode);
		switch (keyCode) {
		case GLFW_KEY_W:
		case GAMEPAD_BUTTON_A:
			wireframe = !wireframe;
			updateDrawCommandBuffers();
			break;
		case GLFW_KEY_P:
			scene->renderSingleScenePart = !scene->renderSingleScenePart;
			updateDrawCommandBuffers();
			updateTextOverlay();
			break;
		case GLFW_KEY_KP_ADD:
			scene->scenePartIndex = (scene->scenePartIndex < static_cast<uint32_t>(scene->meshes.size())) ? scene->scenePartIndex + 1 : 0;
			updateDrawCommandBuffers();
			updateTextOverlay();
			break;
		case GLFW_KEY_KP_SUBTRACT:
			scene->scenePartIndex = (scene->scenePartIndex > 0) ? scene->scenePartIndex - 1 : static_cast<uint32_t>(scene->meshes.size()) - 1;
			updateTextOverlay();
			updateDrawCommandBuffers();
			break;
		case GLFW_KEY_L:
			attachLight = !attachLight;
			updateUniformBuffers();
			break;

		case GLFW_KEY_UP:
			camera.keys.up = true;
			break;

		case GLFW_KEY_DOWN:
			camera.keys.down = true;
			break;

		case GLFW_KEY_LEFT:
			camera.keys.left = true;
			break;

		case GLFW_KEY_RIGHT:
			camera.keys.right = true;
			break;
		}
	}


	virtual void getOverlayText(vkx::TextOverlay *textOverlay) {
#if defined(__ANDROID__)
		textOverlay->addText("Press \"Button A\" to toggle wireframe", 5.0f, 85.0f, vkx::TextOverlay::alignLeft);
#else
		textOverlay->addText("Press \"w\" to toggle wireframe", 5.0f, 85.0f, vkx::TextOverlay::alignLeft);
		if ((scene) && (scene->renderSingleScenePart)) {
			textOverlay->addText("Rendering mesh " + std::to_string(scene->scenePartIndex + 1) + " of " + std::to_string(static_cast<uint32_t>(scene->meshes.size())) + "(\"p\" to toggle)", 5.0f, 100.0f, vkx::TextOverlay::alignLeft);
		}
		else {
			textOverlay->addText("Rendering whole scene (\"p\" to toggle)", 5.0f, 100.0f, vkx::TextOverlay::alignLeft);
		}
#endif
	}
};

RUN_EXAMPLE(VulkanExample)
