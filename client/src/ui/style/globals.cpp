#include "globals.h"
#include <cassert>
#include <QFont>
#include <QColor>

Q_GLOBAL_STATIC(QnGlobals, globalsInstance);

QnGlobals::QnGlobals(QObject *parent):
    QObject(parent) 
{
    setValue(SETTINGS_FONT,                         QFont()/*("Bodoni MT", 12)*/);
    setValue(SHADOW_COLOR,                          QColor(0, 0, 0, 128));
    setValue(SELECTION_COLOR,                       QColor(0, 150, 255, 110));
    //setValue(MOTION_RUBBER_BAND_BORDER_COLOR,       QColor(16, 128+16, 16, 255));
    //setValue(MOTION_RUBBER_BAND_COLOR,              QColor(0, 255, 0, 64));
    //setValue(MOTION_SELECTION_COLOR,                QColor(0, 255, 0, 40));
    setValue(MOTION_MASK_RUBBER_BAND_BORDER_COLOR,  QColor(255, 255, 255, 80));
	setValue(MOTION_MASK_RUBBER_BAND_COLOR,         QColor(255, 255, 255, 40));
    setValue(MOTION_MASK_COLOR,                     QColor(180, 180, 180, 96));
	setValue(FRAME_COLOR,                           QColor(128, 128, 128, 196));
    setValue(SELECTED_FRAME_COLOR,                  QColor(64, 130, 180, 128));
    setValue(OPACITY_CHANGE_PERIOD,                 250);

    setValue(ERROR_TEXT_COLOR,                      QColor(255, 64, 64));

    /* Graphics scene has problems with handling mouse events on small scales, so the larger this number, the better. */
    setValue(WORKBENCH_UNIT_SIZE,                   10000.0);


    setValue(DEFAULT_LAYOUT_CELL_ASPECT_RATIO,      4.0 / 3.0);
    setValue(DEFAULT_LAYOUT_CELL_SPACING,           QSizeF(0.1, 0.1));



    const int COLOR_LIGHT = 100;

    setValue(RECORD_ALWAYS_COLOR,           QColor(0, COLOR_LIGHT, 0));
    setValue(RECORD_MOTION_COLOR,           QColor(COLOR_LIGHT, 0, 0));
    setValue(NO_RECORD_COLOR,               QColor(COLOR_LIGHT - 32, COLOR_LIGHT - 32, COLOR_LIGHT - 32));
    //setValue(MRS_COLOR,                     QColor(0xCD, 0x7F, 0x32));
    //setValue(MRS_COLOR,                     QColor(0xD9, 0xD9, 0x19));  
    setValue(MRS_COLOR,                     QColor(200, 0, 0));  

    setValue(SELECTION_OPACITY_DELTA,       QColor(0, 0, 0, 0x80));
    setValue(SELECTION_BORDER_DELTA,        QColor(48, 48, 48, 0));

}

QnGlobals::~QnGlobals() {
    return;
}

QVariant QnGlobals::value(Variable variable) {
    assert(variable >= 0 && variable < VARIABLE_COUNT);

    return m_globals[variable];
}

void QnGlobals::setValue(Variable variable, const QVariant &value) {
    assert(variable >= 0 && variable < VARIABLE_COUNT);

    m_globals[variable] = value;
}

QnGlobals *QnGlobals::instance() {
    return globalsInstance();
}
