// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/resource/layout_resource.h>

namespace nx::vms::client::core {

class SystemContext;

/**
 * Monitors available Cloud Layouts. Contains corresponding system context.
 */
class NX_VMS_CLIENT_CORE_API CloudLayoutsManager: public QObject
{
    Q_OBJECT

public:
    CloudLayoutsManager(QObject* parent = nullptr);
    virtual ~CloudLayoutsManager() override;

    /** Convert layout to the cloud one. */
    LayoutResourcePtr convertLocalLayout(
        const LayoutResourcePtr& layout,
        LayoutResource::ItemsRemapHash* itemsRemapHash = nullptr);

    using SaveCallback = std::function<void(bool)>;
    void saveLayout(const CrossSystemLayoutResourcePtr& layout, SaveCallback callback = {});

    bool deleteLayout(const QnLayoutResourcePtr& layout);

    /** Update cloud layout resources by fetching the latest data. */
    void updateLayouts();

    SystemContext* systemContext() const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
