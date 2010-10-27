#include "start_screen_content.h"


QString start_screen = "start screen";

extern int SCENE_LEFT;
extern int SCENE_TOP;
extern QString button_logo;
extern QString button_layout;
extern QString button_system;

LayoutContent& startscreen_content()
{
	static LayoutContent instance;
	static bool frist_time = true;

	if (frist_time)
	{

		instance.setName(start_screen);

		frist_time = false;

		int logo_width = 6400;
		int logo_heih = 4800;

		int btn_width = 6400/2;
		int btn_height = btn_width*3/4;
		int item_distance = 500;

		int logo_left = SCENE_LEFT + btn_width + item_distance;
		int logo_top = SCENE_TOP + btn_height + item_distance;

		int delta_w = (logo_width-btn_width)/2;
		int delta_h = (logo_heih-btn_height)/4;

		instance.addImage("./skin/logo.png", button_logo, "","", logo_left - 600 , logo_top+200 , logo_width, logo_heih);

		instance.addImage("./skin/startscreen/Control Room.png", "control" ,"","", logo_left + delta_w, SCENE_TOP, btn_width, btn_height);

		instance.addImage("./skin/startscreen/Achive.png","archive", "","",logo_left + delta_w, logo_top + logo_heih + item_distance - 500, btn_width, btn_height);

		instance.addImage("./skin/startscreen/System.png", button_system, "","",SCENE_LEFT, logo_top + delta_h, btn_width, btn_height);
		instance.addImage("./skin/startscreen/Layouts.png", button_layout , "","",logo_left + logo_width + item_distance, logo_top + delta_h, btn_width, btn_height);

		CLDeviceCriteria cr(CLDeviceCriteria::NONE);
		

		instance.setDeviceCriteria(cr);

		instance.setEditable(false);

	}

	return instance;
}

