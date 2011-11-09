#ifndef preferences_wnd_h
#define preferences_wnd_h

#include "ui_preferences.h"
#include "settings.h"
#include "core/resource/resource.h"


class QnResource;
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
    void resizeEvent (QResizeEvent * event);

    void updateView();

    QString cameraInfoString(QnResourcePtr device);

private:
    Settings::Data m_settingsData;
    RecordingSettingsWidget *videoRecorderWidget;
    YouTubeSettingsWidget *youTubeSettingsWidget;

    typedef QPair<QString, QString> CameraNameAndInfo;

    QList<CameraNameAndInfo> m_cameras;

private slots:
    void updateCameras();

    void accept();
    void mainMediaFolderBrowse();
    void auxMediaFolderBrowse();
    void auxMediaFolderRemove();
    void cameraSelected(int);
    void enterLicenseClick();
    void auxMediaFolderSelectionChanged();
};

#endif // preferences_wnd_h
