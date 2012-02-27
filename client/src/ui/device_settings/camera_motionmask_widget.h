#ifndef __CAMERA_MOTIONMASK_WIDGET_H__
#define __CAMERA_MOTIONMASK_WIDGET_H__

#include <QGraphicsScene>
#include <QGraphicsView>
#include "core/resource/resource.h"
#include "core/resource/camera_resource.h"

class QnGraphicsView;
class QnWorkbench;
class QnWorkbenchDisplay;
class QnWorkbenchController;
class QnWorkbenchItem;
class QnResourceWidget;

class QnCameraMotionMaskWidget: public QWidget
{
    Q_OBJECT;
        
public:
    QnCameraMotionMaskWidget(const QnResourcePtr &resource, QWidget *parent = 0);
    QnCameraMotionMaskWidget(QWidget *parent = 0);
    virtual ~QnCameraMotionMaskWidget();

	void setCamera(const QnResourcePtr &resource);

    const QList<QRegion>& motionMaskList() const;

protected slots:
    void at_viewport_resized();

	void at_motionRegionSelected(QGraphicsView *, QnResourceWidget *, const QRect &);
	void at_motionRegionCleared(QGraphicsView *, QnResourceWidget *);

private:
	void init();

    QnResourceWidget *widget() {
        return m_widget.data();
    }

private:
	QnVirtualCameraResourcePtr m_camera;
	QList<QRegion> m_motionMaskList;

    QGraphicsScene *m_scene;
    QnGraphicsView *m_view;

    QnWorkbench *m_workbench;
    QnWorkbenchDisplay *m_display;
    QnWorkbenchController *m_controller;

	QWeakPointer<QnResourceWidget> m_widget;
};

#endif // __CAMERA_MOTIONMASK_WIDGET_H__
