#ifndef CONTEXT_AWARE_H
#define CONTEXT_AWARE_H

class Context;

class ContextAware {
public:
    ContextAware(QObject *parent);

    Context *context() const {
        return m_context;
    }

private:
    Context *m_context;
};

#endif // CONTEXT_AWARE_H
