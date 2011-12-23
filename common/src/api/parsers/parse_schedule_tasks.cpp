#include "api/parsers/parse_schedule_tasks.h"

#include "core/resource/media_resource.h"

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
                                           i->afterThreshold(),
                                           (QnStreamQuality)i->streamQuality(),
                                           i->fps()
                                        );

        scheduleTasks.append(scheduleTask);
    }
}
