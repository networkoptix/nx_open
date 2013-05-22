#include "statistics_colors.h"

#include <utils/common/json.h>
#include <utils/math/math.h>

namespace {
    int asciisum(const QString &value) {
        int result = 0;
        foreach (QChar c, value)
            result += c.toAscii();
        return result;
    }

} // anonymous namespace


QnStatisticsColors::QnStatisticsColors():
    grid(QColor(66, 140, 237, 100)),
    frame(QColor(66, 140, 237)),
    cpu(QColor(66, 140, 237)),
    ram(QColor(219, 59, 169)),
    networkLimit(QColor(Qt::white))
{
    ensureVectors();
}

QnStatisticsColors::QnStatisticsColors(const QnStatisticsColors &other) {
    *this = other;
    ensureVectors();
}

QnStatisticsColors::~QnStatisticsColors() {

}

QColor QnStatisticsColors::hddByKey(const QString &key) const {
    static int sda = asciisum(QLatin1String("sda"));
    static int hda = asciisum(QLatin1String("hda"));

    int id = 0;
    if (key.contains(QLatin1Char(':'))) {
        // cutting keys like 'C:' to 'C'. Also works with complex keys such as 'C: E:'
        id = key.at(0).toAscii() - 'C';
    }
    else if (key.startsWith(QLatin1String("sd")))
        id = asciisum(key) - sda;
    else if (key.startsWith(QLatin1String("hd")))
        id = asciisum(key) - hda;
    return hdds[qMod(id, hdds.size())];
}

QColor QnStatisticsColors::networkInByKey(const QString &key) const {
    int id = asciisum(key);
    return networkIn[qMod(id, networkIn.size())];
}

QColor QnStatisticsColors::networkOutByKey(const QString &key) const {
    int id = asciisum(key);
    return networkOut[qMod(id, networkOut.size())];
}

void QnStatisticsColors::update(const QByteArray &serializedValue) {
    if (!QJson::deserialize(serializedValue, this))
        return;

    ensureVectors();
}

void QnStatisticsColors::ensureVectors() {
    if (hdds.isEmpty()) {
        hdds << QColor(237, 237, 237)   // C: sda
             << QColor(237, 200, 66)    // D: sdb
             << QColor(103, 237, 66)    // E: sdc
             << QColor(255, 131, 48)    // F: sdd
             << QColor(178, 0, 255)     // etc
             << QColor(0, 255, 255)
             << QColor(38, 127, 0)
             << QColor(255, 127, 127)
             << QColor(201, 0, 0);
    }

    if (networkIn.isEmpty()) {
        networkIn   << QColor(255, 52, 52)
                    << QColor(240, 255, 52)
                    << QColor(228, 52, 255)
                    << QColor(255, 52, 132);
    }

    if (networkOut.isEmpty()) {
        networkOut  << QColor(52, 94, 255)
                    << QColor(52, 198, 255)
                    << QColor(52, 255, 140)
                    << QColor(194, 255, 52);
    }
}
