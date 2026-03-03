// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QRectF>

#include <common/common_globals.h>
#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/media/abstract_time_period_storage.h>
#include <nx/vms/client/core/system_context_aware.h>
#include <recording/time_period_list.h>

Q_MOC_INCLUDE("core/resource/resource.h")

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API ChunkProvider:
    public AbstractTimePeriodStorage
{
    Q_OBJECT
    Q_ENUMS(Qn::TimePeriodContent)
    using base_type = AbstractTimePeriodStorage;

    Q_PROPERTY(QnResource* resource READ rawResource WRITE setRawResource NOTIFY resourceChanged)
    Q_PROPERTY(qint64 bottomBound READ bottomBound NOTIFY bottomBoundChanged)
    Q_PROPERTY(bool loading READ isLoading NOTIFY loadingChanged)
    Q_PROPERTY(bool loadingMotion READ isLoadingMotion NOTIFY loadingMotionChanged)
    Q_PROPERTY(bool loadingAnalytics READ isLoadingAnalytics NOTIFY loadingAnalyticsChanged)
    Q_PROPERTY(QString motionFilter READ motionFilter WRITE setMotionFilter
        NOTIFY motionFilterChanged)
    Q_PROPERTY(QRectF analyticsRoi READ analyticsRoi WRITE setAnalyticsRoi
        NOTIFY analyticsRoiChanged)

public:
    explicit ChunkProvider(QObject* parent = nullptr);
    virtual ~ChunkProvider() override;

    virtual const QnTimePeriodList& periods(Qn::TimePeriodContent type) const override;
    qint64 bottomBound() const;

    QnResourcePtr resource() const;

    QString motionFilter() const;
    void setMotionFilter(const QString& value);

    QRectF analyticsRoi() const;
    void setAnalyticsRoi(const QRectF& value);

    bool isLoading() const;
    bool isLoadingMotion() const;
    bool isLoadingAnalytics() const;

    Q_INVOKABLE qint64 closestChunkEndMs(qint64 position, bool forward) const;
    Q_INVOKABLE void update();
    Q_INVOKABLE bool hasChunks() const;
    Q_INVOKABLE bool hasMotionChunks() const;

    static void registerQmlType();

signals:
    void resourceChanged();
    void bottomBoundChanged();
    void loadingChanged();
    void loadingMotionChanged();
    void loadingAnalyticsChanged();
    void motionFilterChanged();
    void analyticsRoiChanged();

private:
    QnResource* rawResource() const;
    void setRawResource(QnResource* value);
    void handleLoadingChanged(Qn::TimePeriodContent contentType);

private:
    class Private;
    class ChunkProviderInternal;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
