#ifndef RENDERER_H
#define RENDERER_H

#include <QVulkanWindow>
#include <vector>
#include <qelapsedtimer.h>
#include <unordered_map>
#include "Camera.h"
#include "Octree.h"
#include "Triangle.h"
#include "TriangleSurface.h"
#include "VisualObject.h"
#include "Utilities.h"
class Sphere;
class TriangleSurface;

class Renderer : public QVulkanWindowRenderer
{
public:
    Renderer(QVulkanWindow *w, bool msaa = false);

    //Initializes the Vulkan resources needed,
    // the buffers
    // vertex descriptions for the shaders
    // making the shaders, etc
    void initResources() override;

    //Set up resources - only MVP-matrix for now:
    void initSwapChainResources() override;

    //Empty for now - needed since we implement QVulkanWindowRenderer
    void releaseSwapChainResources() override;

    //Release Vulkan resources when program ends
    //Called by Qt
    void releaseResources() override;

    //Render the next frame
    void startNextFrame() override;

    //Get Vulkan info - just for fun
    void getVulkanHWInfo();

    std::vector<VisualObject*>& getObjects() { return mObjects; }
    std::unordered_map<std::string, VisualObject*>& getMap() { return mMap; }

    Octree* mTreeRoot;
    std::vector<Triangle> mTriangles;   // Stores all triangles in the scene

protected:

    //Creates the Vulkan shader module from the precompiled shader files in .spv format
    VkShaderModule createShader(const QString &name);

	void setModelMatrix(QMatrix4x4 modelMatrix, QVector3D color);
    void setViewProjectionMatrix();
    void setTexture(TextureHandle& textureHandle, VkCommandBuffer commandBuffer);

	void setRenderPassParameters(VkCommandBuffer commandBuffer);

    //The ModelViewProjection MVP matrix
    QMatrix4x4 mProjectionMatrix;
    //Rotation angle of the triangle
    float mRotation{ 0.0f };

    //Vulkan resources:
    QVulkanWindow* mWindow{ nullptr };
    QVulkanDeviceFunctions* mDeviceFunctions{ nullptr };
 
    //For Uniform buffers
    VkDescriptorPool mDescriptorPool{ VK_NULL_HANDLE };
    VkDescriptorSetLayout mDescriptorSetLayout{ VK_NULL_HANDLE };
    VkDescriptorSet mDescriptorSet{ VK_NULL_HANDLE }; // [QVulkanWindow::MAX_CONCURRENT_FRAME_COUNT] { VK_NULL_HANDLE };

    //For Textures
    VkDescriptorPool mTextureDescriptorPool{ VK_NULL_HANDLE };
    VkDescriptorSetLayout mTextureDescriptorSetLayout{ VK_NULL_HANDLE };
	VkSampler mTextureSampler{ VK_NULL_HANDLE };
    VkDeviceMemory mBufferMemory{ VK_NULL_HANDLE };
    VkBuffer mBuffer{ VK_NULL_HANDLE };
 
    // VkDescriptorPool mDescriptorPool{ VK_NULL_HANDLE };
    // VkDescriptorSetLayout mDescriptorSetLayout{ VK_NULL_HANDLE };
	//Only need one descriptor set for now:
    //VkDescriptorSet mDescriptorSet{ VK_NULL_HANDLE }; // [QVulkanWindow::MAX_CONCURRENT_FRAME_COUNT] { VK_NULL_HANDLE };

    VkPipelineCache mPipelineCache{ VK_NULL_HANDLE };
    //VkPipelineLayout mPipelineLayout{ VK_NULL_HANDLE };
    //VkPipeline mPipeline1{ VK_NULL_HANDLE };
    VkPipeline mPipeline2{ VK_NULL_HANDLE };

    VkQueue mGraphicsQueue{ VK_NULL_HANDLE };

private:
    friend class VulkanWindow;
    QElapsedTimer mTimer;
    float deltaTime{0.0f};
    std::vector<VisualObject*> mObjects;    //All objects in the program

    // Temporary pointers for easy access
    Sphere* mSphere;
    TriangleSurface* mSurface;

    QVector3D mGravity{0.0, -9.8, 0.0};

    std::unordered_map<std::string, VisualObject*> mMap;    // alternativ container

    void createBuffer(VkDevice logicalDevice,
                      const VkDeviceSize uniAlign, VisualObject* visualObject,
                      VkBufferUsageFlags usage=VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

	//Start of Uniforms and DescriptorSets
	void createVertexBuffer(const VkDeviceSize uniformAlignment, VisualObject* visualObject);
	void createIndexBuffer(const VkDeviceSize uniformAlignment, VisualObject* visualObject);
    void createDescriptorSetLayouts();
    void createUniformBuffer();
	void createDescriptorSet();
	void createDescriptorPool();
    void destroyBuffer(BufferHandle handle);

	void createTextureSampler();
    TextureHandle createTexture(const std::string filename);
	TextureHandle createImage(int width, int height, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkFormat format);
	void transitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout);
	void copyBufferToImage(VkBuffer buffer, VkImage image, int width, int height);
	VkImageView createImageView(VkImage image, VkFormat format);

	void destroyTexture(TextureHandle& textureHandle);

    //Texture variables
    VkSurfaceFormatKHR mSurfaceFormat{};
    TextureHandle mTextureHandle;

	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags requiredProperties);

	BufferHandle createGeneralBuffer(const VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);

    Camera mCamera;
    class VulkanWindow* mVulkanWindow{ nullptr };

	VkCommandBuffer BeginTransientCommandBuffer();
	void EndTransientCommandBuffer(VkCommandBuffer commandBuffer);

    BufferHandle mUniformBuffer{};
    uint8_t* mUniformBufferLocation{ nullptr };

    class Light* mLight{nullptr};

    // Line Color shader
    struct {
        VkShaderModule vertShaderModule;
        VkShaderModule fragShaderModule;
        VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };
        VkPipeline pipeline{ VK_NULL_HANDLE };
    } mLineMaterial;

    // Color shader
    struct {
        VkShaderModule vertShaderModule;
        VkShaderModule fragShaderModule;
        VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
        VkPipeline pipeline{VK_NULL_HANDLE};
    } mColorMaterial;

    // Phong shader
    struct {
        VkDeviceSize vertUniSize;
        VkDeviceSize fragUniSize;
        // VkDeviceSize uniMemStartOffset;
        VkShaderModule vertShaderModule;
        VkShaderModule fragShaderModule;
        VkDescriptorPool descriptorPool{VK_NULL_HANDLE};
        VkDescriptorSetLayout descriptorSetLayout{VK_NULL_HANDLE};
        VkDescriptorSet descriptorSet;
        VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
        VkPipeline pipeline{VK_NULL_HANDLE};
    } mPhongMaterial;
};

#endif // RENDERER_H
