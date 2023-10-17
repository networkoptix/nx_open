// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <QtCore/QJsonObject>

#include <nx/vms/client/desktop/common/dialogs/qml_dialog_wrapper.h>
#include <ui/workbench/workbench_state_manager.h>

namespace nx::vms::client::desktop {

class AnalyticsActionSettingsDialog:
    public QmlDialogWrapper,
    public QnSessionAwareDelegate
{
    Q_OBJECT
public:
    AnalyticsActionSettingsDialog(
        const QJsonObject& settingsModel,
        QWidget* parent = nullptr);

    QJsonObject getValues() const;

    virtual bool tryClose(bool force) override;

    static std::optional<QJsonObject> request(
        const QJsonObject& settingsModel,
        QWidget* parent = nullptr);
};

} // nx::vms::client::desktop
