#ifndef QN_CLIENT_COLOR_TYPES
#define QN_CLIENT_COLOR_TYPES

#include <QtCore/QMetaType>
#include <QtGui/QColor>

#include <utils/common/json_fwd.h>

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
QN_DECLARE_JSON_SERIALIZATION_FUNCTIONS(QnTimeSliderColors)


struct QnBackgroundColors {
    QnBackgroundColors();

    QColor normal;
    QColor panic;
};

Q_DECLARE_METATYPE(QnBackgroundColors)
QN_DECLARE_JSON_SERIALIZATION_FUNCTIONS(QnBackgroundColors)


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
QN_DECLARE_JSON_SERIALIZATION_FUNCTIONS(QnCalendarColors)


#endif // QN_CLIENT_COLOR_TYPES
