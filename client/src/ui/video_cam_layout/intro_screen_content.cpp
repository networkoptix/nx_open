#include "start_screen_content.h"

QString intro_screen = QLatin1String("intro screen");

extern int SCENE_LEFT;
extern int SCENE_TOP;

LayoutContent& intro_screen_content()
{
	static LayoutContent instance;
	static bool first_time = true;
	if (first_time)
	{
		first_time = false;
		instance.setName(intro_screen);
		instance.addDevice(QLatin1String("intro.mov"));
	}

	return instance;
}

