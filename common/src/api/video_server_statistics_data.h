#ifndef QN_STATISTICS_DATA
#define QN_STATISTICS_DATA

#include <QtCore/QMetaType>
#include <QtCore/QString>

struct QnStatisticsData {
    enum DeviceType {
        CPU,
        HDD
    };

    QString description;
    int value;
    DeviceType device;

    QnStatisticsData(){}

    QnStatisticsData(QString description, int value, DeviceType device): description(description), value(value), device(device) {}
};

typedef QVector<QnStatisticsData> QnStatisticsDataVector;

Q_DECLARE_METATYPE(QnStatisticsDataVector)

#endif // QN_STATISTICS_DATA