#ifndef QN_EMITTER_H
#define QN_EMITTER_H

#include <QObject>

class QString;
class QVariant;

/**
 * QnEmitter is a simple class that defines some Qt stuff that requires moc
 * usage. 
 * 
 * Currently it defines several signals and slots, but more stuff can be 
 * added in the future. The main purpose of this class is to be used as 
 * an implementation detail in contexts where moc is not applicable 
 * (i.e. in <tt>.cpp</tt> files and in templates).
 */
class QnEmitter: public QObject {
    Q_OBJECT;
public:
    explicit QnEmitter(QObject *parent = NULL): QObject(parent) {}

signals:
    void signal();
    void signal(int);
    void signal(void *);
    void signal(const QString &);
    void signal(const QVariant &);

public slots:
    virtual void slot() {};
    virtual void slot(int) {};
    virtual void slot(void *) {};
    virtual void slot(const QString &) {};
    virtual void slot(const QVariant &) {};
};


#endif // QN_EMITTER_H
