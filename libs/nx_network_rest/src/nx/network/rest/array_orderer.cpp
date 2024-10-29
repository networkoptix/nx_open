// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <cmath>
#include <nx/utils/log/assert.h>

#include "array_orderer.h"

static inline void swap(QJsonValueRef v1, QJsonValueRef v2)
{
    QJsonValue temp(v1);
    v1 = QJsonValue(v2);
    v2 = temp;
}

namespace nx::network::rest::json {

namespace {

int compare(
    const QJsonValue& lhs, const QJsonValue& rhs, const QStringList& path, int pathPos = 0);

int compare(const QJsonObject& lhs, const QJsonObject& rhs, const QStringList& path, int pathPos)
{
    while (pathPos < path.length() && path.at(pathPos).startsWith('#'))
        ++pathPos;
    if (pathPos >= path.length())
        return 0;

    const QString& fieldKey = path.at(pathPos);
    const auto lhsField = lhs.find(fieldKey);
    const auto rhsField = rhs.find(fieldKey);
    if (lhsField == lhs.end())
        return (rhsField == rhs.end()) ? 0 : 1;
    if (rhsField == rhs.end())
        return -1;
    return compare(*lhsField, *rhsField, path, ++pathPos);
}

int compare(const QJsonValue& lhs, const QJsonValue& rhs, const QStringList& path, int pathPos)
{
    const auto lhsType = lhs.type();
    const auto rhsType = rhs.type();
    if (!NX_ASSERT(lhsType != QJsonValue::Type::Undefined))
        return NX_ASSERT(rhsType != QJsonValue::Type::Undefined) ? 1 : 0;
    if (!NX_ASSERT(rhsType != QJsonValue::Type::Undefined))
        return -1;
    if (lhsType == QJsonValue::Type::Null)
        return (rhsType == QJsonValue::Type::Null) ? 0 : 1;
    if (rhsType == QJsonValue::Type::Null)
        return -1;
    if (lhsType != rhsType)
        return ((int) lhsType) - ((int) rhsType);
    switch (lhsType)
    {
        case QJsonValue::Type::Undefined: //< Shouldn't get here.
        case QJsonValue::Type::Null: //< Shouldn't get here.
        case QJsonValue::Type::Array:
            break;
        case QJsonValue::Type::Bool:
            return ((int) lhs.toBool()) - ((int) rhs.toBool());
        case QJsonValue::Type::Double:
        {
            const double delta = lhs.toDouble() - rhs.toDouble();
            return (std::fabs(delta) < std::numeric_limits<double>::epsilon()) ? 0 : ((int) delta);
        }
        case QJsonValue::Type::String:
            return lhs.toString().compare(rhs.toString());
        case QJsonValue::Type::Object:
            return compare(lhs.toObject(), rhs.toObject(), path, pathPos);
    }
    return 0;
}

} // namespace

ArrayOrderer::ArrayOrderer(QStringList values)
{
    for (auto item: values)
    {
        if (!NX_ASSERT(!item.isEmpty()))
            continue;
        addValue(std::move(item));
    }
}

bool ArrayOrderer::operator()(QJsonArray* array) const
{
    const auto path = m_values.find(m_path.join('.'));
    if (path == m_values.end())
        return false;
    std::stable_sort(
        array->begin(),
        array->end(),
        [path](const QJsonValue& lhs, const QJsonValue& rhs)
        {
            return compare(lhs, rhs, *path) < 0;
        });
    return true;
}

void ArrayOrderer::addValue(const QString& value)
{
    static const QString kArrayFieldSuffix = "[]";
    static const QChar kFieldSeparator = '.';
    static const QString kArrayFieldSuffixWithSeparator = kArrayFieldSuffix + kFieldSeparator;
    const int pos = value.lastIndexOf(kArrayFieldSuffixWithSeparator);
    if (pos == -1)
    {
        m_values[QString()] = value.split(kFieldSeparator);
    }
    else
    {
        m_values[value.left(pos + kArrayFieldSuffix.length())] =
            value.right(value.length() - pos - kArrayFieldSuffixWithSeparator.length())
                .split(kFieldSeparator);
    }
}

} // namespace nx::network::rest::json
