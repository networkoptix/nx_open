#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include <QtGui/QDialog>

#include "settings.h"


class QnWorkbenchContext;

class ConnectionsSettingsWidget;
class LicenseManagerWidget;
class RecordingSettingsWidget;
class YouTubeSettingsWidget;

namespace Ui {
    class PreferencesDialog;
}

class PreferencesDialog : public QDialog
{
    Q_OBJECT

public:
    enum SettingsPage {
        PageGeneral = 0,
        PageConnections = 1,
        PageRecordingSettings = 2,
        PageYouTubeSettings = 3,
        PageLicense = 4
    };

    PreferencesDialog(QnWorkbenchContext *context, QWidget *parent = 0);
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

    QScopedPointer<Ui::PreferencesDialog> ui;
    QnSettings::Data m_settingsData;
    ConnectionsSettingsWidget *connectionsSettingsWidget;
    RecordingSettingsWidget *videoRecorderWidget;
    YouTubeSettingsWidget *youTubeSettingsWidget;
    LicenseManagerWidget *licenseManagerWidget;

    typedef QPair<QString, QString> CameraNameAndInfo;

    QList<CameraNameAndInfo> m_cameras;
};

#endif // PREFERENCESDIALOG_H
