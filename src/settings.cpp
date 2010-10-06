#include "settings.h"

bool global_show_item_text = true;
qreal global_rotation_angel = 0;
QFont settings_font("Bodoni MT", 12);

QColor global_shadow_color(0, 0, 0, 128);

// how often we run new device search and how often layout synchronizes with device manager 
int devices_update_interval = 2000;