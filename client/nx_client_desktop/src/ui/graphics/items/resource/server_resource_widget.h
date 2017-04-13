#ifndef QN_SERVER_RESOURCE_WIDGET_H
#define QN_SERVER_RESOURCE_WIDGET_H

#include <QtCore/QSharedPointer>
#include <QtCore/QElapsedTimer>
#include <QtCore/QMetaType>

#include <api/model/statistics_reply.h>

#include <client/client_color_types.h>

#include <ui/animation/animated.h>
#include <ui/animation/animation_timer_listener.h>

#include "resource_widget.h"

class QnMediaServerStatisticsManager;
class StatisticsOverlayWidget;
class QnGlFunctions;


class QnServerResourceWidget: public QnResourceWidget, public AnimationTimerListener {
    Q_OBJECT
    Q_PROPERTY(QnStatisticsColors colors READ colors WRITE setColors)
    typedef QnResourceWidget base_type;

public:
    typedef QHash<QString, bool> HealthMonitoringButtons;

#define ShowLogButton ShowLogButton
#define CheckIssuesButton CheckIssuesButton

    QnServerResourceWidget(QnWorkbenchContext *context, QnWorkbenchItem *item, QGraphicsItem *parent = NULL);
    virtual ~QnServerResourceWidget();

    /**
     * \returns                         Resource associated with this widget.
     */
    QnMediaServerResourcePtr resource() const;

    const QnStatisticsColors &colors() const;
    void setColors(const QnStatisticsColors &colors);

    HealthMonitoringButtons checkedHealthMonitoringButtons() const;
    void setCheckedHealthMonitoringButtons(const HealthMonitoringButtons &buttons);

protected:
    virtual int helpTopicAt(const QPointF &pos) const override;

    virtual Qn::RenderStatus paintChannelBackground(QPainter* painter, int channel,
        const QRectF& channelRect, const QRectF& paintRect) override;

    virtual QString calculateTitleText() const override;
    virtual int calculateButtonsVisibility() const override;
    virtual Qn::ResourceStatusOverlay calculateStatusOverlay() const override;

    virtual void tick(int deltaMSecs) override;

    virtual void at_itemDataChanged(int role) override;

    virtual void updateHud(bool animate) override;

private slots:
    void at_statistics_received();

    void updateHoverKey();
    void updateGraphVisibility();
    void updateColors();

private:
    enum LegendButtonBar {
        CommonButtonBar,
        NetworkButtonBar,
        ButtonBarCount
    };

    void addOverlays();

    LegendButtonBar buttonBarByDeviceType(const Qn::StatisticsDeviceType deviceType) const;

    void updateLegend();

    QColor getColor(Qn::StatisticsDeviceType deviceType, int index);

    void updateCheckedHealthMonitoringButtons();

    bool isLegendVisible() const;
private:
    //TODO: #GDM #Common move all required fields to inner class
    friend class StatisticsOverlayWidget;

    QnStatisticsColors m_colors;

    QnMediaServerStatisticsManager *m_manager;

    /** Video server resource. */
    QnMediaServerResourcePtr m_resource;

    /** History of last usage responses. */
    QnStatisticsHistory m_history;

    /** Sorted keys of history data. */
    QStringList m_sortedKeys;

    /** Id of the last received response. */
    qint64 m_lastHistoryId;

    // TODO: #GDM #Common uncomment when will implement time labels
    // /** Timestamp of the last received response in msecs since epoch */
    //qint64 m_lastTimeStamp;

    /** Id of the our widget in the statistics manager. */
    int m_statisticsId;

    /** Number of successful responses received, required to smooth scroll. */
    int m_counter;

    /** Number of data points displayed simultaneously. */
    int m_pointsLimit;

    /** Period of updating data from the server in milliseconds. */
    qreal m_updatePeriod;

    /** Status of the frame. */
    Qn::RenderStatus m_renderStatus;

    /** Elapsed timer for smooth scroll. */
    QElapsedTimer m_elapsedTimer;

    /** Button bars with corresponding buttons */
    QnImageButtonBar *m_legendButtonBar[ButtonBarCount];

    struct GraphData {
        GraphData(): bar(NULL), button(NULL), mask(0), visible(false), opacity(1.0) {}

        QnImageButtonBar *bar;
        QnImageButtonWidget *button;
        int mask;
        bool visible;
        qreal opacity;
        QColor color;
    };

    /** Which buttons are checked on each button bar */
    QHash<QString, GraphData> m_graphDataByKey;

    QString m_hoveredKey;

    int m_hddCount;
    int m_networkCount;
};

Q_DECLARE_METATYPE(QnServerResourceWidget *)

#endif // QN_SERVER_RESOURCE_WIDGET_H
