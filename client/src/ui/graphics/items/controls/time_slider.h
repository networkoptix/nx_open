#ifndef QN_TIME_SLIDER_H
#define QN_TIME_SLIDER_H

#include "tool_tip_slider.h"
#include <recording/time_period.h>
#include <ui/processors/kinetic_process_handler.h>

class QnNoptixStyle;

class QnTimeSlider: public QnToolTipSlider, protected KineticProcessHandler {
    Q_OBJECT;
    Q_PROPERTY(qint64 windowStart READ windowStart WRITE setWindowStart);
    Q_PROPERTY(qint64 windowEnd READ windowEnd WRITE setWindowEnd);

    typedef QnToolTipSlider base_type;

public:
    enum Option {
        /** Whether window start should stick to slider's minimum value. 
         * If this flag is set and window starts at slider's minimum,
         * window start will change when minimum is changed. */
        StickToMinimum = 0x1,

        /** Whether window end should stick to slider's maximum value. */
        StickToMaximum = 0x2,

        /** Whether slider's tooltip is to be autoupdated using the provided
         * tool tip format. If this flag is set, slider's position is considered 
         * to be a time in milliseconds. */
        UpdateToolTip = 0x4,
    };
    Q_DECLARE_FLAGS(Options, Option);

    explicit QnTimeSlider(QGraphicsItem *parent = NULL);
    virtual ~QnTimeSlider();

    int lineCount() const;
    void setLineCount(int lineCount);

    QnTimePeriodList timePeriods(int line, Qn::TimePeriodType type) const;
    void setTimePeriods(int line, Qn::TimePeriodType type, const QnTimePeriodList &timePeriods);

    Options options() const;
    void setOptions(Options options);

    qint64 windowStart() const;
    void setWindowStart(qint64 windowStart);

    qint64 windowEnd() const;
    void setWindowEnd(qint64 windowEnd);

    void setWindow(qint64 start, qint64 end);

    const QString &toolTipFormat() const;
    void setToolTipFormat(const QString &format);

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    virtual QPointF positionFromValue(qint64 logicalValue) const override;
    virtual qint64 valueFromPosition(const QPointF &position) const override;

signals:
    void windowChanged(qint64 windowStart, qint64 windowEnd);

protected:
    virtual void sliderChange(SliderChange change) override;
    virtual QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    virtual void wheelEvent(QGraphicsSceneWheelEvent *event) override;

    virtual void kineticMove(const QVariant &degrees) override;

private:
    void scaleWindow(qreal factor, qint64 anchor);

    void drawPeriodsBar(QPainter *painter, QnTimePeriodList &recorded, QnTimePeriodList &motion, qreal top, qreal height);
    void drawPeriods(QPainter *painter, QnTimePeriodList &periods, qreal top, qreal height, const QColor &preColor, const QColor &pastColor);
    void drawScale(QPainter *painter, qreal top, qreal height);

    void updateToolTipVisibility();
    void updateToolTipText();

private:
    Q_DECLARE_PRIVATE(GraphicsSlider);

    struct TypedPeriods {
        QnTimePeriodList forType[Qn::TimePeriodTypeCount];
    };

    qint64 m_windowStart, m_windowEnd;
    qint64 m_oldMinimum, m_oldMaximum;
    Options m_options;
    QString m_toolTipFormat;
    bool m_dateLikeToolTipFormat;

    qint64 m_zoomAnchor;

    QVector<TypedPeriods> m_timePeriods;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnTimeSlider::Options);

#endif // QN_TIME_SLIDER_H
