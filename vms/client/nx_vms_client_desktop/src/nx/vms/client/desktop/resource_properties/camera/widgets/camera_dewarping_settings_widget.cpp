// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_dewarping_settings_widget.h"

#include <QtQml/QQmlEngine>
#include <QtQuick/QQuickItem>

#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/thumbnails/live_camera_thumbnail.h>

#include "../flux/camera_settings_dialog_store.h"

namespace nx::vms::client::desktop {

namespace {

static constexpr QSize kMinimumSize(800, 400);

} // namespace

CameraDewarpingSettingsWidget::CameraDewarpingSettingsWidget(
    CameraSettingsDialogStore* store,
    QSharedPointer<LiveCameraThumbnail> thumbnail,
    QQmlEngine* engine,
    QWidget* parent)
    :
    base_type(engine, parent),
    m_thumbnail(thumbnail)
{
    setClearColor(palette().window().color());
    setResizeMode(QQuickWidget::SizeRootObjectToView);
    setSource(QUrl("Nx/Dialogs/CameraSettings/CameraDewarpingSettings.qml"));
    setMinimumSize(kMinimumSize);

    if (!NX_ASSERT(rootObject()))
        return;

    if (NX_ASSERT(thumbnail))
        engine->setObjectOwnership(thumbnail.get(), QQmlEngine::CppOwnership);

    setHelpTopic(this, HelpTopic::Id::MainWindow_MediaItem_Dewarping);

    rootObject()->setProperty("store", QVariant::fromValue(store));
    rootObject()->setProperty("previewSource", QVariant::fromValue(thumbnail.get()));
}

} // namespace nx::vms::client::desktop
