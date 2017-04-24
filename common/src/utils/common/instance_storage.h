#pragma once

#include <cassert>
#include <nx/utils/log/assert.h>

#include <type_traits>

#include <QtCore/QObject>
#include <QtCore/QByteArray>
#include <QtCore/QPointer>
#include <QtCore/QList>
#include <QtCore/QHash>

class QnInstanceStorage
{
public:
    QnInstanceStorage() {}

    virtual ~QnInstanceStorage()
    {
        clear();
    }

    template<class T>
    T* instance()
    {
        if (!m_thisInitialized)
        {
            m_this = dynamic_cast<QObject*>(this);
            m_thisInitialized = true;
        }

        QObject*& result = m_instanceByMetaObject[&T::staticMetaObject];
        if (!result)
        {
            result = new T(m_this.data());
            m_instances.push_back(result);
        }

        return static_cast<T*>(result);
    }

    // TODO: Rename findInstance to instance and remove the original method.
    template<class T>
    T* findInstance()
    {
        return static_cast<T*>(m_instanceByMetaObject.value(&T::staticMetaObject));
    }

protected:
    template<class T>
    T* store(T* instance)
    {
        NX_ASSERT(&T::staticMetaObject != &QObject::staticMetaObject,
            Q_FUNC_INFO, "Do you forget to add Q_OBJECT macro?");
        NX_ASSERT(!m_instanceByMetaObject.contains(&T::staticMetaObject));

        m_instanceByMetaObject.insert(&T::staticMetaObject, instance);
        m_instances.push_back(instance);

        return instance;
    }

protected:
    void clear()
    {
        /* Destroy typed subobjects in reverse order to how they were constructed. */
        while (!m_instances.empty())
        {
            QObject* instance = m_instances.back();
            m_instances.pop_back();
            m_instanceByMetaObject.remove(instance->metaObject());
            delete instance;
        }
    }

private:
    Q_DISABLE_COPY(QnInstanceStorage)

    bool m_thisInitialized = false;
    QPointer<QObject> m_this;
    
    QHash<const QMetaObject*, QObject*> m_instanceByMetaObject;
    QList<QObject*> m_instances;
};
