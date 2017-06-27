#pragma once

#include <QtCore/QObject>

#include <client_core/connection_context_aware.h>

#include <recording/time_period_list.h>

class QnFlatCameraDataLoader;

class QnCameraChunkProvider: public QObject, public QnConnectionContextAware
{
    Q_OBJECT
    Q_PROPERTY(QString resourceId READ resourceId WRITE setResourceId NOTIFY resourceIdChanged)
    Q_PROPERTY(QDateTime bottomBoundDate READ bottomBoundDate NOTIFY bottomBoundDateChanged)
    Q_PROPERTY(qint64 bottomBound READ bottomBound NOTIFY bottomBoundChanged)
    Q_PROPERTY(bool loading READ isLoading NOTIFY loadingChanged)

public:
    QnCameraChunkProvider(QObject* parent = nullptr);

    QnTimePeriodList timePeriods() const;

    QString resourceId() const;
    void setResourceId(const QString& id);

    qint64 bottomBound() const;
    QDateTime bottomBoundDate() const;

    bool isLoading() const;

    Q_INVOKABLE QDateTime closestChunkStartDate(const QDateTime& dateTime, bool forward) const;
    Q_INVOKABLE qint64 closestChunkStartMs(qint64 position, bool forward) const;
    Q_INVOKABLE QDateTime closestChunkEndDate(const QDateTime& dateTime, bool forward) const;
    Q_INVOKABLE qint64 closestChunkEndMs(qint64 position, bool forward) const;
    Q_INVOKABLE QDateTime nextChunkStartDate(const QDateTime& dateTime, bool forward) const;
    Q_INVOKABLE qint64 nextChunkStartMs(qint64 position, bool forward) const;

    Q_INVOKABLE void update();

signals:
    void timePeriodsUpdated();
    void resourceIdChanged();
    void bottomBoundChanged();
    void bottomBoundDateChanged();
    void loadingChanged();

private:
    QnFlatCameraDataLoader* m_loader = nullptr;
    QnTimePeriodList m_periodList;
    bool m_loading = false;
};
