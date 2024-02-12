// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "uuid_mover.h"

#include <nx/utils/uuid.h>
#include <nx/fusion/serialization/json_functions.h>

namespace nx::vms::api::json {

bool UuidMover::move(const FieldMap& fields, const QJsonValue& value, QnJsonContext* ctx)
{
    if (!value.isObject())
        return true;
    m_root.json = value.toObject();
    if (!move(fields, ctx))
        return false;
    return true;
}

static void setFailedKeyValue(QnJsonContext* ctx, const std::vector<QString>& fields, const QJsonValue& value)
{
    QString fieldName;
    for (const auto& subname : fields)
    {
        if (fieldName.isEmpty())
            fieldName = subname;
        else
            fieldName += '.' + subname;
    }
    ctx->setFailedKeyValue({fieldName, QString(QJson::serialized(value))});
}

bool UuidMover::move(const FieldMap& fields, QnJsonContext* ctx)
{
    for (const auto& field: fields)
    {
        const auto id = m_root.json.find(field.first);
        if (id == m_root.json.end())
            continue;
        std::vector<Level*> path;
        path.push_back(&m_root);
        for (size_t i = 0; i < field.second.size() - 1; ++i)
        {
            const auto it = path.back()->kids.find(field.second[i]);
            if (it == path.back()->kids.end())
            {
                const auto jsonIt = path.back()->json.find(field.second[i]);
                QJsonObject json;
                if (jsonIt == path.back()->json.end())
                {
                    json.insert(field.second[i], QJsonObject());
                }
                else
                {
                    if (!jsonIt->isObject())
                    {
                        setFailedKeyValue(ctx, field.second, *jsonIt);
                        return false;
                    }
                    json = jsonIt->toObject();
                }
                path.push_back(&path.back()->kids[field.second[i]]);
                path.back()->json = json;
            }
            else
            {
                path.push_back(&path.back()->kids[field.second[i]]);
            }
        }
        const auto bodyId = path.back()->json.find(field.second.back());
        if (bodyId == path.back()->json.end())
        {
            path.back()->json.insert(field.second.back(), *id);
            for (auto& pathItem: path)
                pathItem->isModified = true;
        }
        else if (nx::Uuid(id->toString()) != nx::Uuid(bodyId->toString()))
        {
            setFailedKeyValue(ctx, field.second, *bodyId);
            return false;
        }
    }
    return true;
}

QJsonValue UuidMover::result()
{
    if (NX_ASSERT(m_root.isModified))
    {
        Level::traverse(&m_root);
        m_root.isModified = false;
    }
    return m_root.json;
}

void UuidMover::Level::traverse(Level* level)
{
    for (auto& kid : level->kids)
    {
        if (!kid.second.isModified)
            continue;
        traverse(&kid.second);
        kid.second.isModified = false;
        level->json.insert(kid.first, kid.second.json);
    }
}

} // namespace nx::vms::api::json
