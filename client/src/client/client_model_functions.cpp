#include <utils/common/model_functions.h>

#include "client_globals.h"
#include "client_model_types.h"

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


