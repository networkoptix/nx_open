#include <client/client_model_types.h>

#include <nx/fusion/model_functions.h>

#include <client/client_globals.h>
#include <client/client_meta_types.h>

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS
    (Qn, ImageBehaviour,
    (Qn::ImageBehaviour::Stretch,   "Stretch")
    (Qn::ImageBehaviour::Crop,      "Crop")
    (Qn::ImageBehaviour::Fit,       "Fit")
    (Qn::ImageBehaviour::Tile,      "Tile")
)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnServerStorageKey, (datastream)(eq)(hash), (serverUuid)(storagePath));
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnLicenseWarningState, (datastream), (lastWarningTime));
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnBackgroundImage, (json)(eq), QnBackgroundImage_Fields, (brief, true));

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(
    Qn, PaneState,
    (Qn::PaneState::Unpinned, "Unpinned")
    (Qn::PaneState::Opened, "Opened")
    (Qn::PaneState::Closed, "Closed")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(
    Qn, WorkbenchPane,
    (Qn::WorkbenchPane::Title, "Title")
    (Qn::WorkbenchPane::Tree, "Tree")
    (Qn::WorkbenchPane::Notifications, "Notifications")
    (Qn::WorkbenchPane::Navigation, "Navigation")
    (Qn::WorkbenchPane::Calendar, "Calendar")
    (Qn::WorkbenchPane::Thumbnails, "Thumbnails")
)

QnBackgroundImage::QnBackgroundImage():
    enabled(false),
    name(),
    originalName(),
    mode(Qn::ImageBehaviour::Stretch),
    opacity(0.5)
{
}

qreal QnBackgroundImage::actualImageOpacity() const
{
    return enabled ? opacity : 0.0;
}

QnWorkbenchState::QnWorkbenchState()
{

}

bool QnWorkbenchState::isValid() const
{
    return !userId.isNull()
        && !localSystemId.isNull();
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnWorkbenchState, (json), QnWorkbenchState_Fields, (brief, true));
