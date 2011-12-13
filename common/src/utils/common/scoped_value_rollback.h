#ifndef QN_SCOPED_VALUE_ROLLBACK_H
#define QN_SCOPED_VALUE_ROLLBACK_H

#include <QtGlobal>

namespace detail {
    template<class T>
    struct QnScopedValueRollbackInit {
        T *variable;
        T value;
    };

    template<class T>
    QnScopedValueRollbackInit<T> scopedValueRollbackInit(T &variable, const T &newValue) {
        QnScopedValueRollbackInit<T> result;
        result.variable = &variable;
        result.value = variable;
        variable = newValue;
        return result;
    }

} // namespace detail


template<class T>
class QnScopedValueRollback {
public:
    QnScopedValueRollback(T &variable):
        m_variable(variable),
        m_value(variable),
        m_committed(false)
    {}

    QnScopedValueRollback(const detail::QnScopedValueRollbackInit<T> &init):
        m_variable(*init.variable),
        m_value(init.value),
        m_committed(false)
    {}

    ~QnScopedValueRollback() {
        if(!m_committed)
            m_variable = m_value;
    }

    void commit() {
        m_committed = true;
    }

    /**
     * This operator is here to make it possible to define scoped rollbacks in
     * if statement conditions.
     */
    operator bool() const {
        return false;
    }

private:
    Q_DISABLE_COPY(QnScopedValueRollback);

    T &m_variable;
    T m_value;
    bool m_committed;
};


#define QN_SCOPED_VALUE_ROLLBACK_INITIALIZER(VARIABLE, NEW_VALUE)               \
    ::detail::scopedValueRollbackInit(VARIABLE, NEW_VALUE)

#endif // QN_SCOPED_VALUE_ROLLBACK_H
