#ifndef __CAMERA_MOTIONMASK_WIDGET_H__
#define __CAMERA_MOTIONMASK_WIDGET_H__

#include <QGLWidget>
#include <QGraphicsScene>
#include <QGraphicsView>
#include "utils/common/qnid.h"

class QnResourceWidget;

class QnCameraMotionMaskWidget: public QWidget
{
public:
    QnCameraMotionMaskWidget(const QString& unicId, QWidget* parent = 0);
    virtual ~QnCameraMotionMaskWidget();
protected:
    virtual void resizeEvent(QResizeEvent* event);
private:
    QGraphicsScene m_scene;
    QGraphicsView* m_view;
    QnResourceWidget* m_widget;
};

#endif // __CAMERA_MOTIONMASK_WIDGET_H__
