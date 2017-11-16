#pragma once

#include <functional>
#include <list>
#include <map>

#include <nx/utils/singleton.h>

#include "http_types.h"

namespace nx_http {

/**
 * Reponsible for modifying HTTP request.
 */
class NX_NETWORK_API HttpModManager:
    public QObject
{
    Q_OBJECT

    using base_type = QObject;

public:
    HttpModManager(QObject* parent = nullptr);

    /**
     * Performs some modifications on request.
     */
    void apply(Request* const request);

    /**
     * Replaces path originalPath to effectivePath.
     */
    void addUrlRewriteExact(const QString& originalPath, const QString& effectivePath);
    /**
     * Register functor that will receive every incoming request
     * before any processing and have a chance to modify it.
     */
    void addCustomRequestMod(std::function<void(Request*)> requestMod);

private:
    std::map<QString, QString> m_urlRewriteExact;
    std::list<std::function<void(Request*)>> m_requestModifiers;
};

} // namespace nx_http
