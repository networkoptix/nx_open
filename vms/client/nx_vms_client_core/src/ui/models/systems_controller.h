// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "abstract_systems_controller.h"

class NX_VMS_CLIENT_CORE_API SystemsController: public AbstractSystemsController
{
    Q_OBJECT

public:
    SystemsController(QObject* parent = nullptr);
    virtual ~SystemsController() override;

public:
    virtual QnCloudStatusWatcher::Status cloudStatus() const override;
    virtual QString cloudLogin() const override;
    virtual QnAbstractSystemsFinder::SystemDescriptionList systemsList() const override;
    virtual nx::vms::client::core::welcome_screen::TileVisibilityScope visibilityScope(
        const QnUuid& localId) const override;
    virtual bool setScopeInfo(
        const QnUuid& localId,
        const QString& name,
        nx::vms::client::core::welcome_screen::TileVisibilityScope visibilityScope) override;
};
