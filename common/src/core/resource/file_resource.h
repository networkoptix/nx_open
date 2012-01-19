#ifndef file_device_h_227
#define file_device_h_227

#include "resource.h"

class QN_EXPORT  QnLocalFileResource : public QnResource
{
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

#endif // file_device_h_227
