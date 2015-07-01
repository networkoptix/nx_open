#ifndef QN_CALENDAR_WIDGET_H
#define QN_CALENDAR_WIDGET_H


#include "plugins/resource/server_camera/server_camera.h"

class QnIOStateDisplayWidget: public QWidget 
{
    Q_OBJECT
    typedef QWidget base_type;

public: 
    QnIOStateDisplayWidget(QWidget *parent = NULL);
    void setCamera(const QnServerCameraPtr& camera);
signals:
private slots:
    void at_connectionOpened();
    void at_connectionClosed();
    void at_cameraPropertyChanged(const QnResourcePtr & /*res*/, const QString & key);
    void at_ioStateChanged(const QnIOStateData& value);
    void openConnection();
private:
    void updateControls();
private:
    QnServerCameraPtr m_camera;
    QnIOModuleMonitorPtr m_monitor;
};


#endif // QN_CALENDAR_WIDGET_H
