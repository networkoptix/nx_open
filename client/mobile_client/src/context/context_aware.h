#ifndef CONTEXT_AWARE_H
#define CONTEXT_AWARE_H

#include <common/common_globals.h>

#include <QtCore/QObject>

class QnContext;

class QnContextAware {
public:
    QnContextAware(QObject *parent = NULL);
    virtual ~QnContextAware() {}

    QnContext *context() const;

private:
    mutable QnContext *m_context;
};

#endif // CONTEXT_AWARE_H
