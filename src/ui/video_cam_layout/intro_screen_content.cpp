#include "start_screen_content.h"

QString intro_screen = "intro screen";

extern int SCENE_LEFT;
extern int SCENE_TOP;

LayoutContent& intro_screen_content()
{
	static LayoutContent instance;
	static bool frist_time = true;

	if (frist_time)
	{
		instance.setName(intro_screen);
		frist_time = false;
        

        instance.addDevice("intro.mov");


	}


	return instance;
}

