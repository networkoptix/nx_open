#pragma once

#include <type_traits>
#include <memory>


/* Abstract access interfaces */
/* -------------------------- */

template<class T>
struct QnAbstractGetter
{
    virtual ~QnAbstractGetter() {}
    virtual T operator () () const = 0;
};

template<class T>
struct QnAbstractSetter
{
    virtual ~QnAbstractSetter() {}
    virtual void operator () (const T&) const = 0;
};

template<class T, class Object>
struct QnAbstractExternalGetter
{
    virtual ~QnAbstractExternalGetter() {}
    virtual T operator () (const Object*) const = 0;
};

template<class T, class Object>
struct QnAbstractExternalSetter
{
    virtual ~QnAbstractExternalSetter() {}
    virtual void operator () (Object*, const T&) const = 0;
};


/* Abstract member access interfaces */
/* --------------------------------- */

template<class T, class Object>
class QnAbstractMemberGetter : public QnAbstractGetter<T>
{
public:
    explicit QnAbstractMemberGetter(const Object* object) : m_object(object) {}
    const Object* object() const { return m_object; }

private:
    const Object* m_object;
};

template<class T, class Object>
class QnAbstractMemberSetter : public QnAbstractSetter<T>
{
public:
    explicit QnAbstractMemberSetter(Object* object) : m_object(object) {}
    Object* object() const { return m_object; }

private:
    Object* m_object;
};


/* Cascade member access */
/* --------------------- */

template<class T, class Object>
class QnCascadeMemberGetter : public QnAbstractMemberGetter<T, Object>
{
    typedef QnAbstractMemberGetter<T, Object> base_type;
    typedef QnAbstractExternalGetter<T, Object> ExternalGetter;

public:
    explicit QnCascadeMemberGetter(const Object* object, const ExternalGetter* getter) : base_type(object), m_getter(getter) {}
    QnCascadeMemberGetter(const QnCascadeMemberGetter& other) : base_type(other), m_getter(new Getter(*other.getter)) {}

    virtual T operator () () const override { return (*m_getter)(object()); }

private:
    std::unique_ptr<const ExternalGetter> m_getter;
};

template<class T, class Object>
class QnCascadeMemberSetter : public QnAbstractMemberSetter<T, Object>
{
    typedef QnAbstractMemberSetter<T, Object> base_type;
    typedef QnAbstractExternalSetter<T, Object> ExternalSetter;

public:
    explicit QnCascadeMemberSetter(Object* object, const ExternalSetter* setter) : base_type(object), m_setter(setter) {}
    QnCascadeMemberSetter(const QnCascadeMemberSetter& other) : base_type(other), m_setter(new Setter(*other.setter)) {}

    virtual void operator () (const T& value) const override { return (*m_setter)(object(), value); }

private:
    std::unique_ptr<const ExternalSetter> m_setter;
};


/* Field access */
/* ------------ */

template<class T, class Object>
class QnExternalFieldGetter : public QnAbstractExternalGetter<T, Object>
{
    typedef T Object::* FieldPointer;

public:
    QnExternalFieldGetter(const FieldPointer field) : m_field(field) {}

    virtual T operator () (const Object* object) const override { return object->*m_field; }

private:
    FieldPointer m_field;
};

template<class T, class Object>
class QnExternalFieldSetter : public QnAbstractExternalSetter<T, Object>
{
    typedef T Object::* FieldPointer;

public:
    QnExternalFieldSetter(FieldPointer field) : m_field(field) {}

    virtual void operator () (Object* object, const T& value) const override { object->*m_field = value; }

private:
    FieldPointer m_field;
};

template<class T, class Object>
class QnFieldGetter : public QnAbstractMemberGetter<T, Object>
{
    typedef QnAbstractMemberGetter<T, Object> base_type;

public:
    explicit QnFieldGetter(const Object* object, QnExternalFieldGetter field) : base_type(object), m_fieldGetter(field) {}

    virtual T operator () () const override { return m_fieldGetter(object); }

private:
    QnExternalFieldGetter m_fieldGetter;
};

template<class T, class Object>
class QnFieldSetter : public QnAbstractMemberSetter<T, Object>
{
    typedef QnAbstractMemberSetter<T, Object> base_type;

public:
    explicit QnFieldSetter(Object* object, QnExternalFieldSetter field) : base_type(object), m_fieldSetter(field) {}

    virtual void operator () (const T& value) const override { m_fieldSetter(object, value); }

private:
    QnExternalFieldSetter m_fieldSetter;
};


/* Member function access */
/* ---------------------- */

template<class T, class Object, class MemberFunction>
class QnExternalMethodGetter : public QnAbstractExternalGetter<T, Object>
{
public:
    QnExternalMethodGetter(MemberFunction method) : m_method(method) {}

    virtual T operator () (const Object* object) const override { return (object->*m_method)(); }

private:
    MemberFunction m_method;
};

template<class T, class Object, class MemberFunction>
class QnExternalMethodSetter : public QnAbstractExternalSetter<T, Object>
{
public:
    QnExternalMethodSetter(MemberFunction method) : m_method(method) {}

    virtual void operator () (Object* object, const T& value) const override { (object->*m_method)(value); }

private:
    MemberFunction m_method;
};

template<class T, class Object, class MemberFunction>
class QnMethodGetter : public QnAbstractMemberGetter<T, Object>
{
    typedef QnAbstractMemberGetter<T, Object> base_type;
    typedef QnExternalMethodGetter<T, Object, MemberFunction> ExternalGetter;

public:
    explicit QnMethodGetter(const Object* object, ExternalGetter method) : base_type(object), m_methodGetter(method) {}

    virtual T operator () () const override { return m_methodGetter(object()); }

private:
    ExternalGetter m_methodGetter;
};

template<class T, class Object, class MemberFunction>
class QnMethodSetter : public QnAbstractMemberSetter<T, Object>
{
    typedef QnAbstractMemberSetter<T, Object> base_type;
    typedef QnExternalMethodSetter<T, Object, MemberFunction> ExternalSetter;

public:
    explicit QnMethodSetter(Object* object, ExternalSetter method) : base_type(object), m_methodSetter(method) {}

    virtual void operator () (const T& value) const override { m_methodSetter(object(), value); }

private:
    ExternalSetter m_methodSetter;
};


/* Automatically selected member access */
/* ------------------------------------ */

namespace nx_private
{
    template<class T, class Object, class GetMember>
    struct QnMemberGetterTypeSelector
    {
        typedef typename std::conditional<std::is_member_object_pointer<GetMember>::value,   /* ? */
                    QnFieldGetter<T, Object>,  /* : */
                typename std::conditional<std::is_member_function_pointer<GetMember>::value, /* ? */
                    QnMethodGetter<T, Object, GetMember>, /* : */
             /* default (error): */
                    void>::type>::type
            type;
    };

    template<class T, class Object, class SetMember>
    struct QnMemberSetterTypeSelector
    {
        typedef typename std::conditional<std::is_member_object_pointer<SetMember>::value,   /* ? */
                    QnFieldSetter<T, Object>,  /* : */
                typename std::conditional<std::is_member_function_pointer<SetMember>::value, /* ? */
                    QnMethodSetter<T, Object, SetMember>, /* : */
             /* default (error): */
                    void>::type>::type
            type;
    };

    template<class T, class Object, class GetMember>
    struct QnExternalMemberGetterTypeSelector
    {
        typedef typename std::conditional<std::is_member_object_pointer<GetMember>::value,   /* ? */
                    QnExternalFieldGetter<T, Object>,  /* : */
                typename std::conditional<std::is_member_function_pointer<GetMember>::value, /* ? */
                    QnExternalMethodGetter<T, Object, GetMember>, /* : */
             /* default (error): */
                    void>::type>::type
            type;
    };

    template<class T, class Object, class SetMember>
    struct QnExternalMemberSetterTypeSelector
    {
        typedef typename std::conditional<std::is_member_object_pointer<SetMember>::value,   /* ? */
                    QnExternalFieldSetter<T, Object>,  /* : */
                typename std::conditional<std::is_member_function_pointer<SetMember>::value, /* ? */
                    QnExternalMethodSetter<T, Object, SetMember>, /* : */
             /* default (error): */
                    void>::type>::type
            type;
    };

} // namespace nx_private

template<class T, class Object, class GetMember>
using QnMemberGetter = typename nx_private::QnMemberGetterTypeSelector<T, Object, GetMember>::type;

template<class T, class Object, class SetMember>
using QnMemberSetter = typename nx_private::QnMemberSetterTypeSelector<T, Object, SetMember>::type;

template<class T, class Object, class GetMember>
using QnExternalMemberGetter = typename nx_private::QnExternalMemberGetterTypeSelector<T, Object, GetMember>::type;

template<class T, class Object, class SetMember>
using QnExternalMemberSetter = typename nx_private::QnExternalMemberSetterTypeSelector<T, Object, SetMember>::type;


/* Access via functor */
/* ------------------ */

template<class T, class Functor>
class QnFunctorGetter : public QnAbstractGetter<T>
{
public:
    explicit QnFunctorGetter(Functor functor) : m_functor(functor) {}

    virtual T operator () () const override { return m_functor(); }

private:
    Functor m_functor;
};

template<class T, class Functor>
class QnFunctorSetter : public QnAbstractSetter<T>
{
public:
    explicit QnFunctorSetter(Functor functor) : m_functor(functor) {}

    virtual void operator () (const T& value) const override { m_functor(value); }

private:
    Functor m_functor;
};


/* Direct variable access */
/* ---------------------- */

template<class T>
class QnVariableGetter : public QnAbstractGetter<T>
{
public:
    explicit QnVariableGetter(const T& reference) : m_reference(reference) {}
    explicit QnVariableGetter(const T* pointer) : m_reference(*pointer) {}

    virtual T operator () () const override { return m_reference; }

private:
    const T& m_reference;
};

template<class T>
class QnVariableSetter : public QnAbstractSetter<T>
{
public:
    explicit QnVariableSetter(T& reference) : m_reference(reference) {}
    explicit QnVariableSetter(T* pointer) : m_reference(*pointer) {}

    virtual void operator () (const T& value) const override { m_reference = value; }

private:
    T& m_reference;
};

/* Automatically selected non-object access */
/* ---------------------------------------- */

namespace nx_private
{
    template<class T, class Get>
    struct QnGlobalScopeGetterTypeSelector
    {
        typedef typename std::conditional<std::is_same<T, typename std::remove_reference<Get>::type>::value, /* ? */
                    QnVariableGetter<T>, /* : */
                    QnFunctorGetter<T, Get>>::type
            type;
    };

    template<class T, class Set>
    struct QnGlobalScopeSetterTypeSelector
    {
        typedef typename std::conditional<std::is_same<T, typename std::remove_reference<Set>::type>::value, /* ? */
                    QnVariableSetter<T>, /* : */
                    QnFunctorSetter<T, Set>>::type
            type;
    };

} // namespace nx_private

template<class T, class Get>
using QnGlobalScopeGetter = typename nx_private::QnGlobalScopeGetterTypeSelector<T, Get>::type;

template<class T, class Set>
using QnGlobalScopeSetter = typename nx_private::QnGlobalScopeSetterTypeSelector<T, Set>::type;


/* Automatically selected access */
/* ----------------------------- */

namespace nx_private
{
    template<class T, class Object, class Get>
    struct QnGetterTypeSelector
    {
        struct GlobalScopeAdaptor : public QnGlobalScopeGetter<T, Get>
        {
            GlobalScopeAdaptor(const Object*, Get get) : QnGlobalScopeGetter<T, Get>(get) {}
        };

        typedef typename std::conditional<std::is_member_pointer<Get>::value, /* ? */
                    QnMemberGetter<T, Object, Get>, /* : */
                    GlobalScopeAdaptor>::type
            type;
    };

    template<class T, class Object, class Set>
    struct QnSetterTypeSelector
    {
        struct GlobalScopeAdaptor : public QnGlobalScopeSetter<T, Set>
        {
            GlobalScopeAdaptor(Object*, Set set) : QnGlobalScopeSetter<T, Set>(set) {}
        };

        typedef typename std::conditional<std::is_member_pointer<Set>::value, /* ? */
                    QnMemberSetter<T, Object, Set>, /* : */
                    GlobalScopeAdaptor>::type
            type;
    };

} // namespace nx_private

template<class T, class Object, class Get>
using QnGetter = typename nx_private::QnGetterTypeSelector<T, Object, Get>::type;

template<class T, class Object, class Set>
using QnSetter = typename nx_private::QnSetterTypeSelector<T, Object, Set>::type;


/* Typed accessor template */
/* ----------------------- */

template<class T, class Object>
class QnTypedAccessor
{
public:
    template<class Get, class Set>
    explicit QnTypedAccessor(Object* object, Get get, Set set) :
        m_getter(new QnGetter<T, Object, Get>(object, get)),
        m_setter(new QnSetter<T, Object, Set>(object, set))
    {
    }

    template<class Get, class Set>
    QnTypedAccessor(Get get, Set set) :
        m_getter(new QnGlobalScopeGetter<T, Get>(get)),
        m_setter(new QnGlobalScopeSetter<T, Set>(set))
    {
    }

    const T& get() const { return (*m_getter)(); }
    void set(const T& value) const { (*m_setter)(value); }

private:
    QnTypedAccessor() {}

    std::unique_ptr<const QnAbstractGetter<T>> m_getter;
    std::unique_ptr<const QnAbstractSetter<T>> m_setter;
};

namespace Qn {

/* Helper functions to choose accessor overloads */
/* --------------------------------------------- */

template <class Get>
Get getter_helper(Get get)
{
    return get;
}

template <class Set>
Set setter_helper(Set set)
{
    return get;
}

template<class T>
auto getter_helper(T (*get)())
{
    return get;
}

template<class T>
auto getter_helper(const T& (*get)())
{
    return get;
}

template<class T, class Object>
auto getter_helper(T (Object::*get)() const)
{
    return get;
}

template<class T, class Object>
auto getter_helper(const T& (Object::*get)() const)
{
    return get;
}

template<class T>
auto setter_helper(void (*set)(T))
{
    return set;
}

template<class T>
auto setter_helper(void (*set)(const T&))
{
    return set;
}

template<class T, class Object>
auto setter_helper(void (Object::*set)(T))
{
    return set;
}

template<class T, class Object>
auto setter_helper(void (Object::*set)(const T&))
{
    return set;
}

#define QN_GETTER(x) Qn::getter_helper(&x)
#define QN_SETTER(x) Qn::setter_helper(&x)

/* Sample: cutting off wrong overload of QWidget::setContentsMargins
 *    QnTypedAccessor<QMargins, QWidget> accessor(widget, &QWidget::contentsMargins, QN_SETTER(QWidget::setContentsMargins));
 */

} // namespace Qn
