#include "client_color_types.h"

#include <ui/style/globals.h>

#include <utils/math/color_transformations.h>


QnTimeSliderColors::QnTimeSliderColors() {
    tickmark = QColor(255, 255, 255, 255);
    positionMarker = QColor(255, 255, 255, 196);
    indicator = QColor(128, 160, 192, 128);

    selection = qnGlobals->selectionColor();
    selectionMarker = selection.lighter();

    pastBackground = QColor(255, 255, 255, 24);
    futureBackground = QColor(0, 0, 0, 0);

    pastRecording = QColor(64, 255, 64, 128);
    futureRecording = QColor(64, 255, 64, 64);

    pastMotion = QColor(255, 0, 0, 128);
    futureMotion = QColor(255, 0, 0, 64);

    separator = QColor(255, 255, 255, 64);

    dateOverlay = QColor(255, 255, 255, 48);
    dateOverlayAlternate = withAlpha(selection, 48);
}

QnBackgroundColors::QnBackgroundColors() {
    normal = QColor(26, 26, 240, 40);
    panic = QColor(255, 0, 0, 255);
}

QnCalendarColors::QnCalendarColors() {
    selection = withAlpha(qnGlobals->selectionColor(), 192);
    primaryRecording = QColor(32, 128, 32, 255);
    secondaryRecording = QColor(32, 255, 32, 255);
    primaryMotion = QColor(128, 0, 0, 255);
    secondaryMotion = QColor(255, 0, 0, 255);
    separator = QColor(0, 0, 0, 255);
}
