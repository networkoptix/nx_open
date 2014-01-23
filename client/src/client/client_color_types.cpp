#include "client_color_types.h"

#include <ui/style/globals.h>

#include <utils/math/color_transformations.h>
#include <utils/common/json.h>
#include <utils/math/math.h>

namespace {
    int asciisum(const QString &value) {
        int result = 0;
        foreach (QChar c, value)
            result += c.toLatin1();
        return result;
    }

} // anonymous namespace


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

QnStatisticsColors::QnStatisticsColors() {
    grid = QColor(66, 140, 237, 100);
    frame = QColor(66, 140, 237);
    cpu = QColor(66, 140, 237);
    ram = QColor(219, 59, 169);
    networkLimit = QColor(Qt::white);
    
    hdds 
        << QColor(237, 237, 237)   // C: sda
        << QColor(237, 200, 66)    // D: sdb
        << QColor(103, 237, 66)    // E: sdc
        << QColor(255, 131, 48)    // F: sdd
        << QColor(178, 0, 255)     // etc
        << QColor(0, 255, 255)
        << QColor(38, 127, 0)
        << QColor(255, 127, 127)
        << QColor(201, 0, 0);

    network 
        << QColor(255, 52, 52)
        << QColor(240, 255, 52)
        << QColor(228, 52, 255)
        << QColor(255, 52, 132);
}

QColor QnStatisticsColors::hddByKey(const QString &key) const {
    static int sda = asciisum(QLatin1String("sda"));
    static int hda = asciisum(QLatin1String("hda"));

    if(hdds.isEmpty())
        return QColor();

    int id = 0;
    if (key.contains(QLatin1Char(':'))) {
        // cutting keys like 'C:' to 'C'. Also works with complex keys such as 'C: E:'
        id = key.at(0).toLatin1() - 'C';
    } else if (key.startsWith(QLatin1String("sd"))) {
        id = asciisum(key) - sda;
    } else if (key.startsWith(QLatin1String("hd"))) {
        id = asciisum(key) - hda;
    }
    return hdds[qMod(id, hdds.size())];
}

QColor QnStatisticsColors::networkByKey(const QString &key) const {
    if(network.isEmpty())
        return QColor();

    int id = asciisum(key);
    return network[qMod(id, network.size())];
}


QN_DEFINE_STRUCT_JSON_SERIALIZATION_FUNCTIONS_EX(
    QnTimeSliderColors, 
    (tickmark)(positionMarker)(indicator)(selection)(selectionMarker)
        (pastBackground)(futureBackground)(pastRecording)(futureRecording)(pastMotion)(futureMotion)
        (separator)(dateOverlay)(dateOverlayAlternate), 
    QJson::Optional
)

QN_DEFINE_STRUCT_JSON_SERIALIZATION_FUNCTIONS_EX(
    QnBackgroundColors, 
    (normal)(panic), 
    QJson::Optional
)

QN_DEFINE_STRUCT_JSON_SERIALIZATION_FUNCTIONS_EX(
    QnCalendarColors, 
    (selection)(primaryRecording)(secondaryRecording)(primaryMotion)(secondaryMotion)(separator), 
    QJson::Optional
)

QN_DEFINE_STRUCT_JSON_SERIALIZATION_FUNCTIONS_EX(
    QnStatisticsColors, 
    (grid)(frame)(cpu)(ram)(hdds), 
    QJson::Optional
)

