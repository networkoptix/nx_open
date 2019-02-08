#include "manual_camera_seach_reply.h"

#include <nx/fusion/serialization/lexical.h>
#include <nx/fusion/serialization/lexical_enum.h>


QString toString(QnManualResourceSearchStatus::State state)
{
    return QnLexical::serialized(state);
}

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(QnManualResourceSearchStatus, State,
    (QnManualResourceSearchStatus::Init, "Init")
    (QnManualResourceSearchStatus::CheckingOnline, "CheckingOnline")
    (QnManualResourceSearchStatus::CheckingHost, "CheckingHost")
    (QnManualResourceSearchStatus::Finished, "Finished")
    (QnManualResourceSearchStatus::Aborted, "Aborted")
    (QnManualResourceSearchStatus::Count, "Count")
)
