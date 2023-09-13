// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <QtCore/QScopedPointer>

#include <nx/vms/client/desktop/common/dialogs/abstract_preferences_dialog.h>
#include <ui/workbench/workbench_context_aware.h>

namespace Ui
{
    class LocalSettingsDialog;
}

class QLabel;
class QnAdvancedSettingsWidget;
class QnLookAndFeelPreferencesWidget;

class QnLocalSettingsDialog:
    public nx::vms::client::desktop::AbstractPreferencesDialog,
    public QnWorkbenchContextAware
{
    Q_OBJECT

    using base_type = nx::vms::client::desktop::AbstractPreferencesDialog;

    using Callback = std::function<void()>;

public:
    enum DialogPage
    {
        GeneralPage,
        LookAndFeelPage,
        RecordingPage,
        NotificationsPage,
        AdvancedPage,

        PageCount
    };

    QnLocalSettingsDialog(QWidget *parent = 0);
    ~QnLocalSettingsDialog();

protected:
    virtual bool canApplyChanges() const override;
    virtual void applyChanges() override;
    virtual void updateButtonBox() override;

    virtual void accept() override;
private:
    bool isRestartRequired() const;

    void addRestartLabel();
    void executeWithRestartCheck(Callback function) const;

private:
    Q_DISABLE_COPY(QnLocalSettingsDialog)

    QScopedPointer<Ui::LocalSettingsDialog> ui;
    QnLookAndFeelPreferencesWidget* m_lookAndFeelWidget;
    QnAdvancedSettingsWidget* m_advancedSettingsWidget;
    QLabel* m_restartLabel;

    mutable bool m_checkingRestart = false;
};
