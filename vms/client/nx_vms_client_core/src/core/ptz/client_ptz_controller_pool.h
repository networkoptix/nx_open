// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/ptz/ptz_controller_pool.h>

class QnClientPtzControllerPool: public QnPtzControllerPool
{
    Q_OBJECT
    typedef QnPtzControllerPool base_type;

public:
    QnClientPtzControllerPool(
        nx::vms::common::SystemContext* systemContext,
        QObject* parent = nullptr);

protected:
    virtual void registerResource(const QnResourcePtr& resource) override;
    virtual void unregisterResource(const QnResourcePtr& resource) override;
    virtual QnPtzControllerPtr createController(const QnResourcePtr& resource) const override;

private:
    void cacheCameraPresets(const QnResourcePtr& resource);
    void reinitServerCameras(const QnMediaServerResourcePtr& server);
};
