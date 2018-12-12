#pragma once

#include <string>

#include <nx/sdk/settings.h>

namespace nx {
namespace sdk {
namespace common {

/**
 * @return Possibly multiline JSON string ending with `\n`:
 * <code><pre>
 *     [
 *         { "name": "...", "value": "..." },
 *         ...
 *     ]
 * </pre></code>
 */
std::string toString(const Settings* settings, int overallIndent = 0);

} // namespace common
} // namespace sdk
} // namespace nx
