// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QMap>

namespace nx::network::rest::json {

class NX_NETWORK_REST_API ArrayOrderer
{
public:
    struct FieldScope
    {
        FieldScope(const QString& name, ArrayOrderer* p): parent(p) { p->beginField(name); }
        ~FieldScope() { parent->endField(); }

        ArrayOrderer* parent;
    };

    ArrayOrderer() = default;
    ArrayOrderer(QStringList values);
    bool operator()(QJsonArray* array) const;

private:
    void beginField(const QString& name) { m_path.push_back(name); }
    void endField() { m_path.pop_back(); }
    void addValue(const QString& value);

private:
    QMap<QString, QStringList> m_values;
    QStringList m_path;
};

} // namespace nx::network::rest::json
