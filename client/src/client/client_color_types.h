#ifndef QN_CLIENT_COLOR_TYPES
#define QN_CLIENT_COLOR_TYPES

#include <QtCore/QMetaType>
#include <QtGui/QColor>

#include <utils/common/json.h>


struct QnTimeSliderColors {
public:
    QnTimeSliderColors();

    QColor tickmark;
    QColor positionMarker;
    QColor indicator;

    QColor selection;
    QColor selectionMarker;

    QColor pastBackground;
    QColor futureBackground;

    QColor pastRecording;
    QColor futureRecording;

    QColor pastMotion;
    QColor futureMotion;

    QColor separator;

    QColor dateOverlay;
    QColor dateOverlayAlternate;
};

Q_DECLARE_METATYPE(QnTimeSliderColors)
QN_DEFINE_STRUCT_JSON_SERIALIZATION_FUNCTIONS(
    QnTimeSliderColors, 
    (tickmark)(positionMarker)(indicator)(selection)(selectionMarker)
        (pastBackground)(futureBackground)(pastRecording)(futureRecording)(pastMotion)(futureMotion)
        (separator)(dateOverlay)(dateOverlayAlternate), 
    inline
)


struct QnBackgroundColors {
    QnBackgroundColors();

    QColor normal;
    QColor panic;
};

Q_DECLARE_METATYPE(QnBackgroundColors)
QN_DEFINE_STRUCT_JSON_SERIALIZATION_FUNCTIONS(QnBackgroundColors, (normal)(panic), inline)


struct QnCalendarColors {
    QnCalendarColors();

    QColor selection;
    QColor primaryRecording;
    QColor secondaryRecording;
    QColor primaryMotion;
    QColor secondaryMotion;
    QColor separator;

    /* These are defined in calendar_item_delegate.cpp. */
    QColor primary(int fillType) const;
    QColor secondary(int fillType) const;
};

Q_DECLARE_METATYPE(QnCalendarColors)
QN_DEFINE_STRUCT_JSON_SERIALIZATION_FUNCTIONS(
    QnCalendarColors, 
    (selection)(primaryRecording)(secondaryRecording)(primaryMotion)(secondaryMotion)(separator), 
    inline
)


#endif // QN_CLIENT_COLOR_TYPES
