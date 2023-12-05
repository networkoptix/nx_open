// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "entity_item_model_test_fixture.h"

#include <random>

#include <QtTest/QAbstractItemModelTester>

#include <client_core/client_core_module.h>
#include <client/client_runtime_settings.h>
#include <client/client_startup_parameters.h>
#include <common/common_module.h>
#include <common/static_common_module.h>
#include <nx/utils/debug_helpers/model_transaction_checker.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity_item_model.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/system_settings.h>

namespace nx::vms::client::desktop {
namespace entity_item_model {
namespace test {

void EntityItemModelTest::SetUp()
{
    m_entityItemModel.reset(new EntityItemModel());
    nx::utils::ModelTransactionChecker::install(m_entityItemModel.get());

    // Create QAbstractItemModelTester owned by the model.
    new QAbstractItemModelTester(m_entityItemModel.get(),
        QAbstractItemModelTester::FailureReportingMode::Fatal, /*parent*/ m_entityItemModel.get());
}

void EntityItemModelTest::TearDown()
{
    m_entityItemModel.reset();
}

QnResourcePool* EntityItemModelTest::resourcePool() const
{
    return systemContext()->resourcePool();
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
