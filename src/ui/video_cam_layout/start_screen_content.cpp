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

		frist_time = false;

        instance.setName(start_screen);

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

        CLBasicLayoutItemSettings settings;
        settings.angle = 0;
        settings.coordType = CLBasicLayoutItemSettings::Pixels;

        settings.name = "abc";
        settings.pos_x = noLogoLeft;
        settings.pos_y = noLogoTop;
        settings.width = noLogoWidth;
        settings.height = noLogoHeight;
        instance.addImage(":/skin/startscreen/no_logo.png", "","", settings);

        settings.name = button_logo;
        settings.pos_x = eveLogoLeft;
        settings.pos_y = eveLogoTop;
        settings.width = eveLogoWidth;
        settings.height = eveLogoHeight;
		instance.addImage(":/skin/startscreen/eve_logo.png", "","", settings);


        settings.name = button_layout;
        settings.pos_x = btn2_left;
        settings.pos_y = btn_top;
        settings.width = btn_width;
        settings.height = btn_height;
        instance.addImage(":/skin/startscreen/launch_1.png", "","", settings);


        settings.name = button_system;
        settings.pos_x = btn_left;
        settings.pos_y = btn_top;
        settings.width = btn_width;
        settings.height = btn_height;
        instance.addImage(":/skin/startscreen/setup_2.png", "","", settings);

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

