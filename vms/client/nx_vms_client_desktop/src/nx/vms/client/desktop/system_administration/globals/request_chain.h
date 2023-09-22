// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <QtCore/QString>

#include <nx/utils/move_only_func.h>

namespace nx::vms::client::desktop {

/**
 * Allows to sequentially execute async requests and to stop when the request fails.
 */
template <typename T>
class RequestChain
{
public:
    using ChainCompleteCallback = nx::utils::MoveOnlyFunc<void(bool, const QString&)>;

public:
    virtual ~RequestChain()
    {
        if (m_chainComplete)
            m_chainComplete(/*success*/ !hasNext(), {});
    }

    template<typename ...V>
    void append(V&& ... args)
    {
        m_chain.emplace_back(std::forward<V>(args)...);
    }

    bool hasNext() const
    {
        return m_next >= 0 && m_next < (int) m_chain.size();
    }

    void start(ChainCompleteCallback callback)
    {
        m_chainComplete = std::move(callback);
        requestNext();
    }

    size_t size() const { return m_chain.size(); }

    int nextIndex() const { return m_next; }

    void requestComplete(bool success, const QString& errorString = {})
    {
        if (!success)
        {
            if (m_chainComplete)
            {
                m_chainComplete(success, errorString);
                m_chainComplete = {};
            }
            return;
        }

        if (success)
            requestNext();
    }

protected:
    /** Override this method to make a request. Must call advance(). */
    virtual void makeRequest() = 0;

    const T& peekNext() const { return m_chain[m_next]; }
    void advance() { ++m_next; }

    T& operator[](int i) { return m_chain[i]; }
    const T& operator[](int i) const { return m_chain[i]; }

private:
    void requestNext()
    {
        if (!hasNext())
        {
            if (m_chainComplete)
            {
                m_chainComplete(true, {});
                m_chainComplete = {};
            }
            return;
        }

        const auto next = m_next;
        makeRequest();
        NX_ASSERT(next != m_next);
    }

private:
    std::vector<T> m_chain;
    int m_next = 0;
    ChainCompleteCallback m_chainComplete;
};

} // namespace nx::vms::client::desktop
