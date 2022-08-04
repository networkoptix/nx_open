// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/analytics/taxonomy/abstract_scope.h>

namespace nx::analytics::taxonomy {

class Engine;
class Group;

class Scope: public AbstractScope
{
public:
    Scope(QObject* parent = nullptr);

    virtual AbstractEngine* engine() const override;

    virtual AbstractGroup* group() const override;

    virtual QString provider() const override;

    virtual std::vector<QnUuid> deviceIds() const override;

    virtual bool isEmpty() const override;

    void setEngine(Engine* engine);

    void setGroup(Group* group);

    void setProvider(const QString& provider);

    void setDeviceIds(std::vector<QnUuid> deviceIds);

private:
    Engine* m_engine = nullptr;
    Group* m_group = nullptr;
    QString m_provider;
    std::vector<QnUuid> m_deviceIds;

};

} // namespace nx::analytics::taxonomy
