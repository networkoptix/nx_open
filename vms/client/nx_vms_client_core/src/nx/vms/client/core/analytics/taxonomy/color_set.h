// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>

namespace nx::analytics::taxonomy { class AbstractColorType; }

namespace nx::vms::client::core::analytics::taxonomy {

class NX_VMS_CLIENT_CORE_API ColorSet: public QObject
{
    Q_OBJECT
    Q_PROPERTY(std::vector<QString> items READ items CONSTANT)

public:
    ColorSet(QObject* parent);
    virtual ~ColorSet() override;

    std::vector<QString> items() const;

    Q_INVOKABLE QString color(const QString& item) const;

    void addColorType(nx::analytics::taxonomy::AbstractColorType* colorType);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core::analytics::taxonomy
