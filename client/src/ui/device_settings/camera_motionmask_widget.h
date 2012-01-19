#ifndef __CAMERA_MOTIONMASK_WIDGET_H__
#define __CAMERA_MOTIONMASK_WIDGET_H__

#include <QGraphicsScene>
#include <QGraphicsView>
#include <core/resource/resource.h>

class QnResourceWidget;

class QnCameraMotionMaskWidget: public QWidget
{
    Q_OBJECT

public:
    QnCameraMotionMaskWidget(const QnResourcePtr &resource, QWidget *parent = 0);
    virtual ~QnCameraMotionMaskWidget();

protected:
    void resizeEvent(QResizeEvent *event);

private:
    QGraphicsScene m_scene;
    QGraphicsView *m_view;
    QnResourceWidget *m_widget;
};

#endif // __CAMERA_MOTIONMASK_WIDGET_H__
