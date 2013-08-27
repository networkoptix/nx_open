#ifndef QN_SERVER_RESOURCE_WIDGET_H
#define QN_SERVER_RESOURCE_WIDGET_H

#include <QtCore/QSharedPointer>
#include <QtCore/QElapsedTimer>
#include <QtCore/QMetaType>

#include <api/model/statistics_reply.h>

#include <ui/animation/animated.h>
#include <ui/animation/animation_timer_listener.h>

#include "resource_widget.h"

class QnRadialGradientPainter;
class QnMediaServerStatisticsManager;
class StatisticsOverlayWidget;
class QnGlFunctions;


class QnServerResourceWidget: public Animated<QnResourceWidget>, AnimationTimerListener {
    Q_OBJECT

    typedef Animated<QnResourceWidget> base_type;

public:
    static const Button PingButton = static_cast<Button>(0x08);
    static const Button ShowLogButton = static_cast<Button>(0x10);
    static const Button CheckIssuesButton = static_cast<Button>(0x20);
#define PingButton PingButton
#define ShowLogButton ShowLogButton
#define CheckIssuesButton CheckIssuesButton

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

    virtual void tick(int deltaMSecs) override;

private slots:
    void at_statistics_received();
    void at_pingButton_clicked();
    void at_showLogButton_clicked();
    void at_checkIssuesButton_clicked();
    
    void updateHoverKey();
    void updateGraphVisibility();
    void updateInfoOpacity();

private:
    enum LegendButtonBar {
        CommonButtonBar,
        NetworkButtonBar,
        ButtonBarCount
    };

    /** Background painting function. */
    void drawBackground(const QRectF &rect, QPainter *painter);

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
    QnImageButtonBar *m_legendButtonBar[ButtonBarCount];

    struct GraphData {
        GraphData(): bar(NULL), button(NULL), mask(0), visible(false), opacity(1.0) {}

        QnImageButtonBar *bar;
        QnImageButtonWidget *button;
        int mask;
        bool visible;
        qreal opacity;
    };

    /** Which buttons are checked on each button bar */
    QHash<QString, GraphData> m_graphDataByKey;

    QString m_hoveredKey;

    qreal m_infoOpacity;
};

Q_DECLARE_METATYPE(QnServerResourceWidget *)

#endif // QN_SERVER_RESOURCE_WIDGET_H
