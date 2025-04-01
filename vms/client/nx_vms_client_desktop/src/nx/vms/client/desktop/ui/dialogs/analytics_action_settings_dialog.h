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
        const QJsonObject& initialValues = {},
        QWidget* parent = nullptr);

    QJsonObject getValues() const;

    virtual bool tryClose(bool force) override;

    static void request(
        const QJsonObject& settingsModel,
        std::function<void(std::optional<QJsonObject>)> requestHandler,
        const QJsonObject& initialValues = {},
        QWidget* parent = nullptr);
};

} // namespace nx::vms::client::desktop
