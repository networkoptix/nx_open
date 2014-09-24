#ifndef QN_INSTANCE_STORAGE_H
#define QN_INSTANCE_STORAGE_H

#include <type_traits>

#include <QtCore/QObject>
#include <QtCore/QByteArray>
#include <QtCore/QPointer>
#include <QtCore/QList>
#include <QtCore/QHash>

class QnInstanceStorage {
public:
    QnInstanceStorage(): m_thisInitialized(false) {}

    virtual ~QnInstanceStorage() {
        clear();
    }

    template<class T>
    T *instance() {
        if(!m_thisInitialized) {
            m_this = dynamic_cast<QObject *>(this);
            m_thisInitialized = true;
        }

        QObject *&result = m_instanceByMetaObject[&T::staticMetaObject];
        if(!result) {
            result = new T(m_this.data());
            m_instances.push_back(result);
        }
        return static_cast<T *>(result);
    }

protected:
    template<class T, class I>
    void store(I *instance) {
        static_assert(std::is_convertible<I *, T *>::value, "Provided value must be convertible to the specified instance type T.");

        assert(!m_instanceByMetaObject.contains(&T::staticMetaObject));

        m_instanceByMetaObject.insert(&T::staticMetaObject, instance);
        m_instances.push_back(instance);
    }

protected:
    void clear() {
        /* Destroy typed subobjects in reverse order to how they were constructed. */
        while(!m_instances.empty()) {
            QObject *instance = m_instances.back();
            m_instances.pop_back();
            m_instanceByMetaObject.remove(instance->metaObject());
            delete instance;
        }
    }

private:
    Q_DISABLE_COPY(QnInstanceStorage);

    bool m_thisInitialized;
    QPointer<QObject> m_this;
    
    QHash<const QMetaObject *, QObject *> m_instanceByMetaObject;
    QList<QObject *> m_instances;
};


#endif // QN_INSTANCE_STORAGE_H
