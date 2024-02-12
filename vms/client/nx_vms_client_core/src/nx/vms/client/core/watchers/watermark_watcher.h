// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QUrl>

#include <nx/core/watermark/watermark.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/system_context_aware.h>

namespace nx::vms::client::core {

/** Watches over the watermark parameters/states and provides urls for the images if needed. */
class NX_VMS_CLIENT_CORE_API WatermarkWatcher:
    public QObject,
    public SystemContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    static void registerQmlType();

    WatermarkWatcher(SystemContext* systemContext, QObject* parent = nullptr);
    virtual ~WatermarkWatcher() override;

    /**
     * Register watermark image url watcher with the specified parameters.
     * @param id Identifier of the target watermark image url. May be video item identifier,
     * for example.
     * @param size Size of the target watermark frame.
     */
    Q_INVOKABLE void addWatermarkImageUrlWatcher(
        const nx::Uuid& id,
        const QSize& size);

    /** Release watermark image url resources. */
    Q_INVOKABLE void removeWatermarkImageUrlWatcher(const nx::Uuid& id);

    /** Return source url for the watermark image. May be used with the watermark provider. */
    Q_INVOKABLE QUrl watermarkImageUrl(const nx::Uuid& id) const;

    /** Update target size for the watermark image url with the specified id. */
    Q_INVOKABLE void updateWatermarkImageUrlSize(
        const nx::Uuid& id,
        const QSize& size);

signals:
    void watermarkImageUrlChanged(const nx::Uuid& id);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // nx::vms::client::core
