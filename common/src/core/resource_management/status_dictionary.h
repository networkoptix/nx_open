#ifndef __STATUS_DICTIONARY_H
#define __STATUS_DICTIONARY_H

#include <nx/utils/singleton.h>
#include <nx/utils/uuid.h>
#include <nx/utils/thread/mutex.h>

class QnResourceStatusDictionary: public QObject, public Singleton<QnResourceStatusDictionary>
{
    Q_OBJECT
public:
    QnResourceStatusDictionary(QObject *parent = NULL);

    Qn::ResourceStatus value(const QnUuid& resourceId) const;
    void setValue(const QnUuid& resourceId, Qn::ResourceStatus status);
    void clear();
    void clear(const QVector<QnUuid>& idList);
    QMap<QnUuid, Qn::ResourceStatus> values() const;
private:
    QMap<QnUuid, Qn::ResourceStatus> m_items;
    mutable QnMutex m_mutex;
};

#define qnStatusDictionary QnResourceStatusDictionary::instance()

#endif // __STATUS_DICTIONARY_H
