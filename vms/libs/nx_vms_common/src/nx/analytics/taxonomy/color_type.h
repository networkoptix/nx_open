// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/analytics/taxonomy/abstract_color_type.h>

#include <nx/vms/api/analytics/descriptors.h>

namespace nx::analytics::taxonomy {

struct InternalState;
class ErrorHandler;

class ColorType: public AbstractColorType
{

public:
    ColorType(
        nx::vms::api::analytics::ColorTypeDescriptor colorTypeDescriptor,
        QObject* parent = nullptr);

    virtual QString id() const override;

    virtual QString name() const override;

    virtual AbstractColorType* base() const override;

    virtual std::vector<QString> baseItems() const override;

    virtual std::vector<QString> ownItems() const override;

    virtual std::vector<QString> items() const override;

    virtual QString color(const QString& item) const override;

    virtual nx::vms::api::analytics::ColorTypeDescriptor serialize() const override;

    void resolve(InternalState* inOutInternalState, ErrorHandler* errorHandler);

private:
    bool resolveItem(
        nx::vms::api::analytics::ColorItem* inOutColorItem, ErrorHandler* errorHandler);

private:
    nx::vms::api::analytics::ColorTypeDescriptor m_descriptor;
    ColorType* m_base = nullptr;

    std::map<QString, QString> m_colorByName;
    std::vector<QString> m_cachedOwnItems;
    std::set<QString> m_cachedBaseColorTypeNames;

    bool m_resolved = false;
};

} // namespace nx::analytics::taxonomy
