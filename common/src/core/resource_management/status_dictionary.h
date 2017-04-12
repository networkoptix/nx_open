#pragma once

#include <nx/utils/singleton.h>
#include <nx/utils/uuid.h>
#include <nx/utils/thread/mutex.h>
#include <common/common_module_aware.h>

class QnResourceStatusDictionary: public QObject, public QnCommonModuleAware
{
    Q_OBJECT
public:
    QnResourceStatusDictionary(QObject* parent);

    Qn::ResourceStatus value(const QnUuid& resourceId) const;
    void setValue(const QnUuid& resourceId, Qn::ResourceStatus status);
    void clear();
    void clear(const QVector<QnUuid>& idList);
    QMap<QnUuid, Qn::ResourceStatus> values() const;
private:
    QMap<QnUuid, Qn::ResourceStatus> m_items;
    mutable QnMutex m_mutex;
};
