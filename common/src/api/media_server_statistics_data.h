#ifndef QN_STATISTICS_DATA
#define QN_STATISTICS_DATA

#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <QtCore/QHash>
#include <QtCore/QHashIterator>
#include <QtCore/QMutableHashIterator>
#include <QtCore/QLinkedList>
#include <QtCore/QLinkedListIterator>

enum  QnStatisticsDeviceType {
    CPU,
    RAM,
    HDD
};

struct QnStatisticsDataItem {

    QnStatisticsDataItem() {}
    QnStatisticsDataItem(const QString &description, qreal value, QnStatisticsDeviceType deviceType): description(description), value(value), deviceType(deviceType) {}

    QString description;
    qreal value;
    QnStatisticsDeviceType deviceType;
};

typedef QList<QnStatisticsDataItem> QnStatisticsDataList;

struct QnStatisticsData{
    QString description;
    QnStatisticsDeviceType deviceType;
    QLinkedList<qreal> values;
};

typedef QHash<QString, QnStatisticsData> QnStatisticsHistory;

// TODO: remove this iterator hell.
typedef QLinkedListIterator<qreal> QnStatisticsDataIterator; 
typedef QHashIterator<QString, QnStatisticsData> QnStatisticsIterator;
typedef QMutableHashIterator<QString, QnStatisticsData> QnStatisticsCleaner;

Q_DECLARE_METATYPE(QnStatisticsDataList)
Q_DECLARE_METATYPE(QnStatisticsData)

#endif // QN_STATISTICS_DATA
