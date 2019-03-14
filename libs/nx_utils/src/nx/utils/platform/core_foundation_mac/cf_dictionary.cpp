#include "cf_dictionary.h"

namespace cf {

QnCFDictionary::QnCFDictionary(CFDictionaryRef ref):
    base_type(ref)
{
}

QnCFDictionary QnCFDictionary::makeOwned(CFDictionaryRef ref)
{
    return base_type::makeOwned<QnCFDictionary, CFDictionaryRef>(ref);
}

QString QnCFDictionary::getStringValue(const QString& key) const
{
    const auto value = getValue<CFStringRef>(key);
    return (value ? QnCFString(value).toString() : nullptr);
}

} // namespace cf

