#ifndef VIDEO_SERVER_CAMERAS_DATA_H
#define VIDEO_SERVER_CAMERAS_DATA_H

#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <QtCore/QList>

struct QnCamerasFoundInfo
{
    QnCamerasFoundInfo(){}
    QnCamerasFoundInfo(QString _url, QString _name, QString _manufacturer, bool _exist):
        url(_url),
        name(_name),
        manufacturer(_manufacturer),
        existInPool(_exist)
    {}
    QString url;
    QString name;
    QString manufacturer;
    bool existInPool;
};
typedef QList<QnCamerasFoundInfo> QnCamerasFoundInfoList;

Q_DECLARE_METATYPE(QnCamerasFoundInfoList)

#endif // VIDEO_SERVER_CAMERAS_DATA_H
