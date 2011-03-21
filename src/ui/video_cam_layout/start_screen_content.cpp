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

        int noLogoWidth = 702*1.7;
        int noLogoHeight = 243*1.7;
		int noLogoLeft = SCENE_LEFT - 150;
		int noLogoTop = SCENE_TOP;


        int eveLogoWidth = 720*0.5;
        int eveLogoHeight = 475*0.5;
        int eveLogoLeft = SCENE_LEFT + 250;
        int eveLogoTop = noLogoTop + noLogoHeight -50;
        


        int btn_width = 400*0.7;
        int btn_height = 300*0.7;


		int btn_left = SCENE_LEFT + -100;
        int btn2_left = btn_left + 800;

		int btn_top = eveLogoTop + eveLogoHeight - 100;

		

        instance.addImage(":/skin/startscreen/no_logo.png", "abc", "","", noLogoLeft, noLogoTop, noLogoWidth, noLogoHeight);

		instance.addImage(":/skin/startscreen/eve_logo.png", button_logo, "","", eveLogoLeft, eveLogoTop, eveLogoWidth, eveLogoHeight);

		//instance.addImage(":/skin/startscreen/control room.png", "control" ,"","", logo_left + delta_w, SCENE_TOP, btn_width, btn_height);
		//instance.addImage(":/skin/startscreen/achive.png","archive", "","",logo_left + delta_w, logo_top + logo_heih + item_distance - 500, btn_width, btn_height);

        instance.addImage(":/skin/startscreen/dynamic_launch/", button_layout , "","",btn2_left, btn_top, btn_width, btn_height);
        instance.addImage(":/skin/startscreen/setup_2.png", button_system, "","", btn_left, btn_top, btn_width, btn_height);

		CLDeviceCriteria cr(CLDeviceCriteria::STATIC);

		CLRectAdjustment adj;
		adj.x1 = 1500;
		adj.y1 = 1580;
		adj.x2 = -1500;
		adj.y2 = -1500;
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


        instance.addDecorationFlag(LayoutContent::BackGroundLogo);
	}


	return instance;
}

