#ifndef QN_SERVER_RESOURCE_WIDGET_H
#define QN_SERVER_RESOURCE_WIDGET_H

#include <QtCore/QSharedPointer>
#include <QtCore/QElapsedTimer>
#include <QtCore/QMetaType>

#include <api/video_server_connection.h>

#include "resource_widget.h"

class QnRadialGradientPainter;

typedef QPair<QString, QList <int>> QnStatisticsHistoryData;

class QnServerResourceWidget: public QnResourceWidget {
    Q_OBJECT;

    typedef QnResourceWidget base_type;

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
    virtual ~QnServerResourceWidget();

    /**
     * \returns                         Resource associated with this widget.
     */
    QnVideoServerResourcePtr resource() const;

protected:
    virtual Qn::RenderStatus paintChannel(QPainter *painter, int channel, const QRectF &rect) override;
    virtual Buttons calculateButtonsVisibility() const override;

private slots:
    void at_statisticsReceived(const QnStatisticsDataVector &data);
    void at_timer_timeout();

private:
    /** Main painting function. */
    void drawStatistics(const QRectF &rect, QPainter *painter);

private:
    /** Video server resource. */
    QnVideoServerResourcePtr m_resource;

    /** History of last usage responses. */
    QList<QnStatisticsHistoryData> m_history;

    /** Total number of responses received. */ 
    uint m_counter;

    /** Status of the frame history. */
    Qn::RenderStatus m_renderStatus;

    /** Status of the update request. */
    bool m_alreadyUpdating;

    /** Elapsed timer for smooth scroll. */
    QElapsedTimer m_elapsedTimer;

    /** Helper for the background painting. */
    QSharedPointer<QnRadialGradientPainter> m_backgroundGradientPainter;
};

Q_DECLARE_METATYPE(QnServerResourceWidget *)

#endif // QN_SERVER_RESOURCE_WIDGET_H
