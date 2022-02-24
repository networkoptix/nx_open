// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <nx/utils/platform/core_foundation_mac/cf_ref_holder.h>

namespace cf {

class NX_UTILS_API QnCFString : public QnCFRefHolder<CFStringRef>
{
    typedef QnCFRefHolder<CFStringRef> base_type;

public:
    QnCFString();

    QnCFString(const QnCFString& other);

    QnCFString(CFStringRef ref);

    explicit QnCFString(const QString& value);

    static QnCFString makeOwned(CFStringRef ref);

    ~QnCFString();

    QString toString() const;

};

}
