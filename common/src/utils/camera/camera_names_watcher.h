
#pragma once

#include <QtCore/QObject>
#include <QtCore/QCoreApplication>

namespace utils
{
    class QnCameraNamesWatcher : public QObject
    {
        Q_OBJECT

        typedef QObject base_type;
    public:
        explicit QnCameraNamesWatcher(bool showCameraIp = false
            , QObject *parent = nullptr);

        ~QnCameraNamesWatcher();

        QString getCameraName(const QString &cameraUuid);

    signals:
        void cameraNameChanged(const QString &cameraUuid);

    public:
        typedef QHash<QString, QString> NamesHash;

        bool m_showCameraIp;
        NamesHash m_names;
    };
}