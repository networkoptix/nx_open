#include "statistics_colors.h"

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

namespace detail {
    QN_DEFINE_STRUCT_JSON_SERIALIZATION_FUNCTIONS(QnStatisticsColors, (grid)(frame)(cpu)(ram)(hdds), static)
}

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
    return;
}

QColor QnStatisticsColors::hddByKey(const QString &key) const {
    static int sda = asciisum(QLatin1String("sda"));
    static int hda = asciisum(QLatin1String("hda"));

    int id = 0;
    if (key.contains(QLatin1Char(':'))) {
        // cutting keys like 'C:' to 'C'. Also works with complex keys such as 'C: E:'
        id = key.at(0).toLatin1() - 'C';
    }
    else if (key.startsWith(QLatin1String("sd")))
        id = asciisum(key) - sda;
    else if (key.startsWith(QLatin1String("hd")))
        id = asciisum(key) - hda;
    return hdds[qMod(id, hdds.size())];
}

QColor QnStatisticsColors::networkByKey(const QString &key) const {
    int id = asciisum(key);
    return network[qMod(id, network.size())];
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

    if (network.isEmpty()) {
        network   << QColor(255, 52, 52)
                    << QColor(240, 255, 52)
                    << QColor(228, 52, 255)
                    << QColor(255, 52, 132);
    }
}

void serialize(const QnStatisticsColors &value, QJsonValue *target) {
    detail::serialize(value, target);
}

bool deserialize(const QJsonValue &value, QnStatisticsColors *target) {
    if(value.type() == QJsonValue::Null) {
        /* That's null color storage. */
        *target = QnStatisticsColors();
        return true;
    }

    return detail::deserialize(value, target);
}
