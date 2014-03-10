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
        if(m_cachedWhat.isNull())
            m_cachedWhat = message().toLocal8Bit();
        return m_cachedWhat.data();
    }

private:
    QString m_message;

    // TODO: #Elric uncache, throwing an exception is an expensive operation,
    // so there is no point trying to avoid an additional latin1 conversion.

    mutable QByteArray m_cachedWhat; 
};


#endif // QN_EXCEPTION_H
