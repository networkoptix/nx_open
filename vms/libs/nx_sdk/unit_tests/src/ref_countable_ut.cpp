#include <string>
#include <memory>

#include <nx/kit/test.h>
#include <nx/kit/debug.h>

#include <nx/sdk/interface.h>
#include <nx/sdk/helpers/ref_countable.h>

namespace nx {
namespace sdk {
namespace test {

static constexpr int kOldInterfaceIdSize = 16;

//-------------------------------------------------------------------------------------------------
// TEST_QUERY_INTERFACE macro.

namespace {

/**
 * Supports both old SDK and current SDK objects/interfaces. Assumes that the initial object's
 * refCount is 1. The object should implement methods:
 * ```
 * int refCountForTest() const;
 * void* interfaceIdForTest() const; //< Should return object's immediate interface.
 * ```
 */
#define TEST_QUERY_INTERFACE(EXPECTED_RESULT, OBJECT_PTR, INTERFACE_ID) \
    testQueryInterface(__LINE__, \
        ExpectedQueryInterfaceResult::EXPECTED_RESULT, OBJECT_PTR, INTERFACE_ID)

enum class ExpectedQueryInterfaceResult {null, nonNull};

/**
 * Calls queryInterface() on an object, asserting the initial reference count is 1, and that the
 * same object pointer is returned on success.
 */
template<typename ObjectPtr, typename InterfaceId>
void testQueryInterface(
    int line,
    ExpectedQueryInterfaceResult expectedQueryInterfaceResult,
    ObjectPtr objectPtr,
    InterfaceId interfaceId)
{
    ASSERT_TRUE_AT_LINE(line, objectPtr);
    ASSERT_EQ_AT_LINE(line, 1, objectPtr->refCountForTest());

    const auto ptr = (void*) objectPtr->queryInterface(interfaceId);

    const auto printDetails =
        [interfaceId, objectPtr]()
        {
            // Both old and new interface ids are supported: for new ids, print the first 16 chars.
            NX_PRINT_HEX_DUMP("Requested interfaceId",
                interfaceId.value, kOldInterfaceIdSize);
            NX_PRINT_HEX_DUMP("Object's interfaceId",
                objectPtr->interfaceIdForTest(), kOldInterfaceIdSize);
        };

    switch (expectedQueryInterfaceResult)
    {
        case ExpectedQueryInterfaceResult::null:
            if (ptr) //< Test will fail: print detailed diagnostics.
            {
                ASSERT_EQ_AT_LINE(line, (void*) objectPtr, ptr); //< Fail here if ptr is bad.
                printDetails();
                ASSERT_FALSE_AT_LINE(line, ptr); //< Fail the test.
            }
            ASSERT_EQ_AT_LINE(line, 1, objectPtr->refCountForTest());
            break;

        case ExpectedQueryInterfaceResult::nonNull:
            if (ptr != (void*) objectPtr) //< Test will fail: print detailed diagnostics.
            {
                ASSERT_EQ_AT_LINE(line, nullptr, ptr); //< Fail here if ptr is bad.
                printDetails();
                ASSERT_EQ_AT_LINE(line, (void*) objectPtr, ptr); //< Fail the test.
            }
            ASSERT_EQ_AT_LINE(line, 2, objectPtr->refCountForTest());
            ASSERT_EQ_AT_LINE(line, 1, (int) objectPtr->releaseRef()); //< Cast for old interfaces.
            ASSERT_EQ_AT_LINE(line, 1, objectPtr->refCountForTest());
            break;
    }
}

} // namespace

//-------------------------------------------------------------------------------------------------

TEST(RefCountable, interfaceId)
{
    constexpr char kInterfaceIdStr[] = "arbitrary string not shorter than 15 chars";
    const IRefCountable::InterfaceId interfaceId(kInterfaceIdStr);
    ASSERT_STREQ(kInterfaceIdStr, interfaceId.value);
}

//-------------------------------------------------------------------------------------------------
// Test classes IRefCountable and RefCountable.

namespace {

class IBase: public Interface<IBase, IRefCountable>
{
public:
    static auto interfaceId() { return InterfaceId("nx::sdk::test::IBase"); }
    virtual void* interfaceIdForTest() const = 0;
    virtual int refCountForTest() const = 0;
};

class IData: public Interface<IData, IBase>
{
public:
    static auto interfaceId() { return InterfaceId("nx::sdk::test::IData"); }
};

class Data: public RefCountable<IData>
{
public:
    static bool s_destructorCalled;

    Data() = default;

    virtual ~Data() override
    {
        ASSERT_FALSE(s_destructorCalled);
        s_destructorCalled = true;
    }

    virtual void* interfaceIdForTest() const override { return (void*) interfaceId().value; }
    virtual int refCountForTest() const override { return refCount(); }
};
bool Data::s_destructorCalled = false;

} // namespace

TEST(RefCountable, refCountableBasics)
{
    Data::s_destructorCalled = false;

    auto data = new Data;
    ASSERT_EQ(1, data->refCount());

    ASSERT_EQ(2, data->addRef());
    ASSERT_EQ(2, data->refCount());

    ASSERT_EQ(1, data->releaseRef());
    ASSERT_FALSE(Data::s_destructorCalled);
    ASSERT_EQ(1, data->refCount());

    ASSERT_EQ(0, data->releaseRef());
    ASSERT_TRUE(Data::s_destructorCalled);
}

TEST(RefCountable, queryInterfaceNonConst)
{
    Data::s_destructorCalled = false;
    Data* const data = new Data;

    TEST_QUERY_INTERFACE(nonNull, data, IData::interfaceId());
    IBase* const iIntermediate = data;
    TEST_QUERY_INTERFACE(nonNull, iIntermediate, IData::interfaceId());
    IData* const iData = data;
    TEST_QUERY_INTERFACE(nonNull, iData, IData::interfaceId());

    // Test template version: queryInterface<Interface>().
    IData* const iDataViaTemplate = data->queryInterface<IData>();
    ASSERT_EQ(iDataViaTemplate, iData);
    ASSERT_EQ(1, iDataViaTemplate->releaseRef());

    ASSERT_EQ(0, data->releaseRef());
    ASSERT_TRUE(Data::s_destructorCalled);
}

TEST(RefCountable, queryInterfaceConst)
{
    Data::s_destructorCalled = false;
    const Data* const data = new Data;

    TEST_QUERY_INTERFACE(nonNull, data, IData::interfaceId());
    const IBase* const iIntermediate = data;
    TEST_QUERY_INTERFACE(nonNull, iIntermediate, IData::interfaceId());
    const IData* const iData = data;
    TEST_QUERY_INTERFACE(nonNull, iData, IData::interfaceId());

    // Test template version: queryInterface<Interface>().
    const IData* const iDataViaTemplate = data->queryInterface<IData>();
    ASSERT_EQ(iDataViaTemplate, iData);
    ASSERT_EQ(1, iDataViaTemplate->releaseRef());

    ASSERT_EQ(0, data->releaseRef());
    ASSERT_TRUE(Data::s_destructorCalled);
}

//-------------------------------------------------------------------------------------------------
// Test the binary compatibility with the old SDK.

namespace {

#define NEW_INTERFACE_ID "new_interfaceId" //< Has the minimum allowed length: 15 chars.

/**
 * Mock for a base interface from the old SDK - used for compatibility testing.
 *
 * Has the VMT layout starting with all the entries from IRefCountable (and thus as in old SDK).
 */
class OldInterface
{
public:
    /** Defined exactly the same way as in the old SDK (NX_GUID). */
    struct InterfaceId
    {
        unsigned char value[kOldInterfaceIdSize];

        explicit InterfaceId(const char (&charArray)[kOldInterfaceIdSize])
        {
            // All old interface ids in this test have last byte equal to 0 for easy conversion to
            // the new interface id.
            ASSERT_EQ(0, value[kOldInterfaceIdSize - 1]);
            std::copy(charArray, charArray + kOldInterfaceIdSize, value);
        }

        IRefCountable::InterfaceId asNew() const
        {
            ASSERT_EQ(0, value[kOldInterfaceIdSize - 1]);
            return IRefCountable::InterfaceId((const char (&)[kOldInterfaceIdSize]) value);
        }
    };

    static const InterfaceId& interfaceId()
    {
        static InterfaceId id{"old_interfaceId"}; //< 15 chars, 16 bytes.
        return id;
    }

    /** VMT #0. */
    virtual ~OldInterface() = default;

    /** #1 in VMT. */
    virtual void* queryInterface(const InterfaceId& id) = 0;

    /** VMT #2. */
    virtual unsigned int addRef() const = 0;

    /** VMT #3. */
    virtual unsigned int releaseRef() const = 0;

    /** VMT #4 - added for tests; couples with the same method of NewInterface. */
    virtual void* interfaceIdForTest() { return (void*) interfaceId().value; }

    /** VMT #5 - added for tests; couples with the same method of NewInterface. */
    virtual int refCountForTest() const = 0;
};

/** Mock for an object implementing the old SDK interface - used for compatibility testing. */
class OldObject: public OldInterface
{
public:
    static bool s_destructorCalled;

    OldObject() = default;

    OldObject(const OldObject&) = delete;
    OldObject& operator=(const OldObject&) = delete;
    OldObject(OldObject&&) = delete;
    OldObject& operator=(OldObject&&) = delete;

    virtual ~OldObject() override
    {
        ASSERT_FALSE(s_destructorCalled);
        s_destructorCalled = true;
    }

    virtual void* queryInterface(const InterfaceId& id) override
    {
        // Compare using memcmp(), as the old-SDK-based VMS and plugins both do.
        if (memcmp(interfaceId().value, id.value, kOldInterfaceIdSize) == 0)
        {
            addRef();
            return (void*) this;
        }
        return nullptr;
    }

    virtual unsigned int addRef() const override
    {
        ASSERT_TRUE(m_refCount > 0);
        return ++m_refCount;
    }

    virtual unsigned int releaseRef() const override
    {
        ASSERT_TRUE(m_refCount > 0);
        const int newRefCount = --m_refCount;
        if (m_refCount == 0)
            delete this;
        return newRefCount;
    }

    virtual int refCountForTest() const override { return m_refCount; }

private:
    mutable int m_refCount = 1;
};
bool OldObject::s_destructorCalled = false;

/** Mock for a base interface from the new SDK - used for compatibility testing. */
class NewInterface: public Interface<NewInterface, IRefCountable>
{
public:
    static auto interfaceId() { return InterfaceId(NEW_INTERFACE_ID); }

    /** Support old SDK interface id. */
    virtual IRefCountable* queryInterface(InterfaceId id) override
    {
        ASSERT_TRUE(id.value != nullptr);
        ASSERT_TRUE(interfaceId().value != nullptr);

        return queryInterfaceSupportingDeprecatedId(id, Uuid(OldInterface::interfaceId().value));
    }

    /** VMT #4 - added for tests; couples with the same method of OldInterface. */
    virtual void* interfaceIdForTest() const { return (void*) interfaceId().value; }

    /** VMT #5 - added for tests; couples with the same method of OldInterface. */
    virtual int refCountForTest() const = 0;
};

/** Mock for an object implementing the current SDK interface - used for compatibility testing. */
class NewObject: public RefCountable<NewInterface>
{
public:
    static bool s_destructorCalled;

    virtual ~NewObject() override
    {
        ASSERT_FALSE(s_destructorCalled);
        s_destructorCalled = true;
    }

    virtual int refCountForTest() const override { return refCount(); }
};
bool NewObject::s_destructorCalled = false;

} // namespace

/**
 * Test that InterfaceId is compatible with the old SDK interface identification type - needed to
 * guarantee binary compatibility with plugins compiled using the old SDK.
 */
TEST(RefCountable, binaryCompatibilityWithOldSdk)
{
    NewObject::s_destructorCalled = false;
    OldObject::s_destructorCalled = false;

    // Test integrity of the test.
    ASSERT_EQ(kOldInterfaceIdSize, (int) sizeof(OldInterface::InterfaceId));

    // Test that queryInterface() argument has the same size in the new and old SDK.
    ASSERT_EQ(sizeof(IRefCountable::InterfaceId), sizeof(const OldObject::InterfaceId*));

    const auto newObject = new NewObject;
    ASSERT_EQ(1, newObject->refCountForTest());

    const auto oldObject = new OldObject;
    ASSERT_EQ(1, oldObject->refCountForTest());

    const auto newObjectAsOldInterface = reinterpret_cast<OldInterface*>(newObject);
    const auto oldObjectAsNewInterface = reinterpret_cast<NewInterface*>(oldObject);

    const IRefCountable::InterfaceId otherNewInterfaceId{NEW_INTERFACE_ID "_WITH_SUFFIX"};
    const OldObject::InterfaceId otherOldInterfaceId{"old2interfaceId"}; //< 15 chars, 16 bytes.

    // Test the regular queryInterface() behavior in the new SDK.
    TEST_QUERY_INTERFACE(nonNull, newObject, NewObject::interfaceId());
    TEST_QUERY_INTERFACE(null, newObject, otherNewInterfaceId);

    // Mock an old plugin calling queryInterface() on an object from the new VMS.
    TEST_QUERY_INTERFACE(nonNull, newObjectAsOldInterface, OldInterface::interfaceId());
    TEST_QUERY_INTERFACE(null, newObjectAsOldInterface, otherOldInterfaceId);

    // Test the regular queryInterface() behavior of the old SDK.
    TEST_QUERY_INTERFACE(nonNull, oldObject, OldInterface::interfaceId());
    TEST_QUERY_INTERFACE(null, oldObject, otherOldInterfaceId);

    // Mock a new VMS calling queryInterface() on an object from the old plugin.
    TEST_QUERY_INTERFACE(null, oldObjectAsNewInterface, NewInterface::interfaceId());
    TEST_QUERY_INTERFACE(nonNull, oldObjectAsNewInterface, OldObject::interfaceId().asNew());
    TEST_QUERY_INTERFACE(null, oldObjectAsNewInterface, otherOldInterfaceId.asNew());
    TEST_QUERY_INTERFACE(null, oldObjectAsNewInterface, otherNewInterfaceId);

    oldObject->releaseRef();
    ASSERT_TRUE(OldObject::s_destructorCalled);
    newObject->releaseRef();
    ASSERT_TRUE(NewObject::s_destructorCalled);
}

} // namespace test
} // namespace sdk
} // namespace nx
