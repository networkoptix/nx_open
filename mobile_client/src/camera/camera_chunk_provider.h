#pragma once

#include <QtCore/QObject>

#include <utils/common/id.h>
#include <recording/time_period_list.h>
#include <core/resource/resource_fwd.h>

class QnFlatCameraDataLoader;

class QnCameraChunkProvider : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString resourceId READ resourceId WRITE setResourceId NOTIFY resourceIdChanged)
    Q_PROPERTY(QDateTime bottomBoundDate READ bottomBoundDate NOTIFY bottomBoundDateChanged)
    Q_PROPERTY(qint64 bottomBound READ bottomBound NOTIFY bottomBoundChanged)

public:
    QnCameraChunkProvider(QObject *parent = 0);

    QnTimePeriodList timePeriods() const;

    QString resourceId() const;
    void setResourceId(const QString &id);

    qint64 bottomBound() const;
    QDateTime bottomBoundDate() const;

    Q_INVOKABLE QDateTime closestChunkStartDate(const QDateTime &dateTime, bool forward) const;
    Q_INVOKABLE qint64 closestChunkStartMs(qint64 position, bool forward) const;
    Q_INVOKABLE QDateTime closestChunkEndDate(const QDateTime &dateTime, bool forward) const;
    Q_INVOKABLE qint64 closestChunkEndMs(qint64 position, bool forward) const;
    Q_INVOKABLE QDateTime nextChunkStartDate(const QDateTime &dateTime, bool forward) const;
    Q_INVOKABLE qint64 nextChunkStartMs(qint64 position, bool forward) const;

public slots:
    void update();

signals:
    void timePeriodsUpdated();
    void resourceIdChanged();
    void bottomBoundChanged();
    void bottomBoundDateChanged();

private:
    QnFlatCameraDataLoader *m_loader;
    QnTimePeriodList m_periodList;
};
