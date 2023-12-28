// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <QtCore/QHash>
#include <QtCore/QString>

#include <api/model/statistics_reply.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/desktop/system_context_aware.h>

class QnMediaServerStatisticsStorage;

/**
  * Class that receives, parses and stores statistics data from all servers.
  * Also handles request sending through an inner timer.
  */
class QnMediaServerStatisticsManager: public QObject,
    public nx::vms::client::desktop::SystemContextAware
{
    Q_OBJECT

public:
    /** Number of data points that are stored simultaneously. */
    static constexpr int kPointsLimit = 60;

    /**
     * Constructor
     *
     * \param parent            Parent of the object
     */
    QnMediaServerStatisticsManager(
        nx::vms::client::desktop::SystemContext* systemContext,
        QObject *parent = nullptr);

    /**
     *  Register the consumer object.
     *
     * \param resource          Server resource whose history we want to receive.
     * \param target            Object that will be notified about new data.
     * \param slot              Slot that will be called when new data will be received.
     */
    void registerConsumer(
        const QnMediaServerResourcePtr &resource, QObject *target, std::function<void()> callback);

    /**
     *  Unregister the consumer object.
     *
     * \param resource          Server resource whose history we do not want to receive anymore.
     * \param target            Object that will not be notified about new data anymore.
     */
    void unregisterConsumer(const QnMediaServerResourcePtr &resource, QObject *target);

    QnStatisticsHistory history(const QnMediaServerResourcePtr &resource) const;
    qint64 uptimeMs(const QnMediaServerResourcePtr &resource) const;
    qint64 historyId(const QnMediaServerResourcePtr &resource) const;

    /** Data update period in milliseconds. It is taken from the server's response. */
    int updatePeriod(const QnMediaServerResourcePtr &resource) const;


    /** Filter statistics items of some deviceType by flags (ignore all replies that do not contain flags provided). */
    void setFlagsFilter(Qn::StatisticsDeviceType deviceType, int flags);

private:
    QHash<QnUuid, QnMediaServerStatisticsStorage *> m_statistics;
    QHash<Qn::StatisticsDeviceType, int> m_flagsFilter;
};
