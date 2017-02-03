#pragma once

#include <functional>
#include <QtCore/QSharedPointer>

class QnRaiiGuard;
typedef QSharedPointer<QnRaiiGuard> QnRaiiGuardPtr;

class NX_UTILS_API QnRaiiGuard
{
public:
    typedef std::function<void()> Handler;

public:
    explicit QnRaiiGuard(const Handler& creationHandler, const Handler& destructionHandler);
    explicit QnRaiiGuard(const Handler& destructionHandler);

    QnRaiiGuard(QnRaiiGuard&& other);

    QnRaiiGuard(const QnRaiiGuard&) = delete;
    QnRaiiGuard& operator=(const QnRaiiGuard&) = delete;

    ~QnRaiiGuard();

    void finalize(); //< early destruction

    void enableDestructionHandler();
    void disableDestructionHandler();

    static QnRaiiGuardPtr create(
        const Handler& creationHandler, const Handler& destructionHandler);
    static QnRaiiGuardPtr createDestructible(const Handler& destructionHandler);
    static QnRaiiGuardPtr createEmpty();

private:
    Handler m_destructionHandler;
    bool m_destructionHandlerEnabled = true;
};

Q_DECLARE_METATYPE(QnRaiiGuardPtr)
