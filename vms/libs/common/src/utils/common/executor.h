#ifndef QN_EXUCUTOR_H
#define QN_EXUCUTOR_H

#include <QtCore/QObject>

class QnExecutor: public QObject {
    Q_OBJECT
public:
    QnExecutor(QObject *parent): QObject(parent) {}

public slots:
    virtual void execute() = 0;
};


template<class Functor>
class QnFunctorExecutor: public QnExecutor {
public:
    QnFunctorExecutor(const Functor &functor, QObject *parent = NULL): QnExecutor(parent), m_functor(functor) {}

    virtual void execute() override {
        m_functor();
    }

private:
    Functor m_functor;
};

template<class Functor>
QnFunctorExecutor<Functor> *newExecutor(const Functor &functor, QObject *parent = NULL) {
    return new QnFunctorExecutor<Functor>(functor, parent);
}

#endif // QN_EXUCUTOR_H
