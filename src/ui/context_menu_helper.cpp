#include "context_menu_helper.h"

QAction cm_exit("Exit",0);
QAction cm_fitinview("Fit in View",0);
QAction cm_arrange("Arrange",0);
QAction cm_togglefs("Toggle fullscreen",0);
QAction cm_fullscren("Fullscreen",0);
QAction cm_settings("Settings...",0);



QAction dis_0("0%",0);
QAction dis_5("5%",0);
QAction dis_10("10%",0);
QAction dis_15("15%",0);
QAction dis_20("20%",0);
QAction dis_25("25%",0);
QAction dis_30("30%",0);
QAction dis_35("35%",0);


void initContextMenu()
{
	dis_0.setData(0.0);
	dis_5.setData(0.05);
	dis_10.setData(0.10);
	dis_15.setData(0.15);
	dis_20.setData(0.20);
	dis_25.setData(0.25);
	dis_30.setData(0.30);
	dis_35.setData(0.35);

}