#include "start_screen_content.h"

#include "ui/skin/skin.h"

QString start_screen = QLatin1String("start screen");

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

        int eveLogoWidth = 720*0.5;
        int eveLogoHeight = 475*0.5;
        int eveLogoLeft = SCENE_LEFT;
        int eveLogoTop = SCENE_TOP;


        int noLogoWidth = 1500/6.3;
        int noLogoHeight = 658/6.3;
        int noLogoLeft = SCENE_LEFT + 50;
        int noLogoTop = eveLogoTop + 250;


        //int btn_width = 400*0.7;
        //int btn_height = 300*0.7;


        //int btn_left = SCENE_LEFT -100;
        //int btn2_left = btn_left + 800;

        //int btn_top = eveLogoTop + eveLogoHeight - 100;

        CLBasicLayoutItemSettings settings;
        settings.angle = 0;
        settings.coordType = CLBasicLayoutItemSettings::Pixels;

        settings.name = button_layout;
        settings.pos_x = noLogoLeft;
        settings.pos_y = noLogoTop;
        settings.width = noLogoWidth;
        settings.height = noLogoHeight;
        instance.addImage(Skin::path(QLatin1String("startscreen/no_logo.png")), QString(), QString(), settings);

        settings.name = button_layout;
        settings.pos_x = eveLogoLeft;
        settings.pos_y = eveLogoTop;
        settings.width = eveLogoWidth;
        settings.height = eveLogoHeight;
        instance.addImage(Skin::path(QLatin1String("startscreen/eve_logo.png")), QString(), QString(), settings);


        //settings.name = button_layout;

        CLDeviceCriteria cr(CLDeviceCriteria::STATIC);

        CLRectAdjustment adj;

        adj.x1 = 1800 + 460;
        adj.y1 = 1800;
        adj.x2 = -1800 + 460;
        adj.y2 = -1800;
        /**/
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


        instance.addDecorationFlag(
            LayoutContent::BackGroundLogo |
            LayoutContent::ExitButton |
            LayoutContent::ToggleFullScreenButton |
            LayoutContent::SettingButton);
    }


    return instance;
}

