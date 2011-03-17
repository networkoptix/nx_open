#include "settings.h"

bool global_show_item_text = false;
qreal global_rotation_angel = 0;
//QFont settings_font("Bodoni MT", 12);
QFont settings_font;//("Bodoni MT", 12);

QColor global_shadow_color(0, 0, 0, 128);

QColor global_selection_color(0, 150, 255, 110);
QColor global_can_be_droped_color(0, 255, 150, 110);

// how often we run new device search and how often layout synchronizes with device manager 
int devices_update_interval = 1000;

QColor app_bkr_color(0,5,5,125);

qreal global_menu_opacity =  0.8;
qreal global_dlg_opacity  = 0.9;
qreal global_decoration_opacity  = 0.2;
qreal global_decoration_max_opacity  = 0.7;

qreal global_base_scene_z_level = 1.0;

int global_grid_aparence_delay = 2000;

int global_opacity_change_period = 1000;
