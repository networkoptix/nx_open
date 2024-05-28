// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/api/data/camera_data_ex.h>
#include <nx/vms/api/data/camera_history_data.h>
#include <nx/vms/api/data/license_data.h>
#include <nx/vms/api/data/module_information.h>
#include <nx/vms/api/data/system_settings.h>
#include <nx/vms/api/data/user_model.h>

namespace rest {

class ServerConnection;
using ServerConnectionPtr = std::shared_ptr<ServerConnection>;

} // namespace rest

namespace nx::vms::client::desktop {

/**
 * Loads full set of data required for the cross-system cameras to work.
 *
 * Does not require additional control (create-and-forget). Emits `ready()` on first full dataset,
 * then `camerasUpdated()` when cameras are updated.
 */
class CloudCrossSystemContextDataLoader: public QObject
{
    Q_OBJECT

public:
    CloudCrossSystemContextDataLoader(
        rest::ServerConnectionPtr connection,
        const QString& username,
        QObject* parent = nullptr);
    virtual ~CloudCrossSystemContextDataLoader() override;

    void start(bool requestUser);

    nx::vms::api::UserModel user() const;
    nx::vms::api::ServerInformationV1List servers() const;
    nx::vms::api::ServerFootageDataList serverFootageData() const;
    nx::vms::api::CameraDataExList cameras() const;
    nx::vms::api::SystemSettings systemSettings() const;
    nx::vms::api::LicenseDataList licenses() const;
    void requestData();

signals:
    void ready();
    void camerasUpdated();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
