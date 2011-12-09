#ifndef preferences_wnd_h
#define preferences_wnd_h

#include "ui_preferences.h"

#include "settings.h"

class LicenseWidget;
class RecordingSettingsWidget;
class YouTubeSettingsWidget;

class PreferencesWindow : public QDialog, Ui::PreferencesDialog
{
    Q_OBJECT

public:
    PreferencesWindow(QWidget *parent = 0);
    ~PreferencesWindow();

    void setCurrentTab(int index);

private:
    void updateView();

private:
    Settings::Data m_settingsData;
    RecordingSettingsWidget *videoRecorderWidget;
    YouTubeSettingsWidget *youTubeSettingsWidget;
    LicenseWidget *licenseWidget;

    typedef QPair<QString, QString> CameraNameAndInfo;

    QList<CameraNameAndInfo> m_cameras;

private slots:
    void updateCameras();

    void accept();
    void mainMediaFolderBrowse();
    void auxMediaFolderBrowse();
    void auxMediaFolderRemove();
    void cameraSelected(int);
    void auxMediaFolderSelectionChanged();
};

#endif // preferences_wnd_h
