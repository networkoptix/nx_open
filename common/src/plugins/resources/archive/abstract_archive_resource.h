#ifndef abstract_archive_device_h1838
#define abstract_archive_device_h1838

#include "core/resource/media_resource.h"

class QnAbstractArchiveResource : public QnMediaResource
{
    Q_OBJECT;

public:
    QnAbstractArchiveResource();
    ~QnAbstractArchiveResource();

    virtual QString getUniqueId() const;

    virtual void setStatus(Status newStatus, bool silenceMode = false) override;
};

#endif //abstract_archive_device_h1838
