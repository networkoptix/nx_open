#pragma once

#include <QtCore/QMetaType>

typedef QSet<QString> QnStringSet;
Q_DECLARE_METATYPE(QnStringSet)

class QnClientCoreMetaTypes
{
public:
    static void initialize();
};
