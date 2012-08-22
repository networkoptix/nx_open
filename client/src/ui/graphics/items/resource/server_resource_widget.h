#ifndef QN_SERVER_RESOURCE_WIDGET_H
#define QN_SERVER_RESOURCE_WIDGET_H

#include <QtCore/QSharedPointer>
#include <QtCore/QElapsedTimer>
#include <QtCore/QMetaType>

#include <api/video_server_statistics_data.h>

#include "resource_widget.h"

class QnRadialGradientPainter;

class QnServerResourceWidget: public QnResourceWidget {
    Q_OBJECT

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
    virtual Qn::RenderStatus paintChannelBackground(QPainter *painter, int channel, const QRectF &rect) override;
    virtual Buttons calculateButtonsVisibility() const override;

private slots:
    void at_timer_timeout();

private:
    /** Main painting function. */
    void drawStatistics(const QRectF &rect, QPainter *painter);

    void updateValues(QString key, QnStatisticsData *newValues, qint64 newId);

private:

    /** Video server resource. */
    QnVideoServerResourcePtr m_resource;

    /** History of last usage responses. */
    QnStatisticsHistory m_history;

    /** Id of the last received response. */
    qint64 m_lastHistoryId;

    /** Timestamp of the last received response in msecs since epoch */
    qint64 m_lastTimeStamp;

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
