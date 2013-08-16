#ifndef QN_PREFERENCES_DIALOG_H
#define QN_PREFERENCES_DIALOG_H

#include <QtCore/QScopedPointer>
#include <QtWidgets/QDialog>

#include <ui/workbench/workbench_context_aware.h>


class QnWorkbenchContext;
class QnClientSettings;

class QnConnectionsSettingsWidget;
class QnLicenseManagerWidget;
class QnRecordingSettingsWidget;
class QnYouTubeSettingsWidget;
class QnServerSettingsWidget;
class QnPopupSettingsWidget;


namespace Ui {
    class PreferencesDialog;
}

class QnPreferencesDialog: public QDialog, protected QnWorkbenchContextAware {
    Q_OBJECT

    typedef QDialog base_type;

public:
    QnPreferencesDialog(QnWorkbenchContext *context, QWidget *parent = 0);
    ~QnPreferencesDialog();

    void openLicensesPage();
    void openServerSettingsPage();
    void openPopupSettingsPage();

    virtual void accept() override;

    bool restartPending() const;

private:
    void updateFromSettings();
    void submitToSettings();

    void initTranslations();

private slots:
    void at_browseMainMediaFolderButton_clicked();
    void at_addExtraMediaFolderButton_clicked();
    void at_removeExtraMediaFolderButton_clicked();
    void at_extraMediaFoldersList_selectionChanged();
    void at_context_userChanged();
    void at_timeModeComboBox_activated();
    void at_onDecoderPluginsListChanged();
    void at_downmixAudioCheckBox_toggled(bool checked);
    void at_languageComboBox_currentIndexChanged(int index);

private:
    Q_DISABLE_COPY(QnPreferencesDialog)

    QScopedPointer<Ui::PreferencesDialog> ui;
    QnRecordingSettingsWidget *m_recordingSettingsWidget;
    QnYouTubeSettingsWidget *m_youTubeSettingsWidget;
    QnPopupSettingsWidget *m_popupSettingsWidget;
    QnLicenseManagerWidget *m_licenseManagerWidget;
    QnServerSettingsWidget *m_serverSettingsWidget;

    QnClientSettings *m_settings;

    /** Index of "Licenses" tab to open it from outside. */
    int m_licenseTabIndex;

    /** Index of "Server Settings" tab to open it from outside. */
    int m_serverSettingsTabIndex;

    int m_popupSettingsTabIndex;

    bool m_oldDownmix;
    int m_oldLanguage;
    bool m_restartPending;
    bool m_oldHardwareAcceleration;
};

#endif // QN_PREFERENCES_DIALOG_H
