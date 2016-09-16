#pragma once

#include <stdlib.h>
#include <stdarg.h>
#include <vector>
#include <map>

#include <psp2/kernel/sysmem.h>
#include <psp2/gxm.h>
#include <psp2/display.h>

//macros and utilities
#define RGBA8(r, g, b, a)		((((a)&0xFF)<<24) | (((b)&0xFF)<<16) | (((g)&0xFF)<<8) | (((r)&0xFF)<<0))
#define ALIGN_MEM(addr, align)	(((addr) + ((align) - 1)) & ~((align) - 1))
#define LERP(value, from_max, to_max) ((((value*10) * (to_max*10))/(from_max*10))/10)
#define BIT2BYTE(bit)    ( ((!!(bit&4))<<23) | ((!!(bit&2))<<15) | ((!!(bit&1))<<7) )
#define UNUSED(a)				(void)(a)

//Color defines
#define COLOR_RED RGBA8(255, 0, 0, 255)
#define COLOR_GREEN RGBA8(0, 255, 0, 255)
#define COLOR_BLUE RGBA8(0, 0, 255, 255)
#define COLOR_WHITE RGBA8(255, 255, 255, 255)
#define COLOR_BLACK RGBA8(0, 0, 0, 255)

//Define the width and height at native resolution
#define DISPLAY_WIDTH				960
#define DISPLAY_HEIGHT				544
#define DISPLAY_STRIDE_IN_PIXELS	1024

//Define the libgxm color format used, should be kept in sync with display format 'SceDisplay....'
#define DISPLAY_COLOR_FORMAT		SCE_GXM_COLOR_FORMAT_A8B8G8R8
#define DISPLAY_PIXEL_FORMAT		SCE_DISPLAY_PIXELFORMAT_A8B8G8R8

//Define the number of back buffers. 1 (single buffering), 2 (double buffering) or 3 (triple buffering).
#define DISPLAY_BUFFER_COUNT		3

/*Maximum number of queued swaps that the display queue will allow.
This limits the number of frames that the CPU can get ahead of the GPU,
The display queue will block during sceGxmDisplayQueueAddEntry if this number of swaps
have already been queued.
*/
#define DISPLAY_MAX_PENDING_SWAPS	2

//Anti-aliasing; can be none, 4x or 2x.
#define MSAA_MODE					SCE_GXM_MULTISAMPLE_NONE

//Data structures for vertex types
//clear geometry
typedef struct ClearVertex
{
	float x;
	float y;
} ClearVertex;
//basic geometry
typedef struct BasicVertex
{
	float x;
	float y;
	float z;
	unsigned int color;
} BasicVertex;

typedef enum VertexStreamType
{
	ERROR_NOT_SET = 0,
	GXM_CLEAR_INDEX_16BIT = 1,
	GXM_CLEAR_INDEX_32BIT,
	GXM_CLEAR_INSTANCE_16BIT,
	GXM_CLEAR_INSTANCE_32BIT,
	GXM_BASIC_INDEX_16BIT,
	GXM_BASIC_INDEX_32BIT,
	GXM_BASIC_INSTANCE_16BIT,
	GXM_BASIC_INSTANCE_32BIT
} VertexStreamType;
#define NUMBER_OF_STREAM_TYPES 8

//patcher buffer sizes
typedef struct PatcherSizes
{
	const SceSize patchBufferSize;
	const SceSize patchVertexUsseSize;
	const SceSize patchFragmentUsseSize;
} PatcherSizes;

//the default patcher sizes
static PatcherSizes defaultPatcher = {
	(64 * 1024),	//buffer size
	(64 * 1024),	//vertex USSE size
	(64 * 1024)		//fragments USSE size
};

/*	Structure to pass to displayQueue.  Used during sceGxmDisplayQueueAddEntry, 
and is used to pass data to the display callback function, called from an internal
thread once the back buffer is ready to be displayed.
For this program, we only need to pass the base address of the buffer.
*/
typedef struct DisplayData
{
	void *addr;
} DisplayData;

//C++ singleton Graphics class
class Graphics
{
protected:
	Graphics();
	Graphics(Graphics const&);
	void operator=(Graphics const&);
public:
	~Graphics();
	static Graphics* getInstance();

	//Initializes the GXM, allocates ring buffer memory for vertexes, fragments, maps memory, everything
	//TO DO: either make a few more overloads which accept gxmInitParams, gxmContextParams &/or RenderTargetParams or template the function
	//TO DO: return something to indicate success
	void initGraphics();
	void initGraphics(SceGxmInitializeParams* parameters);
	void shutdownGraphics();

	void startScene();
	void endScene();
	void swapBuffers();
	void clearScreen();
	void clearScreen(uint32_t color);

	void draw(SceGxmPrimitiveType primitive, SceGxmIndexFormat format, const void *indexData, unsigned int indexCount);

	/*----- For dealing with shaders -----*/
	//Register shader programs with the patcher
	SceGxmShaderPatcherId patcherRegisterProgram(const SceGxmProgram *const programHeader);
	//int patcherUnregisterProgram(SceGxmShaderPatcherId programID); now uses a private method to do this all at once during shutdown
	void patcherSetProgramCreationParams(VertexStreamType vertexStreamType); //TO DO: only supports 1 vertex stream, change this. Also, make overloads to change other parameters (i.e. blend modes, SceGxmOutputRegisterFormat, etc)
	SceGxmVertexProgram* patcherCreateVertexProgram(SceGxmShaderPatcherId programID, SceGxmVertexAttribute* attributes, int attributeCount, ...); //the arguments to pass are the names of the attributes as found in shader binary
	SceGxmFragmentProgram* patcherCreateFragmentProgram(SceGxmShaderPatcherId programID, SceGxmShaderPatcherId vertexProgramID);
	void patcherSetVertexProgram(const SceGxmVertexProgram* program);
	void patcherSetFragmentProgram(const SceGxmFragmentProgram* program);
	void patcherSetVertexStream(unsigned int streamIndex, const void* stream);
	void patcherSetVertexProgramConstants(void* uniformBuffer, const SceGxmProgramParameter* worldViewProjection, unsigned int componentOffset, unsigned int componentCount, const float *sourceData);

private:
	//There is no need for these member vars to be declared static, being in a singleton class makes them so by default
	//This is true after the Graphics class has been initialized
	bool initialized;

	//Gxm context and its parameters
	SceGxmContext* gxmContext_ptr;
	SceGxmContextParams gxmContextParams;

	/* Render target and its parameters */
	SceGxmRenderTarget* gxmRenderTarget_ptr;
	SceGxmRenderTargetParams gxmRenderTargetParams;

	/* Display buffers, color surfaces and sync objects */
	//Frame buffers for multibuffering
	void* _displayBuffers[DISPLAY_BUFFER_COUNT];
	SceUID _displayBufferUIDs[DISPLAY_BUFFER_COUNT];
	//GXM color surfaces and sync objects for much faster rendering
	SceGxmColorSurface _colorSurfaces[DISPLAY_BUFFER_COUNT];
	SceGxmSyncObject* _displaySyncObjects[DISPLAY_BUFFER_COUNT];
	/* frame buffer indexes */
	unsigned int backBufIndex;
	unsigned int frontBufIndex;

	/* Ring buffers */
	//TO DO: further comment the purpose/function of each of these
	//ring buffers
	void* vdmRingBuf_ptr;
	void* vertexRingBuf_ptr;
	void* fragmentRingBuf_ptr;
	void* fragmentUsseRingBuf_ptr;
	void* vertexUsseRingBuf_ptr;
	SceUID vdmRingBufUID;
	SceUID vertexRingBufUID;
	SceUID fragmentRingBufUID;
	SceUID fragmentUsseRingBufUID;
	SceUID vertexUsseRingBufUID;
	unsigned int fragmentUsseRingBufOffset;
	unsigned int vertexUsseRingBufOffset;

	//depth buffer
	void* depthBuf_ptr;
	SceUID depthBufUID;
	//depth stencil surface
	SceGxmDepthStencilSurface depthStencilSurface;

	/* Shader patcher and its parameters */
	SceGxmShaderPatcher* patcher_ptr;
	SceGxmShaderPatcherParams patcherParams;
	SceUID patcherBufUID;
	void* patcherBuf_ptr;
	SceUID patcherVertexUsseUID;
	void* patcherVertexUsse_ptr;
	unsigned int patcherVertexUsseOffset;
	SceUID patcherFragmentUsseUID;
	void* patcherFragmentUsse_ptr;
	unsigned int patcherFragmentUsseOffset;
	//all of the registered programs
	std::vector<SceGxmShaderPatcherId> _registeredProgramIDs;
	std::vector<SceGxmVertexProgram*> _vertexPrograms;
	std::vector<SceGxmFragmentProgram*> _fragmentPrograms;
	//The settings for creating programs, can be changed using patcherSetProgramCreatingParams()
	std::map<VertexStreamType, const SceGxmVertexStream*> _vertexStreamMap;
	VertexStreamType currentStreamType;
	short createdStreams; //how many types of vertex streams have been created
	SceGxmVertexStream _vertexStreams[NUMBER_OF_STREAM_TYPES];
	SceGxmOutputRegisterFormat outputRegisterFormat;

	//internal initialize functions
	//The shader patcher initialization is put into its own method to keep the total initialization code easier to read
	void initShaderPatcher(PatcherSizes* sizes);
	void patcherUnregisterPrograms();

	//Callback and memory related methods
	//Allocates memory and maps it to the GPU
public:
	void *allocGraphicsMem(SceKernelMemBlockType type, unsigned int size, unsigned int alignment, unsigned int attribs, SceUID *uid);
	void freeGraphicsMem(SceUID uid);
private:

	//Allocates memory and maps it as a vertex USSE
	void *allocVertexUsseMem(unsigned int size, SceUID *uid, unsigned int *usseOffset);
	void freeVertexUsseMem(SceUID uid);

	//Allocates memory and maps it as a fragment USSE
	void *allocFragmentUsseMem(unsigned int size, SceUID *uid, unsigned int *usseOffset); 
	void freeFragmentUsseMem(SceUID uid);

	//These are static callback functions, they will not be members of Graphics
	//Callback function which allocates memory for the shader patcher
	//void* allocPatcherMem(void *userData, SceSize size);
	//void freePatcherMem(void *userData, void *memory);
	//Callback when displaying a frame buffer
	//void displayBufferCallback(const void *callbackData);
};