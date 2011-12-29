#ifndef QN_EXCEPTION_H
#define QN_EXCEPTION_H

#include <utils/common/config.h>
#include <exception>
#include <QString>
#include <QByteArray>

class QnException: virtual public std::exception {
public:
    QnException(const QString &message = QString()): m_message(message) {}

    virtual ~Exception() noexcept {}

    virtual QString message() const noexcept {
        return m_message;
    }

    virtual const char *what() const noexcept override {
        if(mCachedWhat.isNull())
            mCachedWhat = message().toLocal8Bit();
        return mCachedWhat.data();
    }

private:
    QString m_message;
    mutable QByteArray m_cachedWhat;
};


#endif // QN_EXCEPTION_H
