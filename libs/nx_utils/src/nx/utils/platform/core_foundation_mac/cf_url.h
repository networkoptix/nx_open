#pragma once

#include <nx/utils/platform/core_foundation_mac/cf_ref_holder.h>

class QString;

namespace cf {

class QnCFUrl : public QnCFRefHolder<CFURLRef>
{
    typedef QnCFRefHolder<CFURLRef> base_type;

public:
    static QnCFUrl createFileUrl(const QString& fileName);

    static QString toString(const CFURLRef ref);

    ~QnCFUrl();


private:
    QnCFUrl();
};

} // namespace cf
