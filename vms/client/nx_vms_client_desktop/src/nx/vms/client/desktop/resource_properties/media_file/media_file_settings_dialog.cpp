// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "media_file_settings_dialog.h"

#include <QtCore/QPointer>
#include <QtCore/QScopedValueRollback>
#include <QtQml/QQmlEngine>
#include <QtQuick/QQuickItem>
#include <QtQuickWidgets/QQuickWidget>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFrame>
#include <QtWidgets/QVBoxLayout>

#include <client_core/client_core_module.h>
#include <core/resource/media_resource.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/thumbnails/local_media_thumbnail.h>

#include "../fisheye/fisheye_preview_controller.h"

namespace nx::vms::client::desktop {

namespace {

static constexpr QSize kDefaultSize{800, 800};

} // namespace

using namespace nx::vms::api;

struct MediaFileSettingsDialog::Private
{
    MediaFileSettingsDialog* const q;

    QnMediaResourcePtr resource;
    dewarping::MediaData parameters;
    bool updating = false;

    FisheyePreviewController* const fisheyePreviewController = new FisheyePreviewController(q);
    LocalMediaThumbnail* const mediaPreview = new LocalMediaThumbnail(q);
    QQuickWidget* const quickWidget = new QQuickWidget(qnClientCoreModule->mainQmlEngine(), q);

    QPointer<QQuickItem> rootObject;
};

MediaFileSettingsDialog::MediaFileSettingsDialog(QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    d(new Private{this})
{
    resize(kDefaultSize);
    setHelpTopic(this, HelpTopic::Id::MainWindow_MediaItem_Dewarping);

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins({});

    auto contentLayout = new QVBoxLayout();
    contentLayout->setContentsMargins(style::Metrics::kDefaultTopLevelMargins);

    auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    auto line = new QFrame(this);
    line->setFrameStyle(QFrame::HLine | QFrame::Sunken);
    setButtonBox(buttonBox);

    layout->addLayout(contentLayout);
    layout->addWidget(line);
    layout->addWidget(buttonBox);

    contentLayout->addWidget(d->quickWidget);

    d->quickWidget->setClearColor(palette().window().color());
    d->quickWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    d->quickWidget->setSource(QUrl("Nx/Items/DewarpingSettings.qml"));

    d->mediaPreview->setMaximumSize(LocalMediaThumbnail::kUnlimitedSize);

    if (!NX_ASSERT(d->rootObject = d->quickWidget->rootObject()))
        return;

    qnClientCoreModule->mainQmlEngine()->setObjectOwnership(d->mediaPreview,
        QQmlEngine::CppOwnership);

    d->rootObject->setProperty("previewSource", QVariant::fromValue(d->mediaPreview));
    d->rootObject->setProperty("helpTopic", HelpTopic::Id::MainWindow_MediaItem_Dewarping);

    connect(d->rootObject, SIGNAL(dataChanged()), this, SLOT(handleDataChanged()));
}

MediaFileSettingsDialog::~MediaFileSettingsDialog()
{
    // Required here for forward-declared scoped pointer destruction.
}

void MediaFileSettingsDialog::updateFromResource(const QnMediaResourcePtr& resource)
{
    if (d->resource == resource)
        return;

    QScopedValueRollback<bool> updateRollback(d->updating, true);

    d->resource = resource;
    const auto resourcePtr = resource->toResourcePtr();

    static const QString kWindowTitlePattern("%1 - %2");
    const QString caption = tr("File Settings");

    setWindowTitle(resourcePtr
        ? kWindowTitlePattern.arg(caption, resourcePtr->getName())
        : caption);

    d->mediaPreview->setResource(resourcePtr);
    if (!d->resource)
        return;

    d->parameters = resource->getDewarpingParams();

    if (!NX_ASSERT(d->rootObject))
        return;

    d->rootObject->setProperty("dewarpingEnabled", d->parameters.enabled);
    d->rootObject->setProperty("radius", d->parameters.radius);
    d->rootObject->setProperty("centerX", d->parameters.xCenter);
    d->rootObject->setProperty("centerY", d->parameters.yCenter);
    d->rootObject->setProperty("stretch", d->parameters.hStretch);
    d->rootObject->setProperty("rollCorrectionDegrees", d->parameters.fovRot);
    d->rootObject->setProperty("cameraMount", int(d->parameters.viewMode));
    d->rootObject->setProperty("cameraProjection", int(d->parameters.cameraProjection));
    d->rootObject->setProperty("alphaDegrees", d->parameters.sphereAlpha);
    d->rootObject->setProperty("betaDegrees", d->parameters.sphereBeta);
}

void MediaFileSettingsDialog::submitToResource(const QnMediaResourcePtr& resource)
{
    resource->setDewarpingParams(d->parameters);
}

void MediaFileSettingsDialog::handleDataChanged()
{
    if (d->updating || !NX_ASSERT(d->rootObject))
        return;

    d->parameters.enabled = d->rootObject->property("dewarpingEnabled").toBool();
    d->parameters.radius = d->rootObject->property("radius").toReal();
    d->parameters.xCenter = d->rootObject->property("centerX").toReal();
    d->parameters.yCenter = d->rootObject->property("centerY").toReal();
    d->parameters.hStretch = d->rootObject->property("stretch").toReal();
    d->parameters.fovRot = d->rootObject->property("rollCorrectionDegrees").toReal();
    d->parameters.sphereAlpha = d->rootObject->property("alphaDegrees").toReal();
    d->parameters.sphereBeta = d->rootObject->property("betaDegrees").toReal();

    d->parameters.viewMode = dewarping::FisheyeCameraMount(
        d->rootObject->property("cameraMount").toInt());

    d->parameters.cameraProjection = dewarping::CameraProjection(
        d->rootObject->property("cameraProjection").toInt());

    d->fisheyePreviewController->preview(d->resource, d->parameters);
}

} // namespace nx::vms::client::desktop
