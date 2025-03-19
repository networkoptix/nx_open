// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>

#include <nx/utils/log/assert.h>

#include "application_context.h"

namespace nx::vms::client::core {

template<typename ContextType>
class QmlContextInitializer
{
public:
    ContextType* context() const
    {
        auto qmlContext = QQmlEngine::contextForObject(m_owner);

        while (NX_ASSERT(qmlContext))
        {
            auto result = qmlContext->contextProperty(m_qmlPropertyName);
            if (result.isValid())
                return result.template value<ContextType*>();

            qmlContext = qmlContext->parentContext();
        }

        return nullptr;
    }

protected:
    QmlContextInitializer(QObject* owner, const QString& qmlPropertyName):
        m_owner(owner),
        m_qmlPropertyName(qmlPropertyName)
    {
    }

    static void storeToQmlContext(ContextType* context, const QString& qmlPropertyName)
    {
        if (const auto engine = appContext()->qmlEngine())
            engine->rootContext()->setContextProperty(qmlPropertyName, context);
    }

private:
    const QPointer<QObject> m_owner;
    const QString m_qmlPropertyName;
};

} // namespace nx::vms::client::core
