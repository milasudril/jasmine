#ifdef __WAND__
target[platform[;GNU/Linux]name[jasmine-player] type[application]]
target[platform[;Windows]name[jasmine-player.exe] type[application]]
#endif

#include "jasmine.h"

const char* ports[]={"Left","Right",nullptr};

int main()
	{
	Jasmine player("Jasmine",nullptr,ports);
	}
