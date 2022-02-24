// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "entity_item_model_test_fixture.h"

#include <random>

#include <api/global_settings.h>
#include <common/common_module.h>
#include <common/static_common_module.h>

#include <client_core/client_core_module.h>
#include <client/client_startup_parameters.h>
#include <client/client_runtime_settings.h>

#include <nx/utils/debug_helpers/model_transaction_checker.h>

#include <nx/vms/client/desktop/resource_views/entity_item_model/entity_item_model.h>

namespace nx::vms::client::desktop {
namespace entity_item_model {
namespace test {

void EntityItemModelTest::SetUp()
{
    m_clientRuntimeSettings.reset(new QnClientRuntimeSettings(QnStartupParameters()));
    m_staticCommonModule.reset(new QnStaticCommonModule());
    m_clientCoreModule.reset(new QnClientCoreModule(QnClientCoreModule::Mode::unitTests));
    m_entityItemModel.reset(new EntityItemModel());
    nx::utils::ModelTransactionChecker::install(m_entityItemModel.get());
}

void EntityItemModelTest::TearDown()
{
    m_entityItemModel.reset();
    m_clientCoreModule.reset();
    m_staticCommonModule.reset();
    m_clientRuntimeSettings.reset();
}

QnCommonModule* EntityItemModelTest::commonModule() const
{
    return m_clientCoreModule->commonModule();
}

QnResourcePool* EntityItemModelTest::resourcePool() const
{
    return commonModule()->resourcePool();
}

EntityItemModelTest::SequentalIntegerGenerator EntityItemModelTest::sequentalIntegerGenerator(
    int startFrom /*= 0*/) const
{
    return [n = startFrom] () mutable { return n++; };
}

EntityItemModelTest::RandomIntegerGenerator EntityItemModelTest::randomIntegerGenerator(
    int min, int max) const
{
    std::srand(unsigned(std::time(nullptr)));
    return [min, max] ()
        {
            if (min == max && min == 0)
                return 0;
            return min + std::rand() % (max - min);
        };
}

EntityItemModelTest::SequentialNameGenerator EntityItemModelTest::sequentialNameGenerator(
    const QString& stringNamePart,
    int startFrom /*= 0*/) const
{
    return
        [stringNamePart, startFrom] () mutable
        {
            return QString("%1_%2").arg(stringNamePart).arg(startFrom++);
        };
}

EntityItemModelTest::RandomNameGenerator EntityItemModelTest::randomNameGenerator(
    const QString& stringNamePart,
    int minIntegerPart,
    int maxIntegerPart) const
{
    std::srand(unsigned(std::time(nullptr)));
    return
        [stringNamePart, minIntegerPart, maxIntegerPart] () mutable
        {
            int range = maxIntegerPart - minIntegerPart;
            if (range == 0)
                return stringNamePart + "_0";
            return QString("%1_%2").arg(stringNamePart).arg(minIntegerPart + std::rand() % range);
        };
}

std::vector<int> EntityItemModelTest::inversePermutation(const std::vector<int>& permutation) const
{
    std::vector<int> result(permutation.size(), 0);
    for (int i = 0; i < permutation.size(); ++i)
        result[permutation[i]] = i;
    return result;
}

} // namespace test
} // namespace entity_item_model
} // namespace nx::vms::client::desktop
