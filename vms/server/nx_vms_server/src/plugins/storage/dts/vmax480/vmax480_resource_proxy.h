#pragma once

#include <set>
#include <map>

#include <utils/common/connective.h>
#include <common/common_globals.h>
#include <common/common_module_aware.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/server/resource/resource_fwd.h>

class QnVmax480ResourceProxy: public Connective<QObject>, public QnCommonModuleAware
{
    Q_OBJECT
public:
    QnVmax480ResourceProxy(QnCommonModule* commonModule);

    QString url(const QString& groupId) const;
    QString hostAddress(const QString& groupId) const;
    QAuthenticator auth(const QString& groupId) const;

    void setArchiveRange(const QString& groupId, qint64 startTimeUs, qint64 endTimeUs);

public slots:
    void at_resourceAdded(const QnResourcePtr& resource);
    void at_resourceRemoved(const QnResourcePtr& resource);

private:
    QnPlVmax480ResourcePtr resourceUnsafe(const QString& groupId) const;

private:
    mutable QnMutex m_mutex;
    std::map<QString, std::set<QnPlVmax480ResourcePtr>> m_vmaxResourcesByGroupId;
};
