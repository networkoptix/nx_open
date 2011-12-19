#ifndef _eve_parse_schedule_tasks_
#define _eve_parse_schedule_tasks_

#include "api/Types.h"
#include "core/resource/resource.h"
#include "core/misc/scheduleTask.h"

void parseScheduleTasks(QnScheduleTaskList& scheduleTasks, const QnApiScheduleTasks& xsdScheduleTasks, QnResourceFactory& /*resourceFactory*/)
{
    using xsd::api::scheduleTasks::ScheduleTasks;
    using xsd::api::resourceTypes::ParentIDs;
    using xsd::api::resourceTypes::PropertyTypes;
    using xsd::api::resourceTypes::ParamType;

    for (ScheduleTasks::scheduleTask_const_iterator i (xsdScheduleTasks.begin()); i != xsdScheduleTasks.end(); ++i)
    {
        QnScheduleTask scheduleTask(       i->id().c_str(),
                                           i->sourceId().c_str(),
                                           i->startTime(),
                                           i->endTime(),
                                           i->doRecordAudio(),
                                           (QnScheduleTask::RecordingType) i->recordType(),
                                           i->dayOfWeek(),
                                           i->beforeThreshold(),
                                           i->afterThreshold()
                                        );

        scheduleTasks.append(scheduleTask);
    }
}

#endif // _eve_parse_schedule_tasks_
