#pragma once

#include <QtCore/QString>
#include <QtCore/QList>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

namespace nx::update {

static const QString kTargetKey = "target";
static const QString kInstalledKey = "installed";

struct PersistentUpdateStorage
{
    QList<QnUuid> servers;
    bool manuallySet = false;

    PersistentUpdateStorage(const QList<QnUuid>& servers, bool manuallySet):
        servers(servers),
        manuallySet(manuallySet)
    {}

    PersistentUpdateStorage() = default;
};

#define PersistentUpdateStorage_Fields (servers)(manuallySet)
QN_FUSION_DECLARE_FUNCTIONS(PersistentUpdateStorage, (ubjson)(json)(eq))

} // namespace nx::update

Q_DECLARE_METATYPE(nx::update::PersistentUpdateStorage);
