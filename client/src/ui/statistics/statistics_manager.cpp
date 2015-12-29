
#include "statistics_manager.h"

#include <ui/statistics/actions_statistics_handler.h>


#include <ui/actions/actions.h> // TODO: remove, for test

//

statistics::QnStatisticsManager::QnStatisticsManager(QObject *parent)
    : QObject(parent)
    , QnWorkbenchContextAware(parent)

    , m_actionsHandler(new details::ActionsStatisticHandler(parent))
{
    // TODO: #ynikitenkov Remove, for test
    m_actionsHandler->watchAction(Qn::OpenBookmarksSearchAction, lit("BookmarksSearchOpened")
        , details::ActionMetricParams(kTriggeredCount, details::kAllEmitters));
    m_actionsHandler->watchAction(Qn::BookmarksModeAction, lit("BookmarksModeActivationsCount")
        , details::ActionMetricParams(kActivationsCount, details::kAllEmitters));
    m_actionsHandler->watchAction(Qn::BookmarksModeAction, lit("BookmarksModeDectivationsCount")
        , details::ActionMetricParams(kDeactivationsCount, details::kAllEmitters));
    m_actionsHandler->watchAction(Qn::BookmarksModeAction, lit("BookmarksActiveMode")
        , details::ActionMetricParams(kActiveTime, details::kAllEmitters));
    m_actionsHandler->watchAction(Qn::BookmarksModeAction, lit("BookmarksInctiveMode")
        , details::ActionMetricParams(kInactiveTime, details::kAllEmitters));


}

statistics::QnStatisticsManager::~QnStatisticsManager()
{
}



