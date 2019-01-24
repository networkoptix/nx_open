#include "api_ioport_data.h"

#include <nx/fusion/serialization/json.h>

QString toString(const QnIOPortData &portData)
{
    return QJson::serialized(portData);
}
