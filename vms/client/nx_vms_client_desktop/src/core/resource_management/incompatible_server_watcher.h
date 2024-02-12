// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/system_context_aware.h>

class QnIncompatibleServerWatcher: public QObject, public nx::vms::client::core::SystemContextAware
{
    Q_OBJECT

public:
    explicit QnIncompatibleServerWatcher(
        nx::vms::common::SystemContext* systemContext,
        QObject* parent = nullptr);
    virtual ~QnIncompatibleServerWatcher() override;

    void setMessageProcessor(QnClientMessageProcessor* messageProcessor);

    void start();
    void stop();

    /**
     * Prevent incompatible resource from removal if it goes offline.
     * @param id Real server GUID.
     * @param keep true to keep the resource (if present), false to not keep. In case when the
     *     resource was removed when was having 'keep' flag on, it will be removed in this method.
     */
    void keepServer(const nx::Uuid& id, bool keep);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};
