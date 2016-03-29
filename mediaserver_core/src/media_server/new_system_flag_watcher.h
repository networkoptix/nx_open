#pragma once

#include <core/resource/resource_fwd.h>

/**
 * Class for Validating 'new system' flag.
 * This flag must be set on server start if there are no enabled local administrators
 * and system is not linked to the cloud.
 * This flag must be cleaned on server start if there are enabled local administrators except admin/admin
 * or system is linked to cloud
 */
class QnNewSystemServerFlagWatcher: public QObject
{
    Q_OBJECT

    typedef QObject base_type;
public:
    QnNewSystemServerFlagWatcher(QObject* parent = nullptr);
    virtual ~QnNewSystemServerFlagWatcher();

private:
    void update();

    void at_resourcePool_resourceAdded(const QnResourcePtr &resource);

private:
    QnMediaServerResourcePtr m_server;
};