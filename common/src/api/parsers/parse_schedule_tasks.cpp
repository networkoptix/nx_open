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
        QnScheduleTask scheduleTask(       i->id().present() ? (*(i->id())).c_str() : "",
                                           i->sourceId().present() ? (*(i->sourceId())).c_str() : "",
                                           i->dayOfWeek(),
                                           i->startTime(),
                                           i->endTime(),
                                           (QnScheduleTask::RecordingType) i->recordType(),
                                           i->beforeThreshold(),
                                           i->afterThreshold(),
                                           (QnStreamQuality)i->streamQuality(),
                                           i->fps(),
                                           i->doRecordAudio()
                                        );

        scheduleTasks.append(scheduleTask);
    }
}
