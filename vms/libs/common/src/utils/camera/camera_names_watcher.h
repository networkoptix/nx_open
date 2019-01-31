#pragma once

#include <QtCore/QObject>
#include <QtCore/QCoreApplication>

#include <nx/utils/uuid.h>
#include <common/common_module_aware.h>

namespace utils {

class QnCameraNamesWatcher: public QObject, public QnCommonModuleAware
{
    Q_OBJECT

    typedef QObject base_type;

public:
    explicit QnCameraNamesWatcher(QnCommonModule* commonModule);
    ~QnCameraNamesWatcher();
    QString getCameraName(const QnUuid& cameraId);

signals:
    void cameraNameChanged(const QnUuid& cameraId);

public:
    QHash<QnUuid, QString> m_names;
};

} // namespace utils
