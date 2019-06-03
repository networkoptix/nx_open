#pragma once

#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

namespace nx::update {

struct PersistentUpdateStorage
{
    QSet<QnUuid> servers;
    bool manuallySet = false;
};

#define PersistentUpdateStorage_Fields (servers)(manuallySet)
QN_FUSION_DECLARE_FUNCTIONS(Package, (ubjson)(json)(eq))

} // namespace nx::update

Q_DECLARE_METATYPE(nx::update::PersistentUpdateStorage);
