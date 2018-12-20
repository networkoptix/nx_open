#include "metatypes.h"

#include <atomic>

#include "mac_address.h"
#include "url.h"
#include "uuid.h"
#include "scope_guard.h"

namespace nx::utils {

void Metatypes::initialize()
{
    static std::atomic_bool initialized = false;

    if (initialized.load())
        return;

    initialized = true;

    // Fully qualified namespaces are not needed here but are mandatory in all signal declarations.

    qRegisterMetaType<MacAddress>();

    qRegisterMetaType<Url>();
    qRegisterMetaTypeStreamOperators<Url>();
    qRegisterMetaTypeStreamOperators<QList<Url>>();

    qRegisterMetaType<QnUuid>();
    qRegisterMetaType<QSet<QnUuid>>("QSet<QnUuid>");
    qRegisterMetaTypeStreamOperators<QnUuid>();

    qRegisterMetaType<SharedGuardPtr>();
};

} // namespace nx::utils
