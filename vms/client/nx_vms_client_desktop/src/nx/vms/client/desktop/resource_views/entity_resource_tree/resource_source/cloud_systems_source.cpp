// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_systems_source.h"

#include <QtCore/QVector>

#include <common/common_module.h>
#include <finders/systems_finder.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/common/system_settings.h>

using namespace nx::vms::common;

namespace {

QVector<QString> getOtherCloudSystemsIds(SystemSettings* globalSettings)
{
    QVector<QString> result;

    const auto thisCloudSystemId = globalSettings->cloudSystemId();

    const auto systemsDescriptions = qnSystemsFinder->systems();
    for (const auto& systemDescription: systemsDescriptions)
    {
        if (systemDescription->isCloudSystem() && systemDescription->id() != thisCloudSystemId)
            result.append(systemDescription->id());
    }

    return result;
}

} // namespace

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

CloudSystemsSource::CloudSystemsSource(const QnCommonModule* commonModule):
    m_commonModule(commonModule)
{
    initializeRequest =
        [this]
        {
            const auto systemsFinder = appContext()->systemsFinder();
            const bool isUnitTestsEnvironment = !systemsFinder;
            if (isUnitTestsEnvironment)
                return;

            m_connectionsGuard.add(systemsFinder->QObject::connect(
                systemsFinder, &QnSystemsFinder::systemDiscovered,
                    [this](const QnSystemDescriptionPtr& systemDescription)
                    {
                        if (systemDescription->isCloudSystem() && systemDescription->id()
                            != m_commonModule->globalSettings()->cloudSystemId())
                        {
                            (*addKeyHandler)(systemDescription->id());
                        }
                    }));

            m_connectionsGuard.add(systemsFinder->QObject::connect(
                systemsFinder, &QnSystemsFinder::systemLost,
                    [this](const QString& systemId)
                    {
                        (*removeKeyHandler)(systemId);
                    }));

            setKeysHandler(getOtherCloudSystemsIds(m_commonModule->globalSettings()));
        };
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop

