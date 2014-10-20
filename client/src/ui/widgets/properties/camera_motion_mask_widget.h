#ifndef QN_CAMERA_MOTION_MASK_WIDGET_H
#define QN_CAMERA_MOTION_MASK_WIDGET_H

#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsView>

#include <core/resource/resource_fwd.h>
#include <core/resource/motion_window.h>

#include <utils/common/connective.h>

class MotionSelectionInstrument;
class ClickInstrument;
class ClickInfo;

class QnGraphicsView;
class QnGraphicsScene;
class QnWorkbench;
class QnWorkbenchDisplay;
class QnWorkbenchController;
class QnWorkbenchItem;
class QnWorkbenchContext;
class QnMediaResourceWidget;

class QnCameraMotionMaskWidget: public Connective<QWidget> {
    Q_OBJECT
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)
    typedef Connective<QWidget> base_type;
        
public:
    QnCameraMotionMaskWidget(QWidget *parent = 0);
    virtual ~QnCameraMotionMaskWidget();

    QnResourcePtr camera() const;
    void setCamera(const QnResourcePtr &resource);

    QList<QnMotionRegion> motionRegionList() const;

    bool isReadOnly() const;
    void setReadOnly(bool readOnly);

    int motionSensitivity() const;
    void setMotionSensitivity(int motionSensitivity);

    void setControlMaxRects(bool controlMaxRects);
    bool isControlMaxRects() const;

    /** Check if motion region is valid */
    bool isValidMotionRegion();

signals:
    void motionRegionListChanged();

public slots:
    void clearMotion();

protected slots:
    void at_viewport_resized();
    void at_motionRegionSelected(QGraphicsView *view, QnMediaResourceWidget *widget, const QRect &gridRect);
    void at_motionRegionCleared();
    void at_itemClicked(QGraphicsView *view, QGraphicsItem *item, const ClickInfo &info);

private:
    void init();
    void showTooManyWindowsMessage(const QnMotionRegion &region, const QnMotionRegion::ErrorCode errCode);

private:
    QnVirtualCameraResourcePtr m_camera;

    /* Destruction order is important here, hence the scoped pointers. */

    QScopedPointer<QnWorkbenchContext> m_context;

    QScopedPointer<QnGraphicsScene> m_scene;
    QScopedPointer<QnGraphicsView> m_view;

    QScopedPointer<QnWorkbenchController> m_controller;

    MotionSelectionInstrument *m_motionSelectionInstrument;
    ClickInstrument *m_clickInstrument;
    QnMediaResourceWidget *m_resourceWidget;

    bool m_readOnly;
    int m_motionSensitivity;
    bool m_controlMaxRects;
};

#endif // QN_CAMERA_MOTION_MASK_WIDGET_H
