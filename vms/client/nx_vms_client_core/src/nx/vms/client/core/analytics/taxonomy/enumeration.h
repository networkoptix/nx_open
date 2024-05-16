// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>

namespace nx::analytics::taxonomy { class AbstractEnumType; }

namespace nx::vms::client::core::analytics::taxonomy {

class NX_VMS_CLIENT_CORE_API Enumeration: public QObject
{
    Q_OBJECT
    Q_PROPERTY(std::vector<QString> items READ items CONSTANT)

public:
    Enumeration(QObject* parent = nullptr);
    virtual ~Enumeration() override;

    std::vector<QString> items() const;

    void addEnumType(nx::analytics::taxonomy::AbstractEnumType* enumType);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core::analytics::taxonomy
