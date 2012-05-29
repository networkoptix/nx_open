#ifndef QN_PREFERENCES_DIALOG_H
#define QN_PREFERENCES_DIALOG_H

#include <QtCore/QScopedPointer>
#include <QtGui/QDialog>

#include <ui/workbench/workbench_context_aware.h>


class QnWorkbenchContext;

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

private:
    void updateFromSettings();
    void updateStoredConnections();
    void initColorPicker();

private slots:
    void at_animateBackgroundCheckBox_stateChanged(int state);
    void at_backgroundColorPicker_colorChanged(const QColor &color);

    void updateCameras();

    void accept();
    void mainMediaFolderBrowse();
    void auxMediaFolderBrowse();
    void auxMediaFolderRemove();
    void cameraSelected(int);
    void auxMediaFolderSelectionChanged();

private:
    Q_DISABLE_COPY(QnPreferencesDialog)

    QScopedPointer<Ui::PreferencesDialog> ui;
    QnSettings::Data m_settingsData;
    QnConnectionsSettingsWidget *connectionsSettingsWidget;
    QnRecordingSettingsWidget *videoRecorderWidget;
    QnYouTubeSettingsWidget *youTubeSettingsWidget;
    QnLicenseManagerWidget *licenseManagerWidget;

    typedef QPair<QString, QString> CameraNameAndInfo;

    QList<CameraNameAndInfo> m_cameras;
};

#endif // QN_PREFERENCES_DIALOG_H
