#pragma once

#include <QtCore/QObject>

#include <client_core/connection_context_aware.h>

#include <common/common_globals.h>
#include <recording/time_period_list.h>

class QnFlatCameraDataLoader;

class QnCameraChunkProvider: public QObject, public QnConnectionContextAware
{
    Q_OBJECT
    using base_type = QObject;

    Q_PROPERTY(QString resourceId READ resourceId WRITE setResourceId NOTIFY resourceIdChanged)
    Q_PROPERTY(QDateTime bottomBoundDate READ bottomBoundDate NOTIFY bottomBoundDateChanged)
    Q_PROPERTY(qint64 bottomBound READ bottomBound NOTIFY bottomBoundChanged)
    Q_PROPERTY(bool loading READ isLoading NOTIFY loadingChanged)
    Q_PROPERTY(bool loadingMotion READ isLoadingMotion NOTIFY loadingMotionChanged)
    Q_PROPERTY(QString motionFilter READ motionFilter WRITE setMotionFilter NOTIFY motionFilterChanged)

public:
    QnCameraChunkProvider(QObject* parent = nullptr);

    QnTimePeriodList timePeriods(Qn::TimePeriodContent contentType) const;

    QString resourceId() const;
    void setResourceId(const QString& id);

    QDateTime bottomBoundDate() const;
    qint64 bottomBound() const;

    QString motionFilter() const;
    void setMotionFilter(const QString& value);

    bool isLoading() const;
    bool isLoadingMotion() const;

    Q_INVOKABLE qint64 closestChunkEndMs(qint64 position, bool forward) const;
    Q_INVOKABLE void update();
    Q_INVOKABLE bool hasChunks() const;
    Q_INVOKABLE bool hasMotionChunks() const;

signals:
    void timePeriodsUpdated();
    void resourceIdChanged();
    void bottomBoundChanged();
    void bottomBoundDateChanged();
    void loadingChanged();
    void loadingMotionChanged();
    void motionFilterChanged();

private:
    void handleLoadingChanged(Qn::TimePeriodContent contentType);

private:
    class ChunkProviderInternal;
    using ChunkProviderPtr = QSharedPointer<ChunkProviderInternal>;
    using ProvidersHash = QHash<Qn::TimePeriodContent, ChunkProviderPtr>;
    const ProvidersHash m_providers;
};
