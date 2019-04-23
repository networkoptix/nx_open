#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::desktop {

class ServerSettingsDialogStore;

class ServerPluginDataWatcher: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit ServerPluginDataWatcher(ServerSettingsDialogStore* store, QObject* parent = nullptr);
    virtual ~ServerPluginDataWatcher() override;

    QnMediaServerResourcePtr server() const;
    void setServer(const QnMediaServerResourcePtr& value);

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // nx::vms::client::desktop
