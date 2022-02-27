// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QHash>
#include <QSharedPointer>
#include <nx/utils/thread/mutex.h>

class QOpenGLWidget;

template<class T>
class QnGlContextDataStardardFactory
{
public:
    T *operator()(QOpenGLWidget* /*glWidget*/)
    {
        return new T();
    }
};

template<class T>
class QnGlContextDataForwardingFactory
{
public:
    T *operator()(QOpenGLWidget* glWidget)
    {
        return new T(glWidget);
    }
};

template<class T, class Factory = QnGlContextDataForwardingFactory<T>>
class QnGlContextData: public QObject
{
public:
    using PointerType = QSharedPointer<T>;
    using HashType = QHash<const QOpenGLWidget*, PointerType>;

    QnGlContextData(const Factory &factory = Factory()):
        m_factory(factory)
    {
    }

    PointerType get(QOpenGLWidget* glWidget)
    {
        NX_MUTEX_LOCKER locker(&m_mutex);

        typename HashType::iterator pos = m_map.find(glWidget);
        if (pos == m_map.end())
        {
            pos = m_map.insert(glWidget, QSharedPointer<T>(m_factory(glWidget)));
            connect(glWidget, &QObject::destroyed, this,
                [this, glWidget]()
                {
                    NX_MUTEX_LOCKER lock(&m_mutex);
                    m_map.remove(glWidget);
                });
        }

        return *pos;
    }

private:
    nx::Mutex m_mutex;
    Factory m_factory;
    HashType m_map;
};
