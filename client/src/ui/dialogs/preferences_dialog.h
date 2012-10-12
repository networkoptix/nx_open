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


private:
    Q_DISABLE_COPY(QnPreferencesDialog)

    QScopedPointer<Ui::PreferencesDialog> ui;
    QnRecordingSettingsWidget *m_recordingSettingsWidget;
    QnYouTubeSettingsWidget *m_youTubeSettingsWidget;
    QnLicenseManagerWidget *m_licenseManagerWidget;

    QnSettings *m_settings;

    /** Index of "Licenses" tab to open it from outside */
    int m_licenseTabIndex;
};

#endif // QN_PREFERENCES_DIALOG_H
