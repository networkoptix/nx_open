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
    void at_statistics_received(int cpuUsage, const QByteArray &model, const QnHddUsageVector &hddUsage);
    void at_timer_update();

protected:
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

protected slots:
    virtual void updateOverlayText() override;

private:
    QPainterPath createGraph(QList<int> *values, const qreal x_step, int &prev_value, int &last_value);

    void drawStatistics(const QRectF &rect, QPainter *painter);

    /** History of last cpu usage responses */
    QList<int> m_cpuUsageHistory;

    /** History of last hdd usage responses */
    QList< QList <int>  > m_hddUsageHistory;

    /** Cpu model */
    QString m_model;

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

    /** Gradient for background drawing */
    QRadialGradient m_background_gradient;

    QnRadialGradientPainter m_backgroundGradientPainter;
};

Q_DECLARE_METATYPE(QnResourceServerWidget *)

#endif // QN_RESOURCE_SERVER_WIDGET_H
