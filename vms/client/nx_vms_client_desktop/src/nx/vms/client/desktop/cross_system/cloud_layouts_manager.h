// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/desktop/resource/resource_fwd.h>

namespace nx::vms::client::desktop {

class SystemContext;

/**
 * Monitors available Cloud Layouts. Contains corresponding system context.
 */
class CloudLayoutsManager: public QObject
{
    Q_OBJECT

public:
    CloudLayoutsManager(QObject* parent = nullptr);
    virtual ~CloudLayoutsManager() override;

    /** Convert layout to the cloud one. */
    LayoutResourcePtr convertLocalLayout(const LayoutResourcePtr& layout);

    using SaveCallback = std::function<void(bool)>;
    void saveLayout(const CrossSystemLayoutResourcePtr& layout, SaveCallback callback = {});

    void deleteLayout(const QnLayoutResourcePtr& layout);

    /** Update cloud layout resources by fetching the latest data. */
    void updateLayouts();

    SystemContext* systemContext() const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
