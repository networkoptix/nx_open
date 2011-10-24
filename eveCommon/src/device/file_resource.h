#ifndef file_device_h_227
#define file_device_h_227

#include "qnresource.h"

class QnLocalFileResource : public QnResource
{
public:
    QnLocalFileResource(const QString &filename);


    inline QString toString() const
    { return getName(); }
    inline QString getFileName() const
    { return getUniqueId(); }

    virtual QnAbstractStreamDataProvider* createDataProvider(ConnectionRole role);
    virtual QString getUniqueId() const;
private:
    QString m_fileName;
};

typedef QSharedPointer<QnLocalFileResource> QnLocalFileResourcePtr;

#endif // file_device_h_227
