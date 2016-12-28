#include <gtest/gtest.h>

#include <nx/utils/uuid.h>

#include <utils/common/counter_hash.h>

namespace {

static const QnUuid kId = QnUuid::createUuid();

}

class QnCounterHashTest: public testing::Test
{
protected:

    // virtual void SetUp() will be called before each test is run.
    virtual void SetUp()
    {
        hash.reset(new QnCounterHash<QnUuid>());
    }

    // virtual void TearDown() will be called after each test is run.
    virtual void TearDown()
    {
        hash.reset();
    }

    QScopedPointer<QnCounterHash<QnUuid>> hash;
};

TEST_F(QnCounterHashTest, checkInit)
{
    ASSERT_TRUE(hash->keys().isEmpty());
}

TEST_F(QnCounterHashTest, addOnce)
{
    hash->insert(kId);
    ASSERT_TRUE(hash->contains(kId));
}

TEST_F(QnCounterHashTest, addRemoveOnce)
{
    hash->insert(kId);
    hash->remove(kId);
    ASSERT_FALSE(hash->contains(kId));
}

TEST_F(QnCounterHashTest, addTwiceRemoveOnce)
{
    hash->insert(kId);
    hash->insert(kId);
    hash->remove(kId);
    ASSERT_TRUE(hash->contains(kId));
}

TEST_F(QnCounterHashTest, addRemoveSeveralTimes)
{
    const int N = 255;

    for (int i = 0; i < N; ++i)
        hash->insert(kId);

    for (int i = 0; i < N; ++i)
        hash->remove(kId);

    ASSERT_FALSE(hash->contains(kId));
}

TEST_F(QnCounterHashTest, addRemoveOnceSeveralTimes)
{
    const int N = 255;

    for (int i = 0; i < N; ++i)
    {
        hash->insert(kId);
        hash->remove(kId);
    }

    ASSERT_FALSE(hash->contains(kId));
}
