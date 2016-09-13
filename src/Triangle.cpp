#include "Triangle.h"

#include "commonUtils.h"
#include "Logger.h"

#include <math.h>
#include <assert.h>

#define PI 3.14159265358979323846

//#define GXM_CONTEXT Graphics::getInstance()->getGxmContext()

/*
The build process for the sample embeds the shader programs directly into the
executable using the symbols below.  This is purely for convenience, it is
equivalent to simply load the binary file into memory and cast the contents
to type SceGxmProgram.
*/
extern const SceGxmProgram clear_v_gxp_start;
extern const SceGxmProgram clear_f_gxp_start;
extern const SceGxmProgram color_v_gxp_start;
extern const SceGxmProgram color_f_gxp_start;

static const SceGxmProgram *const clearVertexProgramGXP = &clear_v_gxp_start;
static const SceGxmProgram *const clearFragmentProgramGXP = &clear_f_gxp_start;
static const SceGxmProgram *const basicVertexProgramGXP = &color_v_gxp_start;
static const SceGxmProgram *const basicFragmentProgramGXP = &color_f_gxp_start;

//----------------------------------------------------------------------------------
// Triangle class
//----------------------------------------------------------------------------------

Triangle::Triangle() :
	clearVertices((ClearVertex*)Graphics::getInstance()->allocGraphicsMem(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
		3 * sizeof(ClearVertex),
		4,
		SCE_GXM_MEMORY_ATTRIB_READ,
		&clearVerticesUID
	)),
	clearIndices((uint16_t*)Graphics::getInstance()->allocGraphicsMem(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
		3 * sizeof(uint16_t),
		2,
		SCE_GXM_MEMORY_ATTRIB_READ,
		&clearIndicesUID
	)),
	basicVertices((BasicVertex*)Graphics::getInstance()->allocGraphicsMem(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
		3 * sizeof(BasicVertex),
		4,
		SCE_GXM_MEMORY_ATTRIB_READ,
		&basicVerticesUID
	)),
	basicIndices((uint16_t*)Graphics::getInstance()->allocGraphicsMem(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
		3 * sizeof(uint16_t),
		2,
		SCE_GXM_MEMORY_ATTRIB_READ,
		&basicIndicesUID
	))
{
	clearVertexProgram_ptr = nullptr;
	basicVertexProgram_ptr = nullptr;
	clearFragmentProgram_ptr = nullptr;
	basicFragmentProgram_ptr = nullptr;

	clearVertexProgramID = nullptr;
	clearFragmentProgramID = nullptr;
	basicVertexProgramID = nullptr;
	basicFragmentProgramID = nullptr;

	clearVerticesUID = -1;
	clearIndicesUID = -1;
	basicVerticesUID = -1;
	basicIndicesUID = -1;
}

Triangle::~Triangle()
{
}

void Triangle::init()
{
	vitaPrintf("\nInitializing a triangle object\n");
	int error = 0;

	//register the programs with the patcher that were compiled and linked to using the CG tool
	clearVertexProgramID = Graphics::getInstance()->patcherRegisterProgram(clearVertexProgramGXP);
	clearFragmentProgramID = Graphics::getInstance()->patcherRegisterProgram(clearFragmentProgramGXP);
	basicVertexProgramID = Graphics::getInstance()->patcherRegisterProgram(basicVertexProgramGXP);
	basicFragmentProgramID = Graphics::getInstance()->patcherRegisterProgram(basicFragmentProgramGXP);

	//create vertex format for clear triangle
	SceGxmVertexAttribute clearVertexAttribs[1];
	clearVertexAttribs[0].streamIndex = 0;
	clearVertexAttribs[0].offset = 0;
	clearVertexAttribs[0].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
	clearVertexAttribs[0].componentCount = 2;
	//This is set in Graphics::patcherCreateVertexProgram()
	//clearVertexAttribs[0].regIndex = sceGxmProgramParameterGetResourceIndex(paramClearPositionAttribute_ptr);

	//create vertex format for a shaded triangle
	SceGxmVertexAttribute basicVertexAttribs[2];
	basicVertexAttribs[0].streamIndex = 0;
	basicVertexAttribs[0].offset = 0;
	basicVertexAttribs[0].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
	basicVertexAttribs[0].componentCount = 3;
	//This is set in Graphics::patcherCreateVertexProgram()
	//basicVertexAttribs[0].regIndex = sceGxmProgramParameterGetResourceIndex(paramBasicPositionAttribute_ptr);

	basicVertexAttribs[1].streamIndex = 0;
	basicVertexAttribs[1].offset = 12;
	basicVertexAttribs[1].format = SCE_GXM_ATTRIBUTE_FORMAT_U8N;
	basicVertexAttribs[1].componentCount = 4;
	//This is set in Graphics::patcherCreateVertexProgram()
	//basicVertexAttribs[1].regIndex = sceGxmProgramParameterGetResourceIndex(paramBasicColorAttribute_ptr);
	
	//Set up the graphics system to create the correct kind of shader for intended geometry
	Graphics::getInstance()->patcherSetProgramCreationParams(GXM_CLEAR_INDEX_16BIT);
	//TO DO: Graphics::getInstance()->patcherSetProgramCreationParams(outputRegister = SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4);
	//TO DO: Graphics::getInstance()->patcherSetProgramCreationParams(Vertex Stream Params/Count);
	//Create the clear programs
	clearVertexProgram_ptr = Graphics::getInstance()->patcherCreateVertexProgram(
		clearVertexProgramID, 
		clearVertexAttribs, 
		1, 
		"aPosition"
	);
	clearFragmentProgram_ptr = Graphics::getInstance()->patcherCreateFragmentProgram(
		clearFragmentProgramID,
		clearVertexProgramID
	);

	//Set up the graphics system to create the correct kind of shader for intended geometry
	Graphics::getInstance()->patcherSetProgramCreationParams(GXM_BASIC_INDEX_16BIT);
	//Create the color programs
	basicVertexProgram_ptr = Graphics::getInstance()->patcherCreateVertexProgram(
		basicVertexProgramID,
		basicVertexAttribs,
		2,
		"aPosition", "aColor"
	);
	basicFragmentProgram_ptr = Graphics::getInstance()->patcherCreateFragmentProgram(
		basicFragmentProgramID, 
		basicVertexProgramID
	);

	//The memory for all of these was allocated before the constructor 
	vitaPrintf("Setting up clear vertices\n");
	//create clear triangle vertices/indice
	clearVertices[0].x = -1.0f;
	clearVertices[0].y = -1.0f;
	clearVertices[1].x = 3.0f;
	clearVertices[1].y = -1.0f;
	clearVertices[2].x = -1.0f;
	clearVertices[2].y = 3.0f;

	vitaPrintf("Setting up clear indices\n");
	clearIndices[0] = 0;
	clearIndices[1] = 1;
	clearIndices[2] = 2;

	vitaPrintf("Setting up basic vertices\n");
	//create basic shaded triangle vetices/indice
	basicVertices[0].x = -0.5f;
	basicVertices[0].y = -0.5f;
	basicVertices[0].z = 0.0f;
	basicVertices[0].color = 0xff0000ff;
	basicVertices[1].x = 0.5f;
	basicVertices[1].y = -0.5f;
	basicVertices[1].z = 0.0f;
	basicVertices[1].color = 0xff00ff00;
	basicVertices[2].x = -0.5f;
	basicVertices[2].y = 0.5f;
	basicVertices[2].z = 0.0f;
	basicVertices[2].color = 0xffff0000;

	vitaPrintf("Setting up basic indices\n");
	basicIndices[0] = 0;
	basicIndices[1] = 1;
	basicIndices[2] = 2;

	//Just to get something working real quick... this is suppossed to be const correct, but I'll fix that later
	//for now just do const_cast when using this
	vitaPrintf("Loading World-View-Projection parameters from vertex program at address: %p\n", basicVertexProgram_ptr);
	const SceGxmProgramParameter* wvpParam_ptr = sceGxmProgramFindParameterByName(sceGxmShaderPatcherGetProgramFromId(basicVertexProgramID), "wvp");
	assert(wvpParam_ptr && (sceGxmProgramParameterGetCategory(wvpParam_ptr) == SCE_GXM_PARAMETER_CATEGORY_UNIFORM));
	_wvpParams.insert(_wvpParams.begin(), std::make_pair("wvp", wvpParam_ptr));
}

void Triangle::update()
{
	//update trianlge angle
	triangleRotation += (PI * 2) / 60.f;
	if (triangleRotation > PI * 2)
		triangleRotation -= PI * 2;

	//4x4 matrix for rotation
	float aspectRatio = (float)DISPLAY_WIDTH / (float)DISPLAY_HEIGHT;

	float s = sin(triangleRotation);
	float c = cos(triangleRotation);

	//update the World View Projection data
	wvpData[0] = c / aspectRatio;
	wvpData[1] = s;
	wvpData[2] = 0.0f;
	wvpData[3] = 0.0f;

	wvpData[4] = -s / aspectRatio;
	wvpData[5] = c;
	wvpData[6] = 0.0f;
	wvpData[7] = 0.0f;

	wvpData[8] = 0.0f;
	wvpData[9] = 0.0f;
	wvpData[10] = 1.0f;
	wvpData[11] = 0.0f;

	wvpData[12] = 0.0f;
	wvpData[13] = 0.0f;
	wvpData[14] = 0.0f;
	wvpData[15] = 1.0f;
}

void Triangle::cleanup()
{	
	vitaPrintf("\nCleaning up after a triangle object\n");

	/* This is done automatically in Graphics::shutdown()
	Graphics::getInstance()->patcherUnregisterProgram(basicFragmentProgramID);
	Graphics::getInstance()->patcherUnregisterProgram(basicVertexProgramID);
	Graphics::getInstance()->patcherUnregisterProgram(clearFragmentProgramID);
	Graphics::getInstance()->patcherUnregisterProgram(clearVertexProgramID);
	*/
}

void Triangle::draw()
{
	//set clear shaders
	Graphics::getInstance()->patcherSetVertexProgram(clearVertexProgram_ptr);
	Graphics::getInstance()->patcherSetFragmentProgram(clearFragmentProgram_ptr);
	//draw the clear triangle
	Graphics::getInstance()->patcherSetVertexStream(0, clearVertices);
	Graphics::getInstance()->draw(SCE_GXM_PRIMITIVE_TRIANGLES, SCE_GXM_INDEX_FORMAT_U16, clearIndices, 3);

	//set basic shaders
	Graphics::getInstance()->patcherSetVertexProgram(basicVertexProgram_ptr);
	Graphics::getInstance()->patcherSetFragmentProgram(basicFragmentProgram_ptr);

	//set vertex program constants
	void* defaultVertexBuffer;
	Graphics::getInstance()->patcherSetVertexProgramConstants(defaultVertexBuffer, _wvpParams.find("wvp")->second, 0, 16, wvpData);

	//draw the rotating triangle
	Graphics::getInstance()->patcherSetVertexStream(0, basicVertices);
	Graphics::getInstance()->draw(SCE_GXM_PRIMITIVE_TRIANGLES, SCE_GXM_INDEX_FORMAT_U16, basicIndices, 3);
}