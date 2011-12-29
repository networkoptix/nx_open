#ifndef QN_EXCEPTION_H
#define QN_EXCEPTION_H

#include <utils/common/config.h>
#include <exception>
#include <QString>
#include <QByteArray>

class QnException: virtual public std::exception {
public:
    QnException(const QString &message = QString()): m_message(message) {}

    virtual ~QnException() noexcept {}

    virtual QString message() const noexcept {
        return m_message;
    }

    virtual const char *what() const noexcept override {
        if(m_cachedWhat.isNull())
            m_cachedWhat = message().toLocal8Bit();
        return m_cachedWhat.data();
    }

private:
    QString m_message;
    mutable QByteArray m_cachedWhat;
};


#endif // QN_EXCEPTION_H
