#include "start_screen_content.h"


extern int SCENE_LEFT;
extern int SCENE_TOP;


LayoutContent& startscreen_content()
{
	static LayoutContent instance;
	static bool frist_time = true;

	if (frist_time)
	{
		frist_time = false;

		instance.addImage("./skin/logo.jpg","", 0,0, SCENE_LEFT + 1000, SCENE_TOP + 1000, 6400, 4800);
		instance.addButton("View Video", 0,0, SCENE_LEFT + 1000, SCENE_TOP + 1000, 6400/2, 4800/2);

		CLDeviceCriteria cr;
		cr.mCriteria = CLDeviceCriteria::NONE;

		instance.setDeviceCriteria(cr);

	}

	return instance;
}

