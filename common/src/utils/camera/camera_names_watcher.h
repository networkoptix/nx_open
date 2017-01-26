#pragma once

#include <QtCore/QObject>
#include <QtCore/QCoreApplication>

#include <nx/utils/uuid.h>

namespace utils {

class QnCameraNamesWatcher: public QObject
{
    Q_OBJECT

    typedef QObject base_type;

public:
    explicit QnCameraNamesWatcher(QObject *parent = nullptr);
    ~QnCameraNamesWatcher();
    QString getCameraName(const QnUuid& cameraId);

signals:
    void cameraNameChanged(const QnUuid& cameraId);

public:
    QHash<QnUuid, QString> m_names;
};

} // namespace utils
