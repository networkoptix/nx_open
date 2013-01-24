#ifndef QN_PREFERENCES_DIALOG_H
#define QN_PREFERENCES_DIALOG_H

#include <QtCore/QScopedPointer>
#include <QtGui/QDialog>

#include <ui/workbench/workbench_context_aware.h>


class QnWorkbenchContext;
class QnSettings;

class QnConnectionsSettingsWidget;
class QnLicenseManagerWidget;
class QnRecordingSettingsWidget;
class QnYouTubeSettingsWidget;
class QnSmtpSettingsWidget; //TODO: temporary, until we have wider server settings widget
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

    virtual void accept() override;

private:
    void updateFromSettings();
    void submitToSettings();

    void initColorPicker();
    void initLanguages();

private slots:
    void at_animateBackgroundCheckBox_stateChanged(int state);
    void at_backgroundColorPicker_colorChanged(const QColor &color);
    void at_browseMainMediaFolderButton_clicked();
    void at_addExtraMediaFolderButton_clicked();
    void at_removeExtraMediaFolderButton_clicked();
    void at_extraMediaFoldersList_selectionChanged();
    void at_context_userChanged();
    void at_timeModeComboBox_activated();
    void at_onDecoderPluginsListChanged();

private:
    Q_DISABLE_COPY(QnPreferencesDialog)

    QScopedPointer<Ui::PreferencesDialog> ui;
    QnRecordingSettingsWidget *m_recordingSettingsWidget;
    QnYouTubeSettingsWidget *m_youTubeSettingsWidget;
    QnLicenseManagerWidget *m_licenseManagerWidget;
    QnSmtpSettingsWidget *m_smtpSettingsWidget;
    QnPopupSettingsWidget *m_popupSettingsWidget;

    QnSettings *m_settings;

    /** Index of "Licenses" tab to open it from outside */
    int m_licenseTabIndex;

    /** Index of "Server Settings" tab to open it from outside */
    int m_serverSettingsTabIndex;

    int m_popupSettingsTabIndex;
};

#endif // QN_PREFERENCES_DIALOG_H
