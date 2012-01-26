#ifndef QN_LOCAL_FILE_RESOURCE_H
#define QN_LOCAL_FILE_RESOURCE_H

#include "resource.h"

class QN_EXPORT QnLocalFileResource : public QnResource
{
    Q_OBJECT;

public:
    QnLocalFileResource(const QString &filename);

    inline QString toString() const
    { return getName(); }
    inline QString getFileName() const
    { return getUrl(); }

    virtual QString getUniqueId() const;

protected:
    virtual QnAbstractStreamDataProvider* createDataProviderInternal(ConnectionRole role);
};

typedef QSharedPointer<QnLocalFileResource> QnLocalFileResourcePtr;

#endif // QN_LOCAL_FILE_RESOURCE_H
