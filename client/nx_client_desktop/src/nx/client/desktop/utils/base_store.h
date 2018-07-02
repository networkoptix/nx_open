#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedValueRollback>

namespace nx {
namespace client {
namespace desktop {

namespace details {

class StoreDetail: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    StoreDetail(QObject* parent = nullptr);

signals:
    void stateChanged();
};

} // namespace details

template<typename State>
class BaseStore: public details::StoreDetail
{
    using base_type = details::StoreDetail;

public:
    BaseStore(QObject* parent = nullptr);

    const State& state() const;
    void setState(const State& state);

    using ReduceFunction = std::function<State (State&&)>;
    void execute(ReduceFunction reduce);

private:
    State m_state;
    bool m_actionInProgress;
};

template<typename State>
BaseStore<State>::BaseStore(QObject* parent):
    base_type(parent),
    m_actionInProgress(false)
{
}

template<typename State>
void BaseStore<State>::execute(BaseStore::ReduceFunction reduce)
{
    if (!reduce || m_actionInProgress)
        return;

    const QScopedValueRollback<bool> guard(m_actionInProgress, true);
    m_state = reduce(std::move(m_state));
    emit stateChanged();
}

template<typename State>
const State& BaseStore<State>::state() const
{
    return m_state;
}

template<typename State>
void BaseStore<State>::setState(const State& state)
{
    execute([state](const State& /* oldState */) { return state; });
}

} // namespace desktop
} // namespace client
} // namespace nx
