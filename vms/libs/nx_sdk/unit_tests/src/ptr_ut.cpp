// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <nx/kit/test.h>

#include <nx/sdk/interface.h>
#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/helpers/ptr.h>

namespace nx {
namespace sdk {
namespace test {

namespace {

class IBase: public Interface<IBase, IRefCountable>
{
public:
    static auto interfaceId() { return InterfaceId("nx::sdk::test::*::IBase"); }
};

class IData: public Interface<IData, IBase>
{
public:
    static auto interfaceId() { return InterfaceId("nx::sdk::test::*::IData"); }

    virtual int number() const = 0;
};

class Base: public RefCountable<IBase>
{
public:
    static bool s_destructorCalled;

    Base() = default;

    virtual ~Base() override
    {
        ASSERT_FALSE(s_destructorCalled);
        s_destructorCalled = true;
    }
};
bool Base::s_destructorCalled = false;

class Data: public RefCountable<IData>
{
public:
    static bool s_destructorCalled;

public:
    Data() = default;
    Data(int number): m_number(number) {}

    virtual int number() const override { return m_number; }

    virtual ~Data() override { s_destructorCalled = true; }

private:
    const int m_number = 42;
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

    ASSERT_EQ(expectedRefCount, actual->refCountThreadUnsafe());

    // This is the only correct way to access the ref counter of an arbitrary interface.
    const int increasedRefCount = actual->addRef();
    const int actualRefCount = actual->releaseRef();
    ASSERT_EQ(increasedRefCount, actualRefCount + 1); //< Check test integrity.
    ASSERT_EQ(expectedRefCount, actualRefCount);
}

} // namespace

//-------------------------------------------------------------------------------------------------

TEST(Ptr, null)
{
    const auto nullPtr1 = Ptr<IData>();
    ASSERT_EQ(nullptr, nullPtr1.get());

    // Standalone operator==() and operator!=() with nullptr_t.
    ASSERT_TRUE(nullptr == nullPtr1);
    ASSERT_TRUE(nullPtr1 == nullptr);
    ASSERT_FALSE(nullptr != nullPtr1);
    ASSERT_FALSE(nullPtr1 != nullptr);

    const auto nullPtr2 = Ptr<IData>(nullptr);
    ASSERT_EQ(nullptr, nullPtr2.get());
}

TEST(Ptr, basic)
{
    Data::s_destructorCalled = false;
    {
        const Ptr<Data> data = makePtr<Data>(42);
        ASSERT_EQ(sizeof(Data*), sizeof(data)); //< Ptr layout should be the same as a raw pointer.
        ASSERT_EQ(data->number(), 42);
        ASSERT_EQ(1, data->refCount());
        ASSERT_TRUE(static_cast<bool>(data)); //< operator bool()
        ASSERT_TRUE(data.get() != nullptr); //< get()

        // Standalone operator==() and operator!=() with nullptr_t.
        ASSERT_FALSE(nullptr == data);
        ASSERT_FALSE(data == nullptr);
        ASSERT_TRUE(nullptr != data);
        ASSERT_TRUE(data != nullptr);

        ASSERT_FALSE(Data::s_destructorCalled);
    } //< data destroyed
    ASSERT_TRUE(Data::s_destructorCalled);
}

TEST(Ptr, inheritance)
{
    Data::s_destructorCalled = false;
    {
        const Ptr<Data> data = makePtr<Data>(42);

        const IBase* tmp = data.get();

        Ptr<const IBase> base(toPtr(tmp));
        ASSERT_EQ(1, data->refCount());
        base.releasePtr();

        ASSERT_FALSE(Data::s_destructorCalled);
    } //< data destroyed
    ASSERT_TRUE(Data::s_destructorCalled);
}

TEST(Ptr, toPtr)
{
    Data::s_destructorCalled = false;
    {
        Data* p = new Data(42);
        const Ptr<Data> data = toPtr(p);
        ASSERT_EQ(p, data.get());
        ASSERT_EQ(1, data->refCount());
        ASSERT_FALSE(Data::s_destructorCalled);
    } //< data destroyed via ~Ptr().
    ASSERT_TRUE(Data::s_destructorCalled);
}

TEST(Ptr, assign)
{
    Data::s_destructorCalled = false;
    {
        auto oldData = makePtr<Data>(42);
        ASSERT_EQ(oldData->number(), 42);

        ASSERT_EQ(1, oldData->refCount());
        ASSERT_FALSE(Data::s_destructorCalled);
        {
            Ptr<Data> newData; //< Should be of exactly the same Ptr template instance as oldData.
            ASSERT_EQ(nullptr, newData.get());

            newData = oldData; //< operator=(const)
            assertEq(oldData, newData, /*expectedRefCount*/ 2);

            auto movedData = std::move(newData); //< operator=(&&)
            assertEq(oldData, movedData, /*expectedRefCount*/ 2);
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

TEST(Ptr, queryInterfacePtrNonConst)
{
    Data::s_destructorCalled = false;
    {
        Data* const nullDataPtr = nullptr;
        ASSERT_FALSE(queryInterfacePtr<IData>(nullDataPtr));

        // NOTE: `auto` is not used to ensure proper return types of the tested functions.

        const Ptr<Data> dataPtr = makePtr<Data>(42);

        const Ptr<IData> iDataPtrFromData{queryInterfacePtr<IData>(dataPtr.get())};
        ASSERT_EQ(dataPtr, iDataPtrFromData);

        const Ptr<IData> iDataPtrFromIData{queryInterfacePtr<IData>(dataPtr)};
        ASSERT_EQ(dataPtr, iDataPtrFromIData);
    }
    ASSERT_TRUE(Data::s_destructorCalled);
}

TEST(Ptr, queryInterfacePtrConst)
{
    Data::s_destructorCalled = false;
    {
        const Data* const nullDataPtr = nullptr;
        ASSERT_FALSE(queryInterfacePtr<const IData>(nullDataPtr));

        // NOTE: `auto` is not used to ensure proper return types of the tested functions.

        const Ptr<const Data> dataPtr = makePtr<const Data>(42);

        const Ptr<const IData> iDataPtrFromData{queryInterfacePtr<const IData>(dataPtr.get())};
        ASSERT_EQ(dataPtr, iDataPtrFromData);

        const Ptr<const IData> iDataPtrFromIData{queryInterfacePtr<const IData>(dataPtr)};
        ASSERT_EQ(dataPtr, iDataPtrFromIData);
    }
    ASSERT_TRUE(Data::s_destructorCalled);
}

} // namespace test
} // namespace sdk
} // namespace nx
