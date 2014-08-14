#include "context_aware.h"

#include <cassert>

#include <QtQml/QtQml>

#include "context.h"

QnContextAware::QnContextAware(QObject *parent):
    m_context(NULL)
{
    while(parent) {
        if(QnContextAware *contextAware = dynamic_cast<QnContextAware *>(parent)) {
            m_context = contextAware->context();
            return;
        }

        if(QnContext *context = dynamic_cast<QnContext *>(parent)) {
            m_context = context;
            return;
        }

        parent = parent->parent();
    }
}

QnContext *QnContextAware::context() const {
    if(m_context)
        return m_context;

    /* Try to get the context from the QML engine. */
    const QObject *object = dynamic_cast<const QObject *>(this);
    assert(object);

    QQmlContext *qmlContext = QtQml::qmlContext(object);
    assert(qmlContext);

    QnContext *result = qmlContext->contextProperty(lit("context")).value<QnContext *>();
    assert(result);

    m_context = result;
    return result;
}
