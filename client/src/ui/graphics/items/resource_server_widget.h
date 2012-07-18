#ifndef QN_RESOURCE_SERVER_WIDGET_H
#define QN_RESOURCE_SERVER_WIDGET_H

#include <QtGui/QGraphicsWidget>

#include <ui/graphics/items/resource_widget.h>
#include <ui/graphics/painters/radial_gradient_painter.h>

#include <api/video_server_connection.h>

/* Get rid of stupid win32 defines. */
#ifdef NO_DATA
#   undef NO_DATA
#endif

namespace Qn {

    // TODO: remove this?
    enum RedrawStatus {
        NewFrame,     /**< New frame needs to be rendered. */
        OldFrame,     /**< No new frames available, old frame needs to be rendered. */
        LoadingFrame, /**< No frames to render, so nothing needs to be rendered. */
        CannotLoad    /**< Something went wrong. */
    };
} // namespace Qn

typedef QPair<QString, QList <int>> QnStatisticsHistoryData;

// TODO: rename to QnServerResourceWidget (we have QnServerResource => QnServerResource + Widget = QnServerResourceWidget)
class QnResourceServerWidget: public QnResourceWidget {
    Q_OBJECT;
public:
    /**
     * Constructor.
     *
     * \param item                      Workbench item that this resource widget will represent.
     * \param parent                    Parent item.
     */
    QnResourceServerWidget(QnWorkbenchContext *context, QnWorkbenchItem *item, QGraphicsItem *parent = NULL);

    /**
     * Virtual destructor.
     */
    virtual ~QnResourceServerWidget() {}

public slots:
    void at_statistics_received(const QnStatisticsDataVector &data);
    void at_timer_update();

protected:
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

protected slots:
    virtual void updateOverlayText() override;

private:
    QPainterPath createGraph(QList<int> *values, const qreal x_step, const qreal scale, int &prev_value, int &last_value);

    void drawStatistics(const QRectF &rect, QPainter *painter);

    /** History of last usage responses */
    QList< QnStatisticsHistoryData  > m_history;

    /** Total number of responses received */ 
    uint m_counter;

    /** Timer for server requests scheduling */
    QTimer* m_timer;

    /** Status of the frame history */
    Qn::RedrawStatus m_redrawStatus;

    /** Status of the update request */
    bool m_alreadyUpdating;

    /** Elapsed timer for smooth scroll */
    QElapsedTimer m_elapsed_timer;

    QnRadialGradientPainter m_backgroundGradientPainter;
};

Q_DECLARE_METATYPE(QnResourceServerWidget *)

#endif // QN_RESOURCE_SERVER_WIDGET_H
