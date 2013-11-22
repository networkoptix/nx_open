#include "time_slider_colors.h"

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
