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
    void at_statistics_received();

private:
    /** Main painting function. */
    void drawStatistics(const QRectF &rect, QPainter *painter);

    /**
     * Util function that add new values to the statistics storage and mantain its number.
     *
     * \param key                       Id of the corresponding history item.
     * \param newValues                 Updated values.
     * \param newId                     Id of the last provided value.
     */
    void updateValues(QString key, QnStatisticsData *newValues, qint64 newId);

private:

    /** Video server resource. */
    QnVideoServerResourcePtr m_resource;

    /** History of last usage responses. */
    QnStatisticsHistory m_history;

    /** Id of the last received response. */
    qint64 m_lastHistoryId;

    // TODO: #GDM uncomment when will implement time labels
    // /** Timestamp of the last received response in msecs since epoch */
    //qint64 m_lastTimeStamp;

    /** Id of the our widget in the statistics manager. */
    int m_statisticsId;

    /** Number of successfull responces received, required to smooth scroll. */
    int m_counter;

    /** Status of the frame. */
    Qn::RenderStatus m_renderStatus;

    /** Elapsed timer for smooth scroll. */
    QElapsedTimer m_elapsedTimer;

    /** Helper for the background painting. */
    QSharedPointer<QnRadialGradientPainter> m_backgroundGradientPainter;
};

Q_DECLARE_METATYPE(QnServerResourceWidget *)

#endif // QN_SERVER_RESOURCE_WIDGET_H
