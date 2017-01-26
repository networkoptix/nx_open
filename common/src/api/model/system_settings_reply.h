#pragma once

#include <nx/fusion/model_functions_fwd.h>

class QnSystemSettingsReply
{
public:
    //map<name, value>
    QHash<QString, QString> settings;
};
#define QnSystemSettingsReply_Fields (settings)

QN_FUSION_DECLARE_FUNCTIONS(QnSystemSettingsReply, (json))

