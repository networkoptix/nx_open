#ifndef __VIDEO_SERVER_STATISTICS_H_
#define __VIDEO_SERVER_STATISTICS_H_

struct QnStatisticsData{
    enum DeviceType {
        CPU,
        HDD
    };

    QString Description;
    int Value;
    DeviceType Device;

    QnStatisticsData(){}

    QnStatisticsData(QString description, int value, DeviceType device):
        Description(description),
        Value(value),
        Device(device){}
};

typedef QVector<QnStatisticsData> QnStatisticsDataVector;

Q_DECLARE_METATYPE(QnStatisticsDataVector)

#endif __VIDEO_SERVER_STATISTICS_H_