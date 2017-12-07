#include "cursor_handler.h"

#include <nx/utils/type_utils.h>

namespace nx {
namespace utils {
namespace db {
namespace detail {

void CursorHandlerPool::add(
    QnUuid id,
    std::unique_ptr<AbstractCursorHandler> cursorHandler)
{
    m_cursors.emplace(id, std::move(cursorHandler));
}

AbstractCursorHandler* CursorHandlerPool::cursorHander(QnUuid id)
{
    QnMutexLocker lock(&m_mutex);
    auto it = m_cursors.find(id);
    return it == m_cursors.end() ? nullptr : it->second.get();
}

int CursorHandlerPool::cursorCount() const
{
    QnMutexLocker lock(&m_mutex);
    return (int) m_cursors.size();
}

void CursorHandlerPool::remove(QnUuid id)
{
    QnMutexLocker lock(&m_mutex);
    m_cursors.erase(id);
}

void CursorHandlerPool::cleanupDroppedCursors()
{
    std::vector<std::unique_ptr<AbstractCursorHandler>> cursorsToDrop;
    {
        QnMutexLocker lock(&m_mutex);

        for (const auto& cursorId: m_cursorsMarkedForDeletion)
        {
            auto it = m_cursors.find(cursorId);
            if (it != m_cursors.end())
            {
                cursorsToDrop.emplace_back(std::move(it->second));
                m_cursors.erase(it);
            }
        }
        m_cursorsMarkedForDeletion.clear();
    }

    cursorsToDrop.clear();
}

void CursorHandlerPool::markCursorForDeletion(QnUuid id)
{
    QnMutexLocker lock(&m_mutex);
    m_cursorsMarkedForDeletion.push_back(id);
}

//-------------------------------------------------------------------------------------------------

BasicCursorOperationExecutor::BasicCursorOperationExecutor(
    CursorHandlerPool* cursorContextPool)
    :
    base_type(QueryType::lookup),
    m_cursorContextPool(cursorContextPool)
{
}

CursorHandlerPool* BasicCursorOperationExecutor::cursorContextPool()
{
    return m_cursorContextPool;
}

DBResult BasicCursorOperationExecutor::executeQuery(QSqlDatabase* const connection)
{
    m_cursorContextPool->cleanupDroppedCursors();

    executeCursor(connection);

    return DBResult::ok;
}

//-------------------------------------------------------------------------------------------------

CursorCreator::CursorCreator(
    CursorHandlerPool* cursorContextPool,
    std::unique_ptr<AbstractCursorHandler> cursorHandler)
    :
    base_type(cursorContextPool),
    m_cursorHandler(std::move(cursorHandler))
{
}

void CursorCreator::reportErrorWithoutExecution(DBResult errorCode)
{
    m_cursorHandler->reportErrorWithoutExecution(errorCode);
}

void CursorCreator::executeCursor(QSqlDatabase* const connection)
{
    m_cursorHandler->initialize(connection); //< Throws on failure.

    cursorContextPool()->add(m_cursorHandler->id(), std::move(m_cursorHandler));
}

//-------------------------------------------------------------------------------------------------

CleanUpDroppedCursorsExecutor::CleanUpDroppedCursorsExecutor(
    CursorHandlerPool* cursorContextPool)
    :
    base_type(cursorContextPool)
{
}

void CleanUpDroppedCursorsExecutor::reportErrorWithoutExecution(DBResult /*errorCode*/)
{
}

void CleanUpDroppedCursorsExecutor::executeCursor(QSqlDatabase* const /*connection*/)
{
    // No need to do anything. Cleanup is done before this call.
}

} // namespace detail
} // namespace db
} // namespace utils
} // namespace nx
