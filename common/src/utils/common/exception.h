#ifndef QN_EXCEPTION_H
#define QN_EXCEPTION_H

#include <exception>

#include <QtCore/QString>
#include <QtCore/QByteArray>

class QnException: virtual public std::exception {
public:
    QnException(const QString &message = QString()): m_message(message) {}

    virtual ~QnException() throw() {}

    virtual QString message() const throw() {
        return m_message;
    }

    virtual const char *what() const throw() override {
        /* Note that message() can be overridden, so we need to do latin1
         * conversion here, and not in constructor. */
        if(m_cachedWhat.isNull())
            m_cachedWhat = message().toLatin1();
        return m_cachedWhat.data();
    }

private:
    QString m_message;
    mutable QByteArray m_cachedWhat; 
};

#endif // QN_EXCEPTION_H
