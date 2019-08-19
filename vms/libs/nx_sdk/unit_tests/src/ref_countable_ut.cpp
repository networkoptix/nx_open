// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <string>
#include <memory>

#include <nx/kit/test.h>

#define NX_DEBUG_ENABLE_OUTPUT false //< Change to enable verbose output for the test.
#include <nx/kit/debug.h>

#include <nx/sdk/interface.h>
#include <nx/sdk/helpers/ref_countable.h>

namespace nx {
namespace sdk {
namespace ref_countable_ut {

static constexpr int kOldInterfaceIdSize = 16;

//-------------------------------------------------------------------------------------------------
// TEST_QUERY_INTERFACE macro.

/**
 * Supports both old SDK and current SDK objects/interfaces. Assumes that the initial object's
 * refCount is 1. The object should implement methods:
 * ```
 * int refCountForTest() const;
 * const char* interfaceIdForTest() const; //< Should return object's immediate interface.
 * ```
 */
#define TEST_QUERY_INTERFACE(EXPECTED_RESULT, OBJECT_PTR, INTERFACE) \
    detail::testQueryInterface<INTERFACE>(__LINE__, \
        ExpectedQueryInterfaceResult::EXPECTED_RESULT, OBJECT_PTR)

enum class ExpectedQueryInterfaceResult {null, nonNull};

namespace detail {

template<typename Interface>
using IsIRefCountable = std::is_base_of<IRefCountable, Interface>;

/** Sfinae: overload for an old interface: queryInterface() receives interfaceId. */
template<class Interface, typename ObjectPtr,
    typename = std::enable_if_t<!IsIRefCountable<Interface>::value>
>
const void* callQueryInterface(int /*line*/, ObjectPtr objectPtr)
{
    return objectPtr->queryInterface(Interface::interfaceId());
}

/** Sfinae: overload for a new interface: queryInterface<>() extracts interfaceId from the type. */
template<class Interface, typename ObjectPtr,
    typename = std::enable_if_t<IsIRefCountable<Interface>::value>
>
const void* callQueryInterface(int line, ObjectPtr objectPtr, int sfinaeDummy = 0)
{
    /*unused*/ (void) sfinaeDummy;

    const Ptr<Interface> ptr = objectPtr->IRefCountable::template queryInterface<Interface>();

    // Increase result's ref-count because the caller will call releaseRef() when finished.
    if (ptr)
        ASSERT_EQ_AT_LINE(line, 3, objectPtr->addRef());

    return ptr.get();
}

/**
 * Calls queryInterface() on an object, asserting the initial reference count is 1, and that the
 * same object pointer is returned on success.
 */
template<class Interface, typename ObjectPtr>
void testQueryInterface(
    int line, ExpectedQueryInterfaceResult expectedQueryInterfaceResult, ObjectPtr objectPtr)
{
    const auto printDetails =
        [objectPtr]()
        {
            // Both old and new interface ids are supported: for new ids, print the first 16 chars.
            NX_PRINT_HEX_DUMP("Requested interfaceId",
                Interface::interfaceId(), kOldInterfaceIdSize);
            NX_PRINT_HEX_DUMP("Object's interfaceId",
                objectPtr->interfaceIdForTest(), kOldInterfaceIdSize);
        };

    ASSERT_TRUE_AT_LINE(line, objectPtr);
    ASSERT_EQ_AT_LINE(line, 1, objectPtr->refCountForTest());

    NX_OUTPUT << "Test at line " << line << " for object " << nx::kit::utils::toString(objectPtr);
    if (NX_DEBUG_ENABLE_OUTPUT)
        printDetails();

    const void* const ptr = callQueryInterface<Interface, ObjectPtr>(line, objectPtr);

    NX_OUTPUT << "queryInterface() -> " << nx::kit::utils::toString(ptr);

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

} // namespace detail

//-------------------------------------------------------------------------------------------------
// Test classes IRefCountable and RefCountable.

class IBase: public Interface<IBase>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::test::IBase"); }
    virtual const char* interfaceIdForTest() const = 0; //< VMT #4 - should match old SDK method.
    virtual int refCountForTest() const = 0; //< VMT #5 - should match old SDK method.
};

class IData: public Interface<IData, IBase>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::test::IData"); }
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

    virtual const char* interfaceIdForTest() const override { return interfaceId()->value; }
    virtual int refCountForTest() const override { return refCount(); }
};
bool Data::s_destructorCalled = false;

TEST(RefCountable, interfaceId)
{
    ASSERT_STREQ(reinterpret_cast<const char*>(IData::interfaceId()), IData::interfaceId()->value);
}

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

    TEST_QUERY_INTERFACE(nonNull, data, IData);
    IBase* const iIntermediate = data;
    TEST_QUERY_INTERFACE(nonNull, iIntermediate, IData);
    IData* const iData = data;
    TEST_QUERY_INTERFACE(nonNull, iData, IData);

    ASSERT_EQ(0, data->releaseRef());
    ASSERT_TRUE(Data::s_destructorCalled);
}

TEST(RefCountable, queryInterfaceConst)
{
    Data::s_destructorCalled = false;
    const Data* const data = new Data;

    TEST_QUERY_INTERFACE(nonNull, data, const IData);
    const IBase* const iIntermediate = data;
    TEST_QUERY_INTERFACE(nonNull, iIntermediate, const IData);
    const IData* const iData = data;
    TEST_QUERY_INTERFACE(nonNull, iData, const IData);

    ASSERT_EQ(0, data->releaseRef());
    ASSERT_TRUE(Data::s_destructorCalled);
}

//-------------------------------------------------------------------------------------------------
// Test the binary compatibility with the old SDK.

/** Defined exactly the same way as in the old SDK (NX_GUID). */
struct OldInterfaceId
{
    unsigned char value[kOldInterfaceIdSize];

    explicit OldInterfaceId(const char (&charArray)[kOldInterfaceIdSize])
    {
        // All old interface ids in this test have last byte equal to 0 for easy conversion to
        // the new interface id.
        std::copy(charArray, charArray + kOldInterfaceIdSize, value);
        ASSERT_EQ(0, value[kOldInterfaceIdSize - 1]);
    }

    /** To enable passing to NX_PRINT_HEX_DUMP(). */
    operator const char*() const { return reinterpret_cast<const char*>(value); }
};
static_assert(sizeof(OldInterfaceId) == kOldInterfaceIdSize,
    "Old SDK InterfaceId should have 16 bytes");

/**
 * Mock for a base interface from the old SDK - used for compatibility testing.
 *
 * Has the VMT layout starting with all the entries from IRefCountable (thus, as in the old SDK).
 *
 * Defined exactly the same way as in the old SDK (nxpl::PluginInterface).
 */
class OldInterface
{
public:
    static const OldInterfaceId& interfaceId()
    {
        static OldInterfaceId id{"old_interfaceId"}; //< 15 chars, 16 bytes.
        return id;
    }

    /** VMT #0. */
    virtual ~OldInterface() = default;

    /** #1 in VMT. */
    virtual void* queryInterface(const OldInterfaceId& id) = 0;

    /** VMT #2. */
    virtual unsigned int addRef() const = 0;

    /** VMT #3. */
    virtual unsigned int releaseRef() const = 0;

    /** VMT #4 - added for tests; couples with the same method of NewInterface. */
    virtual const char* interfaceIdForTest() { return (const char*) interfaceId().value; }

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

    virtual void* queryInterface(const OldInterfaceId& id) override
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

#define NEW_INTERFACE_ID "new_interfaceId" //< Has the minimum allowed length: 15 chars.

/**
 * Mock for a base interface from the new SDK - used for compatibility testing. Supports old
 * interface id in its queryInterface() in addition to the new interface id.
 */
class NewInterface: public Interface<NewInterface>
{
public:
    static auto interfaceId() { return makeId(NEW_INTERFACE_ID); }

    /** Support for old SDK interface id. */
    virtual IRefCountable* queryInterface(const InterfaceId* id) override
    {
        ASSERT_TRUE(id != nullptr);
        ASSERT_TRUE(interfaceId()->value != nullptr);

        return queryInterfaceSupportingDeprecatedId(id, Uuid(OldInterface::interfaceId().value));
    }

    /** VMT #4 - added for tests; couples with the same method of OldInterface. */
    virtual const char* interfaceIdForTest() const { return interfaceId()->value; }

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

class OtherNewInterface: public Interface<OtherNewInterface>
{
public:
    static auto interfaceId() { return makeId(NEW_INTERFACE_ID "_WITH_SUFFIX"); }
};

struct OtherOldInterface
{
    static const OldInterfaceId& interfaceId()
    {
        static OldInterfaceId id{"old2interfaceId"}; //< 15 chars, 16 bytes.
        return id;
    }
};

template<class OldInterface>
struct OldInterfaceWithNewId
{
    static const IRefCountable::InterfaceId* interfaceId()
    {
        ASSERT_EQ(0, OldInterface::interfaceId().value[kOldInterfaceIdSize - 1]);
        return reinterpret_cast<const IRefCountable::InterfaceId*>(
            OldInterface::interfaceId().value);
    }
};

/**
 * Test that InterfaceId is compatible with the old SDK interface identification type - needed to
 * guarantee binary compatibility with plugins compiled using the old SDK.
 */
TEST(RefCountable, binaryCompatibilityWithOldSdk)
{
    NewObject::s_destructorCalled = false;
    OldObject::s_destructorCalled = false;

    // Test integrity of the test.
    ASSERT_EQ(kOldInterfaceIdSize, (int) sizeof(OldInterfaceId));

    const auto newObject = new NewObject;
    ASSERT_EQ(1, newObject->refCountForTest());

    const auto oldObject = new OldObject;
    ASSERT_EQ(1, oldObject->refCountForTest());

    const auto newObjectAsOldInterface = reinterpret_cast<OldInterface*>(newObject);
    const auto oldObjectAsNewInterface = reinterpret_cast<NewInterface*>(oldObject);

    // Test the regular queryInterface() behavior in the new SDK.
    TEST_QUERY_INTERFACE(nonNull, newObject, NewObject);
    TEST_QUERY_INTERFACE(null, newObject, OtherNewInterface);

    // Mock an old plugin calling queryInterface() on an object from the new VMS.
    TEST_QUERY_INTERFACE(nonNull, newObjectAsOldInterface, OldInterface);
    TEST_QUERY_INTERFACE(null, newObjectAsOldInterface, OtherOldInterface);

    // Test the regular queryInterface() behavior of the old SDK.
    TEST_QUERY_INTERFACE(nonNull, oldObject, OldInterface);
    TEST_QUERY_INTERFACE(null, oldObject, OtherOldInterface);

    // Mock a new VMS calling queryInterface() on an object from the old plugin.
    TEST_QUERY_INTERFACE(null, oldObjectAsNewInterface, NewInterface);
    TEST_QUERY_INTERFACE(nonNull, oldObjectAsNewInterface, OldInterfaceWithNewId<OldObject>);
    TEST_QUERY_INTERFACE(null, oldObjectAsNewInterface, OldInterfaceWithNewId<OtherOldInterface>);
    TEST_QUERY_INTERFACE(null, oldObjectAsNewInterface, OtherNewInterface);

    oldObject->releaseRef();
    ASSERT_TRUE(OldObject::s_destructorCalled);
    newObject->releaseRef();
    ASSERT_TRUE(NewObject::s_destructorCalled);
}

} // namespace ref_countable_ut
} // namespace sdk
} // namespace nx
