#ifndef QN_SCOPED_VALUE_ROLLBACK_H
#define QN_SCOPED_VALUE_ROLLBACK_H

#include <QtGlobal>

template<class T, class Object, class Setter, class Getter>
class QnGenericScopedValueRollback {
public:
    QnGenericScopedValueRollback(Object *object, const Setter &setter, const Getter &getter):
        m_object(object),
        m_setter(setter),
        m_rollbackValue(getter(object)),
        m_committed(false)
    {}

    QnGenericScopedValueRollback(Object *object, const Setter &setter, const Getter &getter, const T &newValue):
        m_object(object),
        m_setter(setter),
        m_rollbackValue(getter(object)),
        m_committed(false)
    {
        m_setter(m_object, newValue);
    }

    ~QnGenericScopedValueRollback() {
        if(!m_committed)
            m_setter(m_object, m_rollbackValue);
    }

    void commit() {
        m_committed = true;
    }

    void rollback() {
        m_committed = true;
        m_setter(m_object, m_rollbackValue);
    }

private:
    Q_DISABLE_COPY(QnGenericScopedValueRollback);

    Object *m_object;
    Setter m_setter;
    T m_rollbackValue;
    bool m_committed;
};


namespace detail {
    struct VariableSetterGetter {
        template<class T>
        void operator()(T *variable, const T &value) const {
            *variable = value;
        }

        template<class T>
        T operator()(const T *variable) const {
            return *variable;
        }
    };

} // namespace detail

template<class T>
class QnScopedValueRollback: public QnGenericScopedValueRollback<T, T, detail::VariableSetterGetter, detail::VariableSetterGetter> {
    typedef detail::VariableSetterGetter sg_type;
    typedef QnGenericScopedValueRollback<T, T, sg_type, sg_type> base_type;

public:
    QnScopedValueRollback(T *variable):
        base_type(variable, sg_type(), sg_type())
    {}

    QnScopedValueRollback(T *variable, const T &newValue):
        base_type(variable, sg_type(), sg_type(), newValue)
    {}
};

#endif // QN_SCOPED_VALUE_ROLLBACK_H
