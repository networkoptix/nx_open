#include "hardware_id_version.h"

namespace LLUtil {

int hardwareIdVersion(const QString& hardwareId)
{
    if (hardwareId.length() == 32)
        return 0;

    if (hardwareId.length() == 34)
        return hardwareId.mid(0, 2).toInt();

    return -1;
}

} // namespace LLUtil
