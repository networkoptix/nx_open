#ifndef QN_WORKBENCH_CONTROLLER_H
#define QN_WORKBENCH_CONTROLLER_H

#include <QObject>
#include <QScopedPointer>
#include <utils/common/scene_utility.h>
#include <core/resource/resource.h>

class QGraphicsScene;
class QGraphicsView;
class QGraphicsItem;

class InstrumentManager;
class HandScrollInstrument;
class WheelZoomInstrument;
class DragInstrument;
class RubberBandInstrument;
class ResizingInstrument;
class ArchiveDropInstrument;
class UiElementsInstrument;

class NavigationItem;

class QnWorkbenchManager;
class QnWorkbenchLayout;
class QnWorkbench;
class QnResourceWidget;
class QnWorkbenchItem;
class QnWorkbenchGridMapper;


/**
 * This class implements default scene manipulation logic. 
 * 
 * It also presents some functions for high-level scene content manipulation.
 */
class QnWorkbenchController: public QObject, protected QnSceneUtility {
    Q_OBJECT;
public:
    QnWorkbenchController(QnWorkbenchManager *synchronizer, QObject *parent = NULL);

    virtual ~QnWorkbenchController();

    QnWorkbench *workbench() const;

    QnWorkbenchLayout *layout() const;

    QnWorkbenchGridMapper *mapper() const;

    void drop(const QUrl &url, const QPoint &gridPos, bool findAccepted = true);
    void drop(const QList<QUrl> &urls, const QPoint &gridPos, bool findAccepted = true);
    void drop(const QString &file, const QPoint &gridPos, bool findAccepted = true);
    void drop(const QList<QString> &files, const QPoint &gridPos, bool findAccepted = true);
    void drop(const QnResourcePtr &resource, const QPoint &gridPos);
    void drop(const QnResourceList &resources, const QPoint &gridPos);

protected:
    void updateGeometryDelta(QnResourceWidget *widget);

protected slots:
    void at_resizingStarted(QGraphicsView *view, QGraphicsWidget *widget);
    void at_resizingFinished(QGraphicsView *view, QGraphicsWidget *widget);

    void at_draggingStarted(QGraphicsView *view, QList<QGraphicsItem *> items);
    void at_draggingFinished(QGraphicsView *view, QList<QGraphicsItem *> items);

    void at_item_clicked(QGraphicsView *view, QGraphicsItem *item);
    void at_item_doubleClicked(QGraphicsView *view, QGraphicsItem *item);

    void at_scene_clicked(QGraphicsView *view);
    void at_scene_doubleClicked(QGraphicsView *view);

    void at_viewportGrabbed();
    void at_viewportUngrabbed();

    void at_workbench_focusedItemChanged();

private:
    /** Display synchronizer. */
    QnWorkbenchManager *m_synchronizer;

    /** Instrument manager for the scene. */ 
    InstrumentManager *m_manager;

    /** Hand scroll instrument. */
    HandScrollInstrument *m_handScrollInstrument;

    /** Wheel zoom instrument. */
    WheelZoomInstrument *m_wheelZoomInstrument;

    /** Dragging instrument. */
    DragInstrument *m_dragInstrument;

    /** Rubber band instrument. */
    RubberBandInstrument *m_rubberBandInstrument;

    /** Resizing instrument. */
    ResizingInstrument *m_resizingInstrument;

    /** Archive drop instrument. */
    ArchiveDropInstrument *m_archiveDropInstrument;

    /** Ui elements instrument. */
    UiElementsInstrument *m_uiElementsInstrument;

    /** Focused item. */
    QnWorkbenchItem *m_focusedItem;

    /** Navigation item. */
    NavigationItem *m_navigationItem;
};

#endif // QN_WORKBENCH_CONTROLLER_H
