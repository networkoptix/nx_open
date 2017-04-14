#pragma once

#include <core/resource/resource_fwd.h>

#include <ui/dialogs/common/session_aware_dialog.h>

class QLabel;
class QnServerSettingsWidget;
class QnStorageAnalyticsWidget;
class QnStorageConfigWidget;

namespace Ui {
    class ServerSettingsDialog;
}

class QnServerSettingsDialog: public QnSessionAwareTabbedDialog {
    Q_OBJECT

    typedef QnSessionAwareTabbedDialog base_type;

public:
    enum DialogPage
    {
        SettingsPage,
        StatisticsPage,
        StorageManagmentPage,

        PageCount
    };

    QnServerSettingsDialog(QWidget *parent = NULL);
    virtual ~QnServerSettingsDialog();

    QnMediaServerResourcePtr server() const;
    void setServer(const QnMediaServerResourcePtr &server);

protected:
    virtual void retranslateUi() override;
    virtual void showEvent(QShowEvent* event) override;

    virtual QDialogButtonBox::StandardButton showConfirmationDialog() override;

private:
    void updateWebPageLink();

    void setupShowWebServerLink();

private:
    Q_DISABLE_COPY(QnServerSettingsDialog)

    QScopedPointer<Ui::ServerSettingsDialog> ui;
    QnMediaServerResourcePtr m_server;

    QnServerSettingsWidget* const m_generalPage;
    QnStorageAnalyticsWidget* const m_statisticsPage;
    QnStorageConfigWidget* const m_storagesPage;
    QLabel* const m_webPageLink;
};

Q_DECLARE_METATYPE(QnServerSettingsDialog::DialogPage)
