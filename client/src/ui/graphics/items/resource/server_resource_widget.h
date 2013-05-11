#ifndef QN_SERVER_RESOURCE_WIDGET_H
#define QN_SERVER_RESOURCE_WIDGET_H

#include <QtCore/QSharedPointer>
#include <QtCore/QElapsedTimer>
#include <QtCore/QMetaType>

#include <api/model/statistics_reply.h>

#include "resource_widget.h"

class QnRadialGradientPainter;
class QnMediaServerStatisticsManager;
class StatisticsOverlayWidget;

enum LegendButtonBar {
    CommonButtonBar,
    NetworkOutButtonBar,
    NetworkInButtonBar,

    ButtonBarCount
};

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
    QnMediaServerResourcePtr resource() const;

protected:
    virtual int helpTopicAt(const QPointF &pos) const override;

    virtual Qn::RenderStatus paintChannelBackground(QPainter *painter, int channel, const QRectF &channelRect, const QRectF &paintRect) override;
    virtual QString calculateTitleText() const override;
    virtual Buttons calculateButtonsVisibility() const override;
    virtual QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private slots:
    void at_statistics_received();
    void at_legend_checkedButtonsChanged();
    void at_headerOverlayWidget_opacityChanged(const QVariant &value);

private:
    /** Main painting function. */
    void drawStatistics(const QRectF &rect, QPainter *painter);

    void addOverlays();

    LegendButtonBar buttonBarByDeviceType(const QnStatisticsDeviceType deviceType) const;

    void updateLegend();

private:
    //TODO: #GDM move all required fields to inner class
    friend class StatisticsOverlayWidget;

    QnMediaServerStatisticsManager *m_manager;

    /** Video server resource. */
    QnMediaServerResourcePtr m_resource;

    /** History of last usage responses. */
    QnStatisticsHistory m_history;

    /** Sorted keys of history data. */
    QStringList m_sortedKeys;

    /** Id of the last received response. */
    qint64 m_lastHistoryId;

    // TODO: #GDM uncomment when will implement time labels
    // /** Timestamp of the last received response in msecs since epoch */
    //qint64 m_lastTimeStamp;

    /** Id of the our widget in the statistics manager. */
    int m_statisticsId;

    /** Number of successfull responces received, required to smooth scroll. */
    int m_counter;

    /** Number of data points displayed simultaneously. */
    int m_pointsLimit;

    /** Period of updating data from the server in milliseconds. */
    qreal m_updatePeriod;

    /** Status of the frame. */
    Qn::RenderStatus m_renderStatus;

    /** Elapsed timer for smooth scroll. */
    QElapsedTimer m_elapsedTimer;

    /** Helper for the background painting. */
    QSharedPointer<QnRadialGradientPainter> m_backgroundGradientPainter;

    /** Button bars with corresponding buttons */
    QnImageButtonBar* m_legendButtonBar[ButtonBarCount];

    /** Which buttons are checked on each button bar */
    QHash<QString, bool> m_checkedFlagByKey[ButtonBarCount];

    /** Masks to get corresponding button from button bar */
    QHash<QString, int> m_buttonMaskByKey[ButtonBarCount];

    /** Mask generate variables */
    int m_maxMaskUsed[ButtonBarCount];

    qreal m_infoOpacity;
};

Q_DECLARE_METATYPE(QnServerResourceWidget *)

#endif // QN_SERVER_RESOURCE_WIDGET_H
