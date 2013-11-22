#ifndef QN_TIME_SLIDER_COLORS_H
#define QN_TIME_SLIDER_COLORS_H

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

QN_DEFINE_STRUCT_SERIALIZATION_FUNCTIONS_OPTIONAL(
    QnTimeSliderColors, 
    (tickmark)(positionMarker)(indicator)(selection)(selectionMarker)
        (pastBackground)(futureBackground)(pastRecording)(futureRecording)(pastMotion)(futureMotion)
        (separator)(dateOverlay)(dateOverlayAlternate), 
    inline
);

#endif // QN_TIME_SLIDER_COLORS_H
