// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_tree_settings.h"

#include <QtCore/QPointer>

#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/state/client_state_handler.h>

namespace
{

const QString kResourceTreeSettingsDelegateId = "resourceTreeSettings";
const QString kShowServersInTreeKey = "showServersInTree";

} // namespace

namespace nx::vms::client::desktop {

class ResourceTreeSettings::StateDelegate: public ClientStateDelegate
{
    using base_type = ClientStateDelegate;

public:
    StateDelegate(ResourceTreeSettings* resourceTreeSettings):
        base_type(),
        m_resourceTreeSettings(resourceTreeSettings)
    {
    }

    virtual bool loadState(
        const DelegateState& state,
        SubstateFlags flags,
        const StartupParameters& params) override
    {
        if (!NX_ASSERT(!m_resourceTreeSettings.isNull()))
            return false;

        if (!flags.testFlag(ClientStateDelegate::Substate::systemSpecificParameters))
            return false;

        bool result = true;

        m_resourceTreeSettings->clear();

        const auto showServersInTreeValue = state.value(kShowServersInTreeKey);

        if (showServersInTreeValue.isBool())
            m_resourceTreeSettings->setShowServersInTree(showServersInTreeValue.toBool());
        else
            result = false;

        return result;
    }

    virtual void saveState(DelegateState* state, SubstateFlags flags) override
    {
        if (!NX_ASSERT(!m_resourceTreeSettings.isNull()))
            return;

        if (!flags.testFlag(ClientStateDelegate::Substate::systemSpecificParameters))
            return;

        state->insert(kShowServersInTreeKey, m_resourceTreeSettings->showServersInTree());
    }

    virtual void createInheritedState(
        DelegateState* state,
        SubstateFlags flags,
        const QStringList& resources) override
    {
        if (!NX_ASSERT(!m_resourceTreeSettings.isNull()))
            return;

        if (!flags.testFlag(ClientStateDelegate::Substate::systemSpecificParameters))
            return;

        state->insert(kShowServersInTreeKey, m_resourceTreeSettings->showServersInTree());
    }

    virtual DelegateState defaultState() const override
    {
        DelegateState result;
        result.insert(kShowServersInTreeKey, true);
        return result;
    }

private:
    QPointer<ResourceTreeSettings> m_resourceTreeSettings;
};

ResourceTreeSettings::ResourceTreeSettings(QObject* parent):
    base_type(parent)
{
    if (ini().newPanelsLayout)
        return;

    appContext()->clientStateHandler()->registerDelegate(
        kResourceTreeSettingsDelegateId, std::make_unique<StateDelegate>(this));
}

ResourceTreeSettings::~ResourceTreeSettings()
{
    if (!ini().newPanelsLayout)
        appContext()->clientStateHandler()->unregisterDelegate(kResourceTreeSettingsDelegateId);
}

bool ResourceTreeSettings::showServersInTree() const
{
    return m_showServersInResourceTree;
}

void ResourceTreeSettings::setShowServersInTree(bool show)
{
    if (m_showServersInResourceTree == show)
        return;

    m_showServersInResourceTree = show;
    emit showServersInTreeChanged();
}

void ResourceTreeSettings::clear()
{
    m_showServersInResourceTree = true;
}

} // namespace nx::vms::client::desktop
