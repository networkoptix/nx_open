#pragma once

#include <QtCore/QDateTime>
#include <QtQuick/QQuickItem>

#include <recording/time_period_list.h>

class QnTimelinePrivate;
class QSGGeometryNode;
class QnCameraChunkProvider;

class QnTimeline: public QQuickItem
{
    Q_OBJECT

    Q_PROPERTY(qint64 visualOffset READ visualOffset WRITE setVisualOffset
        NOTIFY visualOffsetChanged)

    Q_PROPERTY(qint64 defaultWindowSize READ defaultWindowSize CONSTANT)
    Q_PROPERTY(qint64 windowSize READ windowSize WRITE setWindowSize NOTIFY windowSizeChanged)
    Q_PROPERTY(qint64 windowStart READ windowStart WRITE setWindowStart NOTIFY windowStartChanged)
    Q_PROPERTY(qint64 windowEnd READ windowEnd WRITE setWindowEnd NOTIFY windowEndChanged)
    Q_PROPERTY(qint64 position READ position WRITE setPosition NOTIFY positionChanged)
    Q_PROPERTY(QDateTime positionDate
        READ positionDate WRITE setPositionDate NOTIFY positionDateChanged)
    Q_PROPERTY(qint64 startBound READ startBound WRITE setStartBound NOTIFY startBoundChanged)
    Q_PROPERTY(bool stickToEnd READ stickToEnd WRITE setStickToEnd NOTIFY stickToEndChanged)
    Q_PROPERTY(bool autoPlay READ autoPlay WRITE setAutoPlay NOTIFY autoPlayChanged)
    Q_PROPERTY(bool autoReturnToBounds
        READ isAutoReturnToBoundsEnabled WRITE setAutoReturnToBoundsEnabled
        NOTIFY autoReturnToBoundsEnabledChanged)
    Q_PROPERTY(int timeZoneShift
        READ timeZoneShift WRITE setTimeZoneShift NOTIFY timeZoneShiftChanged)

    Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor NOTIFY textColorChanged)
    Q_PROPERTY(QColor chunkBarColor
        READ chunkBarColor WRITE setChunkBarColor NOTIFY chunkBarColorChanged)
    Q_PROPERTY(QColor chunkColor READ chunkColor WRITE setChunkColor NOTIFY chunkColorChanged)
    Q_PROPERTY(QFont font READ font WRITE setFont NOTIFY fontChanged)

    Q_PROPERTY(int chunkBarHeight
        READ chunkBarHeight WRITE setChunkBarHeight NOTIFY chunkBarHeightChanged)
    Q_PROPERTY(int textY READ textY WRITE setTextY NOTIFY textYChanged)

    Q_PROPERTY(QnCameraChunkProvider* chunkProvider
        READ chunkProvider WRITE setChunkProvider NOTIFY chunkProviderChanged)

public:
    explicit QnTimeline(QQuickItem* parent = nullptr);
    ~QnTimeline();

    virtual QSGNode* updatePaintNode(
        QSGNode* node, UpdatePaintNodeData* updatePaintNodeData) override;

    QColor textColor() const;
    void setTextColor(const QColor& color);

    QFont font() const;
    void setFont(const QFont& font);

    QColor chunkColor() const;
    void setChunkColor(const QColor& color);

    QColor chunkBarColor() const;
    void setChunkBarColor(const QColor& color);

    int chunkBarHeight() const;
    void setChunkBarHeight(int chunkBarHeight);

    int textY() const;
    void setTextY(int textY);

    qint64 defaultWindowSize() const;

    qint64 visualOffset() const;
    void setVisualOffset(qint64 offset);

    qint64 windowStart() const;
    void setWindowStart(qint64 windowStart);

    qint64 windowEnd() const;
    void setWindowEnd(qint64 windowEnd);

    QDateTime windowEndDate() const;
    void setWindowEndDate(const QDateTime &dateTime);

    Q_INVOKABLE void setWindow(qint64 windowStart, qint64 windowEnd);
    qint64 windowSize() const;
    void setWindowSize(qint64 windowSize);

    qint64 position() const;
    void setPosition(qint64 position);
    Q_INVOKABLE void setPositionImmediately(qint64 position);

    QDateTime positionDate() const;
    void setPositionDate(const QDateTime &dateTime);

    QnTimePeriodList timePeriods(Qn::TimePeriodContent type) const;
    void setTimePeriods(Qn::TimePeriodContent type, const QnTimePeriodList& timePeriods);

    bool stickToEnd() const;
    void setStickToEnd(bool stickToEnd);

    qint64 startBound() const;
    void setStartBound(qint64 startBound);

    bool autoPlay() const;
    void setAutoPlay(bool autoPlay);

    bool isAutoReturnToBoundsEnabled() const;
    void setAutoReturnToBoundsEnabled(bool enabled);

    int timeZoneShift() const;
    void setTimeZoneShift(int timeZoneShift);

    Q_INVOKABLE void zoomIn();
    Q_INVOKABLE void zoomOut();

    Q_INVOKABLE void startZoom(qreal scale);
    Q_INVOKABLE void updateZoom(qreal scale);
    Q_INVOKABLE void finishZoom(qreal scale);

    Q_INVOKABLE void startDrag(int x);
    Q_INVOKABLE void updateDrag(int x);
    Q_INVOKABLE void finishDrag(int x);

    Q_INVOKABLE void clearCorrection();
    Q_INVOKABLE void correctPosition(qint64 position);

    Q_INVOKABLE qint64 positionAtX(qreal x) const;

    QnCameraChunkProvider* chunkProvider() const;
    void setChunkProvider(QnCameraChunkProvider* chunkProvider);

signals:
    void zoomLevelChanged();
    void lowerTextOpacityChanged();
    void windowSizeChanged();
    void windowStartChanged();
    void windowEndChanged();
    void positionChanged();
    void positionDateChanged();
    void stickToEndChanged();
    void startBoundChanged();
    void autoPlayChanged();
    void autoReturnToBoundsEnabledChanged();
    void visualOffsetChanged();

    void timeZoneShiftChanged();

    void textColorChanged();
    void chunkColorChanged();
    void chunkBarColorChanged();
    void fontChanged();

    void chunkBarHeightChanged();
    void textYChanged();

    void chunkProviderChanged();

    void moveFinished();

private:
    QSGNode* updateTextNode(QSGNode* textRootNode);
    QSGGeometryNode* updateChunksNode(QSGGeometryNode* chunksNode);

private:
    QScopedPointer<QnTimelinePrivate> d;
};
