#pragma once

#include <core/resource/resource_fwd.h>

#include <ui/dialogs/common/session_aware_dialog.h>

#include <nx/utils/impl_ptr.h>

namespace Ui { class ServerSettingsDialog; }

class QnServerSettingsDialog: public QnSessionAwareTabbedDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareTabbedDialog;

public:
    enum DialogPage
    {
        SettingsPage,
        StatisticsPage,
        StorageManagmentPage,
        PluginsPage,

        PageCount
    };

    QnServerSettingsDialog(QWidget* parent = nullptr);
    virtual ~QnServerSettingsDialog() override;

    QnMediaServerResourcePtr server() const;
    void setServer(const QnMediaServerResourcePtr& server);

protected:
    virtual void retranslateUi() override;
    virtual void showEvent(QShowEvent* event) override;

    virtual QDialogButtonBox::StandardButton showConfirmationDialog() override;

private:
    void updateWebPageLink();
    void setupShowWebServerLink();

private:
    struct Private;
    nx::utils::ImplPtr<Ui::ServerSettingsDialog> ui;
    nx::utils::ImplPtr<Private> d;
};

Q_DECLARE_METATYPE(QnServerSettingsDialog::DialogPage)
