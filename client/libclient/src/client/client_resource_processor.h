#ifndef QN_CLIENT_RESOURCE_PROCESSOR_H
#define QN_CLIENT_RESOURCE_PROCESSOR_H

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <core/resource/resource_processor.h>

class QnClientResourceProcessor: public QObject, public QnResourceProcessor {
    Q_OBJECT

public:
    virtual void processResources(const QnResourceList &resources) override;
};


#endif // QN_CLIENT_RESOURCE_PROCESSOR_H
