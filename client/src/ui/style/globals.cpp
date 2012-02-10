#include "globals.h"
#include <cassert>
#include <QFont>
#include <QColor>

Q_GLOBAL_STATIC(Globals, globalsInstance);

Globals::Globals(QObject *parent):
    QObject(parent) 
{
    setValue(SETTINGS_FONT,                         QFont()/*("Bodoni MT", 12)*/);
    setValue(SHADOW_COLOR,                          QColor(0, 0, 0, 128));
    setValue(SELECTION_COLOR,                       QColor(0, 150, 255, 110));
    setValue(MOTION_RUBBER_BAND_BORDER_COLOR,       QColor(16, 128+16, 16, 255));
    setValue(MOTION_RUBBER_BAND_COLOR,              QColor(0, 255, 0, 64));
    setValue(MOTION_SELECTION_COLOR,                QColor(0, 255, 0, 40));
    setValue(MOTION_MASK_RUBBER_BAND_BORDER_COLOR,  QColor(255, 255, 255, 80));
	setValue(MOTION_MASK_RUBBER_BAND_COLOR,         QColor(255, 255, 255, 40));
    setValue(MOTION_MASK_COLOR,                     QColor(255, 255, 255, 26));
	setValue(FRAME_COLOR,                           QColor(128, 128, 128, 196));
    setValue(SELECTED_FRAME_COLOR,                  QColor(64, 130, 180, 128));
    setValue(OPACITY_CHANGE_PERIOD,                 500);
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
