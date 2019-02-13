#pragma once

#include <QJsonValue>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

namespace nx::vms::api::rules {

struct NX_VMS_API Field
{
    Q_GADGET

public:
    QnUuid id;
    QString name;
    QString metatype;
    QMap<QString, QJsonValue> props;
};

#define nx_vms_api_rules_Field_Fields \
    (id)(name)(metatype)(props)

QN_FUSION_DECLARE_FUNCTIONS(Field, (json), NX_VMS_API)

} // namespace nx::vms::api::rules
