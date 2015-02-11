#ifndef QN_GL_CONTEXT_DATA_H
#define QN_GL_CONTEXT_DATA_H

#include <QHash>
#include <utils/common/mutex.h>
#include <QSharedPointer>
#include <QtOpenGL/QGLContext>

template<class T>
class QnGlContextDataStardardFactory {
public:
    T *operator()(const QGLContext *) {
        return new T();
    }
};

template<class T>
class QnGlContextDataForwardingFactory {
public:
    T *operator()(const QGLContext *context) {
        return new T(context);
    }
};


template<class T, class Factory = QnGlContextDataForwardingFactory<T> >
class QnGlContextData {
public:
    typedef QHash<const QGLContext *, QSharedPointer<T> > map_type;

    QnGlContextData(const Factory &factory = Factory()): m_factory(factory) {}

    QSharedPointer<T> get(const QGLContext *context) {
        SCOPED_MUTEX_LOCK( locked, &m_mutex);

        typename map_type::iterator pos = m_map.find(context);
        if(pos == m_map.end())
            pos = m_map.insert(context, QSharedPointer<T>(m_factory(context)));

        return *pos;
    }

private:
    QnMutex m_mutex;
    Factory m_factory;
    map_type m_map;
};

#endif // QN_GL_CONTEXT_DATA_H