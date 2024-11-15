// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "encryption.h"

namespace nx::vms::rules {

const QSet<QString>& encryptedActionBuilderProperties(const QString& type)
{
    static const auto kAuthProps = QSet<QString>{"password", "token"};
    static const QSet<QString> kEmpty;

    return (type == "httpAuth") ? kAuthProps : kEmpty;
}

} // namespace nx::vms::rules
