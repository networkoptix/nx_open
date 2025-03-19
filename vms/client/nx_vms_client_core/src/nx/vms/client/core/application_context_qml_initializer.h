// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "application_context.h"
#include "qml_context_initializer.h"

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API ApplicationContextQmlInitializer:
    public QmlContextInitializer<ApplicationContext>
{
    using base_type = QmlContextInitializer<ApplicationContext>;

public:
    ApplicationContextQmlInitializer(QObject* owner);

    static void storeToQmlContext(ApplicationContext* context);
};

} // namespace nx::vms::client::core
