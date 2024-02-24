// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/network_block_data.h>

namespace nx::vms::client::desktop {

class PoeController: public QObject
{
    Q_OBJECT
    using base_type = QObject;

    Q_PROPERTY(bool initialUpdateInProgress READ initialUpdateInProgress
        NOTIFY initialUpdateInProgressChanged)
    Q_PROPERTY(bool updatingPoweringModes READ updatingPoweringModes
        NOTIFY updatingPoweringModesChanged)
public:
    PoeController(QObject* parent = nullptr);
    virtual ~PoeController();

    void setServer(const QnMediaServerResourcePtr& value);
    QnMediaServerResourcePtr server() const;

    void setAutoUpdate(bool value);

    using BlockData = nx::vms::api::NetworkBlockData;
    const BlockData& blockData() const;

    using PortPoweringMode = nx::vms::api::NetworkPortWithPoweringMode;
    using PowerModes = QList<PortPoweringMode>;
    void setPowerModes(const PowerModes& value);

    bool initialUpdateInProgress() const;
    bool updatingPoweringModes() const;
    void cancelRequest();

signals:
    void updated();
    void updatingPoweringModesChanged();
    void initialUpdateInProgressChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
