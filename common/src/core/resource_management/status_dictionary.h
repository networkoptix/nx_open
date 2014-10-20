#ifndef __STATUS_DICTIONARY_H
#define __STATUS_DICTIONARY_H

#include "utils/common/singleton.h"

class QnResourceStatusDiscionary: public Singleton<QnResourceStatusDiscionary>
{
public:
    Qn::ResourceStatus value(const QnUuid& resourceId) const;
    void setValue(const QnUuid& resourceId, Qn::ResourceStatus status);
    void clear();
    void clear(const QVector<QnUuid>& idList);
private:
    QMap<QnUuid, Qn::ResourceStatus> m_items;
    mutable QMutex m_mutex;
};

#define qnStatusDictionary QnResourceStatusDiscionary::instance()

#endif // __STATUS_DICTIONARY_H
