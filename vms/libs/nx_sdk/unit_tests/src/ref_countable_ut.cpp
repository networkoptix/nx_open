// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <string>
#include <memory>

#include <nx/kit/test.h>

#include <nx/sdk/interface.h>
#include <nx/sdk/helpers/ref_countable.h>

#undef NX_DEBUG_ENABLE_OUTPUT
#define NX_DEBUG_ENABLE_OUTPUT false //< Change to enable verbose output for the test.
#include <nx/kit/debug.h>

namespace nx::sdk::ref_countable_ut {

static constexpr int kOldInterfaceIdSize = 16;

/** Prefix for all "new SDK" interface ids used in the tests. */
#define ID_PREFIX "nx::sdk::test::"

#define DEF_INTERFACE(INTERFACE_ID, INTERFACE, BASE) \
    class INTERFACE: public Interface<INTERFACE, BASE> \
    { \
    public: \
        static auto interfaceId() { return makeId(INTERFACE_ID); } \
    }

#define DEF_INTERFACE_WITH_ALTERNATIVE_ID(INTERFACE_ID, ALTERNATIVE_ID, INTERFACE, BASE) \
    class INTERFACE: public Interface<INTERFACE, BASE> \
    { \
    public: \
        static auto interfaceId() { return makeIdWithAlternative(INTERFACE_ID, ALTERNATIVE_ID); } \
    }

#define DEF_REF_COUNTABLE(CLASS, INTERFACE) \
    class CLASS: public RefCountable<INTERFACE> \
    { \
    };

//-------------------------------------------------------------------------------------------------
// TEST_QUERY_INTERFACE()

/**
 * Supports both old SDK and current SDK objects/interfaces. Assumes that the initial object's
 * refCount is 1. Extracts the required interface id from the INTERFACE class.
 */
#define TEST_QUERY_INTERFACE(EXPECTED_RESULT, OBJECT_PTR, INTERFACE) \
    detail::testQueryInterface<INTERFACE>(__LINE__, \
        detail::ExpectedQueryInterfaceResult::EXPECTED_RESULT, \
        OBJECT_PTR)

namespace detail {

enum class ExpectedQueryInterfaceResult { null, nonNull };

template<class Interface, typename ObjectPtr>
const void* callQueryInterface(int line, ObjectPtr objectPtr)
{
    using Object = typename std::remove_pointer<ObjectPtr>::type;

    if constexpr (std::is_base_of<IRefCountable, Interface>::value) //< New interface.
    {
        // To be able to call queryInterface<>() on old SDK object pointers, we need to cast the
        // object pointer to IRefCountable*, preserving its constness.
        using RefCountable = typename std::conditional<std::is_const_v<Object>,
            /*?*/ const IRefCountable*,
            /*:*/ IRefCountable*>::type;
        const Ptr<Interface> ptr =
            reinterpret_cast<RefCountable>(objectPtr)->
                IRefCountable::template queryInterface<Interface>();

        // Increase result's ref-count because the caller will call releaseRef() when finished.
        if (ptr)
            ASSERT_EQ_AT_LINE(line, 3, objectPtr->addRef());

        return ptr.get();
    }
    else //< Old interface.
    {
        // Remove const from the object because the virtual queryInterface() does not have a const
        // overload.
        using NonConstObjectPtr = typename std::remove_const<Object>::type*;
        return const_cast<NonConstObjectPtr>(objectPtr)->queryInterface(Interface::interfaceId());
    }
}

/**
 * Produces a human-readable reference to the object pointer, type, and the requested interface id.
 */
template<class Interface, typename ObjectPtr>
std::string objectAndInterfaceIdRef(ObjectPtr objectPtr)
{
    std::string interfaceIdRef;
    if constexpr (std::is_pointer_v<decltype(Interface::interfaceId())>)
    {
        // New interface with a single id; interfaceId() is a struct pointer.
        interfaceIdRef = nx::kit::utils::toString(Interface::interfaceId()->value);
    }
    else if constexpr (std::is_same_v<decltype(Interface::interfaceId()),
        std::vector<const IRefCountable::InterfaceId*>>)
    {
        // New interface with alternative id; interfaceId() is a vector of struct pointers.
        for (const auto& id: Interface::interfaceId())
        {
            interfaceIdRef += (interfaceIdRef.empty() ? "" : "|")
                + nx::kit::utils::toString(id->value);
        }
    }
    else
    {
        // Old interface; interfaceId() is a struct reference.
        interfaceIdRef = nx::kit::utils::toString((const char*) Interface::interfaceId().value);
    }

    return "Requested interfaceId " + interfaceIdRef + " for " + typeid(*objectPtr).name()
        + " @" + nx::kit::utils::toString(objectPtr);
}

/**
 * Calls queryInterface() on an object, asserting the initial reference count is 1, and that the
 * same object pointer is returned on success.
 */
template<class Interface, typename ObjectPtr>
void doTestQueryInterface(
    int line, ExpectedQueryInterfaceResult expectedQueryInterfaceResult, ObjectPtr objectPtr)
{
    ASSERT_TRUE_AT_LINE(line, objectPtr);
    ASSERT_EQ_AT_LINE(line, 1, objectPtr->refCountThreadUnsafe());

    NX_OUTPUT << "Test at line " << line << ": " << objectAndInterfaceIdRef<Interface>(objectPtr);

    const void* const ptr = callQueryInterface<Interface, ObjectPtr>(line, objectPtr);

    NX_OUTPUT << "queryInterface() -> " << nx::kit::utils::toString(ptr);

    switch (expectedQueryInterfaceResult)
    {
        case ExpectedQueryInterfaceResult::null:
            if (ptr) //< Test will fail: print detailed diagnostics.
            {
                if (!NX_DEBUG_ENABLE_OUTPUT)
                    NX_PRINT << objectAndInterfaceIdRef<Interface>(objectPtr);
                ASSERT_EQ_AT_LINE(line, (void*) objectPtr, ptr); //< Fail here if ptr is bad.
                ASSERT_FALSE_AT_LINE(line, ptr); //< Fail the test.
            }
            ASSERT_EQ_AT_LINE(line, 1, objectPtr->refCountThreadUnsafe());
            break;

        case ExpectedQueryInterfaceResult::nonNull:
            if (ptr != (void*) objectPtr) //< Test will fail: print detailed diagnostics.
            {
                if (!NX_DEBUG_ENABLE_OUTPUT)
                    NX_PRINT << objectAndInterfaceIdRef<Interface>(objectPtr);
                ASSERT_EQ_AT_LINE(line, nullptr, ptr); //< Fail here if ptr is bad.
                ASSERT_EQ_AT_LINE(line, (void*) objectPtr, ptr); //< Fail the test.
            }
            ASSERT_EQ_AT_LINE(line, 2, objectPtr->refCountThreadUnsafe());
            ASSERT_EQ_AT_LINE(line, 1, (int) objectPtr->releaseRef()); //< Cast for old interfaces.
            ASSERT_EQ_AT_LINE(line, 1, objectPtr->refCountThreadUnsafe());
            break;
    }
}

/**
 * Intended to be used from the macro TEST_QUERY_INTERFACE(). Calls queryInterface<>() with removed
 * and added const, to test both its const overloads.
 */
template<class Interface, typename ObjectPtr>
void testQueryInterface(
    int line, ExpectedQueryInterfaceResult expectedQueryInterfaceResult, ObjectPtr objectPtr)
{
    using Object = typename std::remove_pointer<ObjectPtr>::type;
    using NonConstObjectPtr = typename std::remove_const<Object>::type*;

    detail::doTestQueryInterface<Interface>(line,
        expectedQueryInterfaceResult,
        const_cast<NonConstObjectPtr>(objectPtr));

    detail::doTestQueryInterface<const Interface>(line,
        expectedQueryInterfaceResult,
        const_cast<const Object*>(objectPtr));
}

} // namespace detail

//-------------------------------------------------------------------------------------------------
// Test classes IRefCountable and RefCountable.

DEF_INTERFACE(ID_PREFIX "ISuper", ISuper, IRefCountable);
DEF_INTERFACE(ID_PREFIX "IData", IData, ISuper);

class Data: public RefCountable<IData>
{
public:
    static bool s_destructorCalled;

    virtual ~Data() override
    {
        ASSERT_FALSE(s_destructorCalled);
        s_destructorCalled = true;
    }
};
bool Data::s_destructorCalled = false;

TEST(RefCountable, interfaceId)
{
    ASSERT_STREQ(ID_PREFIX "IData", static_cast<const char*>(IData::interfaceId()->value));
    ASSERT_EQ(IData::interfaceId()->value, reinterpret_cast<const char*>(IData::interfaceId()));
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

TEST(RefCountable, queryInterface)
{
    Data::s_destructorCalled = false;
    Data* const data = new Data;

    TEST_QUERY_INTERFACE(nonNull, data, IRefCountable);
    TEST_QUERY_INTERFACE(nonNull, data, ISuper);
    TEST_QUERY_INTERFACE(nonNull, data, IData);
    ISuper* const iIntermediate = data;
    TEST_QUERY_INTERFACE(nonNull, iIntermediate, IRefCountable);
    TEST_QUERY_INTERFACE(nonNull, iIntermediate, ISuper);
    TEST_QUERY_INTERFACE(nonNull, iIntermediate, IData);
    IData* const iData = data;
    TEST_QUERY_INTERFACE(nonNull, iData, IRefCountable);
    TEST_QUERY_INTERFACE(nonNull, iData, ISuper);
    TEST_QUERY_INTERFACE(nonNull, iData, IData);

    ASSERT_EQ(0, data->releaseRef());
    ASSERT_TRUE(Data::s_destructorCalled);
}

//-------------------------------------------------------------------------------------------------
// Test interface id generation for templates.

template<typename IItem>
class IBunch: public Interface<IBunch<IItem>>
{
public:
    static auto interfaceId()
    {
        return IBunch::template makeIdForTemplate<IBunch<IItem>, IItem>(ID_PREFIX "IBunch");
    }
};

DEF_INTERFACE(ID_PREFIX "ISomeItem", ISomeItem, IRefCountable);

TEST(RefCountable, makeIdForTemplate)
{
    ASSERT_STREQ(ID_PREFIX "IBunch<" ID_PREFIX "ISomeItem>",
        static_cast<const char*>(IBunch<ISomeItem>::interfaceId()->value));
}

//-------------------------------------------------------------------------------------------------
// Test alternative interface id.

// Define Id... interfaces to be used as holders for interfaceId() to call queryInterface<Id...>().
#define ID_BASE ID_PREFIX "Base"
#define ID_ALT_BASE ID_PREFIX "AltBase"
#define ID_DERIVED_1 ID_PREFIX "Derived1"
#define ID_DERIVED_2 ID_PREFIX "Derived2"
DEF_INTERFACE(ID_BASE, IdBase, IRefCountable);
DEF_INTERFACE(ID_ALT_BASE, IdAltBase, IRefCountable);
DEF_INTERFACE(ID_DERIVED_1, IdDerived1, IRefCountable);
DEF_INTERFACE(ID_DERIVED_2, IdDerived2, IRefCountable);

DEF_INTERFACE_WITH_ALTERNATIVE_ID(ID_BASE, ID_ALT_BASE, IBase, IRefCountable);

DEF_INTERFACE(ID_DERIVED_1, IDerived1, IBase);
DEF_INTERFACE(ID_DERIVED_2, IDerived2, IBase);
DEF_REF_COUNTABLE(Derived1, IDerived1);

/** Test that an object of a sibling interface does not support its sibling interface. */
TEST(RefCountable, alternativeInterfaceIdOfSibling)
{
    IDerived1* const derived1 = new Derived1;

    TEST_QUERY_INTERFACE(nonNull, derived1, IdBase);
    TEST_QUERY_INTERFACE(nonNull, derived1, IdAltBase);
    TEST_QUERY_INTERFACE(nonNull, derived1, IdDerived1);
    TEST_QUERY_INTERFACE(null, derived1, IdDerived2);

    TEST_QUERY_INTERFACE(nonNull, derived1, IBase);
    TEST_QUERY_INTERFACE(nonNull, derived1, IDerived1);

    // Test that the alternative id of IBase is not treated as an alternative id of IDerived2.
    TEST_QUERY_INTERFACE(null, derived1, IDerived2);

    ASSERT_EQ(0, derived1->releaseRef());
}

#define ID_DEPRECATED ID_PREFIX "Deprecated"
DEF_INTERFACE(ID_DEPRECATED, IDeprecated, IRefCountable);
DEF_REF_COUNTABLE(Deprecated, IDeprecated);

#define ID_CURRENT ID_PREFIX "Current"
DEF_INTERFACE(ID_CURRENT, ICurrent, IRefCountable);
DEF_REF_COUNTABLE(Current, ICurrent);
DEF_INTERFACE_WITH_ALTERNATIVE_ID(ID_CURRENT, ID_DEPRECATED, ICurrentAndDeprecated, IRefCountable);
DEF_REF_COUNTABLE(CurrentAndDeprecated, ICurrentAndDeprecated);

/**
 * Test that an object from a deprecated plugin called from the current application (in which this
 * interface has been renamed and retained the deprecated id as an alternative one) supports its
 * current interface.
 */
TEST(RefCountable, alternativeInterfaceIdDeprecatedPluginFromCurrentApp)
{
    Deprecated* const deprecated = new Deprecated;
    TEST_QUERY_INTERFACE(nonNull, deprecated, IDeprecated);
    TEST_QUERY_INTERFACE(null, deprecated, ICurrent);

    const auto asCurrentAndDeprecated = reinterpret_cast<ICurrentAndDeprecated*>(deprecated);
    TEST_QUERY_INTERFACE(nonNull, asCurrentAndDeprecated, ICurrentAndDeprecated);

    ASSERT_EQ(0, deprecated->releaseRef());
}

/**
 * Test that an object from the current plugin (which has both current and deprecated interface
 * ids) called from a deprecated application supports its deprecated interface.
 */
TEST(RefCountable, alternativeInterfaceIdCurrentPluginFromDeprecatedApp)
{
    CurrentAndDeprecated* const currentAndDeprecated = new CurrentAndDeprecated;
    TEST_QUERY_INTERFACE(nonNull, currentAndDeprecated, IDeprecated);
    TEST_QUERY_INTERFACE(nonNull, currentAndDeprecated, ICurrent);
    TEST_QUERY_INTERFACE(nonNull, currentAndDeprecated, ICurrentAndDeprecated);

    const auto asDeprecated = reinterpret_cast<IDeprecated*>(currentAndDeprecated);
    TEST_QUERY_INTERFACE(nonNull, asDeprecated, IDeprecated);

    ASSERT_EQ(0, currentAndDeprecated->releaseRef());
}

/**
 * Test that an object from a future plugin (which doesn't support the deprecated interface
 * anymore) called from the current application (which supports both the deprecated and the current
 * interface ids) supports its current interface.
 */
TEST(RefCountable, alternativeInterfaceIdFuturePluginFromCurrentApp)
{
    Current* const current = new Current;
    TEST_QUERY_INTERFACE(null, current, IDeprecated);
    TEST_QUERY_INTERFACE(nonNull, current, ICurrent);

    const auto asCurrentAndDeprecated = reinterpret_cast<ICurrentAndDeprecated*>(current);
    TEST_QUERY_INTERFACE(nonNull, asCurrentAndDeprecated, ICurrentAndDeprecated);

    ASSERT_EQ(0, current->releaseRef());
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
        // the new interface id, and instead of the UUID bytes they contain some readable string.
        std::copy(charArray, charArray + kOldInterfaceIdSize, value);
        ASSERT_EQ(0, value[kOldInterfaceIdSize - 1]);
    }
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

    int refCountThreadUnsafe() const
    {
        /*unused*/ (void) addRef();
        return releaseRef();
    }
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

        return queryInterfaceSupportingDeprecatedId(id, Uuid(OldInterface::interfaceId().value));
    }
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
};
bool NewObject::s_destructorCalled = false;

class OtherNewInterface: public Interface<OtherNewInterface>
{
public:
    static auto interfaceId() { return makeId(NEW_INTERFACE_ID "_WITH_SUFFIX"); }
};

class OtherOldInterface
{
public:
    static const OldInterfaceId& interfaceId()
    {
        static OldInterfaceId id{"old2interfaceId"}; //< 15 chars, 16 bytes.
        return id;
    }
};

template<class OldInterface>
class OldInterfaceWithNewId
{
public:
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

    NewObject* const newObject = new NewObject;
    ASSERT_EQ(1, newObject->refCountThreadUnsafe());

    OldObject* const oldObject = new OldObject;
    ASSERT_EQ(1, oldObject->refCountThreadUnsafe());

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

} // namespace nx::sdk::ref_countable_ut
