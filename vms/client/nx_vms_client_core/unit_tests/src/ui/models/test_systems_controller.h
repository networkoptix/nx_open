// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <ui/models/abstract_systems_controller.h>

namespace nx::vms::client::core {
namespace test {

class TestSystemsController: public AbstractSystemsController
{
    struct ScopeInfo
    {
        nx::Uuid localId;
        QString name;
        nx::vms::client::core::welcome_screen::TileVisibilityScope visibilityScope;
    };

public:
    TestSystemsController(QObject* parent = nullptr);
    virtual ~TestSystemsController() override;

public:
    virtual CloudStatusWatcher::Status cloudStatus() const override;
    virtual QString cloudLogin() const override;
    virtual QnAbstractSystemsFinder::SystemDescriptionList systemsList() const override;
    virtual nx::vms::client::core::welcome_screen::TileVisibilityScope visibilityScope(
        const nx::Uuid& localId) const override;
    virtual bool setScopeInfo(
        const nx::Uuid& localId,
        const QString& name,
        nx::vms::client::core::welcome_screen::TileVisibilityScope visibilityScope) override;

    void emitCloudStatusChanged();
    void emitCloudLoginChanged();

    void emitSystemDiscovered(QnSystemDescriptionPtr systemDescription);
    void emitSystemLost(const QString& systemId);

private:
    QnAbstractSystemsFinder::SystemDescriptionList m_systems;
    QHash<nx::Uuid, ScopeInfo> m_scopeInfoHash;
};

} // namespace test
} // namespace nx::vms::client::core
