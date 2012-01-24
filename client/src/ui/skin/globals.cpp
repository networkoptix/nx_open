#include "globals.h"
#include <cassert>
#include <QFont>
#include <QColor>

Q_GLOBAL_STATIC(Globals, globalsInstance);

Globals::Globals(QObject *parent):
    QObject(parent) 
{
    setValue(SETTINGS_FONT, QFont()/*("Bodoni MT", 12)*/);
    setValue(SHADOW_COLOR, QColor(0, 0, 0, 128));
    setValue(SELECTION_COLOR, QColor(0, 150, 255, 110));
    setValue(MOTION_RUBBER_BAND_BORDER_COLOR, QColor(16, 128+16, 16, 255));
    setValue(MOTION_RUBBER_BAND_COLOR, QColor(0, 255, 0, 64));
    setValue(MOTION_SELECTION_COLOR, QColor(0, 255, 0, 40));
    setValue(FRAME_COLOR, QColor(128, 128, 128, 196));
    setValue(OPACITY_CHANGE_PERIOD, 500);

    // DEPRECATED
    setValue(SHOW_ITEM_TEXT, false);
    setValue(ROTATION_ANGLE, 0);
    setValue(CAN_BE_DROPPED_COLOR, QColor(0, 255, 150, 110));

    // how often we run new device search and how often layout synchronizes with device manager
    setValue(DEVICE_UPDATE_INTERVAL, 1000);

    setValue(BACKGROUND_COLOR, QColor(0,5,5,125));

    setValue(DECORATION_OPACITY, 0.3);
    setValue(DECORATION_MAX_OPACITY, 0.7);

    setValue(BASE_SCENE_Z_LEVEL, 1.0);

    setValue(GRID_APPEARANCE_DELAY, 2000);
}

Globals::~Globals() {
    return;
}

QVariant Globals::value(Variable variable) {
    assert(variable >= 0 && variable < VARIABLE_COUNT);

    return m_globals[variable];
}

void Globals::setValue(Variable variable, const QVariant &value) {
    assert(variable >= 0 && variable < VARIABLE_COUNT);

    m_globals[variable] = value;
}

Globals *Globals::instance() {
    return globalsInstance();
}
