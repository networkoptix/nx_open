#ifndef QN_CALENDAR_WIDGET_H
#define QN_CALENDAR_WIDGET_H


#include "plugins/resource/server_camera/server_camera.h"

namespace Ui
{
    class IOStateDisplayWidget;
}

class QnIOStateDisplayWidget: public QDialog 
{
    Q_OBJECT
    typedef QDialog base_type;

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
    void at_buttonClicked();
    void at_buttonStateChanged(bool toggled);
    void at_cameraStatusChanged(const QnResourcePtr & res);
private:
    void updateControls();
private:
    QScopedPointer<Ui::IOStateDisplayWidget> ui;

    QnServerCameraPtr m_camera;
    QnIOModuleMonitorPtr m_monitor;
    
    QMap<QString, QWidget*> m_widgetsByPort;
    QnIOPortDataList m_ioSettings;
};


#endif // QN_CALENDAR_WIDGET_H
