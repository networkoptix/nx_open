#pragma once

#include <QtCore/QString>

#include <CoreFoundation/CoreFoundation.h>

#include <nx/utils/platform/core_foundation_mac/cf_ref_holder.h>
#include <nx/utils/platform/core_foundation_mac/cf_string.h>

namespace cf {

class QnCFDictionary: public QnCFRefHolder<CFDictionaryRef>
{
    using base_type = QnCFRefHolder<CFDictionaryRef>;
public:
    QnCFDictionary(CFDictionaryRef ref);

    static QnCFDictionary makeOwned(CFDictionaryRef ref);

    template<typename CFResultTypeRef>
    CFResultTypeRef getValue(const QString& key) const;

    QString getStringValue(const QString& key) const;
};

template<typename CFResultTypeRef>
CFResultTypeRef QnCFDictionary::getValue(const QString& key) const
{
    return static_cast<CFResultTypeRef>(CFDictionaryGetValue(ref(), QnCFString(key).ref()));
}

} // namespace cf
