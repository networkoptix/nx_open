#include "statistics_colors.h"

#include <utils/common/json.h>
#include <utils/math/math.h>

QnStatisticsColors::QnStatisticsColors():
    grid(QColor(66, 140, 237, 100)),
    frame(QColor(66, 140, 237)),
    cpu(QColor(66, 140, 237)),
    ram(QColor(219, 59, 169))
{
    ensureHdds();
}

QnStatisticsColors::QnStatisticsColors(const QnStatisticsColors &source):
    grid(source.grid),
    frame(source.frame),
    cpu(source.cpu),
    ram(source.ram),
    hdds(source.hdds)
{
    ensureHdds();
}

QnStatisticsColors::~QnStatisticsColors() {

}

int asciisum(const QString &value) {
    int result = 0;
    foreach (QChar c, value)
        result += c.toAscii();
    return result;
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


void QnStatisticsColors::update(const QByteArray &serializedValue) {
    QnStatisticsColors other;

    if (!QJson::deserialize(serializedValue, &other))
        return;

    grid    = other.grid;
    frame   = other.frame;
    cpu     = other.cpu;
    ram     = other.ram;
    hdds    = other.hdds;
    ensureHdds();
}

void QnStatisticsColors::ensureHdds() {
    if (hdds.size() > 0)
        return;

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
