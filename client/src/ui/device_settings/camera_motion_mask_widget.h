#ifndef QN_CAMERA_MOTION_MASK_WIDGET_H
#define QN_CAMERA_MOTION_MASK_WIDGET_H

#include <QGraphicsScene>
#include <QGraphicsView>
#include "core/resource/resource.h"
#include "core/resource/camera_resource.h"
#include "ui/graphics/instruments/click_instrument.h"

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

    const QList<QnMotionRegion> &motionRegionList() const;

    bool isReadOnly() const;
    void setReadOnly(bool readOnly);

    void setMotionSensitivity(int value);

signals:
    void motionRegionListChanged();

public slots:
    void clearMotion();

protected slots:
    void at_viewport_resized();
    void at_motionRegionSelected(QGraphicsView *, QnResourceWidget *, const QRect &);
    void at_motionRegionCleared();
    void at_itemClicked(QGraphicsView*, QGraphicsItem*, const ClickInfo&);

private:
    void init();
    int gridPosToChannelPos(QPoint& pos);

private:
    QnVirtualCameraResourcePtr m_camera;
    //QList<QnMotionRegion> m_motionRegionList;

    /* Destruction order is important here, hence the scoped pointers. */

    QScopedPointer<QnWorkbenchContext> m_context;

    QScopedPointer<QGraphicsScene> m_scene;
    QScopedPointer<QnGraphicsView> m_view;

    QScopedPointer<QnWorkbenchDisplay> m_display;
    QScopedPointer<QnWorkbenchController> m_controller;

    MotionSelectionInstrument *m_motionSelectionInstrument;
    ClickInstrument* m_clickInstrument;
    QnResourceWidget* m_resourceWidget;

    bool m_readOnly;
    int m_motionSensitivity;
};

#endif // QN_CAMERA_MOTION_MASK_WIDGET_H
