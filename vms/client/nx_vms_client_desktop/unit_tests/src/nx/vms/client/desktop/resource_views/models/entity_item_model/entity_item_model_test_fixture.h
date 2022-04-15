// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSharedPointer>

#include <gtest/gtest.h>

#include <nx/vms/client/desktop/test_support/test_context.h>

class QnResourcePool;

namespace nx::vms::client::desktop::entity_item_model { class EntityItemModel; }

namespace nx::vms::client::desktop {
namespace entity_item_model {
namespace test {

class EntityItemModelTest: public nx::vms::client::desktop::test::ContextBasedTest
{
protected:
    virtual void SetUp() override;
    virtual void TearDown() override;

    QnResourcePool* resourcePool() const;

    using SequentalIntegerGenerator = std::function<int()>;
    SequentalIntegerGenerator sequentalIntegerGenerator(int startFrom = 0) const;

    using RandomIntegerGenerator = std::function<int()>;
    RandomIntegerGenerator randomIntegerGenerator(int min, int max) const;

    using SequentialNameGenerator = std::function<QString()>;
    SequentialNameGenerator sequentialNameGenerator(
        const QString& stringNamePart,
        int startFrom = 0) const;

    using RandomNameGenerator = std::function<QString()>;
    RandomNameGenerator randomNameGenerator(
        const QString& stringNamePart,
        int minIntegerPart,
        int maxIntegerPart) const;

    std::vector<int> inversePermutation(const std::vector<int>& permutation) const;

protected:
    QSharedPointer<EntityItemModel> m_entityItemModel;
};

} // namespace test
} // entity_item_model
} // namespace nx::vms::client::desktop
