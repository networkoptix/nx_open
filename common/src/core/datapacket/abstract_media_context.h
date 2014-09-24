#ifndef QN_ABSTRACT_MEDIA_CONTEXT_H
#define QN_ABSTRACT_MEDIA_CONTEXT_H

#include <QtCore/QSharedPointer>

class QnAbstractMediaContext
{
public:
    QnAbstractMediaContext() {}
    virtual ~QnAbstractMediaContext() {}
};
typedef QSharedPointer<QnAbstractMediaContext> QnAbstractMediaContextPtr;

#endif // QN_ABSTRACT_MEDIA_CONTEXT_H
