// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/network/server_certificate_validation_level.h>
#include <ui/widgets/common/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>

namespace Ui { class AdvancedSettingsWidget; }

class QnAdvancedSettingsWidget : public QnAbstractPreferencesWidget, public QnWorkbenchContextAware
{
    Q_OBJECT
    typedef QnAbstractPreferencesWidget base_type;

public:
    explicit QnAdvancedSettingsWidget(QWidget* parent = nullptr);
    virtual ~QnAdvancedSettingsWidget() override;

    virtual void applyChanges() override;
    virtual void loadDataToUi() override;
    virtual bool hasChanges() const override;

    bool isRestartRequired() const;

private slots:
    void at_clearCacheButton_clicked();
    void at_resetAllWarningsButton_clicked();

private:
    bool isAudioDownmixed() const;
    void setAudioDownmixed(bool value);

    bool isDoubleBufferingEnabled() const;
    void setDoubleBufferingEnabled(bool value);

    bool isAutoFpsLimitEnabled() const;
    void setAutoFpsLimitEnabled(bool value);

    bool isBlurEnabled() const;
    void setBlurEnabled(bool value);

    bool isHardwareDecodingEnabled() const;
    void setHardwareDecodingEnabled(bool value);

    int  maximumLiveBufferMs() const;
    void setMaximumLiveBufferMs(int value);

    using ServerCertificateValidationLevel =
        nx::vms::client::core::network::server_certificate::ValidationLevel;
    ServerCertificateValidationLevel certificateValidationLevel() const;
    void setCertificateValidationLevel(ServerCertificateValidationLevel value);
    void updateCertificateValidationLevelDescription();
    void updateLogsManagementWidgetsState();

private:
    QScopedPointer<Ui::AdvancedSettingsWidget> ui;

    struct Private;
    nx::utils::ImplPtr<Private> d;
};
