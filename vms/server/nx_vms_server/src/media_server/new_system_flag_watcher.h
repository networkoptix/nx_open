#pragma once

#include <core/resource/resource_fwd.h>
#include <common/common_module_aware.h>

/**
 * Class for Validating 'new system' flag.
 * This flag must be set on server start if there are no enabled local administrators
 * and system is not linked to the cloud.
 * This flag must be cleaned on server start if there are enabled local administrators except admin/admin
 * or system is linked to cloud
 */
class QnNewSystemServerFlagWatcher: public QObject, public QnCommonModuleAware
{
    Q_OBJECT

    typedef QObject base_type;
public:
    QnNewSystemServerFlagWatcher(QObject* parent);
    virtual ~QnNewSystemServerFlagWatcher() {}
private:
    void update();
};