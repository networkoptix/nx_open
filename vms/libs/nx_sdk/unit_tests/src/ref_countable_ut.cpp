#include <string>

#include <nx/kit/test.h>

#include <plugins/plugin_api.h>
#include <plugins/plugin_tools.h>
#include <nx/sdk/helpers/ptr.h>

namespace nx {
namespace sdk {
namespace common {
namespace test {

namespace {

class IData: public nxpl::PluginInterface
{
public:
    static const nxpl::NX_GUID& iid()
    {
        static const nxpl::NX_GUID kIid =
            {{0x49,0x32,0x2d,0x6d,0x12,0x81,0x40,0xad,0x81,0xcf,0xed,0x0b,0x1d,0xa6,0x4a,0x06}};
        return kIid;
    }

    virtual int number() const = 0;
};

class Data: public nxpt::CommonRefCounter<IData>
{
public:
    static bool s_destructorCalled;

public:
    Data() = default;
    Data(int number): m_number(number) {}


    void* queryInterface(const nxpl::NX_GUID& interfaceId) override
    {
        if (interfaceId == iid())
            return this;
        if (interfaceId == nxpl::IID_PluginInterface)
            return static_cast<PluginInterface*>(this);
        return nullptr;
    }

    virtual int number() const override { return m_number; }

    virtual ~Data() override { s_destructorCalled = true; }

private:
    int m_number = 42;
};
bool Data::s_destructorCalled = false;

template<typename ExpectedRefCountable, typename ActualRefCountable>
void assertEq(
    const Ptr<ExpectedRefCountable>& expected,
    const Ptr<ActualRefCountable>& actual,
    int expectedRefCount)
{
    ASSERT_EQ(expected.get(), actual.get());
    ASSERT_TRUE(expected == actual); //< operator==()
    ASSERT_FALSE(expected != actual); //< operator==()

    // This is the only correct way to access the ref counter of an arbitrary interface.
    const int increasedRefCount = actual->addRef();
    const int actualRefCount = actual->releaseRef();
    ASSERT_EQ(increasedRefCount, actualRefCount + 1); //< Just in case.
    ASSERT_EQ(expectedRefCount, actualRefCount);
}

} // namespace

//-------------------------------------------------------------------------------------------------

TEST(RefCountable, simple)
{
    Data::s_destructorCalled = false;

    Data* data = new Data(113);
    ASSERT_EQ(data->number(), 113);
    ASSERT_EQ(1, data->refCount());

    data->addRef();
    ASSERT_EQ(2, data->refCount());

    data->releaseRef();
    ASSERT_EQ(1, data->refCount());

    data->releaseRef();
    ASSERT_TRUE(Data::s_destructorCalled);
}

// TODO: #mshevchenko: Add more tests.

} // namespace test
} // namespace common
} // namespace sdk
} // namespace nx
