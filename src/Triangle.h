#pragma once

#include "Graphics.h"

//This is just thrown together to get a sample from another SDK working,
//Actually set up to create/draw 2 different triangles; a clear vertice triangle and a basic shaded one with rotation
//not meant to be used as a triangle class for complex geometry
class Triangle
{
public:
	Triangle();
	~Triangle();

	void init();
	void cleanup();
	void update();
	void draw();

private:

	float triangleRotation;

	//Programs to register with the patcher (linked against with shader(s).obj)
	SceGxmShaderPatcherId clearVertexProgramID;
	SceGxmShaderPatcherId basicVertexProgramID;
	SceGxmShaderPatcherId clearFragmentProgramID;
	SceGxmShaderPatcherId basicFragmentProgramID;

	//shader program pointers
	SceGxmVertexProgram* clearVertexProgram_ptr;
	SceGxmVertexProgram* basicVertexProgram_ptr;
	SceGxmFragmentProgram* clearFragmentProgram_ptr;
	SceGxmFragmentProgram* basicFragmentProgram_ptr;

	ClearVertex *const clearVertices;
	uint16_t *const clearIndices;
	BasicVertex *const basicVertices;
	uint16_t *const basicIndices;

	SceUID clearVerticesUID;
	SceUID clearIndicesUID;
	SceUID basicVerticesUID;
	SceUID basicIndicesUID;

	//world view projection parameters
	std::map<const char*, const SceGxmProgramParameter*> _wvpParams;
	float wvpData[16];
};