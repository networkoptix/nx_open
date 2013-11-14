#ifndef QN_PREFERENCES_DIALOG_H
#define QN_PREFERENCES_DIALOG_H

#include <QtCore/QScopedPointer>
#include <QtWidgets/QDialog>

#include <ui/workbench/workbench_context_aware.h>
#include <ui/dialogs/button_box_dialog.h>

class QnWorkbenchContext;
class QnClientSettings;

class QnGeneralPreferencesWidget;
class QnConnectionsSettingsWidget;
class QnLicenseManagerWidget;
class QnRecordingSettingsWidget;
class QnServerSettingsWidget;
class QnPopupSettingsWidget;


namespace Ui {
    class PreferencesDialog;
}

class QnPreferencesDialog: public QnButtonBoxDialog, protected QnWorkbenchContextAware {
    Q_OBJECT

    typedef QnButtonBoxDialog base_type;

public:
    enum DialogPage {
        GeneralPage,
        RecordingPage,
        LicensesPage,
        ServerPage,
        NotificationsPage,

        PageCount
    };

    QnPreferencesDialog(QnWorkbenchContext *context, QWidget *parent = 0);
    ~QnPreferencesDialog();

    DialogPage currentPage() const;
    void setCurrentPage(DialogPage page);

    virtual void accept() override;
private:
    void updateFromSettings();
    void submitToSettings();

    bool isAdmin() const;

private:
    Q_DISABLE_COPY(QnPreferencesDialog)

    QScopedPointer<Ui::PreferencesDialog> ui;
    QnGeneralPreferencesWidget* m_generalPreferencesWidget;
    QnRecordingSettingsWidget *m_recordingSettingsWidget;
    QnPopupSettingsWidget *m_popupSettingsWidget;
    QnLicenseManagerWidget *m_licenseManagerWidget;
    QnServerSettingsWidget *m_serverSettingsWidget;
};

#endif // QN_PREFERENCES_DIALOG_H
