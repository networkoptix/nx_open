#include "context_aware.h"

#include <cassert>

#include "context.h"

ContextAware::ContextAware(QObject *parent):
    m_context(NULL)
{
    while(true) {
        assert(parent);

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

