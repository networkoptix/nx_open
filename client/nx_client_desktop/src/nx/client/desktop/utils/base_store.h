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
    void patchApplied();
};

} // namespace details

template<typename State, typename Patch>
class BaseStore: public details::StoreDetail
{
    using base_type = details::StoreDetail;

public:
    BaseStore(QObject* parent = nullptr);

    const State& state() const;
    void setState(const State& state);

    using ReduceFunction = std::function<State (State&&)>;
    void execute(ReduceFunction reduce);

    using ApplyPatchFunction = std::function<State (State&&)>;
    void applyPatch(const Patch& patch);
    const Patch& lastPatch() const;

private:
    State m_state;
    Patch m_lastPatch;
    bool m_actionInProgress;
};

template<typename State, typename Patch>
BaseStore<State, Patch>::BaseStore(QObject* parent):
    base_type(parent),
    m_actionInProgress(false)
{
}

template<typename State, typename Patch>
void BaseStore<State, Patch>::execute(BaseStore::ReduceFunction reduce)
{
    if (!reduce || m_actionInProgress)
        return;

    const QScopedValueRollback<bool> guard(m_actionInProgress, true);
    m_state = reduce(std::move(m_state));
    emit stateChanged();
}

template<typename State, typename Patch>
void BaseStore<State, Patch>::applyPatch(const Patch& patch)
{
    if (m_actionInProgress)
        return;

    const QScopedValueRollback<bool> guard(m_actionInProgress, true);
    m_state = patch.apply(std::move(m_state));
    m_lastPatch = patch;
    emit patchApplied();
}

template<typename State, typename Patch>
const Patch& BaseStore<State, Patch>::lastPatch() const
{
    return m_lastPatch;
}

template<typename State, typename Patch>
const State& BaseStore<State, Patch>::state() const
{
    return m_state;
}

template<typename State, typename Patch>
void BaseStore<State, Patch>::setState(const State& state)
{
    execute([state](const State& /* oldState */) { return state; });
}

} // namespace desktop
} // namespace client
} // namespace nx
