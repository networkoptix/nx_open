#ifndef QN_PREFERENCES_DIALOG_H
#define QN_PREFERENCES_DIALOG_H

#include <QtCore/QScopedPointer>
#include <QtGui/QDialog>
#include "utils/settings.h"


class QnWorkbenchContext;

class ConnectionsSettingsWidget;
class LicenseManagerWidget;
class RecordingSettingsWidget;
class YouTubeSettingsWidget;

namespace Ui {
    class PreferencesDialog;
}

class QnPreferencesDialog : public QDialog {
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
    void updateView();
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
    ConnectionsSettingsWidget *connectionsSettingsWidget;
    RecordingSettingsWidget *videoRecorderWidget;
    YouTubeSettingsWidget *youTubeSettingsWidget;
    LicenseManagerWidget *licenseManagerWidget;

    typedef QPair<QString, QString> CameraNameAndInfo;

    QList<CameraNameAndInfo> m_cameras;
};

#endif // QN_PREFERENCES_DIALOG_H
