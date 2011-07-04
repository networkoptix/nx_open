#ifndef preferences_wnd_h
#define preferences_wnd_h

#include "ui_preferences.h"

#include "settings.h"

class CLDevice;

class PreferencesWindow : public QDialog, Ui::PreferencesDialog
{
    Q_OBJECT

public:
    PreferencesWindow();
    ~PreferencesWindow();

private:
    void resizeEvent (QResizeEvent * event);

    void updateView();
    void updateCameras();

    QString cameraInfoString(CLDevice *device);

private:
    Settings::Data m_settingsData;

    typedef QPair<QString, QString> CameraNameAndInfo;

    QList<CameraNameAndInfo> m_cameras;

private slots:
    void accept();
    void mainMediaFolderBrowse();
    void auxMediaFolderBrowse();
    void auxMediaFolderRemove();
    void allowChangeIPChanged();
    void cameraSelected(int);
    void enterLicenseClick();
    void auxMediaFolderSelectionChanged();

};

#endif // preferences_wnd_h