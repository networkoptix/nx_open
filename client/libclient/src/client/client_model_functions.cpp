
#include <nx/fusion/model_functions.h>

#include <client/client_globals.h>
#include <client/client_meta_types.h>
#include <client/client_model_types.h>

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(Qn::ClientSkin,
    (Qn::LightSkin, "LightSkin")
    (Qn::DarkSkin, "DarkSkin")
);

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(Qn::BackgroundAnimationMode,
                                          (Qn::DefaultAnimation,    "DefaultAnimation")
                                          (Qn::RainbowAnimation,    "RainbowAnimation")
                                          (Qn::CustomAnimation,     "CustomAnimation")
                                          );

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(Qn::ImageBehaviour,
                                          (Qn::StretchImage,        "StretchImage")
                                          (Qn::CropImage,           "CropImage")
                                          (Qn::FitImage,            "FitImage")
                                          (Qn::TileImage,           "TileImage")
                                          );

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnWorkbenchState, (datastream), (currentLayoutIndex)(layoutUuids));
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnServerStorageKey, (datastream)(eq)(hash), (serverUuid)(storagePath));
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnLicenseWarningState, (datastream), (lastWarningTime));
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnClientBackground, (datastream), QnClientBackground_Fields);
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnPtzHotkey, (json), (id)(hotkey))

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(
    Qn::PaneState,
    (Qn::PaneState::Unpinned, "Unpinned")
    (Qn::PaneState::Opened, "Opened")
    (Qn::PaneState::Closed, "Closed")
    );

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(
    Qn::WorkbenchPane,
    (Qn::WorkbenchPane::Title, "Title")
    (Qn::WorkbenchPane::Tree, "Tree")
    (Qn::WorkbenchPane::Notifications, "Notifications")
    (Qn::WorkbenchPane::Navigation, "Navigation")
    (Qn::WorkbenchPane::Calendar, "Calendar")
    (Qn::WorkbenchPane::Thumbnails, "Thumbnails")
    );
