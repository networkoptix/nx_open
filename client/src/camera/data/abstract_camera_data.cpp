#include "abstract_camera_data.h"

Qn::CameraDataType timePeriodToDataType(const Qn::TimePeriodContent timePeriodType) {
    switch (timePeriodType) {
    case Qn::RecordingContent:
        return Qn::RecordedTimePeriod;
    case Qn::MotionContent:
        return Qn::MotionTimePeriod;
    case Qn::BookmarksContent:
        return Qn::BookmarkTimePeriod;
    default:
        assert(false); //should never get here
        break;
    }
    return Qn::CameraDataTypeCount;
}

Qn::TimePeriodContent dataTypeToPeriod(const Qn::CameraDataType dataType) {
    switch (dataType)
    {
    case Qn::RecordedTimePeriod:
        return Qn::RecordingContent;
    case Qn::MotionTimePeriod:
        return Qn::MotionContent;
    case Qn::BookmarkTimePeriod:
        return Qn::BookmarksContent;
    default:
        assert(false); //should never get here
        break;
    }
    return Qn::TimePeriodContentCount;
}