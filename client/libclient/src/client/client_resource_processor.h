#pragma once

#include <QtCore/QObject>

#include <client_core/connection_context_aware.h>

#include <core/resource/resource_fwd.h>
#include <core/resource/resource_processor.h>

class QnClientResourceProcessor:
    public QObject,
    public QnResourceProcessor,
    public QnConnectionContextAware
{
    Q_OBJECT

public:
    virtual void processResources(const QnResourceList &resources) override;
};
