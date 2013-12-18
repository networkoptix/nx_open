#ifndef QN_INSTANCE_STORAGE_H
#define QN_INSTANCE_STORAGE_H

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

        QByteArray key(typeid(T).name());
        QObject *&result = m_instanceByTypeName[key];
        if(!result) {
            result = new T(m_this.data());
            m_instances.push_back(result);
        }
        return static_cast<T *>(result);
    }

protected:
    void clear() {
        /* Destroy typed subobjects in reverse order to how they were constructed. */
        while(!m_instances.empty()) {
            QObject *instance = m_instances.back();
            m_instances.pop_back();
            m_instanceByTypeName.remove(typeid(*instance).name());
            delete instance;
        }
    }

private:
    Q_DISABLE_COPY(QnInstanceStorage);

    bool m_thisInitialized;
    QPointer<QObject> m_this;
    
    QHash<QByteArray, QObject *> m_instanceByTypeName; // TODO: #Elric use std::type_index
    QList<QObject *> m_instances;
};


#endif // QN_INSTANCE_STORAGE_H
