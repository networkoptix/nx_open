// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <gtest/gtest.h>

#include <QtCore/QSharedPointer>

class QnResourcePool;

namespace nx::vms::client::core::entity_item_model { class EntityItemModel; }

namespace nx::vms::client::core {
namespace entity_item_model {
namespace test {

class EntityItemModelTest: public testing::Test
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
} // namespace nx::vms::client::core
