#ifndef abstract_archive_device_h1838
#define abstract_archive_device_h1838

#include "device/media_resource.h"

class QnAbstractArchiveResource : public QnMediaResource
{
public:
    QnAbstractArchiveResource();
	~QnAbstractArchiveResource();

    virtual QString getUniqueId() const;
};

#endif //abstract_archive_device_h1838
