#ifndef QN_SERVER_RESOURCE_WIDGET_H
#define QN_SERVER_RESOURCE_WIDGET_H

#include <QtGui/QGraphicsWidget>

#include <ui/graphics/items/resource_widget.h>
#include <ui/graphics/painters/radial_gradient_painter.h>

#include <api/video_server_connection.h>

typedef QPair<QString, QList <int>> QnStatisticsHistoryData;

class QnServerResourceWidget: public QnResourceWidget {
    Q_OBJECT;
public:
    /**
     * Constructor.
     *
     * \param item                      Workbench item that this resource widget will represent.
     * \param parent                    Parent item.
     */
    QnServerResourceWidget(QnWorkbenchContext *context, QnWorkbenchItem *item, QGraphicsItem *parent = NULL);

    /**
     * Virtual destructor.
     */
    virtual ~QnServerResourceWidget() {}

public slots:
    void at_statistics_received(const QnStatisticsDataVector &data);
    void at_timer_update();

protected:
    virtual Qn::RenderStatus paintChannel(QPainter *painter, int channel, const QRectF &rect) override;

private:
    /** Main painting function */
    void drawStatistics(const QRectF &rect, QPainter *painter);

    /** History of last usage responses */
    QList< QnStatisticsHistoryData  > m_history;

    /** Total number of responses received */ 
    uint m_counter;

    /** Timer for server requests scheduling */
    QTimer* m_timer;

    /** Status of the frame history */
    Qn::RenderStatus m_renderStatus;

    /** Status of the update request */
    bool m_alreadyUpdating;

    /** Elapsed timer for smooth scroll */
    QElapsedTimer m_elapsed_timer;

    /** Helper for the background painting */
    QnRadialGradientPainter m_backgroundGradientPainter;
};

Q_DECLARE_METATYPE(QnServerResourceWidget *)

#endif // QN_SERVER_RESOURCE_WIDGET_H
