    struct ScheduleTaskData : ApiData
    {
        ScheduleTaskData(): startTime(0), endTime(0), doRecordAudio(false), recordType(Qn::RecordingType_Run), dayOfWeek(1), 
                        beforeThreshold(0), afterThreshold(0), streamQuality(Qn::QualityNotDefined), fps(0.0) {}

        qint32   startTime;
        qint32   endTime;
        bool     doRecordAudio;
        Qn::RecordingType   recordType;
        qint8    dayOfWeek;
        qint16   beforeThreshold;
        qint16   afterThreshold;
        Qn::StreamQuality  streamQuality;
        qint16   fps;
    };

    #define apiScheduleTaskFields (startTime) (endTime) (doRecordAudio) (recordType) (dayOfWeek) (beforeThreshold) (afterThreshold) (streamQuality) (fps) 
    QN_DEFINE_STRUCT_SERIALIZATORS(ScheduleTaskData, apiScheduleTaskFields);
