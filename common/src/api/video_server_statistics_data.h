#ifndef QN_STATISTICS_DATA
#define QN_STATISTICS_DATA

#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <QtCore/QHash>
#include <QtCore/QHashIterator>
#include <QtCore/QMutableHashIterator>
#include <QtCore/QLinkedList>
#include <QtCore/QLinkedListIterator>

struct QnStatisticsDataItem {
    enum DeviceType {
        CPU,
        HDD
    };

    QString description;
    int value;
    DeviceType device;

    QnStatisticsDataItem(){}

    QnStatisticsDataItem(QString description, int value, DeviceType device): description(description), value(value), device(device) {}
};

typedef QList<QnStatisticsDataItem> QnStatisticsDataList;

typedef QLinkedList<int> QnStatisticsData;
typedef QLinkedListIterator<int> QnStatisticsDataIterator;
typedef QHash<QString, QnStatisticsData> QnStatisticsHistory;
typedef QHashIterator<QString, QnStatisticsData> QnStatisticsIterator;
typedef QMutableHashIterator<QString, QnStatisticsData> QnStatisticsCleaner;

Q_DECLARE_METATYPE(QnStatisticsDataList)

#endif // QN_STATISTICS_DATA
