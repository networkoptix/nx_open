// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <CoreFoundation/CoreFoundation.h>

#include <nx/utils/platform/core_foundation_mac/cf_ref_holder.h>
#include <nx/utils/platform/core_foundation_mac/cf_string.h>

namespace cf {

class NX_UTILS_API QnCFDictionary: public QnCFRefHolder<CFDictionaryRef>
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
