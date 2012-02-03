#ifndef CAMERA_SETTINGS_DIALOG_H
#define CAMERA_SETTINGS_DIALOG_H

#include <QtGui/QDialog>

#include "api/AppServerConnection.h"

namespace Ui {
    class CameraSettingsDialog;
}

class CameraSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CameraSettingsDialog(QWidget *parent, QnVirtualCameraResourcePtr camera);
    ~CameraSettingsDialog();

public slots:
    virtual void accept();
    virtual void reject();

    void requestFinished(int status, const QByteArray& errorString, QnResourceList resources, int handle);

private:
    void saveCamera();

private:
    Q_DISABLE_COPY(CameraSettingsDialog)

    QnVirtualCameraResourcePtr m_camera;
    QScopedPointer<Ui::CameraSettingsDialog> ui;

    QnAppServerConnectionPtr connection;
};

#endif // CAMERA_SETTINGS_DIALOG_H
