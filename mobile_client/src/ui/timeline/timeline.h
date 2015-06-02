#ifndef QNTIMELINE_H
#define QNTIMELINE_H

#include <QtCore/QDateTime>
#include <QtQuick/QQuickItem>

#include <recording/time_period_list.h>

class QnTimelinePrivate;
class QSGGeometryNode;

class QnTimeline : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(QDateTime windowStartDate READ windowStartDate WRITE setWindowStartDate NOTIFY windowStartDateChanged)
    Q_PROPERTY(QDateTime windowEndDate READ windowEndDate WRITE setWindowEndDate NOTIFY windowEndDateChanged)
    Q_PROPERTY(QDateTime positionDate READ positionDate WRITE setPositionDate NOTIFY positionDateChanged)

    Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor NOTIFY textColorChanged)
    Q_PROPERTY(QColor chunkColor READ chunkColor WRITE setChunkColor NOTIFY chunkColorChanged)

public:
    QnTimeline(QQuickItem *parent = 0);
    ~QnTimeline();

    virtual QSGNode *updatePaintNode(QSGNode *node, UpdatePaintNodeData *updatePaintNodeData) override;

    QColor textColor() const;
    void setTextColor(const QColor &color);

    QColor chunkColor() const;
    void setChunkColor(const QColor &color);

    qint64 windowStart() const;
    void setWindowStart(qint64 windowStart);

    QDateTime windowStartDate() const;
    void setWindowStartDate(const QDateTime &dateTime);

    qint64 windowEnd() const;
    void setWindowEnd(qint64 windowEnd);

    QDateTime windowEndDate() const;
    void setWindowEndDate(const QDateTime &dateTime);

    void setWindow(qint64 windowStart, qint64 windowEnd);

    qint64 position() const;
    void setPosition(qint64 position);

    QDateTime positionDate() const;
    void setPositionDate(const QDateTime &dateTime);

    QnTimePeriodList timePeriods(Qn::TimePeriodContent type) const;
    void setTimePeriods(Qn::TimePeriodContent type, const QnTimePeriodList &timePeriods);

    Q_INVOKABLE void zoomIn(int x = -1);
    Q_INVOKABLE void zoomOut(int x = -1);

    Q_INVOKABLE void startPinch(int x, qreal scale);
    Q_INVOKABLE void updatePinch(int x, qreal scale);
    Q_INVOKABLE void finishPinch(int x, qreal scale);

    Q_INVOKABLE void startDrag(int x);
    Q_INVOKABLE void updateDrag(int x);
    Q_INVOKABLE void finishDrag(int x);

signals:
    void zoomLevelChanged();
    void lowerTextOpacityChanged();
    void windowStartChanged();
    void windowStartDateChanged();
    void windowEndChanged();
    void windowEndDateChanged();
    void positionChanged();
    void positionDateChanged();

    void textColorChanged();
    void chunkColorChanged();

protected:
    void wheelEvent(QWheelEvent *event) override;

private:
    QSGGeometryNode *updateTicksNode(QSGGeometryNode *ticksNode);
    QSGGeometryNode *updateChunksNode(QSGGeometryNode *chunksNode);

private:
    QScopedPointer<QnTimelinePrivate> d;
};

#endif // QNTIMELINE_H
