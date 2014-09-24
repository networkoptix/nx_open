#ifndef _LLUTIL_BASE64_H_
#define _LLUTIL_BASE64_H_

#include <string>

namespace LLUtil {
::std::string base64_encode(const ::std::string &bindata);
::std::string base64_decode(const ::std::string &ascdata);
}

#endif // _LLUTIL_BASE64_H_