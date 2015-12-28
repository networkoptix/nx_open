
#pragma once

#include <QtCore/QObject>

#include <ui/workbench/workbench_context_aware.h>

namespace statistics
{
    namespace details
    {
        class ActionsStatisticHandler;
    };

    class QnStatisticsManager
        : public QObject
        , public QnWorkbenchContextAware
    {
        Q_OBJECT

    public:
        QnStatisticsManager(QObject *parent);

        virtual ~QnStatisticsManager();

    public:

    private:
        typedef QScopedPointer<details::ActionsStatisticHandler> ActionsStatisticHandlerPtr;

        const ActionsStatisticHandlerPtr m_actionsHandler;
    };
}