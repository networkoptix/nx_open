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


/* Abstract member access bases */
/* ---------------------------- */

template<class T, class Object>
class QnAbstractMemberGetter : public QnAbstractGetter<T>
{
protected:
    QnAbstractMemberGetter(const Object* object) : m_object(object) {}
    const Object* object() const { return m_object; }

private:
    const Object* m_object;
};

template<class T, class Object>
class QnAbstractMemberSetter : public QnAbstractSetter<T>
{
protected:
    QnAbstractMemberSetter(Object* object) : m_object(object) {}
    Object* object() const { return m_object; }

private:
    Object* m_object;
};


/* Field access */
/* ------------ */

template<class T, class Object>
class QnFieldExternalGetter : public QnAbstractExternalGetter<T, Object>
{
    typedef T Object::* FieldPointer;

public:
    QnFieldExternalGetter(const FieldPointer field) : m_field(field) {}

    virtual T operator () (const Object* object) const override { return object->*m_field; }

private:
    FieldPointer m_field;
};

template<class T, class Object>
class QnFieldExternalSetter : public QnAbstractExternalSetter<T, Object>
{
    typedef T Object::* FieldPointer;

public:
    QnFieldExternalSetter(FieldPointer field) : m_field(field) {}

    virtual void operator () (Object* object, const T& value) const override { object->*m_field = value; }

private:
    FieldPointer m_field;
};

template<class T, class Object>
class QnFieldGetter : public QnAbstractMemberGetter<T, Object>
{
    typedef QnAbstractMemberGetter<T, Object> base_type;
    typedef QnFieldExternalGetter<T, Object> ExternalGetter;

public:
    QnFieldGetter(const Object* object, ExternalGetter field) : base_type(object), m_fieldGetter(field) {}
    virtual T operator () () const override { return m_fieldGetter(this->object()); }

private:
    ExternalGetter m_fieldGetter;
};

template<class T, class Object>
class QnFieldSetter : public QnAbstractMemberSetter<T, Object>
{
    typedef QnAbstractMemberSetter<T, Object> base_type;
    typedef QnFieldExternalSetter<T, Object> ExternalSetter;

public:
    QnFieldSetter(Object* object, ExternalSetter field) : base_type(object), m_fieldSetter(field) {}
    virtual void operator () (const T& value) const override { m_fieldSetter(this->object(), value); }

private:
    ExternalSetter m_fieldSetter;
};


/* Member function access */
/* ---------------------- */

template<class T, class Object, class MemberFunction>
class QnMethodExternalGetter : public QnAbstractExternalGetter<T, Object>
{
public:
    QnMethodExternalGetter(MemberFunction method) : m_method(method) {}

    virtual T operator () (const Object* object) const override { return (object->*m_method)(); }

private:
    MemberFunction m_method;
};

template<class T, class Object, class MemberFunction>
class QnMethodExternalSetter : public QnAbstractExternalSetter<T, Object>
{
public:
    QnMethodExternalSetter(MemberFunction method) : m_method(method) {}

    virtual void operator () (Object* object, const T& value) const override { (object->*m_method)(value); }

private:
    MemberFunction m_method;
};

template<class T, class Object, class MemberFunction>
class QnMethodGetter : public QnAbstractMemberGetter<T, Object>
{
    typedef QnAbstractMemberGetter<T, Object> base_type;
    typedef QnMethodExternalGetter<T, Object, MemberFunction> ExternalGetter;

public:
    QnMethodGetter(const Object* object, ExternalGetter method) : base_type(object), m_methodGetter(method) {}
    virtual T operator () () const override { return m_methodGetter(this->object()); }

private:
    ExternalGetter m_methodGetter;
};

template<class T, class Object, class MemberFunction>
class QnMethodSetter : public QnAbstractMemberSetter<T, Object>
{
    typedef QnAbstractMemberSetter<T, Object> base_type;
    typedef QnMethodExternalSetter<T, Object, MemberFunction> ExternalSetter;

public:
    QnMethodSetter(Object* object, ExternalSetter method) : base_type(object), m_methodSetter(method) {}
    virtual void operator () (const T& value) const override { m_methodSetter(this->object(), value); }

private:
    ExternalSetter m_methodSetter;
};


/* Automatically selected member access */
/* ------------------------------------ */

namespace detail
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
    struct QnMemberExternalGetterTypeSelector
    {
        typedef typename std::conditional<std::is_member_object_pointer<GetMember>::value,   /* ? */
                    QnFieldExternalGetter<T, Object>,  /* : */
                typename std::conditional<std::is_member_function_pointer<GetMember>::value, /* ? */
                    QnMethodExternalGetter<T, Object, GetMember>, /* : */
             /* default (error): */
                    void>::type>::type
            type;
    };

    template<class T, class Object, class SetMember>
    struct QnMemberExternalSetterTypeSelector
    {
        typedef typename std::conditional<std::is_member_object_pointer<SetMember>::value,   /* ? */
                    QnFieldExternalSetter<T, Object>,  /* : */
                typename std::conditional<std::is_member_function_pointer<SetMember>::value, /* ? */
                    QnMethodExternalSetter<T, Object, SetMember>, /* : */
             /* default (error): */
                    void>::type>::type
            type;
    };

} // namespace detail

template<class T, class Object, class GetMember>
using QnMemberGetter = typename detail::QnMemberGetterTypeSelector<T, Object, GetMember>::type;

template<class T, class Object, class SetMember>
using QnMemberSetter = typename detail::QnMemberSetterTypeSelector<T, Object, SetMember>::type;

template<class T, class Object, class GetMember>
using QnMemberExternalGetter = typename detail::QnMemberExternalGetterTypeSelector<T, Object, GetMember>::type;

template<class T, class Object, class SetMember>
using QnMemberExternalSetter = typename detail::QnMemberExternalSetterTypeSelector<T, Object, SetMember>::type;


/* Access via functor */
/* ------------------ */

template<class T, class Functor>
class QnFunctorGetter : public QnAbstractGetter<T>
{
public:
    QnFunctorGetter(Functor functor) : m_functor(functor) {}
    virtual T operator () () const override { return m_functor(); }

private:
    Functor m_functor;
};

template<class T, class Functor>
class QnFunctorSetter : public QnAbstractSetter<T>
{
public:
    QnFunctorSetter(Functor functor) : m_functor(functor) {}
    virtual void operator () (const T& value) const override { m_functor(value); }

private:
    Functor m_functor;
};

template<class T, class Object, class Functor>
class QnFunctorExternalGetter : public QnAbstractExternalGetter<T, Object>
{
public:
    QnFunctorExternalGetter(Functor functor) : m_functor(functor) {}
    virtual T operator () (const Object* object) const override { return m_functor(object); }

private:
    Functor m_functor;
};

template<class T, class Object, class Functor>
class QnFunctorExternalSetter : public QnAbstractExternalSetter<T, Object>
{
public:
    QnFunctorExternalSetter(Functor functor) : m_functor(functor) {}
    virtual void operator () (Object* object, const T& value) const override { m_functor(object, value); }

private:
    Functor m_functor;
};


/* Direct variable access */
/* ---------------------- */

template<class T>
class QnVariableGetter : public QnAbstractGetter<T>
{
public:
    QnVariableGetter(const T& reference) : m_reference(reference) {}
    QnVariableGetter(const T* pointer) : m_reference(*pointer) {}

    virtual T operator () () const override { return m_reference; }

private:
    const T& m_reference;
};

template<class T>
class QnVariableSetter : public QnAbstractSetter<T>
{
public:
    QnVariableSetter(T& reference) : m_reference(reference) {}
    QnVariableSetter(T* pointer) : m_reference(*pointer) {}

    virtual void operator () (const T& value) const override { m_reference = value; }

private:
    T& m_reference;
};

/* Automatically selected non-object access */
/* ---------------------------------------- */

namespace detail
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

} // namespace detail

template<class T, class Get>
using QnGlobalScopeGetter = typename detail::QnGlobalScopeGetterTypeSelector<T, Get>::type;

template<class T, class Set>
using QnGlobalScopeSetter = typename detail::QnGlobalScopeSetterTypeSelector<T, Set>::type;


/* Automatically selected access */
/* ----------------------------- */

namespace detail
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

    template<class T, class Object, class Get>
    struct QnExternalGetterTypeSelector
    {
        typedef typename std::conditional<std::is_member_pointer<Get>::value, /* ? */
                    QnMemberExternalGetter<T, Object, Get>, /* : */
                    QnFunctorExternalGetter<T, Object, Get>>::type
            type;
    };

    template<class T, class Object, class Set>
    struct QnExternalSetterTypeSelector
    {
        typedef typename std::conditional<std::is_member_pointer<Set>::value, /* ? */
                    QnMemberExternalSetter<T, Object, Set>, /* : */
                    QnFunctorExternalSetter<T, Object, Set>>::type
            type;
    };

} // namespace detail

template<class T, class Object, class Get>
using QnGetter = typename detail::QnGetterTypeSelector<T, Object, Get>::type;

template<class T, class Object, class Set>
using QnSetter = typename detail::QnSetterTypeSelector<T, Object, Set>::type;

template<class T, class Object, class Get>
using QnExternalGetter = typename detail::QnExternalGetterTypeSelector<T, Object, Get>::type;

template<class T, class Object, class Set>
using QnExternalSetter = typename detail::QnExternalSetterTypeSelector<T, Object, Set>::type;


/* Getter/setter utility aggregators with move semantics: */
/* ------------------------------------------------------ */

template<class T>
class QnGetterAggregator : public QnAbstractGetter<T>
{
public:
    QnGetterAggregator(const QnAbstractGetter<T>* getter) : m_getter(getter) {}
    virtual T operator () () const override { return (*m_getter)(); }

private:
    std::unique_ptr<const QnAbstractGetter<T>> m_getter;
};

template<class T>
class QnSetterAggregator
{
public:
    QnSetterAggregator(const QnAbstractSetter<T>* setter) : m_setter(setter) {}
    virtual void operator () (const T& value) const { (*m_setter)(value); }

private:
    std::unique_ptr<const QnAbstractSetter<T>> m_setter;
};

template<class T, class Object>
class QnExternalGetterAggregator
{
public:
    QnExternalGetterAggregator(const QnAbstractExternalGetter<T, Object>* getter) : m_getter(getter) {}
    virtual T operator () (const Object* object) const { return (*m_getter)(object); }

private:
    std::unique_ptr<const QnAbstractExternalGetter<T, Object>> m_getter;
};

template<class T, class Object>
class QnExternalSetterAggregator
{
public:
    QnExternalSetterAggregator(const QnAbstractExternalSetter<T, Object>* setter) : m_setter(setter) {}
    virtual void operator () (Object* object, const T& value) const { (*m_setter)(object, value); }

private:
    std::unique_ptr<const QnAbstractExternalSetter<T, Object>> m_setter;
};

template<class T, class Object>
class QnCascadeGetterAggregator : public QnAbstractMemberGetter<T, Object>
{
    typedef QnAbstractMemberGetter<T, Object> base_type;
    typedef QnAbstractExternalGetter<T, Object> ExternalGetter;

public:
    QnCascadeGetterAggregator(const Object* object, const ExternalGetter* getter) : base_type(object), m_getter(getter) {}
    virtual T operator () () const override { return (*m_getter)(this->object()); }

private:
    std::unique_ptr<const ExternalGetter> m_getter;
};

template<class T, class Object>
class QnCascadeSetterAggregator : public QnAbstractMemberSetter<T, Object>
{
    typedef QnAbstractMemberSetter<T, Object> base_type;
    typedef QnAbstractExternalSetter<T, Object> ExternalSetter;

public:
    QnCascadeSetterAggregator(Object* object, const ExternalSetter* setter) : base_type(object), m_setter(setter) {}
    virtual void operator () (const T& value) const override { return (*m_setter)(this->object(), value); }

private:
    std::unique_ptr<const ExternalSetter> m_setter;
};


/* Typed accessor template */
/* ----------------------- */

template<class T, class Object>
class QnTypedAccessor
{
public:
    QnTypedAccessor() = delete;
    QnTypedAccessor(const QnTypedAccessor&) = delete;

    template<class Get, class Set>
    QnTypedAccessor(Object* object, Get get, Set set) :
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

    const T& get() const { return m_getter(); }
    void set(const T& value) const { m_setter(value); }

    QnGetterAggregator<T> m_getter;
    QnSetterAggregator<T> m_setter;
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
    return set;
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
