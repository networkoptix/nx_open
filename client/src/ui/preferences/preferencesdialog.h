#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include "ui_preferences.h"

#include "settings.h"

class ConnectionsSettingsWidget;
class LicenseWidget;
class RecordingSettingsWidget;
class YouTubeSettingsWidget;

class PreferencesDialog : public QDialog, Ui::PreferencesDialog
{
    Q_OBJECT

public:
    enum SettingsPage {
        PageGeneral = 0,
        PageConnections = 1,
        PageRecordingSettings = 2,
        PageYouTubeSettings = 3
#ifndef CL_TRIAL_MODE
        , PageLicense = 4
#endif
    };

    PreferencesDialog(QWidget *parent = 0);
    ~PreferencesDialog();

    void setCurrentPage(SettingsPage page);

private:
    void updateView();
    void updateStoredConnections();

private Q_SLOTS:
    void updateCameras();

    void accept();
    void mainMediaFolderBrowse();
    void auxMediaFolderBrowse();
    void auxMediaFolderRemove();
    void cameraSelected(int);
    void auxMediaFolderSelectionChanged();

private:
    Q_DISABLE_COPY(PreferencesDialog)

    Settings::Data m_settingsData;
    ConnectionsSettingsWidget *connectionsSettingsWidget;
    RecordingSettingsWidget *videoRecorderWidget;
    YouTubeSettingsWidget *youTubeSettingsWidget;
    LicenseWidget *licenseWidget;

    typedef QPair<QString, QString> CameraNameAndInfo;

    QList<CameraNameAndInfo> m_cameras;
};

#endif // PREFERENCESDIALOG_H
