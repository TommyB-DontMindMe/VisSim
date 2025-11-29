#include "Renderer.h"
#include <QVulkanFunctions>
#include <QFile>
#include <fstream>
#include "PointCloud.h"
#include "VulkanWindow.h"
#include "Triangle.h"
#include "stb_image.h"
#include "AABB.h"
#include "Light.h"

/*** Renderer class ***/
Renderer::Renderer(QVulkanWindow *w, bool msaa) : mWindow(w)
{
    if (msaa) {
        const QList<int> counts = w->supportedSampleCounts();
        qDebug("\n\n");
        qDebug() << "Supported sample counts:" << counts;
        for (int s = 16; s >= 4; s /= 2) {
            if (counts.contains(s)) {
                qDebug("Requesting sample count %d", s);
                mWindow->setSampleCount(s);
                break;
            }
        }
        qDebug("\n\n");
    }

    QVector3D boundsMin{ 0, -4.0, 0};
    QVector3D boundsMax{ 5, 4, 5};

    //Since the light is a special object we have only one of
    mLight->setPosition(QVector3D(2.5, 8.0, 2.5));
    mLight->setColor({0.88, 0.7, 0.9});

    mTreeRoot = new Octree(mTriangles, AABB(boundsMin, boundsMax), 0);


    mObjects.push_back(mLight);
    mObjects.push_back(new PointCloud(assetPath + "lasdata.txt", boundsMin, boundsMax, mTriangles));
    mObjects.push_back(new WorldAxis());

    mObjects.at(1)->setColor({0.7, 0.7, 0.7});

    // **************************************
	// Optional: Insert all objects into a map
    // **************************************
    for (auto it=mObjects.begin(); it!=mObjects.end(); it++)
        mMap.insert(std::pair<std::string, VisualObject*>{(*it)->getName(),*it});

	//Inital position of the camera
    mCamera.setPosition(QVector3D(-0.5, 3.5, -8));

    //Need access to our VulkanWindow so making a convenience pointer
    mVulkanWindow = dynamic_cast<VulkanWindow*>(w);

    mTimer.start();
}

//Automatically called by Qt on Renderer startup
void Renderer::initResources()
{
    qDebug("\n ***************************** initResources ******************************************* \n");

    VkDevice logicalDevice = mWindow->device();
    mDeviceFunctions = mWindow->vulkanInstance()->deviceFunctions(logicalDevice);

    // Initialize the graphics queue
    uint32_t graphicsQueueFamilyIndex = mWindow->graphicsQueueFamilyIndex();
    mDeviceFunctions->vkGetDeviceQueue(logicalDevice, graphicsQueueFamilyIndex, 0, &mGraphicsQueue);

    // const int concurrentFrameCount = mWindow->concurrentFrameCount(); // 2 on Oles Machine
    const VkPhysicalDeviceLimits *physicalDeviceLimits = &mWindow->physicalDeviceProperties()->limits;
    const VkDeviceSize uniformAlignment = physicalDeviceLimits->minUniformBufferOffsetAlignment;
    qDebug("Uniform buffer offset alignment is %u", (uint)uniformAlignment); //64 on Oles machine

    mPhongMaterial.vertUniSize = aligned(2 * 64, uniformAlignment);         // 2x mat4
                                        //one float can go at the end of the last vec3
    mPhongMaterial.fragUniSize = aligned( 2 * 16 + 12 + 4 * 4, uniformAlignment); // 3x vec3, 3x float + 1x int



	// Create correct buffers for all objects in mObjects with createBuffer() function
    for (auto it=mObjects.begin(); it!=mObjects.end(); it++)
    {
        createVertexBuffer(uniformAlignment, *it);                //New version - more explicit to how Vulkan does it
		//createBuffer(logicalDevice, uniAlign, *it);         //Old version 

		if ((*it)->getIndices().size() > 0) //If object has indices
            createIndexBuffer(uniformAlignment, *it);
    }

    //DescriptorSets must be made before the Pipelines
    createDescriptorSetLayouts();

    /********************************* Vertex layout: *********************************/
    VkVertexInputBindingDescription vertexBindingDesc{};
	vertexBindingDesc.binding = 0;
	vertexBindingDesc.stride = sizeof(Vertex);
	vertexBindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    /********************************* Shader bindings: *********************************/
    //Descritpion of the attributes used for vertices in the shader
    VkVertexInputAttributeDescription vertexAttrDesc[3];
	vertexAttrDesc[0].location = 0;
    vertexAttrDesc[0].binding = 0;
	vertexAttrDesc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	vertexAttrDesc[0].offset = 0;

	vertexAttrDesc[1].location = 1;
	vertexAttrDesc[1].binding = 0;
	vertexAttrDesc[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	vertexAttrDesc[1].offset = 3 * sizeof(float);           // could use offsetof(Vertex, r); from <cstddef>

    vertexAttrDesc[2].location = 2;	    //UV
    vertexAttrDesc[2].binding = 0;
    vertexAttrDesc[2].format = VK_FORMAT_R32G32_SFLOAT;
    vertexAttrDesc[2].offset = 6 * sizeof(float);           // 6 floats before the UVs are found

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.pNext = nullptr;
    vertexInputInfo.flags = 0;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &vertexBindingDesc;
    vertexInputInfo.vertexAttributeDescriptionCount = sizeof(vertexAttrDesc) / sizeof(vertexAttrDesc[0]);   // will be 3
    vertexInputInfo.pVertexAttributeDescriptions = vertexAttrDesc;
    /*******************************************************/

    // Pipeline cache - supposed to increase performance
    VkPipelineCacheCreateInfo pipelineCacheInfo{};          
    pipelineCacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    VkResult result = mDeviceFunctions->vkCreatePipelineCache(logicalDevice, &pipelineCacheInfo, nullptr, &mPipelineCache);
    if (result != VK_SUCCESS)
        qFatal("Failed to create pipeline cache: %d", result);

    // Pipeline layout
    // Set up the push constant info
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = 16 * sizeof(float) + 3 * sizeof(float);    // 16 floats for the model matrix + 3 floats for color

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.pushConstantRangeCount = 1;                  
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;    
    pipelineLayoutInfo.setLayoutCount = 1;                          
    pipelineLayoutInfo.pSetLayouts = &mPhongMaterial.descriptorSetLayout;

    result = mDeviceFunctions->vkCreatePipelineLayout(logicalDevice, &pipelineLayoutInfo, nullptr, &mPhongMaterial.pipelineLayout);
    if (result != VK_SUCCESS)
        qFatal("Failed to create pipeline layout: %d", result);

    /********************************* Create shaders *********************************/
    //Creates our actual shader modules

    /*************** PhongMaterial ******************/
    mPhongMaterial.vertShaderModule = createShader(QStringLiteral(":/phong_vert.spv"));
    mPhongMaterial.fragShaderModule = createShader(QStringLiteral(":/phong_frag.spv"));

	//Updated to more common way to write it:
    VkPipelineShaderStageCreateInfo vertShaderCreateInfoP{};
    vertShaderCreateInfoP.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderCreateInfoP.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderCreateInfoP.module = mPhongMaterial.vertShaderModule;
    vertShaderCreateInfoP.pName = "main";                // start function in shader

    VkPipelineShaderStageCreateInfo fragShaderCreateInfoP{};
    fragShaderCreateInfoP.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderCreateInfoP.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderCreateInfoP.module = mPhongMaterial.fragShaderModule;
    fragShaderCreateInfoP.pName = "main";                // start function in shader

    VkPipelineShaderStageCreateInfo shaderStagesP[] = { vertShaderCreateInfoP, fragShaderCreateInfoP };

    /*************** ColorMaterial ******************/
    mColorMaterial.vertShaderModule = createShader(QStringLiteral(":/color_vert.spv"));
    mColorMaterial.fragShaderModule = createShader(QStringLiteral(":/color_frag.spv"));

    //Updated to more common way to write it:
    VkPipelineShaderStageCreateInfo vertShaderCreateInfoC{};
    vertShaderCreateInfoC.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderCreateInfoC.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderCreateInfoC.module = mColorMaterial.vertShaderModule;
    vertShaderCreateInfoC.pName = "main";                // start function in shader

    VkPipelineShaderStageCreateInfo fragShaderCreateInfoC{};
    fragShaderCreateInfoC.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderCreateInfoC.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderCreateInfoC.module = mColorMaterial.fragShaderModule;
    fragShaderCreateInfoC.pName = "main";                // start function in shader

    VkPipelineShaderStageCreateInfo shaderStagesC[] = { vertShaderCreateInfoC, fragShaderCreateInfoC };


	/*********************** Graphics pipeline ********************************/
    VkGraphicsPipelineCreateInfo pipelineInfo{};    //Will use this variable a lot in the next 100s of lines
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2; //vertex and fragment shader
    pipelineInfo.pStages = shaderStagesP;
    pipelineInfo.pVertexInputState = &vertexInputInfo;

    // The viewport and scissor will be set dynamically via vkCmdSetViewport/Scissor in setRenderPassParameters().
    // This way the pipeline does not need to be touched when resizing the window.
    VkPipelineViewportStateCreateInfo viewport{};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;
    pipelineInfo.pViewportState = &viewport;

	// **** Input Assembly **** - describes how primitives are assembled in the Graphics pipeline
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;       //Draw triangles
	inputAssembly.primitiveRestartEnable = VK_FALSE;                    //Allow strips to be connected, not used in TriangleList
    pipelineInfo.pInputAssemblyState = &inputAssembly;

	// **** Rasterizer **** - takes the geometry and turns it into fragments
    VkPipelineRasterizationStateCreateInfo rasterization{};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;           // VK_POLYGON_MODE_LINE will make a wireframe;
    rasterization.cullMode = VK_CULL_MODE_NONE;                 // VK_CULL_MODE_BACK_BIT will cull backsides
	rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;  // Front face is counter clockwise - could be clockwise with VK_FRONT_FACE_CLOCKWISE
    rasterization.lineWidth = 1.0f;                             // Not important for VK_POLYGON_MODE_FILL
    pipelineInfo.pRasterizationState = &rasterization;

    // Enable multisampling
    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = mWindow->sampleCountFlagBits();
    pipelineInfo.pMultisampleState = &multisample;

	// **** Color Blending **** - 
    // how to blend the color of a fragment that is already in the framebuffer with the color of the fragment being added
 
    VkPipelineColorBlendAttachmentState colorBlendAttachment{}; // Need this struct for ColorBlending CreateInfo
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
        | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;  // Colors to apply blending to - was hardcoded to 0xF;

    VkPipelineColorBlendStateCreateInfo colorBlend{};
    colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.attachmentCount = 1;                             // the one we made above
    colorBlend.pAttachments = &colorBlendAttachment;
    pipelineInfo.pColorBlendState = &colorBlend;                // no blending for now, write out all of rgba

    // **** Depth Stencil ****
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    pipelineInfo.pDepthStencilState = &depthStencil;

	// **** Dynamic State **** - dynamic states can be changed without recreating the pipeline
    VkDynamicState dynamicEnable[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamic{};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = sizeof(dynamicEnable) / sizeof(VkDynamicState);
    dynamic.pDynamicStates = dynamicEnable;
    pipelineInfo.pDynamicState = &dynamic;

    pipelineInfo.layout = mPhongMaterial.pipelineLayout;
    pipelineInfo.renderPass = mWindow->defaultRenderPass();

    result = mDeviceFunctions->vkCreateGraphicsPipelines(logicalDevice, mPipelineCache, 1, &pipelineInfo, nullptr, &mPhongMaterial.pipeline);
    if (result != VK_SUCCESS)
        qFatal("Failed to create phong graphics pipeline: %d", result);

	//Making a pipeline for drawing lines

    mLineMaterial.pipeline = mPhongMaterial.pipeline;          // reusing most of the settings from the first pipeline
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;   // draw lines
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;           // VK_POLYGON_MODE_LINE will make a wireframe; VK_POLYGON_MODE_FILL
    rasterization.lineWidth = 5.0f;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pStages = shaderStagesC;
    result = mDeviceFunctions->vkCreateGraphicsPipelines(logicalDevice, mPipelineCache, 1, &pipelineInfo, nullptr, &mLineMaterial.pipeline);
    if (result != VK_SUCCESS)
        qFatal("Failed to create line graphics pipeline: %d", result);

	//Making a pipeline for drawing colored triangles

    mColorMaterial.pipeline = mLineMaterial.pipeline;          // reusing most of the settings from the first pipeline
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;   // draw lines
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;           // VK_POLYGON_MODE_LINE will make a wireframe; VK_POLYGON_MODE_FILL
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    result = mDeviceFunctions->vkCreateGraphicsPipelines(logicalDevice, mPipelineCache, 1, &pipelineInfo, nullptr, &mColorMaterial.pipeline);
    if (result != VK_SUCCESS)
        qFatal("Failed to create color graphics pipeline: %d", result);

	// Destroying the shader modules, we won't need them anymore after the pipeline is created
    if (mPhongMaterial.vertShaderModule)
        mDeviceFunctions->vkDestroyShaderModule(logicalDevice, mPhongMaterial.vertShaderModule, nullptr);
    if (mPhongMaterial.fragShaderModule)
        mDeviceFunctions->vkDestroyShaderModule(logicalDevice, mPhongMaterial.fragShaderModule, nullptr);
    if (mColorMaterial.vertShaderModule)
        mDeviceFunctions->vkDestroyShaderModule(logicalDevice, mColorMaterial.vertShaderModule, nullptr);
    if (mColorMaterial.fragShaderModule)
        mDeviceFunctions->vkDestroyShaderModule(logicalDevice, mColorMaterial.fragShaderModule, nullptr);

	// Create the uniform buffer
	createUniformBuffer();
    createDescriptorPool();
    createDescriptorSet();

    // Create the texture sampler
    createTextureSampler();

    mTextureHandle = createTexture((assetPath + "Hund.bmp")); //Heightmap.jpg HundA.bmp
    //getVulkanHWInfo(); // if you want to get info about the Vulkan hardware
    qDebug("InitResouce finished");
}

// This function is called at startup, and when the app window is resized
void Renderer::initSwapChainResources()
{
    qDebug("\n ***************************** initSwapChainResources ******************************************* \n");

    // Projection matrix - how the scene will be projected into the render window
	// has to be updated when the window is resized
    // mProjectionMatrix.setToIdentity();

    //can be used to correct for coordinate system differences between OpenGL and Vulkan:
    //QMatrix4x4 QVulkanWindow::clipCorrectionMatrix()

    //find the size of the window
    const QSize sz = mWindow->swapChainImageSize();

    mCamera.perspective(45.0f, sz.width() / (float) sz.height(), 0.01f, 500.0f);
}

void Renderer::startNextFrame()
{
    deltaTime = mTimer.elapsed() / 1000.0f;
    mTimer.restart();
    //Handeling input from keyboard and mouse is done in VulkanWindow
    //Has to be done each frame to get smooth movement
    mVulkanWindow->handleInput();
    mCamera.update();               //input can have moved the camera

    VkCommandBuffer commandBuffer = mWindow->currentCommandBuffer();

	setRenderPassParameters(commandBuffer);

    uint32_t frameIndex = mWindow->currentFrame();
    uint32_t frameUniOffset = frameIndex * (mPhongMaterial.vertUniSize + mPhongMaterial.fragUniSize);
    uint32_t frameUniOffsets[] = { frameUniOffset, frameUniOffset }; // + mPhongMaterial.vertUniSize };

    mDeviceFunctions->vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPhongMaterial.pipelineLayout, 0, 1,
        &mPhongMaterial.descriptorSet, 2, frameUniOffsets);

    setViewProjectionMatrix();   //Update the view and projection matrix in the Uniform

    /********************************* Our draw call!: *********************************/
    VkDeviceSize vbOffset{ 0 };     //Offsets into buffer being bound
    
    //    0 = fill color, 1 = line color, 2 = fill phong
    
    mDeviceFunctions->vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mColorMaterial.pipeline);
    for (std::vector<VisualObject*>::iterator it=mObjects.begin(); it!=mObjects.end(); it++)
    {
        //Draw type
		if ((*it)->getDrawType() != 0)
            continue;

        setModelMatrix((*it)->getMatrix(), (*it)->color()); // Model matrix and object color;

        mDeviceFunctions->vkCmdBindVertexBuffers(commandBuffer, 0, 1, &(*it)->getVBuffer(), &vbOffset);
		//Check if we have an index buffer - if so, use Indexed draw
        if ((*it)->getIndices().size() > 0)
        {
			mDeviceFunctions->vkCmdBindIndexBuffer(commandBuffer, (*it)->getIBuffer(), 0, VK_INDEX_TYPE_UINT32);
			mDeviceFunctions->vkCmdDrawIndexed(commandBuffer, (*it)->getIndices().size(), 1, 0, 0, 0); //size == number of indices
		}
		else   //No index buffer - use regular draw
			mDeviceFunctions->vkCmdDraw(commandBuffer, (*it)->getVertices().size(), 1, 0, 0);   
    }

    mDeviceFunctions->vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mLineMaterial.pipeline);
    for (std::vector<VisualObject*>::iterator it = mObjects.begin(); it != mObjects.end(); it++)
    {
        //Draw type
        if ((*it)->getDrawType() != 1)
            continue;

        setModelMatrix((*it)->getMatrix(), (*it)->color()); // Model matrix and object color;

        mDeviceFunctions->vkCmdBindVertexBuffers(commandBuffer, 0, 1, &(*it)->getVBuffer(), &vbOffset);
        //Check if we have an index buffer - if so, use Indexed draw
        if ((*it)->getIndices().size() > 0)
        {
            mDeviceFunctions->vkCmdBindIndexBuffer(commandBuffer, (*it)->getIBuffer(), 0, VK_INDEX_TYPE_UINT32);
            mDeviceFunctions->vkCmdDrawIndexed(commandBuffer, (*it)->getIndices().size(), 1, 0, 0, 0); //size == number of indices
        }
        else   //No index buffer - use regular draw
            mDeviceFunctions->vkCmdDraw(commandBuffer, (*it)->getVertices().size(), 1, 0, 0);
    }


    mDeviceFunctions->vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPhongMaterial.pipeline);
    for (std::vector<VisualObject*>::iterator it=mObjects.begin(); it!=mObjects.end(); it++)
    {
        //Draw type
        if ((*it)->getDrawType() != 2)
            continue;

        setModelMatrix((*it)->getMatrix(), (*it)->color()); // Model matrix and object color;

        mDeviceFunctions->vkCmdBindVertexBuffers(commandBuffer, 0, 1, &(*it)->getVBuffer(), &vbOffset);
        //Check if we have an index buffer - if so, use Indexed draw
        if ((*it)->getIndices().size() > 0)
        {
            mDeviceFunctions->vkCmdBindIndexBuffer(commandBuffer, (*it)->getIBuffer(), 0, VK_INDEX_TYPE_UINT32);
            mDeviceFunctions->vkCmdDrawIndexed(commandBuffer, (*it)->getIndices().size(), 1, 0, 0, 0); //size == number of indices
        }
        else   //No index buffer - use regular draw
            mDeviceFunctions->vkCmdDraw(commandBuffer, (*it)->getVertices().size(), 1, 0, 0);
    }


    /***************************************/

    mDeviceFunctions->vkCmdEndRenderPass(commandBuffer);

    //Hardcoded!!!
    //quick hack to get a moving light
    static float rotate{ 0.f };
	if (true)  //Change to false to stop the light  
    {
        mLight->move(sinf(rotate) * 0.1, cosf(rotate) * 0.0125, cosf(rotate) * 0.1);     //just to move the light each frame
        rotate += 0.001f;
    }
    
    mWindow->frameReady();
    mWindow->requestUpdate(); // render continuously, throttled by the presentation rate
}

VkShaderModule Renderer::createShader(const QString &name)
{
    //This uses Qt's own file opening and resource system
    //We probably will replace it with pure C++ when expanding the program
    QFile file(name);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning("Failed to read shader %s", qPrintable(name));
        return VK_NULL_HANDLE;
    }
    QByteArray blob = file.readAll();
    file.close();

    VkShaderModuleCreateInfo shaderInfo{};
    shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderInfo.codeSize = blob.size();
    shaderInfo.pCode = reinterpret_cast<const uint32_t *>(blob.constData());

    VkShaderModule shaderModule;
    VkResult err = mDeviceFunctions->vkCreateShaderModule(mWindow->device(), &shaderInfo, nullptr, &shaderModule);
    if (err != VK_SUCCESS) {
        qWarning("Failed to create shader module: %d", err);
        return VK_NULL_HANDLE;
    }

    return shaderModule;
}

//Uses pushconstants to update the Model Matrix in the shader
void Renderer::setModelMatrix(QMatrix4x4 modelMatrix, QVector3D color)
{
	float tempArray[19]{};  // 16 floats for the matrix + 3 floats for the color
	memcpy(tempArray, modelMatrix.constData(), 16 * sizeof(float));

    tempArray[16] = color.x();
    tempArray[17] = color.y();
    tempArray[18] = color.z();
    mDeviceFunctions->vkCmdPushConstants(mWindow->currentCommandBuffer(), mPhongMaterial.pipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT, 0, 19 * sizeof(float), tempArray);

    //mDeviceFunctions->vkCmdPushConstants(mWindow->currentCommandBuffer(), mPhongMaterial.pipelineLayout,
		//VK_SHADER_STAGE_VERTEX_BIT, 0, 16 * sizeof(float), modelMatrix.constData());    //Column-major matrix
}

//Uses Uniform Buffers to transfer the View and Projection matrix - and other data you want to transfer:
void Renderer::setViewProjectionMatrix()
{
    uint8_t* p = mUniformBufferLocation;
    // ************ Vertex shader ******************
	memcpy(p, mCamera.viewMatrix().constData(), 64);    //64 bytes for the view matrix

    QMatrix4x4 temp = mCamera.projectionMatrix();
    temp = temp * mWindow->clipCorrectionMatrix();  //Correcting for Vulkans -Y

    p += 64;
	//Adding 64 bytes to the uniform buffer location to get to the projection matrix position
	memcpy(p, temp.constData(), 64);	//64 bytes for the projection matrix

    // ************ Fragment shader ******************
    p += 64;    // alignment is 16
    float lightPos[3] = {mLight->position().x(), mLight->position().y(), mLight->position().z()};
	memcpy(p, lightPos, 12);    // 12 bytes for the light position
    p += 16;
	memcpy(p, mLight->mLightColor, 12);	// 12 bytes for the light color
    p += 16;
    float cameraPos[3] = {mCamera.position().x(), mCamera.position().y(), mCamera.position().z()};
	memcpy(p, cameraPos, 12);	// 12 bytes for the camera position

    p += 12;    // alignment is 16 but next value is only 4 so fits into this
	memcpy(p, &mLight->mAmbientStrenght, 4);    // one float

    p += 4;    // only 4 up to next allignment
	memcpy(p, &mLight->mLightStrenght, 4);      // one float

    p += 4;    // adding into this allignment block
	memcpy(p, &mLight->mSpecularStrenght, 4);	// one float

    p += 4;    // adding into this allignment block
	memcpy(p, &mLight->mSpecularExponent, 4);	// one int


    /************ NB ************
    Remember to go into
      createUniformBuffer() - bufferSize
    and
      createDescriptorSet() - bufferInfo.range
    and UPDATE THE SIZE of the buffer if you add more data!!!
    */
}

void Renderer::setTexture(TextureHandle& textureHandle, VkCommandBuffer commandBuffer)
{
	mDeviceFunctions->vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
        mPhongMaterial.pipelineLayout, 1, 1, &textureHandle.mTextureDescriptorSet, 0, nullptr);
}

void Renderer::setRenderPassParameters(VkCommandBuffer commandBuffer)
{
    const QSize swapChainImageSize = mWindow->swapChainImageSize();

    //Backtgound color of the render window - dark grey
    VkClearColorValue clearColor = { { 0.3, 0.3, 0.3, 1 } };

    VkClearDepthStencilValue clearDepthStencil = { 1, 0 };
    VkClearValue clearValues[3]{};  //C++11 {} works even on arrays!
    clearValues[0].color = clearValues[2].color = clearColor;
    clearValues[1].depthStencil = clearDepthStencil;

    VkRenderPassBeginInfo renderPassBeginInfo{};
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.renderPass = mWindow->defaultRenderPass();
    renderPassBeginInfo.framebuffer = mWindow->currentFramebuffer();
    renderPassBeginInfo.renderArea.extent.width = swapChainImageSize.width();
    renderPassBeginInfo.renderArea.extent.height = swapChainImageSize.height();
    renderPassBeginInfo.clearValueCount = mWindow->sampleCountFlagBits() > VK_SAMPLE_COUNT_1_BIT ? 3 : 2;
    renderPassBeginInfo.pClearValues = clearValues;
    mDeviceFunctions->vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    //Viewport - area of the image to render to, usually (0,0) to (width, height)
    VkViewport viewport{};
    viewport.x = viewport.y = 0.f;
    viewport.width = swapChainImageSize.width();
    viewport.height = swapChainImageSize.height();
    viewport.minDepth = 0.f;                //min framebuffer depth
    viewport.maxDepth = 1.f;                //max framebuffer depth
    mDeviceFunctions->vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    //Scissor - area to draw in the target frame buffer
    VkRect2D scissor{};
    scissor.offset.x = scissor.offset.y = 0;
    scissor.extent.width = viewport.width;
    scissor.extent.height = viewport.height;
    mDeviceFunctions->vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
}

// Dag 240125
// This function contains some of the body of our former Renderer::initResources() function
// If we want to have more objects, we need to initialize buffers for each of them
// This version is not a version with encapsulation
// We use the VisualObject members mBuffer and mBufferMemory
void Renderer::createBuffer(VkDevice logicalDevice, const VkDeviceSize uniformAlignment,
                                VisualObject* visualObject, VkBufferUsageFlags usage)
{
    //Gets the size of the mesh - aligned to the uniform alignment
    VkDeviceSize vertexAllocSize = aligned(visualObject->getVertices().size() * sizeof(Vertex), uniformAlignment);

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO; // Set the structure type
    bufferInfo.size = vertexAllocSize; //One vertex buffer (we don't use Uniform buffer in this example)
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT; // Set the usage vertex buffer (not using Uniform buffer in this example)

    VkResult err = mDeviceFunctions->vkCreateBuffer(logicalDevice, &bufferInfo, nullptr, &visualObject->getVBuffer());
    if (err != VK_SUCCESS)
        qFatal("Failed to create buffer: %d", err);

    VkMemoryRequirements memReq;
    mDeviceFunctions->vkGetBufferMemoryRequirements(logicalDevice, visualObject->getVBuffer(), &memReq);

    VkMemoryAllocateInfo memAllocInfo{};
    memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memAllocInfo.pNext = nullptr;
	memAllocInfo.allocationSize = memReq.size;
	memAllocInfo.memoryTypeIndex = mWindow->hostVisibleMemoryIndex();

    err = mDeviceFunctions->vkAllocateMemory(logicalDevice, &memAllocInfo, nullptr, &visualObject->getVBufferMemory());
    if (err != VK_SUCCESS)
        qFatal("Failed to allocate memory: %d", err);

    err = mDeviceFunctions->vkBindBufferMemory(logicalDevice, visualObject->getVBuffer(), visualObject->getVBufferMemory(), 0);
    if (err != VK_SUCCESS)
        qFatal("Failed to bind buffer memory: %d", err);

    void* p{nullptr};
    err = mDeviceFunctions->vkMapMemory(logicalDevice, visualObject->getVBufferMemory(), 0, memReq.size, 0, reinterpret_cast<void **>(&p));
    if (err != VK_SUCCESS)
        qFatal("Failed to map memory: %d", err);

    memcpy(p, visualObject->getVertices().data(), visualObject->getVertices().size()*sizeof(Vertex));

    mDeviceFunctions->vkUnmapMemory(logicalDevice, visualObject->getVBufferMemory());
}

//Very similar to createBuffer, but here we find and set the memory type explicitly
//Also the generation of the buffer is in a separate function
//and copy data to GPU read only memory
void Renderer::createVertexBuffer(const VkDeviceSize uniformAlignment, VisualObject* visualObject)
{
    //Get the size of the mesh and align it to the uniform alignment
    VkDeviceSize vertexAllocSize = aligned(visualObject->getVertices().size() * sizeof(Vertex), uniformAlignment);

	BufferHandle stagingHandle = createGeneralBuffer(vertexAllocSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, //Transfer source bit is for copying data to the GPU
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);    // Host visible memory (CPU) is slower to access than device local memory (GPU)

    //Copy the data over to the buffer
    void* data{ nullptr };
    mDeviceFunctions->vkMapMemory(mWindow->device(), stagingHandle.mBufferMemory, 0, vertexAllocSize, 0, &data);
    memcpy(data, visualObject->getVertices().data(), vertexAllocSize);
    mDeviceFunctions->vkUnmapMemory(mWindow->device(), stagingHandle.mBufferMemory);

	//This is for copying the data to the GPU
    BufferHandle gpuHandle = createGeneralBuffer(vertexAllocSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT); // Device local memory (GPU VRam) is faster to access than host visible memory (CPU RAM)
	
    //Set the buffer and buffer memory in the VisualObject for use in the draw call
	visualObject->setVBuffer(gpuHandle.mBuffer);
	visualObject->setVBufferMemory(gpuHandle.mBufferMemory);

    //Copy the data from the staging buffer to the GPU buffer
	VkCommandBuffer commandBuffer = BeginTransientCommandBuffer();
	VkBufferCopy copyRegion{};
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size = vertexAllocSize;
	mDeviceFunctions->vkCmdCopyBuffer(commandBuffer, stagingHandle.mBuffer, gpuHandle.mBuffer, 1, &copyRegion);
	EndTransientCommandBuffer(commandBuffer);
	
    //Free the staging buffer
	destroyBuffer(stagingHandle);
}

void Renderer::createIndexBuffer(const VkDeviceSize uniformAlignment, VisualObject* visualObject)
{
	//Get the size of the mesh and align it to the uniform alignment
	VkDeviceSize indexAllocSize = aligned(visualObject->getIndices().size() * sizeof(uint32_t), uniformAlignment);

	//Create a staging buffer for the index data
	BufferHandle stagingHandle = createGeneralBuffer(indexAllocSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);    // Host visible memory (CPU) is slower to access than device local memory (GPU)
	
    void* data{ nullptr };
	mDeviceFunctions->vkMapMemory(mWindow->device(), stagingHandle.mBufferMemory, 0, indexAllocSize, 0, &data);
	memcpy(data, visualObject->getIndices().data(), indexAllocSize);
	mDeviceFunctions->vkUnmapMemory(mWindow->device(), stagingHandle.mBufferMemory);

    //This is for copying the data to the GPU
	BufferHandle gpuHandle = createGeneralBuffer(indexAllocSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT); // Device local memory (GPU VRam) is faster to access than host visible memory (CPU RAM)

	//Set the buffer and buffer memory in the VisualObject for use in the draw call
	visualObject->setIBuffer(gpuHandle.mBuffer);
	visualObject->setIBufferMemory(gpuHandle.mBufferMemory);

	//Copy the data from the staging buffer to the GPU buffer:
	VkCommandBuffer commandBuffer = BeginTransientCommandBuffer();

	VkBufferCopy copyRegion{};
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size = indexAllocSize;
	mDeviceFunctions->vkCmdCopyBuffer(commandBuffer, stagingHandle.mBuffer, gpuHandle.mBuffer, 1, &copyRegion);
	EndTransientCommandBuffer(commandBuffer);

	//Free the staging buffer
	destroyBuffer(stagingHandle);
}

BufferHandle Renderer::createGeneralBuffer(const VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
{
    BufferHandle bufferHandle{};

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;    // set the structure type
    bufferInfo.size = size;                                     // size of the wanted buffer
    bufferInfo.usage = usage;                                   // buffer usage type
    // bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult err = mDeviceFunctions->vkCreateBuffer(mWindow->device(), &bufferInfo, nullptr, &bufferHandle.mBuffer);
    if (err != VK_SUCCESS)
    {
        qFatal("Failed to create general buffer: %d", err);
    }

    VkMemoryRequirements memoryRequirements;
    mDeviceFunctions->vkGetBufferMemoryRequirements(mWindow->device(), bufferHandle.mBuffer, &memoryRequirements);

    // Manually find a memory type
    uint32_t chosenMemoryType = findMemoryType(memoryRequirements.memoryTypeBits, properties);

    VkMemoryAllocateInfo memoryAllocateInfo{};
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.allocationSize = memoryRequirements.size;
    memoryAllocateInfo.memoryTypeIndex = chosenMemoryType;      //Qt has a helper function for this mWindow->hostVisibleMemoryIndex();

    err = mDeviceFunctions->vkAllocateMemory(mWindow->device(), &memoryAllocateInfo, nullptr, &bufferHandle.mBufferMemory);
    if (err != VK_SUCCESS)
    {
        qFatal("Failed to allocate buffer memory: %d", err);
    }

    mDeviceFunctions->vkBindBufferMemory(mWindow->device(), bufferHandle.mBuffer, bufferHandle.mBufferMemory, 0);

    return bufferHandle;
}

//Create a descriptor set layout that describes the uniform buffer.
void Renderer::createDescriptorSetLayouts()
{
    VkDescriptorSetLayoutBinding descriptorSetLayoutBinding[2]{};
    descriptorSetLayoutBinding[0].binding = 0;
    descriptorSetLayoutBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    descriptorSetLayoutBinding[0].descriptorCount = 1;
    descriptorSetLayoutBinding[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;   // vertex shader
    descriptorSetLayoutBinding[0].pImmutableSamplers = nullptr;


    descriptorSetLayoutBinding[1].binding = 1;
    descriptorSetLayoutBinding[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    descriptorSetLayoutBinding[1].descriptorCount = 1;
    descriptorSetLayoutBinding[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;  //fragment shader
    descriptorSetLayoutBinding[1].pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo{};
    descriptorSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutInfo.pNext = nullptr;
    descriptorSetLayoutInfo.flags = 0;
    descriptorSetLayoutInfo.bindingCount = sizeof(descriptorSetLayoutBinding) / sizeof(descriptorSetLayoutBinding[0]);
    descriptorSetLayoutInfo.pBindings = descriptorSetLayoutBinding;

    VkResult err = mDeviceFunctions->vkCreateDescriptorSetLayout(mWindow->device(),
                                                                 &descriptorSetLayoutInfo, nullptr,
                                                                 &mPhongMaterial.descriptorSetLayout);
    if (err != VK_SUCCESS)
        qFatal("Failed to create DescriptorSetLayout: %d", err);

    //Textures
    VkDescriptorSetLayoutBinding textureLayoutBinding{};
    textureLayoutBinding.binding = 0;
    textureLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    textureLayoutBinding.descriptorCount = 1;
    textureLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;   //We are using the uniform buffer in the vertex shader

    VkDescriptorSetLayoutCreateInfo textureLayoutInfo{};
    textureLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    textureLayoutInfo.bindingCount = 1;
    textureLayoutInfo.pBindings = &textureLayoutBinding;

    err = mDeviceFunctions->vkCreateDescriptorSetLayout(mWindow->device(), &textureLayoutInfo, nullptr, &mTextureDescriptorSetLayout);
    if (err != VK_SUCCESS)
        qFatal("Failed to create TextureDescriptorSetLayout: %d", err);

}

//Creates the actual Uniform Buffer - one important thing being the size of it
void Renderer::createUniformBuffer()
{
    //Vertex and fragment uniforms - // two 4x4 matrices + 12 for color
    VkDeviceSize bufferSize = (mPhongMaterial.vertUniSize + mPhongMaterial.fragUniSize) * mWindow->concurrentFrameCount();

    mUniformBuffer = createGeneralBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    //Map the buffer memory
    VkResult err = mDeviceFunctions->vkMapMemory(mWindow->device(), mUniformBuffer.mBufferMemory,
                                                 0,
                                                 bufferSize,
                                                 0, reinterpret_cast<void **>(&mUniformBufferLocation));
    if (err != VK_SUCCESS)
        qFatal("Failed to map memory: %d", err);
}

//Allocate a descriptor set and update it to point to the uniform buffer
//Also gives the Buffer size so this has to match the buffer made.
void Renderer::createDescriptorSet()
{
    VkDescriptorSetAllocateInfo descritprSetAllocateInfo{};
    descritprSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descritprSetAllocateInfo.pNext = nullptr;
    descritprSetAllocateInfo.descriptorPool = mPhongMaterial.descriptorPool;
    descritprSetAllocateInfo.descriptorSetCount = 1;
    descritprSetAllocateInfo.pSetLayouts = &mPhongMaterial.descriptorSetLayout;

    VkResult err = mDeviceFunctions->vkAllocateDescriptorSets(mWindow->device(),
                                                              &descritprSetAllocateInfo,
                                                              &mPhongMaterial.descriptorSet);
    if (err != VK_SUCCESS)
        qFatal("Failed to allocate descriptor set: %d", err);

    VkDescriptorBufferInfo vertexBufferInfo{};
    vertexBufferInfo.buffer = mUniformBuffer.mBuffer;
    vertexBufferInfo.offset = 0;
    vertexBufferInfo.range = mPhongMaterial.vertUniSize;      // two 4x4 matrices

    VkDescriptorBufferInfo fragmentBufferInfo{};
    fragmentBufferInfo.buffer = mUniformBuffer.mBuffer;
    fragmentBufferInfo.offset = mPhongMaterial.vertUniSize;
    fragmentBufferInfo.range = mPhongMaterial.fragUniSize;      // 3x vec3, 3x float, 1x int

    VkWriteDescriptorSet writeDescriptorSet[2]{};
    writeDescriptorSet[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet[0].dstSet = mPhongMaterial.descriptorSet;
    writeDescriptorSet[0].dstBinding = 0;
    // writeDescriptorSet[0].dstArrayElement = 0;
    writeDescriptorSet[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    writeDescriptorSet[0].descriptorCount = 1;
    writeDescriptorSet[0].pBufferInfo = &vertexBufferInfo;

    writeDescriptorSet[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet[1].dstSet = mPhongMaterial.descriptorSet;
    writeDescriptorSet[1].dstBinding = 1;
    // writeDescriptorSet[1].dstArrayElement = 0;
    writeDescriptorSet[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    writeDescriptorSet[1].descriptorCount = 1;
    writeDescriptorSet[1].pBufferInfo = &fragmentBufferInfo;

                                                            // 2 descriptor sets
    mDeviceFunctions->vkUpdateDescriptorSets(mWindow->device(), 2, writeDescriptorSet, 0, nullptr);
}

//Create a descriptor pool to allocate descriptor sets - for the Uniform Buffers.
void Renderer::createDescriptorPool()
{
	//For Textures
    VkDescriptorPoolSize texturePoolSize{};
    texturePoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    texturePoolSize.descriptorCount = 1024;      // can ask the GPU - properties.limits.maxSamplerAllocationCount;

    VkDescriptorPoolCreateInfo texturePoolInfo{};
    texturePoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    texturePoolInfo.maxSets = 1024;             // can ask the GPU - properties.limits.maxDescriptorSetSamplers;
    texturePoolInfo.poolSizeCount = 1;
    texturePoolInfo.pPoolSizes = &texturePoolSize;
    texturePoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    VkResult err = mDeviceFunctions->vkCreateDescriptorPool(mWindow->device(), &texturePoolInfo, nullptr, &mTextureDescriptorPool);
    if (err != VK_SUCCESS)
        qFatal("Failed to create descriptor pool: %d", err);
    VkDescriptorPoolSize descriptorPoolSize[1]{};
    descriptorPoolSize[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;  //VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
    descriptorPoolSize[0].descriptorCount = 2;

    VkDescriptorPoolCreateInfo descriptorPoolInfo{};
    descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolInfo.maxSets = 1;
    descriptorPoolInfo.poolSizeCount = sizeof(descriptorPoolSize) / sizeof(descriptorPoolSize[0]);
    descriptorPoolInfo.pPoolSizes = descriptorPoolSize;

    err = mDeviceFunctions->vkCreateDescriptorPool(mWindow->device(), &descriptorPoolInfo, nullptr, &mPhongMaterial.descriptorPool);
    if (err != VK_SUCCESS)
        qFatal("Failed to create descriptor pool: %d", err);
}

/*************************************************************************************************/

void Renderer::getVulkanHWInfo()
{
    qDebug("\n ***************************** Vulkan Hardware Info ******************************************* \n");
    QVulkanInstance *inst = mWindow->vulkanInstance();
    mDeviceFunctions = inst->deviceFunctions(mWindow->device());

    QString info;
    info += QString::asprintf("Number of physical devices: %d\n", int(mWindow->availablePhysicalDevices().count()));

    QVulkanFunctions *f = inst->functions();
    VkPhysicalDeviceProperties props;
    f->vkGetPhysicalDeviceProperties(mWindow->physicalDevice(), &props);
    info += QString::asprintf("Active physical device name: '%s' version %d.%d.%d\nAPI version %d.%d.%d\n",
                              props.deviceName,
                              VK_VERSION_MAJOR(props.driverVersion), VK_VERSION_MINOR(props.driverVersion),
                              VK_VERSION_PATCH(props.driverVersion),
                              VK_VERSION_MAJOR(props.apiVersion), VK_VERSION_MINOR(props.apiVersion),
                              VK_VERSION_PATCH(props.apiVersion));

    info += QStringLiteral("Supported instance layers:\n");
    for (const QVulkanLayer &layer : inst->supportedLayers())
        info += QString::asprintf("    %s v%u\n", layer.name.constData(), layer.version);
    info += QStringLiteral("Enabled instance layers:\n");
    for (const QByteArray &layer : inst->layers())
        info += QString::asprintf("    %s\n", layer.constData());

    info += QStringLiteral("Supported instance extensions:\n");
    for (const QVulkanExtension &ext : inst->supportedExtensions())
        info += QString::asprintf("    %s v%u\n", ext.name.constData(), ext.version);
    info += QStringLiteral("Enabled instance extensions:\n");
    for (const QByteArray &ext : inst->extensions())
        info += QString::asprintf("    %s\n", ext.constData());

    info += QString::asprintf("Color format: %u\nDepth-stencil format: %u\n",
                              mWindow->colorFormat(), mWindow->depthStencilFormat());

    info += QStringLiteral("Supported sample counts:");
    const QList<int> sampleCounts = mWindow->supportedSampleCounts();
    for (int count : sampleCounts)
        info += QLatin1Char(' ') + QString::number(count);

    // Get the maximum size of the push constants buffer for this device
    uint32_t maxPushConstantsSize = props.limits.maxPushConstantsSize;
    info += QString::asprintf("\nMaximum push constants size: %u", maxPushConstantsSize);

    info += QLatin1Char('\n');

    qDebug(info.toUtf8().constData());

    qDebug("\n ***************************** Vulkan Hardware Info finished ******************************************* \n");
}

void Renderer::releaseSwapChainResources()
{
    qDebug("\n ***************************** releaseSwapChainResources ******************************************* \n");
    /* from VulkanCubes
     * QFutureWatcher<void> mFrameWatcher;
     *     bool mFramePending{false};
    // It is important to finish the pending frame right here since this is the
    // last opportunity to act with all resources intact.
    mFrameWatcher.waitForFinished();
    // Cannot count on the finished() signal being emitted before returning
    // from here.
    if (mFramePending) {
        mFramePending = false;
        mWindow->frameReady();
    }
    */
}

// Function called by Qt typically when app is closing
// It automatically waits for the GPU to be idle before releasing resources
void Renderer::releaseResources()
{
    qDebug("\n ***************************** releaseResources ******************************************* \n");

    VkDevice dev = mWindow->device();

    if (mPhongMaterial.pipeline) {
        mDeviceFunctions->vkDestroyPipeline(dev, mPhongMaterial.pipeline, nullptr);
        mPhongMaterial.pipeline = VK_NULL_HANDLE;
    }

    if (mPipeline2) {
        mDeviceFunctions->vkDestroyPipeline(dev, mPipeline2, nullptr);
        mPipeline2 = VK_NULL_HANDLE;
    }

    if (mPhongMaterial.pipelineLayout) {
        mDeviceFunctions->vkDestroyPipelineLayout(dev, mPhongMaterial.pipelineLayout, nullptr);
        mPhongMaterial.pipelineLayout = VK_NULL_HANDLE;
    }

    if (mPipelineCache) {
        mDeviceFunctions->vkDestroyPipelineCache(dev, mPipelineCache, nullptr);
        mPipelineCache = VK_NULL_HANDLE;
    }

	destroyBuffer(mUniformBuffer);

    if (mPhongMaterial.descriptorSetLayout) {
        mDeviceFunctions->vkDestroyDescriptorSetLayout(dev, mPhongMaterial.descriptorSetLayout, nullptr);
        mPhongMaterial.descriptorSetLayout = VK_NULL_HANDLE;
    }

    if (mPhongMaterial.descriptorPool) {
        mDeviceFunctions->vkDestroyDescriptorPool(dev, mPhongMaterial.descriptorPool, nullptr);
        mPhongMaterial.descriptorPool = VK_NULL_HANDLE;
    }

    // Free buffers and memory for all objects in container
    for (auto it=mObjects.begin(); it!=mObjects.end(); it++) {
        if ((*it)->getVBuffer()) {
			BufferHandle handle { (*it)->getVBufferMemory(), (*it)->getVBuffer() };
			destroyBuffer(handle);
            (*it)->getVBuffer() = VK_NULL_HANDLE;
        }
        if ((*it)->getIBuffer()) {
            BufferHandle handle{ (*it)->getIBufferMemory(), (*it)->getIBuffer() };
            destroyBuffer(handle);
            (*it)->getIBuffer() = VK_NULL_HANDLE;
        }
    }

    // Destroy textures
    destroyTexture(mTextureHandle);

	if (mTextureSampler) {
		mDeviceFunctions->vkDestroySampler(dev, mTextureSampler, nullptr);
		mTextureSampler = VK_NULL_HANDLE;
	}

	if (mTextureDescriptorSetLayout) {
		mDeviceFunctions->vkDestroyDescriptorSetLayout(dev, mTextureDescriptorSetLayout, nullptr);
		mTextureDescriptorSetLayout = VK_NULL_HANDLE;
	}
	if (mTextureDescriptorPool) {
		mDeviceFunctions->vkDestroyDescriptorPool(dev, mTextureDescriptorPool, nullptr);
		mTextureDescriptorPool = VK_NULL_HANDLE;
	}

	qDebug("\n ***************************** releaseResources finished ******************************************* \n");
}

//Helper function to find the memory type - Qt has this built in, but it is hidden
uint32_t Renderer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags requiredProperties)
{
    VkPhysicalDeviceMemoryProperties memoryProperties;

    // Get the QVulkanFunctions instance - pretty hidden in Qt
    QVulkanFunctions* vulkanFunctions = mWindow->vulkanInstance()->functions();
    vulkanFunctions->vkGetPhysicalDeviceMemoryProperties(mWindow->physicalDevice(), &memoryProperties);
    std::vector<VkMemoryType> memoryTypes; //getting the memory types
    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
    {
        memoryTypes.push_back(memoryProperties.memoryTypes[i]);
    }

    //uint32_t chosenMemoryType{ 0 };
    for (uint32_t i = 0; i < memoryTypes.size(); i++)
    {
        bool isSuitable = (typeFilter & (1 << i));
        //CPU memory
        bool isHostVisible = (memoryTypes[i].propertyFlags & requiredProperties);
        if (isSuitable && isHostVisible)
        {
            return i;
        }
    }
    qFatal("Failed to find memory type! This will crash!");

    return 0;
}


// Function to create a command buffer that is short lived and not a part of the Rendering command
VkCommandBuffer Renderer::BeginTransientCommandBuffer()
{
	VkCommandBufferAllocateInfo allocateInfo{};
	allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocateInfo.commandPool = mWindow->graphicsCommandPool();
	allocateInfo.commandBufferCount = 1;
	
	VkCommandBuffer commandBuffer;
    mDeviceFunctions->vkAllocateCommandBuffers(mWindow->device(), &allocateInfo, &commandBuffer);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	mDeviceFunctions->vkBeginCommandBuffer(commandBuffer, &beginInfo);

	return commandBuffer;
}

// Function to end a short lived command buffer
void Renderer::EndTransientCommandBuffer(VkCommandBuffer commandBuffer)
{
    mDeviceFunctions->vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	//This is the way to submit a command buffer in Vulkan
    mDeviceFunctions->vkQueueSubmit(mGraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    mDeviceFunctions->vkQueueWaitIdle(mGraphicsQueue);
	mDeviceFunctions->vkFreeCommandBuffers(mWindow->device(), mWindow->graphicsCommandPool(), 1, &commandBuffer);
}

// Function to destroy a buffer and its memory
void Renderer::destroyBuffer(BufferHandle handle) {
    mDeviceFunctions->vkDeviceWaitIdle(mWindow->device());
    mDeviceFunctions->vkDestroyBuffer(mWindow->device(), handle.mBuffer, nullptr);
    mDeviceFunctions->vkFreeMemory(mWindow->device(), handle.mBufferMemory, nullptr);
}

void Renderer::createTextureSampler()
{
	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_NEAREST;                   // Magnification filter - try VK_FILTER_LINEAR
	samplerInfo.minFilter = VK_FILTER_NEAREST;                   // Minification filter - try VK_FILTER_LINEAR
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;  // Address mode for U coordinates
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;  // Address mode for V coordinates
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;  // Address mode for W coordinates
	samplerInfo.anisotropyEnable = VK_FALSE;                     // Enable anisotropy
	samplerInfo.maxAnisotropy = 1.0;                             // Anisotropy level
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK; // Border color
	samplerInfo.unnormalizedCoordinates = VK_FALSE;             // Normalized coordinates
	samplerInfo.compareEnable = VK_FALSE;                       // Compare enable
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;               // Compare operation
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;     // Mipmap mode
	samplerInfo.mipLodBias = 0.0f;                              // Mipmap level of detail bias
	samplerInfo.minLod = 0.0f;                                  // Minimum level of detail
	samplerInfo.maxLod = 0.0f;                                  // Maximum level of detail

	VkResult err = mDeviceFunctions->vkCreateSampler(mWindow->device(), &samplerInfo, nullptr, &mTextureSampler);
	if (err != VK_SUCCESS)
		qFatal("Failed to create texture sampler: %d", err);
}

TextureHandle Renderer::createTexture(const std::string filename)
{
    int texWidth, texHeight, texChannels;
    VkDeviceSize bufferSize{};
    VkFormat format{ VK_FORMAT_R8G8B8A8_SRGB }; //could be VK_FORMAT_R8G8B8_SRGB
	BufferHandle stagingBuffer{};
    stbi_uc* pixelData{ nullptr };

	//Open the file and read the data into the imageFileData vector
    std::ifstream file(filename, std::ios::binary);

	//if the file is not open, we create a tiny default texture
    if (!file.is_open()) 
    {
        //Dummy texture 2x2 pixels, 4 bytes per pixel
        pixelData = new stbi_uc[16]{};  //stbi_uc == unsigned char

		//Set some colors - alpha to 255
        pixelData[0] = 255;
		pixelData[3] = 255; //alpha
        pixelData[5] = 255;
        pixelData[7] = 255; //alpha
        pixelData[10] = 255;
		pixelData[11] = 255; //alpha
        pixelData[12] = 255;
        pixelData[13] = 255;
		pixelData[15] = 255; //alpha

        bufferSize = 16;  // 2 * 2 * 4 bytes
        texChannels = 4;
        texWidth = 2;
        texHeight = 2;
        stagingBuffer = createGeneralBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        void* data{};
        mDeviceFunctions->vkMapMemory(mWindow->device(), stagingBuffer.mBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, pixelData, bufferSize);
    }
	//if the file is open, we read the data into the imageFileData vector using stb_image
    else
    {
        const std::uint32_t size = std::filesystem::file_size(filename);
        std::vector<std::uint8_t> imageFileData(size);
        file.read(reinterpret_cast<char*>(imageFileData.data()), size);

        //Use the stb_image library to load the image
		//Force all images to RGBA format
        pixelData = stbi_load_from_memory(imageFileData.data(), size, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

		//texChannels might be 1, 3 or 4, so hardcode it to 4
        bufferSize = 4 * texWidth * texHeight;
        stagingBuffer = createGeneralBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        void* data{};
        mDeviceFunctions->vkMapMemory(mWindow->device(), stagingBuffer.mBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, pixelData, bufferSize);
    }

	mDeviceFunctions->vkUnmapMemory(mWindow->device(), stagingBuffer.mBufferMemory);
                                         
	TextureHandle textureHandle = createImage(texWidth, texHeight, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, format);

    transitionImageLayout(textureHandle.mImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copyBufferToImage(stagingBuffer.mBuffer, textureHandle.mImage, texWidth, texHeight);
    transitionImageLayout(textureHandle.mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	textureHandle.mImageView = createImageView(textureHandle.mImage, format);

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocateInfo.descriptorPool = mTextureDescriptorPool;
    descriptorSetAllocateInfo.descriptorSetCount = 1;
    descriptorSetAllocateInfo.pSetLayouts = &mTextureDescriptorSetLayout;

    VkResult err = mDeviceFunctions->vkAllocateDescriptorSets(mWindow->device(), &descriptorSetAllocateInfo, &textureHandle.mTextureDescriptorSet);
    if (err != VK_SUCCESS) {
        std::exit(EXIT_FAILURE);
    }

    VkDescriptorImageInfo descriptorImageInfo{};
    descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    descriptorImageInfo.imageView = textureHandle.mImageView;
    descriptorImageInfo.sampler = mTextureSampler;

    VkWriteDescriptorSet writeDescriptorSet{};
    writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet.dstSet = textureHandle.mTextureDescriptorSet;
    writeDescriptorSet.dstBinding = 0;
    writeDescriptorSet.dstArrayElement = 0;
    writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writeDescriptorSet.descriptorCount = 1;
    writeDescriptorSet.pImageInfo = &descriptorImageInfo;

    mDeviceFunctions->vkUpdateDescriptorSets(mWindow->device(), 1, &writeDescriptorSet, 0, nullptr);
	
    destroyBuffer(stagingBuffer);

	stbi_image_free(pixelData);

	return textureHandle;
}

TextureHandle Renderer::createImage(int width, int height, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkFormat format)
{
    TextureHandle textureHandle{};

    VkImageCreateInfo textureInfo{};
    textureInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;    // set the structure type
    textureInfo.usage = usage;                                   // buffer usage type
    textureInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	textureInfo.imageType = VK_IMAGE_TYPE_2D;
    textureInfo.extent.width = width;
	textureInfo.extent.height = height;
	textureInfo.extent.depth = 1;
	textureInfo.mipLevels = 1;
	textureInfo.arrayLayers = 1;
	textureInfo.format = format;
	textureInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	textureInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	//textureInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	textureInfo.samples = VK_SAMPLE_COUNT_1_BIT;        // No multisampling
	//textureInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	textureInfo.flags = 0;

    VkResult err = mDeviceFunctions->vkCreateImage(mWindow->device(), &textureInfo, nullptr, &textureHandle.mImage);
    if (err != VK_SUCCESS)
    {
        qFatal("Failed to create image buffer: %d", err);
    }

    VkMemoryRequirements memoryRequirements{};
    mDeviceFunctions->vkGetImageMemoryRequirements(mWindow->device(), textureHandle.mImage, &memoryRequirements);

    // Manually find a memory type
    uint32_t chosenMemoryType = findMemoryType(memoryRequirements.memoryTypeBits, properties);

    VkMemoryAllocateInfo memoryAllocateInfo{};
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.allocationSize = memoryRequirements.size;
    memoryAllocateInfo.memoryTypeIndex = chosenMemoryType;      //Qt has a helper function for this mWindow->hostVisibleMemoryIndex();

    err = mDeviceFunctions->vkAllocateMemory(mWindow->device(), &memoryAllocateInfo, nullptr, &textureHandle.mTextureMemory);
    if (err != VK_SUCCESS)
    {
        qFatal("Failed to allocate image memory: %d", err);
    }

    mDeviceFunctions->vkBindImageMemory(mWindow->device(), textureHandle.mImage, textureHandle.mTextureMemory, 0);

    return textureHandle;
}

void Renderer::transitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout)
{
    VkCommandBuffer commandBuffer = BeginTransientCommandBuffer();

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage{};
    VkPipelineStageFlags destinationStage{};

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }

    mDeviceFunctions->vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    EndTransientCommandBuffer(commandBuffer);
}

void Renderer::copyBufferToImage(VkBuffer buffer, VkImage image, int width, int height)
{
    VkCommandBuffer commandBuffer = BeginTransientCommandBuffer();

	VkBufferImageCopy region{};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), 1 };

    mDeviceFunctions->vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    EndTransientCommandBuffer(commandBuffer);
}

VkImageView Renderer::createImageView(VkImage image, VkFormat format)
{
    VkImageViewCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    info.image = image;
    info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    info.format = format;
    info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    info.subresourceRange.baseMipLevel = 0;
    info.subresourceRange.levelCount = 1;
    info.subresourceRange.baseArrayLayer = 0;
    info.subresourceRange.layerCount = 1;

    VkImageView view;
    VkResult err = mDeviceFunctions->vkCreateImageView(mWindow->device(), &info, nullptr, &view);
    if (err != VK_SUCCESS) {
        std::exit(EXIT_FAILURE);
    }

    return view;
}

void Renderer::destroyTexture(TextureHandle& textureHandle)
{
	mDeviceFunctions->vkDeviceWaitIdle(mWindow->device());
	mDeviceFunctions->vkFreeDescriptorSets(mWindow->device(), mTextureDescriptorPool, 1, &textureHandle.mTextureDescriptorSet);
	mDeviceFunctions->vkDestroyImageView(mWindow->device(), textureHandle.mImageView, nullptr);
    mDeviceFunctions->vkDestroyImage(mWindow->device(), textureHandle.mImage, nullptr);
	mDeviceFunctions->vkFreeMemory(mWindow->device(), textureHandle.mTextureMemory, nullptr);
}
