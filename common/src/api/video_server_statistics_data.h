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
        RAM,
        HDD
    };

    QnStatisticsDataItem() {}
    QnStatisticsDataItem(const QString &description, qreal value, DeviceType deviceType): description(description), value(value), deviceType(deviceType) {}

    QString description;
    qreal value;
    DeviceType deviceType;
};

typedef QList<QnStatisticsDataItem> QnStatisticsDataList;

typedef QLinkedList<qreal> QnStatisticsData;
typedef QHash<QString, QnStatisticsData> QnStatisticsHistory;

// TODO: remove this iterator hell.
typedef QLinkedListIterator<qreal> QnStatisticsDataIterator; 
typedef QHashIterator<QString, QnStatisticsData> QnStatisticsIterator;
typedef QMutableHashIterator<QString, QnStatisticsData> QnStatisticsCleaner;

Q_DECLARE_METATYPE(QnStatisticsDataList)

#endif // QN_STATISTICS_DATA
