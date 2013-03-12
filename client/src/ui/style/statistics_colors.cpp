#include "statistics_colors.h"

#include <utils/common/json.h>


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

    if (value.type() == QVariant::String) {
        QJson::deserialize(value.toString(), &map);
    } else
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


QnStatisticsColors::QnStatisticsColors():
    m_grid(QColor(66, 140, 237, 100)),
    m_frame(QColor(66, 140, 237)),
    m_cpu(QColor(66, 140, 237)),
    m_ram(QColor(219, 59, 169))
{
    m_hdds << QColor(237, 237, 237)   // C: sda
           << QColor(237, 200, 66)    // D: sdb
           << QColor(103, 237, 66)    // E: sdc
           << QColor(255, 131, 48)    // F: sdd
           << QColor(178, 0, 255)     // etc
           << QColor(0, 255, 255)
           << QColor(38, 127, 0)
           << QColor(255, 127, 127)
           << QColor(201, 0, 0);
}

QnStatisticsColors::QnStatisticsColors(const QnStatisticsColors &source):
    m_grid(source.grid()),
    m_frame(source.frame()),
    m_cpu(source.cpu()),
    m_ram(source.ram()),
    m_hdds(source.hdds())
{
}

QnStatisticsColors::~QnStatisticsColors() {

}

QColor QnStatisticsColors::grid() const {
    return m_grid;
}

void QnStatisticsColors::setGrid(const QColor &value) {
    m_grid = value;
}

QColor QnStatisticsColors::frame() const {
    return m_frame;
}

void QnStatisticsColors::setFrame(const QColor &value) {
    m_frame = value;
}

QColor QnStatisticsColors::cpu() const {
    return m_cpu;
}

void QnStatisticsColors::setCpu(const QColor &value) {
    m_cpu = value;
}

QColor QnStatisticsColors::ram() const {
    return m_ram;
}

void QnStatisticsColors::setRam(const QColor &value) {
    m_ram = value;
}

QColor QnStatisticsColors::hdd(const QString &key) const {
    int id = 0;
    if (key.contains(QLatin1Char(':'))) {
        // cutting keys like 'C:' to 'C'. Also works with complex keys such as 'C: E:'
        id = key.at(0).toAscii() - 'C';
    }
    else if (key.startsWith(QLatin1String("sd")))
        id = key.compare(QLatin1String("sda"));
    else if (key.startsWith(QLatin1String("hd")))
        id = key.compare(QLatin1String("hda"));
    return m_hdds[id & m_hdds.size()];
}

QVector<QColor> QnStatisticsColors::hdds() const {
    return m_hdds;
}

void QnStatisticsColors::update(const QString &serializedValue) {
    QnStatisticsColors colors;
    if (!QJson::deserialize(serializedValue, &colors))
        return;

    m_grid = colors.grid();
    m_frame = colors.frame();
    m_cpu = colors.cpu();
    m_ram = colors.ram();

}
