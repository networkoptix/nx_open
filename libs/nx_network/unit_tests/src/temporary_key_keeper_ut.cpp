#include <gtest/gtest.h>

#include <nx/network/temporary_key_keeper.h>

namespace nx {
namespace network {
namespace test {

static std::chrono::milliseconds kTimeout(500);

static nx::String kKey1("k1");
static nx::String kKey2("k2");
static nx::String kKey3("k3");
static nx::String kKey4("k4");

static std::optional<nx::String> kUser1("u1");
static std::optional<nx::String> kUser2("u2");
static std::optional<nx::String> kUser3("u3");
static std::optional<nx::String> kUser4("u4");

TEST(TemporaryKeyKeeper, MakeExpiration)
{
    TemporaryKeyKeeper<nx::String> keeper({kTimeout, /*prolongLifeOnUse*/ false});
    ASSERT_EQ(0, keeper.size());

    const auto key0 = QnUuid::createUuid().toSimpleByteArray();
    ASSERT_FALSE((bool) keeper.get(key0));

    const auto key1 = keeper.make(*kUser1);
    ASSERT_EQ(1, keeper.size());
    ASSERT_FALSE((bool) keeper.get(key0));
    ASSERT_EQ(kUser1, keeper.get(key1));

    std::this_thread::sleep_for(kTimeout / 2);
    ASSERT_EQ(1, keeper.size());
    ASSERT_FALSE((bool) keeper.get(key0));
    ASSERT_EQ(kUser1, keeper.get(key1)); //< Still accesible.

    std::this_thread::sleep_for(kTimeout / 2);
    ASSERT_EQ(0, keeper.size());
    ASSERT_FALSE((bool) keeper.get(key0));
    ASSERT_FALSE((bool) keeper.get(key1)); //< Expired.
}

TEST(TemporaryKeyKeeper, AddExpiration)
{
    TemporaryKeyKeeper<nx::String> keeper({kTimeout, /*prolongLifeOnUse*/ false});
    const auto key = QnUuid::createUuid().toSimpleByteArray();
    ASSERT_FALSE((bool) keeper.get(key));

    ASSERT_TRUE(keeper.addNew(key, *kUser1));
    ASSERT_EQ(kUser1, keeper.get(key));

    std::this_thread::sleep_for(kTimeout / 2);
    ASSERT_FALSE(keeper.addNew(key, *kUser2));
    ASSERT_EQ(kUser1, keeper.get(key));
    ASSERT_EQ(1, keeper.size());

    std::this_thread::sleep_for(kTimeout / 2);
    ASSERT_FALSE((bool) keeper.get(key)); //< Timer was not updated, key is expired.
    ASSERT_EQ(0, keeper.size());

    ASSERT_TRUE(keeper.addNew(key, *kUser3));
    ASSERT_EQ(kUser3, keeper.get(key));
    ASSERT_EQ(1, keeper.size());

    std::this_thread::sleep_for(kTimeout / 2);
    keeper.addOrUpdate(key, *kUser4);
    ASSERT_EQ(kUser4, keeper.get(key));
    ASSERT_EQ(1, keeper.size());

    std::this_thread::sleep_for(kTimeout / 2);
    ASSERT_EQ(kUser4, keeper.get(key));
    ASSERT_EQ(1, keeper.size());

    std::this_thread::sleep_for(kTimeout / 2);
    ASSERT_FALSE((bool) keeper.get(key)); //< Expired now.
    ASSERT_EQ(0, keeper.size());
}

TEST(TemporaryKeyKeeper, Prolongation)
{
    TemporaryKeyKeeper<nx::String> keeper({kTimeout, /*prolongLifeOnUse*/ true});

    const auto key1 = keeper.make(*kUser1);
    const auto key2 = keeper.make(*kUser2);
    ASSERT_EQ(kUser1, keeper.get(key1));
    ASSERT_EQ(kUser2, keeper.get(key2));
    ASSERT_EQ(2, keeper.size());

    std::this_thread::sleep_for(kTimeout / 2);
    ASSERT_EQ(kUser1, keeper.get(key1)); //< Prolong.
    ASSERT_EQ(2, keeper.size());

    std::this_thread::sleep_for(kTimeout / 2);
    ASSERT_EQ(kUser1, keeper.get(key1)); //< Prolonged.
    ASSERT_FALSE((bool) keeper.get(key2)); //< Expired.
    ASSERT_EQ(1, keeper.size());

    std::this_thread::sleep_for(kTimeout);
    ASSERT_FALSE((bool) keeper.get(key1)); //< Both expired anyway.
    ASSERT_FALSE((bool) keeper.get(key2));
    ASSERT_EQ(0, keeper.size());
}

TEST(TemporaryKeyKeeper, Options)
{
    TemporaryKeyKeeper<nx::String> keeper({kTimeout, /*prolongLifeOnUse*/ false});
    const auto key1 = keeper.make(*kUser1);
    const auto key2 = keeper.make(*kUser2);

    std::this_thread::sleep_for(kTimeout / 2);
    ASSERT_EQ(kUser1, keeper.get(key1)); //< Still accesible.
    ASSERT_EQ(kUser2, keeper.get(key2));

    keeper.setOptions(TemporaryKeyOptions{kTimeout * 2, /*prolongLifeOnUse*/ false});
    std::this_thread::sleep_for(kTimeout / 2);
    ASSERT_EQ(kUser1, keeper.get(key1)); //< Still accesible with new life time.
    ASSERT_EQ(kUser2, keeper.get(key2));

    keeper.setOptions(TemporaryKeyOptions{kTimeout * 2, /*prolongLifeOnUse*/ true});
    ASSERT_EQ(kUser1, keeper.get(key1)); //< Prolong.

    std::this_thread::sleep_for(kTimeout);
    ASSERT_EQ(kUser1, keeper.get(key1)); //< Prolonged.
    ASSERT_FALSE((bool) keeper.get(key2)); //< Expired.
}

TEST(TemporaryKeyKeeper, Remove)
{
    TemporaryKeyKeeper<nx::String> keeper({kTimeout, /*prolongLifeOnUse*/ false});
    const auto key1 = keeper.make(*kUser1);
    const auto key2 = keeper.make(*kUser2);
    const auto key3 = keeper.make(*kUser3);
    const auto key4 = keeper.make(*kUser3);
    const auto key5 = keeper.make(*kUser4);

    ASSERT_EQ(5, keeper.size());
    ASSERT_EQ(kUser1, keeper.get(key1));
    ASSERT_EQ(kUser2, keeper.get(key2));
    ASSERT_EQ(kUser3, keeper.get(key3));
    ASSERT_EQ(kUser3, keeper.get(key4));
    ASSERT_EQ(kUser4, keeper.get(key5));

    keeper.remove(key2);

    ASSERT_EQ(4, keeper.size());
    ASSERT_EQ(kUser1, keeper.get(key1));
    ASSERT_FALSE((bool) keeper.get(key2));
    ASSERT_EQ(kUser3, keeper.get(key3));
    ASSERT_EQ(kUser3, keeper.get(key4));
    ASSERT_EQ(kUser4, keeper.get(key5));

    NX_DEBUG(this, lm("Remove user %1").arg(*kUser3));
    keeper.removeIf([](auto /*key*/, auto user, auto /*binding*/) { return user == *kUser3; });

    ASSERT_EQ(2, keeper.size());
    ASSERT_EQ(kUser1, keeper.get(key1));
    ASSERT_FALSE((bool) keeper.get(key2));
    ASSERT_FALSE((bool) keeper.get(key3)); // Both keys for the same user are removed.
    ASSERT_FALSE((bool) keeper.get(key4));
    ASSERT_EQ(kUser4, keeper.get(key5));

    std::this_thread::sleep_for(kTimeout);
    ASSERT_EQ(0, keeper.size());
}

static nx::String kBinding1("b1");
static nx::String kBinding2("b2");
static nx::String kBinding3("b3");

static void testBindings(TemporaryKeyKeeper<nx::String>* keeper)
{
    keeper->addOrUpdate(kKey1, *kUser1, kBinding1);
    keeper->addOrUpdate(kKey2, *kUser2, kBinding2);
    keeper->addOrUpdate(kKey3, *kUser3, kBinding2);
    keeper->addOrUpdate(kKey4, *kUser3, kBinding3);

    ASSERT_EQ(4, keeper->size());
    ASSERT_EQ(kUser1, keeper->get(kKey1, kBinding1));
    ASSERT_EQ(kUser2, keeper->get(kKey2, kBinding2));
    ASSERT_EQ(kUser3, keeper->get(kKey3, kBinding2));
    ASSERT_EQ(kUser3, keeper->get(kKey4, kBinding3));
    ASSERT_FALSE((bool) keeper->get(kKey1, kBinding2)); // Wrong bindings.
    ASSERT_FALSE((bool) keeper->get(kKey2, kBinding3));
    ASSERT_FALSE((bool) keeper->get(kKey3, kBinding1));
    ASSERT_FALSE((bool) keeper->get(kKey4, kBinding1));
}

TEST(TemporaryKeyKeeper, BindingUpdate)
{
    TemporaryKeyKeeper<nx::String> keeper({kTimeout, /*prolongLifeOnUse*/ false});
    testBindings(&keeper);

    keeper.addOrUpdate(kKey1, *kUser2, kBinding1);
    keeper.addOrUpdate(kKey4, *kUser3, kBinding1);
    ASSERT_EQ(4, keeper.size());

    ASSERT_EQ(kUser2, keeper.get(kKey2, kBinding2)); // Old values.
    ASSERT_EQ(kUser3, keeper.get(kKey3, kBinding2));

    ASSERT_EQ(kUser2, keeper.get(kKey1, kBinding1)); // Updated values.
    ASSERT_FALSE((bool) keeper.get(kKey4, kBinding3));
    ASSERT_EQ(kUser2, keeper.get(kKey1, kBinding1));
}

TEST(TemporaryKeyKeeper, BindingRemoveByValue)
{
    TemporaryKeyKeeper<nx::String> keeper({kTimeout, /*prolongLifeOnUse*/ false});
    testBindings(&keeper);

    keeper.removeIf([](auto /*key*/, auto user, auto /*binding*/) { return user == *kUser3; });
    ASSERT_EQ(2, keeper.size());

    ASSERT_EQ(kUser1, keeper.get(kKey1, kBinding1)); //< Still OK.
    ASSERT_EQ(kUser2, keeper.get(kKey2, kBinding2));
}

TEST(TemporaryKeyKeeper, BindingRemoveByBinding)
{
    TemporaryKeyKeeper<nx::String> keeper({kTimeout, /*prolongLifeOnUse*/ false});
    testBindings(&keeper);

    keeper.removeIf([](auto /*key*/, auto /*user*/, auto binding) { return binding == kBinding2; });
    ASSERT_EQ(2, keeper.size());

    ASSERT_EQ(kUser1, keeper.get(kKey1, kBinding1)); //< Still OK.
    ASSERT_EQ(kUser3, keeper.get(kKey4, kBinding3));
}

TEST(TemporaryKeyKeeper, BindingExpiration)
{
    TemporaryKeyKeeper<nx::String> keeper({kTimeout, /*prolongLifeOnUse*/ true});
    testBindings(&keeper);

    std::this_thread::sleep_for(kTimeout / 2);
    ASSERT_EQ(kUser1, keeper.get(kKey1, kBinding1));
    ASSERT_EQ(kUser2, keeper.get(kKey2, kBinding2));

    std::this_thread::sleep_for(kTimeout / 2);
    ASSERT_EQ(2, keeper.size()); //< Only 2 prolonged.
    ASSERT_TRUE(keeper.addNew(kKey3, *kUser1, kBinding1));

    std::this_thread::sleep_for(kTimeout / 2);
    ASSERT_EQ(1, keeper.size()); //< Only 1 prolonged.
    ASSERT_EQ(kUser1, keeper.get(kKey3, kBinding1));

    std::this_thread::sleep_for(kTimeout);
    ASSERT_EQ(0, keeper.size());
}

} // namespace test
} // namespace network
} // namespace nx
