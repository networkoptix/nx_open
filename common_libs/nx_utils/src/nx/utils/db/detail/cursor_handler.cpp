#include "cursor_handler.h"

#include <nx/utils/type_utils.h>

namespace nx {
namespace utils {
namespace db {
namespace detail {

CursorCreator::CursorCreator(
    CursorHandlerPool* cursorContextPool,
    std::unique_ptr<AbstractCursorHandler> cursorHandler)
    :
    base_type(QueryType::lookup),
    m_cursorContextPool(cursorContextPool),
    m_cursorHandler(std::move(cursorHandler))
{
}

void CursorCreator::reportErrorWithoutExecution(DBResult errorCode)
{
    m_cursorHandler->reportErrorWithoutExecution(errorCode);
}

DBResult CursorCreator::executeQuery(QSqlDatabase* const connection)
{
    m_cursorHandler->initialize(connection); //< Throws on failure.

    m_cursorContextPool->cursors.emplace(
        m_cursorHandler->id(),
        std::move(m_cursorHandler));

    return DBResult::ok;
}

//-------------------------------------------------------------------------------------------------

} // namespace detail
} // namespace db
} // namespace utils
} // namespace nx
