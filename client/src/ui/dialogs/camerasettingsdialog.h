#ifndef CAMERA_SETTINGS_DIALOG_H
#define CAMERA_SETTINGS_DIALOG_H

#include <QtGui/QDialog>

#include "api/AppServerConnection.h"

namespace Ui {
    class CameraSettingsDialog;
}

class CameraScheduleWidget;

class CameraSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CameraSettingsDialog(QnVirtualCameraResourcePtr camera, QWidget *parent = NULL);
    virtual ~CameraSettingsDialog();

public slots:
    virtual void accept() override;
    virtual void reject() override;

    void requestFinished(int status, const QByteArray& errorString, QnResourceList resources, int handle);

private:
    void updateView();
    void saveToModel();
    void save();

private:
    Q_DISABLE_COPY(CameraSettingsDialog)

    QnVirtualCameraResourcePtr m_camera;
    QScopedPointer<Ui::CameraSettingsDialog> ui;

    QnAppServerConnectionPtr m_connection;
};

#endif // CAMERA_SETTINGS_DIALOG_H
