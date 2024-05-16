// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_SERVER_RESOURCE_WIDGET_H
#define QN_SERVER_RESOURCE_WIDGET_H

#include <QtCore/QElapsedTimer>
#include <QtCore/QSharedPointer>

#include <api/model/statistics_reply.h>
#include <ui/animation/animated.h>
#include <ui/animation/animation_timer_listener.h>

#include "resource_widget.h"

class QnMediaServerStatisticsManager;
class StatisticsOverlayWidget;
class QnGlFunctions;

class QnServerResourceWidget: public QnResourceWidget
{
    Q_OBJECT
    typedef QnResourceWidget base_type;

public:
    typedef QHash<QString, bool> HealthMonitoringButtons;

    QnServerResourceWidget(
        nx::vms::client::desktop::SystemContext* systemContext,
        nx::vms::client::desktop::WindowContext* windowContext,
        QnWorkbenchItem* item,
        QGraphicsItem* parent = nullptr);
    virtual ~QnServerResourceWidget();

    /**
     * \returns                         Resource associated with this widget.
     */
    QnMediaServerResourcePtr resource() const;

    HealthMonitoringButtons checkedHealthMonitoringButtons() const;
    void setCheckedHealthMonitoringButtons(const HealthMonitoringButtons &buttons);

protected:
    virtual int helpTopicAt(const QPointF &pos) const override;

    virtual Qn::RenderStatus paintChannelBackground(QPainter* painter, int channel,
        const QRectF& channelRect, const QRectF& paintRect) override;

    virtual QString calculateTitleText() const override;
    virtual int calculateButtonsVisibility() const override;
    virtual Qn::ResourceStatusOverlay calculateStatusOverlay() const override;

    virtual void atItemDataChanged(int role) override;

    virtual void updateHud(bool animate) override;

private slots:
    void at_statistics_received();

    void updateHoverKey();
    void updateGraphVisibility();

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

    HealthMonitoringButtons savedCheckedHealthMonitoringButtons() const;
    void updateCheckedHealthMonitoringButtons();

    bool isLegendVisible() const;
    void createButtons();

    void tick(int deltaMSecs);

private:
    // TODO: #sivanov Move all required fields to inner class.
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

    // TODO: #sivanov Uncomment when will implement time labels.
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
        GraphData(): bar(nullptr), button(nullptr), mask(0), visible(false), opacity(1.0) {}

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

    AnimationTimerListenerPtr m_animationTimerListener = AnimationTimerListener::create();
};

#endif // QN_SERVER_RESOURCE_WIDGET_H
