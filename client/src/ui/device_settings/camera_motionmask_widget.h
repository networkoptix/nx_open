#ifndef __CAMERA_MOTIONMASK_WIDGET_H__
#define __CAMERA_MOTIONMASK_WIDGET_H__

#include <QGraphicsScene>
#include <QGraphicsView>
#include <core/resource/resource.h>

class QnGraphicsView;
class QnWorkbench;
class QnWorkbenchDisplay;
class QnWorkbenchController;

class QnCameraMotionMaskWidget: public QWidget
{
    Q_OBJECT;
        
public:
    QnCameraMotionMaskWidget(const QnResourcePtr &resource, QWidget *parent = 0);
    virtual ~QnCameraMotionMaskWidget();

protected slots:
    void at_viewport_resized();

private:
    QGraphicsScene *m_scene;
    QnGraphicsView *m_view;

    QnWorkbench *m_workbench;
    QnWorkbenchDisplay *m_display;
    QnWorkbenchController *m_controller;
};

#endif // __CAMERA_MOTIONMASK_WIDGET_H__
