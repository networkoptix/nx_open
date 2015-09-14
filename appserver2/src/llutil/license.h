#ifndef _LLUTIL_LICENSE_H_
#define _LLUTIL_LICENSE_H_

namespace LLUtil {
bool isSignatureMatch(const std::string &data, const std::string &signature, const std::string &publicKey);
bool isNoptixTextSignatureMatch(const std::string &data, const std::string &signature);
}

#endif // _LLUTIL_LICENSE_H_