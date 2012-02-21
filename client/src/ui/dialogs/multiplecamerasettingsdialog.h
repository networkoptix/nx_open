#ifndef MULTIPLE_CAMERA_SETTINGS_DIALOG_H
#define MULTIPLE_CAMERA_SETTINGS_DIALOG_H

#include <QtGui/QDialog>

#include "api/AppServerConnection.h"

namespace Ui {
    class MultipleCameraSettingsDialog;
}

class CameraScheduleWidget;

class MultipleCameraSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MultipleCameraSettingsDialog(QWidget *parent, QnVirtualCameraResourceList cameras);
    ~MultipleCameraSettingsDialog();

public slots:
    virtual void accept();
    virtual void reject();
    virtual void buttonClicked(QAbstractButton *button);

    void requestFinished(int status, const QByteArray& errorString, QnResourceList resources, int handle);

private:
    void updateView();
    void saveToModel();

    void save();

private:
    Q_DISABLE_COPY(MultipleCameraSettingsDialog)

    QnVirtualCameraResourceList m_cameras;
    QScopedPointer<Ui::MultipleCameraSettingsDialog> ui;

    QString m_login;
    QString m_password;

    QnAppServerConnectionPtr m_connection;
};

#endif // MULTIPLE_CAMERA_SETTINGS_DIALOG_H
