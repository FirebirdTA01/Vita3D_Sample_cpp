#include <string.h>

#include <psp2/kernel/processmgr.h>
#include <psp2/ctrl.h>

#include "Graphics.h"
#include "Triangle.h" //Just a demo class to get something 3d on the screen
#include "commonUtils.h"

//Let's do this
int main()
{
	//initialize the logger
	Logger::getInstance()->init();

	//set up all GXM/Buffers/Shaders/etc using default settings
	Graphics::getInstance()->initGraphics();

	//initialize controller data
	SceCtrlData ctrl;
	memset(&ctrl, 0, sizeof(ctrl));

	Triangle triangle;
	triangle.init();

	//main loop
	bool running = true;
	while (running)
	{
		//check control data
		sceCtrlReadBufferPositive(0, &ctrl, 1);
		if (ctrl.buttons & SCE_CTRL_SELECT)
			running = false;

		//rotate the triangle
		triangle.update();

		Graphics::getInstance()->startScene();

		triangle.draw();

		Graphics::getInstance()->endScene();
		Graphics::getInstance()->swapBuffers();
	}

	//wait until rendering is finished before cleaning things up
	//sceGxmFinish(Graphics::getInstance()->getGxmContext()); done in Graphics::shutdown for now
	triangle.cleanup();
	Graphics::getInstance()->shutdownGraphics();

	Logger::getInstance()->shutdown();

	sceKernelExitProcess(0);
	return 0;
}