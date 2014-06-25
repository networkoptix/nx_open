#ifndef CONTEXT_AWARE_H
#define CONTEXT_AWARE_H

class Context;

class ContextAware {
public:
    ContextAware(QObject *parent = NULL);

    Context *context() const;

private:
    mutable Context *m_context;
};

#endif // CONTEXT_AWARE_H
