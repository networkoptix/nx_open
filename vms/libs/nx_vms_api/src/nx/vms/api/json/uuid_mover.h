// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <vector>
#include <QJsonObject>

class QnJsonContext;

namespace nx::vms::api::json {

class NX_VMS_API UuidMover
{
public:
    using FieldMap = std::map<QString, std::vector<QString>>;

    bool move(const FieldMap& fields, const QJsonValue& value, QnJsonContext* ctx);
    bool move(const FieldMap& fields, QnJsonContext* ctx);
    bool isModified() const { return m_root.isModified; }
    QJsonValue result();

private:
    struct Level
    {
        QJsonObject json;
        std::map<QString, Level> kids;
        bool isModified = false;

        static void traverse(Level* level);
    } m_root;
};

} // namespace nx::vms::api::json
