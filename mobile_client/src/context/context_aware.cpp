#include "context_aware.h"

#include <cassert>

#include <QtQml/QtQml>

#include "context.h"

ContextAware::ContextAware(QObject *parent):
    m_context(NULL)
{
    while(parent) {
        if(ContextAware *contextAware = dynamic_cast<ContextAware *>(parent)) {
            m_context = contextAware->context();
            return;
        }

        if(Context *context = dynamic_cast<Context *>(parent)) {
            m_context = context;
            return;
        }

        parent = parent->parent();
    }
}

Context *ContextAware::context() const {
    if(m_context)
        return m_context;

    /* Try to get the context from the QML engine. */
    QObject *object = dynamic_cast<QObject *>(this);
    assert(object);

    QQmlContext *qmlContext = QtQml::qmlContext(object);
    assert(qmlContext);

    Context *result = qmlContext->contextProperty(lit("context")).value<Context *>();
    assert(result);

    m_context = result;
    return result;
}
