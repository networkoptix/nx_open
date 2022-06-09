// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>

#include <nx/vms/client/desktop/analytics/taxonomy/abstract_color_set.h>

namespace nx::analytics::taxonomy { class AbstractColorType; }

namespace nx::vms::client::desktop::analytics::taxonomy {

class ColorSet: public AbstractColorSet
{
public:
    ColorSet(QObject* parent);

    virtual ~ColorSet() override;

    virtual std::vector<QString> items() const override;

    virtual QString color(const QString& item) const override;

    void addColorType(nx::analytics::taxonomy::AbstractColorType* colorType);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client:;desktop::analytics::taxonomy
