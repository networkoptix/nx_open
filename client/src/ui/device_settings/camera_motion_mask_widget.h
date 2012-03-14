#ifndef QN_CAMERA_MOTION_MASK_WIDGET_H
#define QN_CAMERA_MOTION_MASK_WIDGET_H

#include <QGraphicsScene>
#include <QGraphicsView>
#include "core/resource/resource.h"
#include "core/resource/camera_resource.h"

class MotionSelectionInstrument;

class QnGraphicsView;
class QnWorkbench;
class QnWorkbenchDisplay;
class QnWorkbenchController;
class QnWorkbenchItem;
class QnWorkbenchContext;
class QnResourceWidget;

class QnCameraMotionMaskWidget: public QWidget
{
    Q_OBJECT;
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly);
        
public:
    QnCameraMotionMaskWidget(QWidget *parent = 0);
    virtual ~QnCameraMotionMaskWidget();

    const QnResourcePtr &camera() const;
	void setCamera(const QnResourcePtr &resource);

    const QList<QRegion> &motionMaskList() const;

    bool isReadOnly() const;
    void setReadOnly(bool readOnly);

signals:
    void motionMaskListChanged();

protected slots:
    void at_viewport_resized();
    void at_motionRegionSelected(QGraphicsView *, QnResourceWidget *, const QRect &);
    void at_motionRegionCleared(QGraphicsView *, QnResourceWidget *);

private:
    void init();

private:
    QnVirtualCameraResourcePtr m_camera;
    QList<QRegion> m_motionMaskList;

    /* Destruction order is important here, hence the scoped pointers. */

    QScopedPointer<QnWorkbenchContext> m_context;

    QScopedPointer<QGraphicsScene> m_scene;
    QScopedPointer<QnGraphicsView> m_view;

    QScopedPointer<QnWorkbenchDisplay> m_display;
    QScopedPointer<QnWorkbenchController> m_controller;

    MotionSelectionInstrument *m_motionSelectionInstrument;

    bool m_readOnly;
};

#endif // QN_CAMERA_MOTION_MASK_WIDGET_H
