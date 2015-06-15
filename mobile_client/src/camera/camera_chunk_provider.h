#pragma once

#include <QtCore/QObject>

#include <utils/common/id.h>
#include <recording/time_period_list.h>
#include <core/resource/resource_fwd.h>

class QnFlatCameraDataLoader;

class QnCameraChunkProvider : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString resourceId READ resourceId WRITE setResourceId NOTIFY resourceIdChanged)
    Q_PROPERTY(QDateTime bottomBound READ bottomBound NOTIFY bottomBoundChanged)

public:
    QnCameraChunkProvider(QObject *parent = 0);

    QnTimePeriodList timePeriods() const;

    QString resourceId() const;
    void setResourceId(const QString &id);

    QDateTime bottomBound() const;

    Q_INVOKABLE QDateTime closestChunkStartDate(const QDateTime &dateTime, bool forward) const;
    Q_INVOKABLE qint64 closestChunkStartMs(const QDateTime &dateTime, bool forward) const;
    Q_INVOKABLE QDateTime closestChunkEndDate(const QDateTime &dateTime, bool forward) const;
    Q_INVOKABLE qint64 closestChunkEndMs(const QDateTime &dateTime, bool forward) const;
    Q_INVOKABLE QDateTime nextChunkStartDate(const QDateTime &dateTime, bool forward) const;
    Q_INVOKABLE qint64 nextChunkStartMs(const QDateTime &dateTime, bool forward) const;

public slots:
    void update();

signals:
    void timePeriodsUpdated();
    void resourceIdChanged();
    void bottomBoundChanged();

private:
    QnFlatCameraDataLoader *m_loader;
    QnTimePeriodList m_periodList;
};
