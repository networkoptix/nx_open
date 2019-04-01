#include "exception.h"

namespace nx {
namespace utils {

const char* Exception::what() const noexcept
{
    if (!m_whatCache)
        m_whatCache = message().toStdString();

    return m_whatCache->c_str();
}

} // namespace utils
} // namespace nx

