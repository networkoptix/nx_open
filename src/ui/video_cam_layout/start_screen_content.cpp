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

		int logo_width = 3000*9;
		int logo_heih = 1315*9;

		int btn_width = 339*12;
		int btn_height = 303*12;
		int item_distance = 500*4;

		int logo_left = SCENE_LEFT + item_distance;
		int logo_top = SCENE_TOP;

		int btn_left = SCENE_LEFT + 4000;
		int btn_top = SCENE_TOP + logo_heih + 1000;

		int btn2_left = btn_left + logo_width - 9350;

		instance.addImage("./skin/logo.png", button_logo, "","", logo_left - 600 , logo_top+200 , logo_width, logo_heih);

		//instance.addImage("./skin/startscreen/Control Room.png", "control" ,"","", logo_left + delta_w, SCENE_TOP, btn_width, btn_height);
		//instance.addImage("./skin/startscreen/Achive.png","archive", "","",logo_left + delta_w, logo_top + logo_heih + item_distance - 500, btn_width, btn_height);

		//instance.addImage("./skin/startscreen/System.png", button_system, "","", btn_left, btn_top, btn_width, btn_height);
        //instance.addImage("./skin/startscreen/Layouts.png", button_layout , "","",btn2_left, btn_top, btn_width, btn_height);

        instance.addImage("./skin/try/dynamic_button", button_layout , "","",btn2_left, btn_top, btn_width, btn_height);
        instance.addImage("./skin/try/dynamic_button", button_system, "","", btn_left, btn_top, btn_width, btn_height);

		CLDeviceCriteria cr(CLDeviceCriteria::STATIC);

		CLRectAdjustment adj;
		adj.x1 = 800;
		adj.y1 = 4000;
		adj.x2 = -1000;
		adj.y2 = -1000;
		instance.setRectAdjustment(adj);

		instance.setDeviceCriteria(cr);
		instance.removeIntereactionFlag(
			LayoutContent::Zoomable | 
			LayoutContent::SceneMovable | 
			LayoutContent::ShowAvalable | 
			LayoutContent::ItemMovable | 
			LayoutContent::GridEnable | 
			LayoutContent::ItemRotatable | 
			LayoutContent::ItemSelectable );

	}

	return instance;
}

