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
    void closeEvent (QCloseEvent * event );
    void resizeEvent (QResizeEvent * event);

    void updateView();
    void updateCameras();

    QString cameraInfoString(CLDevice *device);

private:
    Settings::Data m_settingsData;
    QMap<QString, QString> m_cameras;

private slots:
    void accept();
    void mainMediaFolderBrowse();
    void auxMediaFolderBrowse();
    void auxMediaFolderRemove();
    void allowChangeIPChanged();
    void cameraSelected();
    void enterLicenseClick();
};

#endif // preferences_wnd_h