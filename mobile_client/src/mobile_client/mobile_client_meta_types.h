#pragma once

#include <common/common_meta_types.h>

typedef QHash<QnUuid, qreal> QnAspectRatioHash;
Q_DECLARE_METATYPE(QnAspectRatioHash)

typedef QSet<QString> QnStringSet;
Q_DECLARE_METATYPE(QnStringSet)

enum class LiteModeType
{
    LiteModeAuto = -1,
    LiteModeDisabled,
    LiteModeEnabled
};

class QnMobileClientMetaTypes {
public:
    static void initialize();

private:
    static void registerMetaTypes();
    static void registerQmlTypes();
};
