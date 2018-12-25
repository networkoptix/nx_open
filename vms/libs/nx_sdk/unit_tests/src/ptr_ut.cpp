#include <nx/kit/test.h>

#include <map>
#include <string>

#include <plugins/plugin_api.h>
#include <plugins/plugin_tools.h>
#include <nx/sdk/common/ptr.h>

namespace nx {
namespace sdk {
namespace common {
namespace test {

namespace {

class IBase: public nxpl::PluginInterface
{
public:
    static const nxpl::NX_GUID& iid()
    {
        static const nxpl::NX_GUID kIid =
            {{0x58,0x41,0x2e,0x63,0x12,0x31,0x40,0xbd,0x11,0xce,0xfd,0x0b,0x2d,0xa5,0x01,0x02}};
        return kIid;
    }
};

class IData: public IBase
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

class Base: public nxpt::CommonRefCounter<IBase>
{
public:
    static bool s_destructorCalled;

public:
    Base() = default;

    void* queryInterface(const nxpl::NX_GUID& interfaceId) override
    {
        if (interfaceId == iid())
            return this;
        if (interfaceId == nxpl::IID_PluginInterface)
            return static_cast<PluginInterface*>(this);
        return nullptr;
    }

    virtual ~Base() override { s_destructorCalled = true; }

private:
    int m_number = 42;
};
bool Base::s_destructorCalled = false;

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
        if (interfaceId == IBase::iid())
            return static_cast<IBase*>(this);
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
    ASSERT_FALSE(expected != actual); //< operator!=()

    // This is the only correct way to access the ref counter of an arbitrary interface.
    const int increasedRefCount = actual->addRef();
    const int actualRefCount = actual->releaseRef();
    ASSERT_EQ(increasedRefCount, actualRefCount + 1); //< Just in case.
    ASSERT_EQ(expectedRefCount, actualRefCount);
}

} // namespace

//-------------------------------------------------------------------------------------------------

TEST(Ptr, null)
{
    const auto nullPtr1 = Ptr<IData>();
    ASSERT_EQ(nullptr, nullPtr1.get());

    const auto nullPtr2 = Ptr<IData>(nullptr);
    ASSERT_EQ(nullptr, nullPtr2.get());
}

TEST(Ptr, basic)
{
    Data::s_destructorCalled = false;
    {
        const auto data = makePtr<Data>(113);
        ASSERT_EQ(data->number(), 113);
        ASSERT_EQ(1, data->refCount());
        ASSERT_TRUE(static_cast<bool>(data)); //< operator bool()
        ASSERT_TRUE(data.get() != nullptr); //< get()
        ASSERT_FALSE(Data::s_destructorCalled);
    } //< data destroyed
    ASSERT_TRUE(Data::s_destructorCalled);
}

TEST(Ptr, assign)
{
    Data::s_destructorCalled = false;
    {
        auto oldData = makePtr<Data>(113);
        ASSERT_EQ(oldData->number(), 113);

        ASSERT_EQ(1, oldData->refCount());
        ASSERT_FALSE(Data::s_destructorCalled);
        {
            Ptr<Data> newData; //< Should be of exactly the same Ptr template instance.
            ASSERT_EQ(nullptr, newData.get());

            newData = oldData; //< operator=(const)
            assertEq(oldData, newData, /*expectedRefCount*/ 2);

            newData = std::move(oldData); //< operator=(&&)
            assertEq(oldData, newData, /*expectedRefCount*/ 2);
        }
        ASSERT_FALSE(Data::s_destructorCalled);
        ASSERT_EQ(1, oldData->refCount());

        oldData = oldData; //< Assign to itself.
        ASSERT_FALSE(Data::s_destructorCalled);
        ASSERT_EQ(1, oldData->refCount());
        {
            Ptr<Data> newData(oldData); //< Copy-constructor.
            assertEq(oldData, newData, /*expectedRefCount*/ 2);

            newData = oldData; //< Assign another pointer to the same object.
            assertEq(oldData, newData, /*expectedRefCount*/ 2);
        }
        ASSERT_FALSE(Data::s_destructorCalled);
        ASSERT_EQ(1, oldData->refCount());
    }
    ASSERT_TRUE(Data::s_destructorCalled);
}

TEST(Ptr, dynamicCast)
{
    Data::s_destructorCalled = false;
    Base::s_destructorCalled = false;
    {
        const auto data = makePtr<Data>(113);
        ASSERT_EQ(data->number(), 113);
        ASSERT_EQ(1, data->refCount());

        const auto base = makePtr<Base>();
        ASSERT_EQ(1, base->refCount());

        const Ptr<IBase> dataAsIBase(data);
        ASSERT_EQ(2, data->refCount());

        {
            const Ptr<IBase> cast = dataAsIBase.dynamicCast<IBase>(); //< Cast to itself.
            ASSERT_TRUE(cast);
            assertEq(dataAsIBase, cast, /*expectedRefCount*/ 3);
        }
        ASSERT_EQ(2, data->refCount());

        {
            const Ptr<nxpl::PluginInterface> cast = data.dynamicCast<IBase>(); //< Cast to base.
            ASSERT_TRUE(cast);
            ASSERT_EQ(3, data->refCount());
        }
        ASSERT_EQ(2, data->refCount());

        {
            const Ptr<IData> cast = dataAsIBase.dynamicCast<IData>(); //< Cast to derived.
            ASSERT_TRUE(cast);
            ASSERT_EQ(3, data->refCount());
        }
        ASSERT_EQ(2, data->refCount());

        ASSERT_FALSE(Data::s_destructorCalled);
        ASSERT_FALSE(Base::s_destructorCalled);
    }
    ASSERT_TRUE(Data::s_destructorCalled);
    ASSERT_TRUE(Base::s_destructorCalled);
}

TEST(Ptr, releasePtrAndReset)
{
    auto data1 = makePtr<Data>(1);
    auto data2 = makePtr<Data>(2);

    Data::s_destructorCalled = false;
    data2.reset(data1.releasePtr()); //< reset(non-null) and releasePtr()
    ASSERT_TRUE(Data::s_destructorCalled); //< Data(2) deleted
    Data::s_destructorCalled = false;
    ASSERT_EQ(nullptr, data1.get());
    ASSERT_EQ(data2->number(), 1);

    data2.reset(); //< reset()
    ASSERT_TRUE(Data::s_destructorCalled); //< Data(1) deleted
    ASSERT_EQ(nullptr, data2.get());
}

} // namespace test
} // namespace common
} // namespace sdk
} // namespace nx
