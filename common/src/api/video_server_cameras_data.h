#ifndef VIDEO_SERVER_CAMERAS_DATA_H
#define VIDEO_SERVER_CAMERAS_DATA_H

#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <QtCore/QList>

struct QnCamerasFoundInfo
{
    QnCamerasFoundInfo(){}
    QnCamerasFoundInfo(QString _url, QString _name, QString _manufacture):
        url(_url),
        name(_name),
        manufacture(_manufacture)
    {}
    QString url;
    QString name;
    QString manufacture;
};
typedef QList<QnCamerasFoundInfo> QnCamerasFoundInfoList;

Q_DECLARE_METATYPE(QnCamerasFoundInfoList)

#endif // VIDEO_SERVER_CAMERAS_DATA_H
