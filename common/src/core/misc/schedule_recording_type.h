#ifndef SCHEDULE_RECORDING_TYPE_H
#define SCHEDULE_RECORDING_TYPE_H

namespace Qn {
    enum RecordingType {
        RecordingType_Run,
        RecordingType_MotionOnly,
        RecordingType_Never,
        RecordingType_MotionPlusLQ,

        RecordingType_Count
    };
}

#endif // SCHEDULE_RECORDING_TYPE_H
