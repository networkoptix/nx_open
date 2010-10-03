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

		int logo_width = 6400;
		int logo_heih = 4800;

		int btn_width = 6400/2;
		int btn_height = btn_width*3/4;
		int item_distance = 2000;

		int logo_left = SCENE_LEFT + btn_width + item_distance;
		int logo_top = SCENE_TOP + btn_height + item_distance;

		int delta_w = (logo_width-btn_width)/2;
		int delta_h = (logo_heih-btn_height)/4;

		instance.addImage("./skin/logo.jpg","", logo_left , logo_top , logo_width, logo_heih);

		instance.addButton("Wizard", logo_left + delta_w, SCENE_TOP, btn_width, btn_height);

		instance.addButton("Layouts editor", logo_left + delta_w, logo_top + logo_heih + item_distance - 1200, btn_width, btn_height);

		instance.addButton("Servers", SCENE_LEFT, logo_top + delta_h, btn_width, btn_height);
		instance.addButton("Layouts", logo_left + logo_width + item_distance, logo_top + delta_h, btn_width, btn_height);

		CLDeviceCriteria cr;
		cr.mCriteria = CLDeviceCriteria::NONE;

		instance.setDeviceCriteria(cr);

	}

	return instance;
}

