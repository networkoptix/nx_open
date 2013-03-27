#include "statistics_colors.h"

#include <utils/common/json.h>

/*
inline void serialize(const QnStatisticsColors &value, QVariant *target) {
    QVariantMap result;
    QJson::serialize(value.grid(), "grid", &result);
    QJson::serialize(value.frame(), "frame", &result);
    QJson::serialize(value.cpu(), "cpu", &result);
    QJson::serialize(value.ram(), "ram", &result);
    *target = result;
}

inline bool deserialize(const QVariant &value, QnStatisticsColors *target) {

    QVariantMap map;
    if(value.type() == QVariant::Map) {
        map = value.toMap();
    } else
        return false;

    *target = QnStatisticsColors();

    QColor grid, frame, cpu, ram;
    if (QJson::deserialize(map, "grid", &grid))
        target->setGrid(grid);
    if (QJson::deserialize(map, "frame", &frame))
        target->setFrame(grid);
    if (QJson::deserialize(map, "cpu", &cpu))
        target->setCpu(grid);
    if (QJson::deserialize(map, "ram", &ram))
        target->setRam(grid);
    return true;
}
*/

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

QColor QnStatisticsColors::hddByKey(const QString &key) const {
    int id = 0;
    if (key.contains(QLatin1Char(':'))) {
        // cutting keys like 'C:' to 'C'. Also works with complex keys such as 'C: E:'
        id = key.at(0).toAscii() - 'C';
    }
    else if (key.startsWith(QLatin1String("sd")))
        id = key.compare(QLatin1String("sda"));
    else if (key.startsWith(QLatin1String("hd")))
        id = key.compare(QLatin1String("hda"));
    return hdds[id % hdds.size()];
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
