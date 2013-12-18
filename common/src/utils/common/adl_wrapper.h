#ifndef QN_ADL_WRAPPER_H
#define QN_ADL_WRAPPER_H

/**
 * A wrapper class that can be used to wrap values before invoking functions
 * that are to be found via ADL.
 * 
 * It adds global namespace into ADL namespace lookup list and prevents
 * type conversions from kicking in when picking the function to invoke.
 */
template<class T>
class QnAdlWrapper {
public:
    QnAdlWrapper(const T &value): m_value(value) {}

    operator const T &() const {
        return m_value;
    }

private:
    const T &m_value;
};

template<class T>
QnAdlWrapper<T> adlWrap(const T &value) {
    return QnAdlWrapper<T>(value);
}

#endif // QN_ADL_WRAPPER_H
