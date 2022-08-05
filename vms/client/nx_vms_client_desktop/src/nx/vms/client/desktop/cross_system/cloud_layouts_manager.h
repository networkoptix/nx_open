// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>

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
    QnLayoutResourcePtr convertLocalLayout(const QnLayoutResourcePtr& layout);

    using SaveCallback = std::function<void(bool)>;
    void saveLayout(const QnLayoutResourcePtr& layout, SaveCallback callback = {});

    void deleteLayout(const QnLayoutResourcePtr& layout);

    SystemContext* systemContext() const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
