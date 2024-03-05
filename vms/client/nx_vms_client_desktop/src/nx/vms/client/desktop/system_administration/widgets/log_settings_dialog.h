// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <optional>

#include <nx/utils/log/log_level.h>
#include <nx/vms/api/data/log_settings.h>
#include <nx/vms/client/desktop/system_administration/watchers/logs_management_watcher.h>
#include <ui/dialogs/common/dialog.h>

namespace Ui { class LogSettingsDialog; }

namespace nx::vms::client::desktop {

struct ConfigurableLogSettings
{
    ConfigurableLogSettings();
    ConfigurableLogSettings(const nx::vms::api::ServerLogSettings& settings);

    nx::vms::api::ServerLogSettings applyTo(
        const nx::vms::api::ServerLogSettings& settings) const;

    void intersectWith(const ConfigurableLogSettings& other);

    static ConfigurableLogSettings defaults();

    using Level = std::optional<nx::log::Level>;
    using Size = std::optional<double>;
    using Time = std::optional<std::chrono::seconds>;

    Level loggingLevel;
    Size maxVolumeSizeB;
    Size maxFileSizeB;
    Time maxFileTime;

    Qt::CheckState splitByTimeState = Qt::Unchecked;
};

class LogSettingsDialog: public QnDialog
{
    Q_OBJECT
    using base_type = QnDialog;

public:
    explicit LogSettingsDialog(QWidget* parent = nullptr);
    virtual ~LogSettingsDialog();

    void init(const QList<LogsManagementUnitPtr>& units);

    bool hasChanges() const;
    ConfigurableLogSettings changes() const;

private:
    void resetToDefault();
    void updateWarnings();

private:
    void loadDataToUi(const ConfigurableLogSettings& settings);

    ConfigurableLogSettings::Level loggingLevel() const;
    void setLoggingLevel(ConfigurableLogSettings::Level level);

    ConfigurableLogSettings::Size maxVolumeSize() const;
    void setMaxVolumeSize(ConfigurableLogSettings::Size size);

    ConfigurableLogSettings::Size maxFileSize() const;
    void setMaxFileSize(ConfigurableLogSettings::Size size);

    ConfigurableLogSettings::Time maxFileTime() const;
    void setMaxFileTime(ConfigurableLogSettings::Time time);

private:
    QScopedPointer<Ui::LogSettingsDialog> ui;

    QList<nx::Uuid> m_ids;
    ConfigurableLogSettings m_initialSettings;
};

} // namespace nx::vms::client::desktop
