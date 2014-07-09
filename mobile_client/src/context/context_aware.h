#ifndef CONTEXT_AWARE_H
#define CONTEXT_AWARE_H

#include <common/common_globals.h>

#include <QtCore/QObject>

class Context;

class ContextAware {
public:
    ContextAware(QObject *parent = NULL);
    virtual ~ContextAware() {}

    Context *context() const;

private:
    mutable Context *m_context;
};

#endif // CONTEXT_AWARE_H
