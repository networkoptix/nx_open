#pragma once

#include <QtCore/QString>

#include <nx/utils/platform/core_foundation_mac/cf_ref_holder.h>

namespace cf {

class QnCFString : public QnCFRefHolder<CFStringRef>
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
