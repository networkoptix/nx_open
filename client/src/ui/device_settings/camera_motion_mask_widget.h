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

    QnResourceWidget *widget() {
        return m_widget.data();
    }

private:
    QnVirtualCameraResourcePtr m_camera;
    QList<QRegion> m_motionMaskList;

    QGraphicsScene *m_scene;
    QnGraphicsView *m_view;

    QnWorkbenchContext *m_context;
    QnWorkbenchDisplay *m_display;
    QnWorkbenchController *m_controller;

    MotionSelectionInstrument *m_motionSelectionInstrument;

    bool m_readOnly;

    QWeakPointer<QnResourceWidget> m_widget;
};

#endif // QN_CAMERA_MOTION_MASK_WIDGET_H
