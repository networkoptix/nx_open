#ifndef _eve_parse_schedule_tasks_
#define _eve_parse_schedule_tasks_

#include "api/Types.h"
#include "core/resource/resource.h"
#include "core/misc/scheduleTask.h"

void parseScheduleTasks(QnScheduleTaskList& scheduleTasks, const QnApiScheduleTasks& xsdScheduleTasks, QnResourceFactory& /*resourceFactory*/);

#endif // _eve_parse_schedule_tasks_
