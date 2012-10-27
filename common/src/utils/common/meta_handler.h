#ifndef QN_META_HANDLER_H
#define QN_META_HANDLER_H

#include <QtCore/QMetaType>

// TODO: this is available in Qt5's QMetaType
class QnMetaHandler {
public:
    QnMetaHandler(int type): m_type(type) {}

    int type() const {
        return m_type;
    }

    virtual void construct(void *dst) const = 0;
    virtual void destroy(void *dst) const = 0;
    virtual void copy(const void *src, void *dst) const = 0;

private:
    int m_type;
};

template<class T>
class QnTypedMetaHandler: public QnMetaHandler {
public:
    QnTypedMetaHandler(): QnMetaHandler(qMetaTypeId<T>()) {}

    virtual void construct(void *dst) const override {
        new (dst) T();
    }

    virtual void destroy(void *dst) const override {
        static_cast<T *>(dst)->~T();
    }

    virtual void copy(const void *src, void *dst) const override {
        *static_cast<T *>(dst) = *static_cast<const T *>(src);
    }
};

#endif // QN_META_HANDLER_H
