#ifndef VIDEO_SERVER_CAMERAS_DATA_H
#define VIDEO_SERVER_CAMERAS_DATA_H

#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <QtCore/QList>

struct QnCamerasFoundInfo
{
    QnCamerasFoundInfo(){}
    QnCamerasFoundInfo(QString _address, QString _name): address(_address), name(_name) {}
    QString address;
    QString name;
};
typedef QList<QnCamerasFoundInfo> QnCamerasFoundInfoList;

Q_DECLARE_METATYPE(QnCamerasFoundInfoList)

#endif // VIDEO_SERVER_CAMERAS_DATA_H
