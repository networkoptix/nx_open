// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/log/log_main.h>

namespace nx {

template<typename Functor>
auto suppressExceptions(Functor worker)
{
    return
        [worker = std::forward<Functor>(worker)](auto&&... args)
        {
            try
            {
                return worker(std::forward<decltype(args)>(args)...);
            }
            catch (const std::exception& e)
            {
                NX_ERROR(NX_SCOPE_TAG, "Exception '%1' is caught", e.what());
            }
            catch (...)
            {
                NX_ERROR(NX_SCOPE_TAG, "Exception is caught");
            }
            using Result = decltype(worker(std::forward<decltype(args)>(args)...));
            if constexpr (std::is_same_v<void, Result>)
                return;
            else
                return Result();
        };
}

template<typename Functor, typename Scope, typename Message>
auto suppressExceptions(Functor worker, Scope scope, Message message)
{
    return
        [worker = std::forward<Functor>(worker),
            scope = std::forward<Scope>(scope),
            message = std::forward<Message>(message)](
            auto&&... args)
        {
            try
            {
                return worker(std::forward<decltype(args)>(args)...);
            }
            catch (const std::exception& e)
            {
                if constexpr (std::is_invocable_v<Message, decltype(args)...>)
                {
                    NX_ERROR(scope, "Exception '%1' is caught %2", e.what(),
                        message(std::forward<decltype(args)>(args)...));
                }
                else
                {
                    NX_ERROR(scope, "Exception '%1' is caught %2", e.what(), message);
                }
            }
            catch (...)
            {
                if constexpr (std::is_invocable_v<Message, decltype(args)...>)
                {
                    NX_ERROR(scope,
                        "Exception is caught %1", message(std::forward<decltype(args)>(args)...));
                }
                else
                {
                    NX_ERROR(scope, "Exception is caught %1", message);
                }
            }
            using Result = decltype(worker(std::forward<decltype(args)>(args)...));
            if constexpr (std::is_same_v<void, Result>)
                return;
            else
                return Result();
        };
}

template<typename Output, typename Input, typename Scope, typename Message>
std::function<Output(Input)> suppressExceptions(Output (*worker)(Input), Scope scope, Message message)
{
    return
        [worker, scope = std::forward<Scope>(scope), message = std::forward<Message>(message)](
            Input input)
        {
            try
            {
                return worker(input);
            }
            catch (const std::exception& e)
            {
                if constexpr (std::is_invocable_v<Message, Input>)
                    NX_ERROR(scope, "Exception '%1' is caught %2", e.what(), message(input));
                else
                    NX_ERROR(scope, "Exception '%1' is caught %2", e.what(), message);
            }
            catch (...)
            {
                if constexpr (std::is_invocable_v<Message, Input>)
                    NX_ERROR(scope, "Exception is caught %1", message(input));
                else
                    NX_ERROR(scope, "Exception is caught %1", message);
            }
            return Output();
        };
}

} // namespace nx
