// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../action_executor.h"
#include "../engine.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API ActionExecutorBase: public ActionExecutor
{
public:
    ActionExecutorBase(QObject* parent = nullptr);

    template<class A, class T>
    void registerHandler(Engine* engine, void(T::*method)(const QSharedPointer<A>&))
    {
        T* executor = static_cast<T*>(this);
        auto handler =
            [executor, method](const ActionPtr& action)
            {
                const auto typedAction = action.staticCast<A>();
                std::invoke(method, executor, typedAction);
            };

        m_handlers[A::manifest().id] = std::move(handler);
        engine->addActionExecutor(A::manifest().id, this);
    }

    virtual void execute(const ActionPtr& action) override;

private:
    QMap<QString, std::function<void(const ActionPtr& action)>> m_handlers;
};

} // namespace nx::vms::rules
