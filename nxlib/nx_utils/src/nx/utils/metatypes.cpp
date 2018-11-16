#include "metatypes.h"

#include "mac_address.h"
#include "url.h"
#include "uuid.h"
#include "scope_guard.h"

namespace {

static bool nx_utils_metatypes_initialized = false;

} // namespace

namespace nx::utils {

void Metatypes::initialize()
{
    // Note that running the code twice is perfectly OK.
    if (nx_utils_metatypes_initialized)
        return;

    nx_utils_metatypes_initialized = true;

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
