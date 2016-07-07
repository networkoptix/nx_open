
#pragma once

#if defined(Q_OS_MAC)

#include <platform/core_foundation_mac/cf_ref_holder.h>

class QString;

namespace cf {

class QnCFUrl : public QnCFRefHolder<CFURLRef>
{
    typedef QnCFRefHolder<CFURLRef> base_type;

public:
    static QnCFUrl createFileUrl(const QString& fileName);

    ~QnCFUrl();

private:
    QnCFUrl();
};

}

#endif
