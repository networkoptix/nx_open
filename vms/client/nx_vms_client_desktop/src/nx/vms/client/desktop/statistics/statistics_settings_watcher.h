// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/common/utils/common_module_aware.h>
#include <statistics/statistics_settings.h>

namespace nx::vms::client::desktop {

/**
 * Loads statistics server url, storage parameters, actual filters, etc.
 * By default, hardcoded path is used, but system administrator can override it with
 * `clientStatisticsSettingsUrl` global settings parameter.
 * Opensource clients have no hardcoded option.
 */
class StatisticsSettingsWatcher:
    public QObject,
    public core::CommonModuleAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    StatisticsSettingsWatcher(QObject* parent = nullptr);
    virtual ~StatisticsSettingsWatcher() override;

    std::optional<QnStatisticsSettings> settings() const;

    void updateSettings();

signals:
    void settingsAvailableChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
