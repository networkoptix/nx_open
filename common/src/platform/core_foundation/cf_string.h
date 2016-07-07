
#pragma once

#if defined(Q_OS_MAC)

#include <QtCore/QString>
#include <platform/core_foundation/cf_ref_holder.h>

namespace cf {

class QnCFString : public QnCFRefHolder<CFStringRef>
{
    typedef QnCFRefHolder<CFStringRef> base_type;

public:
    QnCFString();

    QnCFString(CFStringRef ref);

    explicit QnCFString(const QString& value);

    ~QnCFString();

    QString toString() const;
};

}

#endif
