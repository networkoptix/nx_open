// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/analytics/taxonomy/abstract_enum_type.h>

#include <nx/vms/api/analytics/descriptors.h>

namespace nx::analytics::taxonomy {

struct InternalState;
class ErrorHandler;

class EnumType: public AbstractEnumType
{
public:
    EnumType(
        nx::vms::api::analytics::EnumTypeDescriptor enumTypeDescriptor,
        QObject* parent = nullptr);

    virtual QString id() const override;

    virtual QString name() const override;

    virtual AbstractEnumType* base() const override;

    virtual std::vector<QString> ownItems() const override;

    virtual std::vector<QString> items() const override;

    virtual nx::vms::api::analytics::EnumTypeDescriptor serialize() const override;

    void resolve(InternalState* inOutInternalState, ErrorHandler* errorHandler);

private:
    nx::vms::api::analytics::EnumTypeDescriptor m_descriptor;
    EnumType* m_base = nullptr;
    bool m_resolved = false;
};

} // namespace nx::analytics::taxonomy
