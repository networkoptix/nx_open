#ifndef __STATUS_DICTIONARY_H
#define __STATUS_DICTIONARY_H

#include "utils/common/singleton.h"

class QnResourceStatusDictionary: public QObject, public Singleton<QnResourceStatusDictionary>
{
    Q_OBJECT
public:
    Qn::ResourceStatus value(const QnUuid& resourceId) const;
    void setValue(const QnUuid& resourceId, Qn::ResourceStatus status);
    void clear();
    void clear(const QVector<QnUuid>& idList);
    QMap<QnUuid, Qn::ResourceStatus> values() const;
private:
    QMap<QnUuid, Qn::ResourceStatus> m_items;
    mutable QMutex m_mutex;
};

#define qnStatusDictionary QnResourceStatusDictionary::instance()

#endif // __STATUS_DICTIONARY_H
