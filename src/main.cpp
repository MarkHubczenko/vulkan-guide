#include <vk_engine.h>
#include <iostream>
#include <windows.h>
#include <string>


int main(int argc, char* argv[])
{
	TCHAR buffer[MAX_PATH] = { 0 };
	GetModuleFileNameA(NULL, buffer, MAX_PATH);
	std::cout << "my directory is " << buffer << "\n";
	VulkanEngine engine;

	engine.init();	
	
	engine.run();	

	engine.cleanup();	

	return 0;
}
