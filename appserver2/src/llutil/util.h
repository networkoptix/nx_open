#ifndef _LLUTIL_UTIL_H_
#define _LLUTIL_UTIL_H_

#include <string>

namespace LLUtil {

void changeGuidByteOrder(std::string& guid);
QString changedGuidByteOrder(const QString& guid);

} // namespace LLutil {}

#endif // _LLUTIL_UTIL_H_