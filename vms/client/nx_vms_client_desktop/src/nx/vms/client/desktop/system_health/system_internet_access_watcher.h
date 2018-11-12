#pragma once

#include <QtCore/QObject>

#include <common/common_module_aware.h>
#include <core/resource/resource_fwd.h>

namespace nx::vms::client::desktop {

class SystemInternetAccessWatcher:
    public QObject,
    public QnCommonModuleAware
{
    Q_OBJECT

public:
    explicit SystemInternetAccessWatcher(QObject* parent = nullptr);
    virtual ~SystemInternetAccessWatcher() override;

    bool systemHasInternetAccess() const;

    static bool serverHasInternetAccess(const QnMediaServerResourcePtr& server);

signals:
    void internetAccessChanged(bool systemHasInternetAccess, QPrivateSignal);

private:
    void updateState();
    void setHasInternetAccess(bool value);

private:
    bool m_hasInternetAccess = false;
    bool m_processRuntimeUpdates = false;
};

} // namespace nx::vms::client::desktop
