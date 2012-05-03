#ifndef QN_TIME_SLIDER_H
#define QN_TIME_SLIDER_H

#include "tool_tip_slider.h"

#include <recording/time_period.h>

#include <ui/processors/kinetic_process_handler.h>
#include <ui/animation/animation_timer_listener.h>
#include <ui/animation/animated.h>

#include "time_step.h"

class QnNoptixStyle;

class QnTimeSlider: public Animated<QnToolTipSlider>, protected KineticProcessHandler, protected AnimationTimerListener {
    Q_OBJECT;
    Q_PROPERTY(qint64 windowStart READ windowStart WRITE setWindowStart);
    Q_PROPERTY(qint64 windowEnd READ windowEnd WRITE setWindowEnd);

    typedef Animated<QnToolTipSlider> base_type;

public:
    enum Option {
        /** 
         * Whether window start should stick to slider's minimum value. 
         * If this flag is set and window starts at slider's minimum,
         * window start will change when minimum is changed. 
         */
        StickToMinimum = 0x1,

        /** 
         * Whether window end should stick to slider's maximum value. 
         */
        StickToMaximum = 0x2,

        /** 
         * Whether slider's tooltip is to be autoupdated using the provided
         * tool tip format. 
         */
        UpdateToolTip = 0x4,

        /**
         * Whether slider's value is considered to be a number of milliseconds that 
         * have passed since 1970-01-01 00:00:00.000, Coordinated Universal Time.
         * 
         * If this flag is not set, slider's value is simply a number of 
         * milliseconds, with no connection to real dates.
         */
        UseUTC = 0x8,

        /**
         * Whether the user can edit current selection with '[' and ']' buttons.
         */
        SelectionEditable = 0x10,
    };
    Q_DECLARE_FLAGS(Options, Option);

    explicit QnTimeSlider(QGraphicsItem *parent = NULL);
    virtual ~QnTimeSlider();

    int lineCount() const;
    void setLineCount(int lineCount);

    void setLineComment(int line, const QString &comment);
    QString lineComment(int line);

    QnTimePeriodList timePeriods(int line, Qn::TimePeriodType type) const;
    void setTimePeriods(int line, Qn::TimePeriodType type, const QnTimePeriodList &timePeriods);

    Options options() const;
    void setOptions(Options options);
    void setOption(Option option, bool value);

    qint64 windowStart() const;
    void setWindowStart(qint64 windowStart);

    qint64 windowEnd() const;
    void setWindowEnd(qint64 windowEnd);

    void setWindow(qint64 start, qint64 end);

    qint64 selectionStart() const;
    void setSelectionStart(qint64 selectionStart);

    qint64 selectionEnd() const;
    void setSelectionEnd(qint64 selectionEnd);

    void setSelection(qint64 start, qint64 end);

    bool isSelectionValid() const;
    void setSelectionValid(bool valid);

    const QString &toolTipFormat() const;
    void setToolTipFormat(const QString &format);

    void finishAnimations();

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    virtual QPointF positionFromValue(qint64 logicalValue) const override;
    virtual qint64 valueFromPosition(const QPointF &position) const override;

signals:
    void windowChanged(qint64 windowStart, qint64 windowEnd);
    void selectionChanged(qint64 selectionStart, qint64 selectionEnd);

protected:
    virtual void sliderChange(SliderChange change) override;
    virtual void wheelEvent(QGraphicsSceneWheelEvent *event) override;
    virtual void resizeEvent(QGraphicsSceneResizeEvent *event) override;
    virtual void keyPressEvent(QKeyEvent *event) override;

    virtual void tick(int deltaMSecs) override;

    virtual void kineticMove(const QVariant &degrees) override;

    static QVector<QnTimeStep> createRelativeSteps();
    static QVector<QnTimeStep> createAbsoluteSteps();
    static QVector<QnTimeStep> createStandardSteps(bool isRelative);
    static QVector<QnTimeStep> enumerateSteps(const QVector<QnTimeStep> &steps);

private:
    bool scaleWindow(qreal factor, qint64 anchor);

    void drawPeriodsBar(QPainter *painter, QnTimePeriodList &recorded, QnTimePeriodList &motion, qreal top, qreal height);
    void drawPeriods(QPainter *painter, QnTimePeriodList &periods, qreal top, qreal height, const QColor &preColor, const QColor &pastColor);
    void drawTickmarks(QPainter *painter, qreal top, qreal height);
    void drawSolidBackground(QPainter *painter, qreal top, qreal height);
    void drawMarker(QPainter *painter, qint64 pos, const QColor &color);
    void drawSelection(QPainter *painter);
    void drawHighlights(QPainter *painter, qreal fillTop, qreal fillHeight, qreal textTop, qreal textHeight);
    void drawThumbnails(QPainter *painter, qreal top, qreal height);

    void updateToolTipVisibility();
    void updateToolTipText();
    void updateSteps();
    void updateMSecsPerPixel();
    void updateMinimalWindow();
    void updateStepAnimationTargets();
    void updateLineCommentPixmap(int line);
    void updateLineCommentPixmaps();
    void updateAggregationValue();
    void updateAggregatedPeriods(int line, Qn::TimePeriodType type);
    
    void animateStepValues(int deltaMSecs);

    const QPixmap &positionPixmap(qint64 position, int height, const QnTimeStep &step);
    const QPixmap &highlightPixmap(qint64 position, int height, const QnTimeStep &step);
    const QPixmap &textPixmap(const QString &text, int height, QPalette::ColorRole colorRole = QPalette::Text);

    qreal lineTop(int line);
    qreal lineHeight();

private:
    Q_DECLARE_PRIVATE(GraphicsSlider);

    struct TypedPeriods {
        QnTimePeriodList normal[Qn::TimePeriodTypeCount];
        QnTimePeriodList aggregated[Qn::TimePeriodTypeCount];
    };

    struct TimeStepData {
        TimeStepData(): currentHeight(0.0), targetHeight(0.0), currentLineOpacity(0.0), targetLineOpacity(0.0), currentTextOpacity(0.0), targetTextOpacity(0.0) {}

        qreal currentHeight;
        qreal targetHeight;
        int currentTextHeight;
        qreal currentLineOpacity;
        qreal targetLineOpacity;
        qreal currentTextOpacity;
        qreal targetTextOpacity;
    };

    qint64 m_windowStart, m_windowEnd;
    qint64 m_minimalWindow;

    qint64 m_selectionStart, m_selectionEnd;
    bool m_selectionValid;

    qint64 m_oldMinimum, m_oldMaximum;
    Options m_options;
    QString m_toolTipFormat;

    qint64 m_zoomAnchor;

    int m_lineCount;
    QVector<TypedPeriods> m_lineTimePeriods;
    QVector<QString> m_lineComments;
    QVector<QPixmap> m_lineCommentPixmaps;
    qreal m_aggregationMSecs;

    QVector<QnTimeStep> m_steps;
    QVector<TimeStepData> m_stepData;
    qreal m_msecsPerPixel;
    qreal m_animationUpdateMSecsPerPixel;
    QVector<qint64> m_nextTickmarkPos;
    QVector<QVector<QPointF> > m_tickmarkLines;
    QHash<qint32, QPixmap> m_pixmapByPositionKey;
    QHash<qint32, QPixmap> m_pixmapByHighlightKey;
    QHash<QPair<QString, int>, QPixmap> m_pixmapByTextKey;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnTimeSlider::Options);

#endif // QN_TIME_SLIDER_H
