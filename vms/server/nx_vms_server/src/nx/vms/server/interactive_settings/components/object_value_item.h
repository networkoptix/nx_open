#pragma once

#include "value_item.h"

namespace nx::vms::server::interactive_settings::components {

/**
 * Base class for items which value is a JSON object.
 *
 * Note that this class overrides setJsonValue() method. When an object value is assigned over
 * another object value, their contents is merged recursively. Only `Object` values are merged. All
 * other values are just re-assigned.
 * E.g. the following objects are merged this way:
 * ```
 *     {"a": 1, "b": [1, 2, 3], "c": {"a": 1}}
 *   + {"d": 3, "b": [4, 5], "c": {"a": 2}}
 *   = {"a": 1, "b": [4, 5], "c": {"a": 2}, "d": 3}
 * ```
 * Object value can be cleared by a `null` value:
 * ```
 *     {"a": 1} + null = null
 *     {"a": 1. "b": {"a": 1}}  + {"b": null} = {"a": 1, "b": null}
 * ```
 */
class ObjectValueItem: public ValueItem
{
public:
    using ValueItem::ValueItem;

    virtual QJsonValue normalizedValue(const QJsonValue& value) const override;
    virtual QJsonValue fallbackDefaultValue() const override;
    virtual void setJsonValue(const QJsonValue& value) override;
};

} // namespace nx::vms::server::interactive_settings::components
