#ifndef preferences_wnd_h
#define preferences_wnd_h

#include "ui_preferences.h"

#include "settings.h"

class CLDevice;
class RecordingSettingsWidget;

class PreferencesWindow : public QDialog, Ui::PreferencesDialog
{
    Q_OBJECT

public:
    PreferencesWindow();
    ~PreferencesWindow();

    void setCurrentTab(int index);

private:
    void resizeEvent (QResizeEvent * event);

    void updateView();

    QString cameraInfoString(CLDevice *device);

private:
    Settings::Data m_settingsData;
    RecordingSettingsWidget *videoRecorderWidget;

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
