// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <memory>

#include <QtCore/QList>
#include <QtCore/QString>

namespace nx::vms::client::desktop {

/** Reports results from multiple callbacks into a single callback. */
class ResultsReporter
{
    ResultsReporter(std::function<void(bool)> callback): m_callback(std::move(callback))
    {
    }

public:
    template <typename ... Args>
    static std::shared_ptr<ResultsReporter> create(Args&& ... args)
    {
        struct makeSharedEnabler: public ResultsReporter
        {
            makeSharedEnabler(Args&& ... args): ResultsReporter(std::forward<Args>(args)...) {}
        };
        return std::make_shared<makeSharedEnabler>(std::forward<Args>(args)...);
    }

    ~ResultsReporter()
    {
        if (m_callback)
            m_callback(m_errors.empty());
    }

    void add(bool success, QString message = {})
    {
        if (!success)
            m_errors.append(message);
    }

    QList<QString> errors() const { return m_errors; }

private:
    std::function<void(bool)> m_callback;
    QList<QString> m_errors;
};

} // namespace nx::vms::client::desktop
