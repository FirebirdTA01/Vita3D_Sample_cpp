#include "Graphics.h"
#include "commonUtils.h"

#include <string.h>
#include <assert.h>

#include <psp2/kernel/processmgr.h>
#include <psp2/sysmodule.h>
#include <psp2/types.h>

//These callbacks need to be static and not a class object (compiler complains of conversion)
//Callback when displaying a frame buffer
static void displayBufferCallback(const void *callbackData);
//Callback function which allocates memory for the shader patcher
static void *allocPatcherMem(void *userData, SceSize size);
//Callback which frees shader patcher memory
static void freePatcherMem(void *userData, void *memory);

/*----- Initialization related functions start here -----*/

Graphics::Graphics()
{
	initialized = false;

	//Gxm context and its parameters
	gxmContext_ptr = nullptr;
	//gxmContextParams = NULL;
	/* Render target and its parameters */
	gxmRenderTarget_ptr = nullptr;
	//gxmRenderTargetParams = NULL;

	/* Display buffers, color surfaces and sync objects */
	for (int i = 0; i < DISPLAY_BUFFER_COUNT; i++)
	{
		_displayBuffers[i] = nullptr;
		_displayBufferUIDs[i] = -1;
		//GXM color surfaces and sync objects for much faster rendering
		//_colorSurfaces[i] = NULL;
		_displaySyncObjects[i] = nullptr;
	}
	
	/* frame buffer indexes */
	backBufIndex = 0;
	frontBufIndex = 0;

	/* Ring buffers */
	//TO DO: further comment the purpose/function of each of these
	//ring buffers
	vdmRingBuf_ptr = nullptr;
	vertexRingBuf_ptr = nullptr;
	fragmentRingBuf_ptr = nullptr;
	fragmentUsseRingBuf_ptr = nullptr;
	vertexUsseRingBuf_ptr = nullptr;
	vdmRingBufUID = -1;
	vertexRingBufUID = -1;
	fragmentRingBufUID = -1;
	fragmentUsseRingBufUID = -1;
	vertexUsseRingBufUID = -1;
	fragmentUsseRingBufOffset = 0;
	vertexUsseRingBufOffset = 0;
	//depth buffer
	depthBuf_ptr = nullptr;
	depthBufUID = -1;
	//depth stencil surface
	//depthStencilSurface = NULL;
	/* Shader patcher and its parameters */
	patcher_ptr = nullptr;
	//patcherParams = NULL;
	patcherBufUID = -1;
	patcherBuf_ptr = nullptr;
	patcherVertexUsseUID = -1;
	patcherVertexUsse_ptr = nullptr;
	patcherVertexUsseOffset = 0;
	patcherFragmentUsseUID = -1;
	patcherFragmentUsse_ptr = nullptr;
	patcherFragmentUsseOffset = 0;
	_registeredProgramIDs.clear();
	//patcherProgramCreationParams
	//default creation vertex stream
	_vertexStreamMap.clear();
	createdStreams = 0;
	currentStreamType = GXM_CLEAR_INDEX_16BIT;
	outputRegisterFormat = SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4;

	_vertexPrograms.clear();
	_fragmentPrograms.clear();
}

Graphics::~Graphics()
{

}

Graphics* Graphics::getInstance()
{
	static Graphics instance;
	return &instance;
}

void Graphics::initGraphics()
{
	//this is set by the return values of many functions to check for success
	int error = 0;

	if (initialized)
	{
		vitaPrintf("Graphics System is already initialized!\n");
		return;
	}

	//Start by initializing libgxm
	vitaPrintf("Initializing graphics system\n");
	//set up the parameters
	SceGxmInitializeParams gxmInitParams;
	memset(&gxmInitParams, 0, sizeof(SceGxmInitializeParams));
	gxmInitParams.flags							= 0;
	gxmInitParams.displayQueueMaxPendingCount	= DISPLAY_MAX_PENDING_SWAPS;
	gxmInitParams.displayQueueCallback			= displayBufferCallback;
	gxmInitParams.displayQueueCallbackDataSize	= sizeof(DisplayData);
	gxmInitParams.parameterBufferSize			= SCE_GXM_DEFAULT_PARAMETER_BUFFER_SIZE; //use default parameter buffer size (16MB)

	//now try initializing with those parameters
	vitaPrintf("Initializing SCE GXM\n");
	error = sceGxmInitialize(&gxmInitParams);
	vitaPrintf("sceGxmInitialize() result: 0x%08X\n", error);
	assert(error == 0);
	//{
		//TO DO: use Logger to log the error and then close its output stream
		//TO DO: use sceImeDialog to display the error
	//}

	//----------------------------------------------------------------------------------
	//Assuming the above was successful, now we create a libgxm context
	//This rendering context is what allows us to render scenes on the GPU
	//start by allocating default ringBuf memory sizes
	//----------------------------------------------------------------------------------
	vitaPrintf("Setting up ring buffers...\n");
	//vdm
	vitaPrintf("\nAllocating memory for the VDM ring buffer...\n");
	vdmRingBuf_ptr = allocGraphicsMem(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
		SCE_GXM_DEFAULT_VDM_RING_BUFFER_SIZE,
		4,
		SCE_GXM_MEMORY_ATTRIB_READ,
		&vdmRingBufUID
	);
	//vertex
	vitaPrintf("\nAllocating memory for the vertex ring buffer...\n");
	vertexRingBuf_ptr = allocGraphicsMem(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
		SCE_GXM_DEFAULT_VERTEX_RING_BUFFER_SIZE,
		4,
		SCE_GXM_MEMORY_ATTRIB_READ,
		&vertexRingBufUID
	);
	//fragment
	vitaPrintf("\nAllocating memory for the fragment ring buffer...\n");
	fragmentRingBuf_ptr = allocGraphicsMem(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
		SCE_GXM_DEFAULT_FRAGMENT_RING_BUFFER_SIZE,
		4,
		SCE_GXM_MEMORY_ATTRIB_READ,
		&fragmentRingBufUID
	);
	//fragment USSE
	vitaPrintf("\nAllocating memory for the fragment USSE ring buffer...\n");
	fragmentUsseRingBuf_ptr = allocFragmentUsseMem(
		SCE_GXM_DEFAULT_FRAGMENT_USSE_RING_BUFFER_SIZE,
		&fragmentUsseRingBufUID,
		&fragmentUsseRingBufOffset
	);

	vitaPrintf("\nSetting libgmx render context parameters - Using defaults\n");
	//now we set the libgxm render context parameters
	memset(&gxmContextParams, 0, sizeof(SceGxmContextParams));
	gxmContextParams.hostMem						= malloc(SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE);
	gxmContextParams.hostMemSize					= SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE;
	gxmContextParams.vdmRingBufferMem				= vdmRingBuf_ptr;
	gxmContextParams.vdmRingBufferMemSize			= SCE_GXM_DEFAULT_VDM_RING_BUFFER_SIZE;
	gxmContextParams.vertexRingBufferMem			= vertexRingBuf_ptr;
	gxmContextParams.vertexRingBufferMemSize		= SCE_GXM_DEFAULT_VERTEX_RING_BUFFER_SIZE;
	gxmContextParams.fragmentRingBufferMem			= fragmentRingBuf_ptr;
	gxmContextParams.fragmentRingBufferMemSize		= SCE_GXM_DEFAULT_FRAGMENT_RING_BUFFER_SIZE;
	gxmContextParams.fragmentUsseRingBufferMem		= fragmentUsseRingBuf_ptr;
	gxmContextParams.fragmentUsseRingBufferMemSize	= SCE_GXM_DEFAULT_FRAGMENT_USSE_RING_BUFFER_SIZE;
	gxmContextParams.fragmentUsseRingBufferOffset	= fragmentUsseRingBufOffset;

	//and now we FINALLY create the gxm render context we were talking about around 50 lines up
	vitaPrintf("Creating GXM context\n");
	error = sceGxmCreateContext(&gxmContextParams, &gxmContext_ptr);
	vitaPrintf("sceGxmCreateContext() result: 0x%08X\n", error);
	assert(error == 0);

	//---------------------------------------------------------------------------------------------------
	//Now we have to create the render target which describes the geometry of the back buffers we will 
	//be rendering to. The render target is used purely for scheduling render jobs for given dimensions.
	//The color surface, as well as the depth and stencil surface must be allocated seperately
	//--------------------------------------------------------------------------------------------------
	vitaPrintf("\nSetting render target parameters\n");
	//set up parameters
	memset(&gxmRenderTargetParams, 0, sizeof(SceGxmRenderTargetParams));
	gxmRenderTargetParams.flags				= 0;				//Bitwise combined flags from #SceGxmRenderTargetFlags.
	gxmRenderTargetParams.width				= DISPLAY_WIDTH;
	gxmRenderTargetParams.height				= DISPLAY_HEIGHT;
	gxmRenderTargetParams.scenesPerFrame		= 1;				//The expected number of scenes per frame, in the range [1,#SCE_GXM_MAX_SCENES_PER_RENDERTARGET]
	gxmRenderTargetParams.multisampleMode		= MSAA_MODE;		//A value from the #SceGxmMultisampleMode enum.
	gxmRenderTargetParams.multisampleLocations = 0;				//If enabled in the flags, the multisample locations to use.
	gxmRenderTargetParams.driverMemBlock		= -1;				//The uncached LPDDR memblock for the render target GPU data structures or SCE_UID_INVALID_UID to specify memory should be allocated in libgxm.

	//And actually create the render target
	vitaPrintf("Creating the render target\n");
	error = sceGxmCreateRenderTarget(&gxmRenderTargetParams, &gxmRenderTarget_ptr);
	vitaPrintf("sceGxmCreateRenderTarget() result: 0x%08X\n", error);
	assert(error == 0);

	//---------------------------------------------------------------------------------------------
	//Allocate display buffers and sync objects. Allocate back buffers in CDRAM and create a color
	//surface for each of them. In order for display operations done by the CPU to sync up with
	//rendering done by the GPU, we also use SceGxmSyncObjects for each display buffer. This object
	//is used by each scene that renders to that buffer and that buffer is queued for display flips (whether to or from)
	//---------------------------------------------------------------------------------------------
	vitaPrintf("\nAllocating display buffers and sync objects...\n");
	//allocate memory/sync objects for frame buffers
	for (uint32_t i = 0; i < DISPLAY_BUFFER_COUNT; i++)
	{
		vitaPrintf("\nWorking on display buffer: %d\n", i);
		//allocate large alignment (1MB) memory to ensure it's physically continuous/not broken
		_displayBuffers[i] = allocGraphicsMem(
			SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,
			//ALIGN_MEM(4 * DISPLAY_STRIDE_IN_PIXELS * DISPLAY_HEIGHT, 1 * 1024 * 1024),
			4 * DISPLAY_STRIDE_IN_PIXELS * DISPLAY_HEIGHT,
			SCE_GXM_COLOR_SURFACE_ALIGNMENT,
			SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
			&_displayBufferUIDs[i]
		);

		vitaPrintf("Setting the buffer to a noticeable color\n");
		//set the buffer to a noticeable debug color
		for (uint32_t j = 0; j < DISPLAY_HEIGHT; j++)
		{
			uint32_t *row = (uint32_t *)_displayBuffers[i] + j * DISPLAY_STRIDE_IN_PIXELS;

			for (uint32_t y = 0; y < DISPLAY_WIDTH; y++)
				row[y] = COLOR_RED;
		}

		vitaPrintf("Initializing gxm color surface for this buffer\n");
		//color surface for this display buffer
		error = sceGxmColorSurfaceInit(
			&_colorSurfaces[i],
			DISPLAY_COLOR_FORMAT,
			SCE_GXM_COLOR_SURFACE_LINEAR,
			(MSAA_MODE == SCE_GXM_MULTISAMPLE_NONE) ? SCE_GXM_COLOR_SURFACE_SCALE_NONE : SCE_GXM_COLOR_SURFACE_SCALE_MSAA_DOWNSCALE,
			SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT,
			DISPLAY_WIDTH,
			DISPLAY_HEIGHT,
			DISPLAY_STRIDE_IN_PIXELS,
			_displayBuffers[i]
		);
		vitaPrintf("sceGxmColorSurfaceInit() result: 0x%08X\n", error);
		assert(error == 0);

		//create a sync object to be associated with this buffer
		vitaPrintf("Creating a sync object for this buffer\n");
		error = sceGxmSyncObjectCreate(&_displaySyncObjects[i]);
		vitaPrintf("sceGxmSyncObjectCreate() result: 0x%08X\n", error);
		assert(error == 0);
	}

	//---------------------------------------------------------------------------------------------
	//Next step is allocating a depth buffer. This application renders strictly in a back-to-front
	//order, which means that a depth buffer is not really required. However, creating one is normally
	//required to handle partial renders. This depth buffer will be created without enabling force load
	//or store, so it will not actually be read or written by the GPU and will have zero impact on perfomance
	//----------------------------------------------------------------------------------------------

	//for antialiasing
	const uint32_t alignedWidth = ALIGN_MEM(DISPLAY_WIDTH, SCE_GXM_TILE_SIZEX);
	const uint32_t alignedHeight = ALIGN_MEM(DISPLAY_HEIGHT, SCE_GXM_TILE_SIZEY);
	uint32_t sampleCount = alignedWidth * alignedHeight;
	uint32_t depthStrideInSamples = alignedWidth;
	if (MSAA_MODE == SCE_GXM_MULTISAMPLE_4X)
	{
		vitaPrintf("\nSetting up 4x antialiasing\n");
		//increase samples across x and y
		sampleCount *= 4;
		depthStrideInSamples *= 2;
	}
	else if (MSAA_MODE == SCE_GXM_MULTISAMPLE_2X)
	{
		vitaPrintf("\nSetting up 2x antialiasing\n");
		//increase samples across Y only
		sampleCount *= 2;
	}
	else
		vitaPrintf("\nNot using antialiasing\n");

	//allocate depth buffer memory
	vitaPrintf("\nCreating the depth-buffer\n");
	depthBuf_ptr = allocGraphicsMem(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
		sampleCount * 4,
		SCE_GXM_DEPTHSTENCIL_SURFACE_ALIGNMENT,
		SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
		&depthBufUID
	);

	//set the depth stencil structure
	vitaPrintf("Initializing depth stencil surface\n");
	error = sceGxmDepthStencilSurfaceInit(
		&depthStencilSurface,
		SCE_GXM_DEPTH_STENCIL_FORMAT_S8D24,
		SCE_GXM_DEPTH_STENCIL_SURFACE_TILED,
		depthStrideInSamples,
		depthBuf_ptr,
		NULL
	);
	vitaPrintf("sceGxmDepthStencilSurfaceInit() result: 0x%08X\n", error);
	assert(error == 0);

	//Initialize the shader patcher in its own function
	//This keeps the code cleaner/easier to read and it also allows the seperate
	//initialization of the patcher using different patcher sizes without clogging up the
	//Graphics::init() parameters
	//we want to use shaders, so init the patcher
	initShaderPatcher(&defaultPatcher);

	initialized = true;
}

//TO DO:
void Graphics::initGraphics(SceGxmInitializeParams* parameters)
{

}

void Graphics::initShaderPatcher(PatcherSizes* sizes)
{
	//-------------------------------------------------------------------------------------------
	//On to Shader Patcher Programs!!, we're getting somewhere now
	//Shader patcher objects are required to produce vertex/fragment programs from the shader
	//compiler output. First is to take our shader patcher and issue callbacks to allocate/free
	//host memory for the internal state
	//We use the shader patcher's internal heap to handle buffer/USSE memory for the final programs.
	//To do this, leave the callback functions as NULL, but provide pointers to static memory blocks
	//To create vertex and fragment programs for a particular shader, the compiled .gxp must be
	//registered to get an ID for that shader. In an ID, vertex and fragment programs are reference
	//counted and can be shared if created with the same parameters. The maximize this sharing, shader
	//programs should only be registered with the shader patcher once if possible. This is where
	//we do that
	//------------------------------------------------------------------------------------------
	vitaPrintf("\nSetting up shader patcher\n");
	//allocate memory for buffers and USSE code
	vitaPrintf("\nAllocating memory for the shader patcher buffer\n");
	patcherBuf_ptr = allocGraphicsMem(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
		(SceSize)sizes->patchBufferSize,
		4,
		SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
		&patcherBufUID
	);

	vitaPrintf("\nAllocating memory for patcher's vertex USSE programs\n");
	patcherVertexUsse_ptr = allocVertexUsseMem(
		(SceSize)sizes->patchVertexUsseSize,
		&patcherVertexUsseUID,
		&patcherVertexUsseOffset
	);

	vitaPrintf("\nAllocating memory for patcher's fragment USSE programs\n");
	patcherFragmentUsse_ptr = allocFragmentUsseMem(
		(SceSize)sizes->patchFragmentUsseSize,
		&patcherFragmentUsseUID,
		&patcherFragmentUsseOffset
	);

	vitaPrintf("\nSetting shader patcher parameters\n");
	//create a shader patcher
	memset(&patcherParams, 0, sizeof(SceGxmShaderPatcherParams));
	patcherParams.userData = NULL;
	patcherParams.hostAllocCallback = &allocPatcherMem;
	patcherParams.hostFreeCallback = &freePatcherMem;
	patcherParams.bufferAllocCallback = NULL;
	patcherParams.bufferFreeCallback = NULL;
	patcherParams.bufferMem = patcherBuf_ptr;
	patcherParams.bufferMemSize = (SceSize)sizes->patchBufferSize;
	patcherParams.vertexUsseAllocCallback = NULL;
	patcherParams.vertexUsseFreeCallback = NULL;
	patcherParams.vertexUsseMem = patcherVertexUsse_ptr;
	patcherParams.vertexUsseMemSize = (SceSize)sizes->patchVertexUsseSize;
	patcherParams.vertexUsseOffset = patcherVertexUsseOffset;
	patcherParams.fragmentUsseAllocCallback = NULL;
	patcherParams.fragmentUsseFreeCallback = NULL;
	patcherParams.fragmentUsseMem = patcherFragmentUsse_ptr;
	patcherParams.fragmentUsseMemSize = (SceSize)sizes->patchFragmentUsseSize;
	patcherParams.fragmentUsseOffset = patcherFragmentUsseOffset;

	vitaPrintf("\nCreating the shader patcher\n");
	int error = sceGxmShaderPatcherCreate(&patcherParams, &patcher_ptr);
	vitaPrintf("sceGxmShaderPatcherCreate() result: 0x%08X\n", error);
	assert(error == 0);
}

/*----- Initialization functions end here -----*/
  /*----- The shutdown function is here -----*/

//waits for the GXM to finish, unmaps and unallocates memory and shuts down the GXM
void Graphics::shutdownGraphics()
{
	int error = 0;
	vitaPrintf("\nShutting down graphics system\n");

	//wait for the gxm to finish
	sceGxmFinish(gxmContext_ptr);

	//unload default loaded shaders here

	//display queue must be finished before deallocating display buffers
	error = sceGxmDisplayQueueFinish();
	assert(error == 0);

	//clean up display queue
	freeGraphicsMem(depthBufUID);
	for (uint32_t i = 0; i < DISPLAY_BUFFER_COUNT; i++)
	{
		//clear buffer and deallocate
		memset(_displayBuffers[i], 0, DISPLAY_HEIGHT * DISPLAY_STRIDE_IN_PIXELS * 4);
		freeGraphicsMem(_displayBufferUIDs[i]);

		//destroy sync object
		sceGxmSyncObjectDestroy(_displaySyncObjects[i]);
	}

	//destroy shader patcher
	vitaPrintf("\nCleaning up shader patcher\n");
	//PROGRAMS MUST BE UNREGISTERED FIRST
	patcherUnregisterPrograms();
	sceGxmShaderPatcherDestroy(patcher_ptr);
	freeFragmentUsseMem(patcherFragmentUsseUID);
	freeVertexUsseMem(patcherVertexUsseUID);    //TO DO: This needs done, there's something wrong with it which makes the app crash on exit
	freeGraphicsMem(patcherBufUID);

	// destroy the render target
	vitaPrintf("Destroying the render target\n");
	sceGxmDestroyRenderTarget(gxmRenderTarget_ptr);

	// destroy the context and ring buffers
	vitaPrintf("Destroying the gxm context\n");
	sceGxmDestroyContext(gxmContext_ptr);
	freeFragmentUsseMem(fragmentUsseRingBufUID);
	freeGraphicsMem(fragmentRingBufUID);
	freeGraphicsMem(vertexRingBufUID);
	freeGraphicsMem(vdmRingBufUID);
	free(gxmContextParams.hostMem);

	// terminate libgxm
	vitaPrintf("Terminating the GXM\n");
	sceGxmTerminate();
}

/*----- The shutdown function ends here -----*/
 /*----- Drawing functions start here -----*/

void Graphics::startScene()
{
	sceGxmBeginScene(
		gxmContext_ptr,
		0,
		gxmRenderTarget_ptr,
		NULL,
		NULL,
		_displaySyncObjects[backBufIndex],
		&_colorSurfaces[backBufIndex],
		&depthStencilSurface
	);
}

void Graphics::endScene()
{
	sceGxmEndScene(gxmContext_ptr, NULL, NULL);

	//PA heartbeat to notify end of frame
	sceGxmPadHeartbeat(&_colorSurfaces[backBufIndex], _displaySyncObjects[backBufIndex]);

	swapBuffers();
}

void Graphics::swapBuffers()
{
	DisplayData displayData;
	displayData.addr = _displayBuffers[backBufIndex];
	sceGxmDisplayQueueAddEntry(
		_displaySyncObjects[frontBufIndex],	//OLD buffer
		_displaySyncObjects[backBufIndex],	//NEW buffer
		&displayData
	);

	//update index
	frontBufIndex = backBufIndex;
	backBufIndex = (backBufIndex + 1) % DISPLAY_BUFFER_COUNT;
}

//TO DO: These should be updated to use built in clear vertex/fragment shaders to do this correctly
void Graphics::clearScreen()
{

	for (uint32_t i = 0; i < DISPLAY_HEIGHT; i++)
	{
		uint32_t *row = (uint32_t *)_displayBuffers[backBufIndex] + i * DISPLAY_STRIDE_IN_PIXELS;

		for (uint32_t j = 0; j < DISPLAY_WIDTH; j++)
			row[j] = COLOR_BLACK;
	}
}
void Graphics::clearScreen(uint32_t color)
{
	for (uint32_t i = 0; i < DISPLAY_HEIGHT; i++)
	{
		uint32_t *row = (uint32_t *)_displayBuffers[backBufIndex] + i * DISPLAY_STRIDE_IN_PIXELS;

		for (uint32_t j = 0; j < DISPLAY_WIDTH; j++)
			row[j] = color;
	}
}

void Graphics::draw(SceGxmPrimitiveType primitive, SceGxmIndexFormat format, const void *indexData, unsigned int indexCount)
{
	sceGxmDraw(gxmContext_ptr, primitive, format, indexData, indexCount);
}

     /*----- Drawing functions end here -----*/
/*----- Shader related functions start here -----*/

SceGxmShaderPatcherId Graphics::patcherRegisterProgram(const SceGxmProgram *const programHeader)
{
	int error = 0;

	//check the program
	vitaPrintf("\nChecking shader program\nProgram address: %p\n", programHeader);
	error = sceGxmProgramCheck(programHeader);
	vitaPrintf("sceGxmProgramCheck() result: 0x%08X\n", error);

	SceGxmShaderPatcherId programID;
	vitaPrintf("Registering shader program with the patcher\n");
	error = sceGxmShaderPatcherRegisterProgram(patcher_ptr, programHeader, &programID);
	vitaPrintf("sceGxmShaderPatcherRegisterProgram() result: 0x%08X\n", error);
	//assert(error == 0);

	_registeredProgramIDs.push_back(programID);

	//return the last element in the vector (what we just added)
	return _registeredProgramIDs.back();
}

void Graphics::patcherUnregisterPrograms()
{
	int error = 0;
	std::vector<SceGxmShaderPatcherId>::iterator iter;
	for (iter = _registeredProgramIDs.begin(); iter != _registeredProgramIDs.end(); iter++)
	{
		vitaPrintf("Unregistering shader program from the patcher\nProgram ID: %d\n", *iter);
		error = sceGxmShaderPatcherUnregisterProgram(patcher_ptr, *iter);
		vitaPrintf("sceGxmShaderPatcherRegisterProgram() result: 0x%08X\n", error);
		//assert(error == 0);
	}
}

//TO DO: make this a template function able to change many "program creation params" by taking two parameters; First - const char* of the parameter to change, Second - it's value
//Future usage: patcherSetProgramCreationParams("VertexStreamType", GXM_BASIC_INDEX_16BIT); patcherSetProgramCreationParams("OutputRegFormat", SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4); ect etc
void Graphics::patcherSetProgramCreationParams(VertexStreamType streamType)
{
	vitaPrintf("\nProgram creation parameter requested change!\n");
	vitaPrintf("Changing vertex stream type...\n");
	//check if a compatible stream already exists
	std::map<VertexStreamType, const SceGxmVertexStream*>::iterator iter;
	iter = _vertexStreamMap.find(streamType);
	if (iter != _vertexStreamMap.end())
	{
		vitaPrintf("Setting vertex stream to type: %u\n", streamType);
		currentStreamType = streamType;
	}
	else
	{
		vitaPrintf("A vertex program requests a vertex stream that doesn't exist yet! Creating one\n");
		vitaPrintf("Vertex stream type: %u\n", streamType);
		createdStreams++;
		switch (currentStreamType)
		{
		case GXM_CLEAR_INDEX_16BIT:
			_vertexStreams[createdStreams - 1].stride = sizeof(ClearVertex);
			_vertexStreams[createdStreams - 1].indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;
			break;
		case GXM_CLEAR_INDEX_32BIT:
			_vertexStreams[createdStreams - 1].stride = sizeof(ClearVertex);
			_vertexStreams[createdStreams - 1].indexSource = SCE_GXM_INDEX_SOURCE_INDEX_32BIT;
			break;
		case GXM_CLEAR_INSTANCE_16BIT:
			_vertexStreams[createdStreams - 1].stride = sizeof(ClearVertex);
			_vertexStreams[createdStreams - 1].indexSource = SCE_GXM_INDEX_SOURCE_INSTANCE_16BIT;
			break;
		case GXM_CLEAR_INSTANCE_32BIT:
			_vertexStreams[createdStreams - 1].stride = sizeof(ClearVertex);
			_vertexStreams[createdStreams - 1].indexSource = SCE_GXM_INDEX_SOURCE_INSTANCE_32BIT;
			break;
		case GXM_BASIC_INDEX_16BIT:
			_vertexStreams[createdStreams - 1].stride = sizeof(BasicVertex);
			_vertexStreams[createdStreams - 1].indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;
			break;
		case GXM_BASIC_INDEX_32BIT:
			_vertexStreams[createdStreams - 1].stride = sizeof(BasicVertex);
			_vertexStreams[createdStreams - 1].indexSource = SCE_GXM_INDEX_SOURCE_INDEX_32BIT;
			break;
		case GXM_BASIC_INSTANCE_16BIT:
			_vertexStreams[createdStreams - 1].stride = sizeof(BasicVertex);
			_vertexStreams[createdStreams - 1].indexSource = SCE_GXM_INDEX_SOURCE_INSTANCE_16BIT;
			break;
		case GXM_BASIC_INSTANCE_32BIT:
			_vertexStreams[createdStreams - 1].stride = sizeof(BasicVertex);
			_vertexStreams[createdStreams - 1].indexSource = SCE_GXM_INDEX_SOURCE_INSTANCE_32BIT;
			break;
		default:
			vitaPrintf("\nERROR: Unknown vertex stream type!\n");
			return;
		}

		vitaPrintf("New stream parameters...\nStride: %u\nIndex source: %u\n", _vertexStreams[createdStreams - 1].stride, _vertexStreams[createdStreams -1].indexSource);
		currentStreamType = streamType;
		_vertexStreamMap.insert(iter, std::make_pair(streamType, _vertexStreams));
	}
}

SceGxmVertexProgram* Graphics::patcherCreateVertexProgram(SceGxmShaderPatcherId programID, SceGxmVertexAttribute* attributes, int attributeCount, ...)
{
	int error = 0;

	vitaPrintf("\nCreating shader patcher vertex program from program with ID: %u\n", programID);
	//first get the linked to / registered program
	const SceGxmProgram *binaryProgram_ptr = sceGxmShaderPatcherGetProgramFromId(programID);
	assert(binaryProgram_ptr);

	//go through the argument list to get the shader program's attribute names
	va_list vl;
	va_start(vl, attributeCount);
	for (int i = 0; i < attributeCount; i++)
	{
		const char* attributeName = va_arg(vl, const char*);
		vitaPrintf("Adding vertex program attribute: %s\n", attributeName);
		const SceGxmProgramParameter *vertexProgramAttribute_ptr = sceGxmProgramFindParameterByName(binaryProgram_ptr, attributeName);
		assert(vertexProgramAttribute_ptr && (sceGxmProgramParameterGetCategory(vertexProgramAttribute_ptr) == SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE));
		vitaPrintf("Setting vertex attribute.regIndex: ");
		attributes[i].regIndex = sceGxmProgramParameterGetResourceIndex(vertexProgramAttribute_ptr);
		vitaPrintf("%d\n", attributes[i].regIndex);
	}
	va_end(vl);

	//which vertex stream is being used
	//Set vertex stream appropriately
	std::map<VertexStreamType, const SceGxmVertexStream*>::const_iterator currentStream;
	currentStream = _vertexStreamMap.find(currentStreamType);
	if (currentStream == _vertexStreamMap.end())
	{
		vitaPrintf("ERROR: Could not find correct vertex stream in _vertexStreams<>!!!\n");
		return nullptr;
	}

	vitaPrintf("\nVertex Program and Stream Attributes...\n");
	vitaPrintf("Pointer the the patcher at address: %p\n", patcher_ptr);
	vitaPrintf("ProgramID: %u\n", programID);
	vitaPrintf("Program attributes at address: %p\n", attributes);
	vitaPrintf("\tAttribute count: %d\n", attributeCount);
	for (int j = 0; j < attributeCount; j++)
	{
		vitaPrintf("\tAttribute %u - stream index: %u\n", j, attributes[j].streamIndex);
		vitaPrintf("\tAttribute %u - offset: %u\n", j, attributes[j].offset);
		vitaPrintf("\tAttribute %u - format: 0x%08\n", j, attributes[j].format);
		vitaPrintf("\tAttribute %u - component count: %u\n", j, attributes[j].componentCount);
		vitaPrintf("\tAttribute %u - reg index: %d\n", j, attributes[j].regIndex);
	}
	vitaPrintf("Current vertex stream at address: %p\n", currentStream->second);
	vitaPrintf("Stream count: %d\n", 1);
	vitaPrintf("\tStream Type: %u\n", currentStreamType);
	vitaPrintf("\tStream Attribute - stride: %d\n", currentStream->second[createdStreams - 1].stride);
	vitaPrintf("\tStream Attribute - index source: 0x%08\n", currentStream->second[createdStreams - 1].indexSource);

	SceGxmVertexProgram* vertexProgram_ptr = nullptr;
	error = sceGxmShaderPatcherCreateVertexProgram(
		patcher_ptr,
		programID,
		attributes,
		attributeCount,
		currentStream->second,
		1,						//TO DO: add support for multiple vertex streams
		&vertexProgram_ptr
	);
	vitaPrintf("sceGxmShaderPatcherCreateVertexProgram() result: 0x%08\n", error);
	assert(error == 0);

	//pushback to vector or map containing loaded programs
	_vertexPrograms.push_back(vertexProgram_ptr);

	return vertexProgram_ptr;
}

SceGxmFragmentProgram* Graphics::patcherCreateFragmentProgram(SceGxmShaderPatcherId programID, SceGxmShaderPatcherId vertexProgramID)
{
	vitaPrintf("\nCreating shader patcher fragment program from program with ID: %u\n", programID);
	
	vitaPrintf("\nFragment Program Attributes...\n");
	vitaPrintf("Pointer the the patcher at address: %p\n", patcher_ptr);
	vitaPrintf("ProgramID: %u\n", programID);
	vitaPrintf("Settings used for program creation:\n");
	vitaPrintf("\tOutput Register Format: 0x%08\n", outputRegisterFormat);
	vitaPrintf("\tAnti-aliasing mode: 0x%08\n", MSAA_MODE);
	vitaPrintf("\tBlend info at address: NOT USED\n");
	vitaPrintf("Using vertex program with ID: %u\n", vertexProgramID);

	SceGxmFragmentProgram* fragmentProgram_ptr;
	int error = sceGxmShaderPatcherCreateFragmentProgram(
		patcher_ptr,
		programID,
		outputRegisterFormat,							//Output format for the fragment program <c>COLOR0</c>
		MSAA_MODE,														//Multisample mode
		NULL,															//Pointer to the blend info structure, or null
		sceGxmShaderPatcherGetProgramFromId(vertexProgramID),		//Pointer to the vertex program (The GXP), or null
		&fragmentProgram_ptr										//Double pointer to storage for fragment program
	);
	vitaPrintf("sceGxmShaderPatcherCreateFragmentProgram() result: 0x%08\n", error);
	assert(error == 0);

	//pushback to vector containing loaded programs
	_fragmentPrograms.push_back(fragmentProgram_ptr);

	return fragmentProgram_ptr;
}

void Graphics::patcherSetVertexProgram(const SceGxmVertexProgram* program)
{
	//vitaPrintf("\nSetting vertex program to program at address: %p\n", program);
	sceGxmSetVertexProgram(gxmContext_ptr, program);
}

void Graphics::patcherSetFragmentProgram(const SceGxmFragmentProgram* program)
{
	//vitaPrintf("\nSetting fragment program to program at address: %p\n", program);
	sceGxmSetFragmentProgram(gxmContext_ptr, program);
}

void Graphics::patcherSetVertexStream(unsigned int streamIndex, const void* vertices)
{
	//vitaPrintf("\nSetting vertex stream...\n");
	/*
	vitaPrintf("Setting vertex stream...\n");
	vitaPrintf("\tStreamIndex: %u\n", streamIndex);
	vitaPrintf("\tStream at address: %p\n", vertices);
	*/
	/*
	BasicVertex *const vertexInfo = (BasicVertex *const) vertices;
	vitaPrintf("Vertice information:\n");
	vitaPrintf("\tVertice[0].x: %f\n", vertexInfo[0].x);
	vitaPrintf("\tVertice[0].y: %f\n", vertexInfo[0].y);
	vitaPrintf("\tVertice[0].z: %f\n", vertexInfo[0].z);
	//vitaPrintf("\tVertice[0].color: %u\n", vertexInfo[0].color);
	vitaPrintf("\tVertice[1].x: %f\n", vertexInfo[1].x);
	vitaPrintf("\tVertice[1].y: %f\n", vertexInfo[1].y);
	vitaPrintf("\tVertice[1].z: %f\n", vertexInfo[1].z);
	//vitaPrintf("\tVertice[1].color: %u\n", vertexInfo[1].color);
	vitaPrintf("\tVertice[2].x: %f\n", vertexInfo[2].x);
	vitaPrintf("\tVertice[2].y: %f\n", vertexInfo[2].y);
	vitaPrintf("\tVertice[2].z: %f\n", vertexInfo[2].z);
	//vitaPrintf("\tVertice[2].color: %u\n", vertexInfo[2].color);
	*/

	sceGxmSetVertexStream(gxmContext_ptr, streamIndex, vertices);
}

void Graphics::patcherSetVertexProgramConstants(void* uniformBuffer, const SceGxmProgramParameter* worldViewProjection, unsigned int componentOffset, unsigned int componentCount, const float *sourceData)
{
	/*
	vitaPrintf("\nSetting vertex program constants...\n");
	if (uniformBuffer == NULL)
		vitaPrintf("\tReserving a NULL 'default' buffer\n");
	else
		vitaPrintf("\tReserving default buffer using one at address: %p\n", uniformBuffer);
	vitaPrintf("\tWorld view projection parameters at address: %p\n", worldViewProjection);
	vitaPrintf("\tComponent offset: %u\n", componentOffset);
	vitaPrintf("\tComponent count: %u\n", componentCount);
	vitaPrintf("\tSource data at address: %p\n", sourceData);

	vitaPrintf("SourceData contents...\n");
	for (int i = 0; i < componentCount; i++)
	{
		vitaPrintf("\tData %u: %ff\n", i, sourceData[i]);
	}
	*/
	sceGxmReserveVertexDefaultUniformBuffer(gxmContext_ptr, &uniformBuffer);
	sceGxmSetUniformDataF(uniformBuffer, worldViewProjection, componentOffset, componentCount, sourceData);
}

/*----- Shader functions end here -----*/

//accessors

/*SceGxmContext* Graphics::getGxmContext()
{
	return gxmContext_ptr;
}
SceGxmContextParams* Graphics::getGxmContextParams()
{
	return &gxmContextParams;
}
SceGxmRenderTarget* Graphics::getGxmRenderTarget()
{
	return gxmRenderTarget_ptr;
}
SceGxmColorSurface* Graphics::getColorSurface(uint32_t bufferIndex)
{
	return &_colorSurfaces[bufferIndex];
}
SceGxmDepthStencilSurface* Graphics::getDepthSurface()
{
	return &depthStencilSurface;
}
SceGxmSyncObject* Graphics::getSyncObject(uint32_t bufferIndex)
{
	return _displaySyncObjects[bufferIndex];
}
uint32_t Graphics::getBackBufferIndex()
{
	return backBufIndex;
}
SceGxmShaderPatcher* Graphics::getShaderPatcher()
{
	return shaderPatcher_ptr;
}
*/

/*/////////////////////////////////////////////////////////////////////////////////////
 *--------------------          Internal Functions          --------------------------
 *///////////////////////////////////////////////////////////////////////////////////*/

/*----- Memory Functions start here -----*/

 //Allocates memory and maps it to the GPU
void *Graphics::allocGraphicsMem(SceKernelMemBlockType type, unsigned int size, unsigned int alignment, unsigned int attributes, SceUID *uid)
{
	int error = 0;

	vitaPrintf("Allocating GPU memory...\n");
	vitaPrintf("SceKernelMemBlockType: %d\n", type);
	vitaPrintf("SceSize: %u\n", size);
	vitaPrintf("SceGxmMemoryAttribFlags: %u\n", attributes);

	/*	Here we use sceKernelAllocMemBlock directly, this means we cannot directly
	use the alignment parameter.  Instead, allocate the minimum size for this memblock
	type, and assert that it covers our desired alignment.

	Applications using it's own heap should be able to use the alignment
	parameter directly for more minimal padding.
	*/
	vitaPrintf("\nAligning memory... ");
	if (type == SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW)
	{
		// CDRAM memblocks must be 256kB aligned
		vitaPrintf("Doing a 256kb alignment\n");
		assert(alignment <= 256 * 1024);
		size = ALIGN_MEM(size, 256 * 1024);
	}
	else
	{
		//LPDDR memblocks must be 4kB aligned
		vitaPrintf("Doing a 4kb alignment\n");
		assert(alignment <= 4 * 1024);
		size = ALIGN_MEM(size, 4 * 1024);
	}
	UNUSED(alignment);

	//allocate memory
	*uid = sceKernelAllocMemBlock("gpu_mem", type, size, NULL);
	vitaPrintf("SceUID created: %d\n", *uid);
	assert(*uid >= 0);

	//get the base address
	void* memory = NULL;
	error = sceKernelGetMemBlockBase(*uid, &memory);
	assert(error == 0);

	//map memory for the GPU
	vitaPrintf("Mapping graphics memory\n");
	error = sceGxmMapMemory(memory, size, (SceGxmMemoryAttribFlags)attributes);
	assert(error == 0);

	return memory;
}

//Frees mapped GPU memory
void Graphics::freeGraphicsMem(SceUID uid)
{
	int error = 0;
	UNUSED(error);

	vitaPrintf("Freeing allocated gpu memory for SceUID: %d\n", uid);

	//get the base address
	void* memory = NULL;
	error = sceKernelGetMemBlockBase(uid, &memory);
	//assert(error == 0);
	if (error < 0)
		return;

	//unmap the memory
	error = sceGxmUnmapMemory(memory);
	vitaPrintf("sceGxmUnmapMemory(%d) result: 0x%08X\n", uid, error);
	assert(error == 0);

	//free the memory
	error = sceKernelFreeMemBlock(uid);
	vitaPrintf("sceKernelFreeMemBlock(%d) result: 0x%08X\n", uid, error);
	assert(error == 0);
}

//Allocates memory and maps it as a vertex USSE
void *Graphics::allocVertexUsseMem(unsigned int size, SceUID *uid, unsigned int *usseOffset)
{
	int error = 0;
	UNUSED(error);

	vitaPrintf("Allocating vertex USSE GPU memory...\n");
	vitaPrintf("SceSize: %u\n", size);

	//align the memory block for LPDDR (4kb alignment)
	size = ALIGN_MEM(size, 4096);

	//allocate the memory
	*uid = sceKernelAllocMemBlock("vertex_usse", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE, size, NULL);
	assert(*uid >= 0);

	//get the base address
	void *memory = NULL;
	error = sceKernelGetMemBlockBase(*uid, &memory);
	vitaPrintf("sceKernelGetMemBlockBase(%d) result: 0x%08X\n", *uid, error);
	//assert(error == 0);
	if (error < 0)
		return NULL;

	//map as vertex USSE code for GPU
	vitaPrintf("Mapping memory as vertex USSE code for gpu\n");
	error = sceGxmMapVertexUsseMemory(memory, size, usseOffset);
	vitaPrintf("sceGxmMapVertexUsseMemory(%d) result: 0x%08X\n", *uid, error);
	//assert(error == 0);
	if (error < 0)
		return NULL;

	return memory;
}

//Frees memory mapped as vertex USSE code
void Graphics::freeVertexUsseMem(SceUID uid)
{
	int error = 0;

	vitaPrintf("Freeing allocated vertex USSE gpu memory for SceUID: %d\n", uid);

	//get base addr
	void *memory = NULL;
	error = sceKernelGetMemBlockBase(uid, &memory);
	vitaPrintf("sceKernelGetMemBlockBase(%d) result: 0x%08X\n", uid, error);
	assert(error == 0);

	//unmap
	vitaPrintf("Unmapping vertex USSE memory\n");
	error = sceGxmUnmapVertexUsseMemory(memory);
	vitaPrintf("sceGxmUnmapVertexUsseMemory(%d) result: 0x%08X\n", uid, error);
	assert(error == 0);

	//free memory
	error = sceKernelFreeMemBlock(uid);
	vitaPrintf("sceKernelFreeMemBlock(%d) result: 0x%08X\n", uid, error);
	assert(error == 0);
}

//Allocates memory and maps it as a fragment USSE
void *Graphics::allocFragmentUsseMem(unsigned int size, SceUID *uid, unsigned int *usseOffset)
{
	int error = 0;
	UNUSED(error);

	vitaPrintf("Allocating fragment USSE GPU memory...\n");
	vitaPrintf("SceSize: %u\n", size);

	//align the memory block for LPDDR (4kb alignment)
	size = ALIGN_MEM(size, 4096);

	//allocate the memory
	*uid = sceKernelAllocMemBlock("fragment_usse", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE, size, NULL);
	assert(*uid >= 0);

	//get the base address
	void *memory = NULL;
	error = sceKernelGetMemBlockBase(*uid, &memory);
	vitaPrintf("sceKernelGetMemBlockBase(%d) result: 0x%08X\n", *uid, error);
	//assert(error == 0);
	if (error < 0)
		return NULL;

	//map as fragment USSE code for GPU
	vitaPrintf("Mapping memory as fragment USSE code for gpu\n");
	error = sceGxmMapFragmentUsseMemory(memory, size, usseOffset);
	vitaPrintf("sceGxmMapFragmentUsseMemory(%d) result: 0x%08X\n", *uid, error);
	//assert(error == 0);
	if (error < 0)
		return NULL;

	return memory;
}

//Frees memory mapped as fragment USSE 
void Graphics::freeFragmentUsseMem(SceUID uid)
{
	int error = 0;
	UNUSED(error);

	vitaPrintf("Freeing allocated fragment USSE gpu memory for SceUID: %d\n", uid);

	//get base addr
	void *memory = NULL;
	error = sceKernelGetMemBlockBase(uid, &memory);
	vitaPrintf("sceKernelGetMemBlockBase(%d) result: 0x%08X\n", uid, error);
	//assert(error == 0);
	if (error < 0)
		return;

	//unmap
	vitaPrintf("Unmapping fragment USSE memory\n");
	error = sceGxmUnmapFragmentUsseMemory(memory);
	vitaPrintf("sceGxmUnmapFragmentUsseMemory(%d) result: 0x%08X\n", uid, error);
	assert(error == 0);

	//free
	error = sceKernelFreeMemBlock(uid);
	vitaPrintf("sceKernelFreeMemBlock(%d) result: 0x%08X\n", uid, error);
	assert(error == 0);
}

/*----- Static Callback functions start here -----*/
       /*----- Still Memory functions -----*/

//static callback function which allocates memory for the shader patcher, not a member of Graphics
static void* allocPatcherMem(void *userData, SceSize size)
{
	vitaPrintf("Allocating patcher memory\n");
	vitaPrintf("SceSize: %u\n", size);
	UNUSED(userData);
	return malloc(size);
}

//static callback which frees shader patcher memory, not a member of Graphics
static void freePatcherMem(void *userData, void *memory)
{
	vitaPrintf("Freeing patcher memory at address: %p\n", memory);
	UNUSED(userData);
	free(memory);
}

/*----- Memory functions end here -----*/
/*----- Still callback functions -----*/

//Static callback when displaying a frame buffer, not a member of Graphics
static void displayBufferCallback(const void *callbackData)
{
	SceDisplayFrameBuf fb;
	int error = 0;
	UNUSED(error);

	//cast parameters back
	const DisplayData* dispData = (const DisplayData *)callbackData;

	//swap buffers on the next VSYNC
	memset(&fb, 0x00, sizeof(SceDisplayFrameBuf));
	fb.size			= sizeof(SceDisplayFrameBuf);
	fb.base			= dispData->addr;
	fb.pitch		= DISPLAY_STRIDE_IN_PIXELS;
	fb.pixelformat	= DISPLAY_PIXEL_FORMAT;
	fb.width		= DISPLAY_WIDTH;
	fb.height		= DISPLAY_HEIGHT;

	error = sceDisplaySetFrameBuf(&fb, SCE_DISPLAY_SETBUF_NEXTFRAME);
	//assert(error == 0);

	//Dont allow this callback unless the buffer swap has finished and the old buffer is no longer displayed
	sceDisplayWaitVblankStart();
	//assert(error == 0);
}