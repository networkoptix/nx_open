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

public:
    enum SettingsPage {
        PageGeneral = 0,
        PageConnections = 1,
        PageRecordingSettings = 2,
        PageLicense = 3,
        PageYouTubeSettings = 10,
    };

    QnPreferencesDialog(QnWorkbenchContext *context, QWidget *parent = 0);
    ~QnPreferencesDialog();

    void setCurrentPage(SettingsPage page);

    virtual void accept() override;

private:
    void updateFromSettings();
    void commitToSettings();

    void updateStoredConnections();
    void initColorPicker();

private slots:
    void at_animateBackgroundCheckBox_stateChanged(int state);
    void at_backgroundColorPicker_colorChanged(const QColor &color);
    void at_browseMainMediaFolderButton_clicked();
    void at_addExtraMediaFolderButton_clicked();
    void at_removeExtraMediaFolderButton_clicked();
    void at_extraMediaFoldersList_selectionChanged();

private:
    Q_DISABLE_COPY(QnPreferencesDialog)

    QScopedPointer<Ui::PreferencesDialog> ui;
    QnConnectionsSettingsWidget *m_connectionsSettingsWidget;
    QnRecordingSettingsWidget *m_recordingSettingsWidget;
    QnYouTubeSettingsWidget *m_youTubeSettingsWidget;
    QnLicenseManagerWidget *m_licenseManagerWidget;

    QnSettings *m_settings;
};

#endif // QN_PREFERENCES_DIALOG_H
