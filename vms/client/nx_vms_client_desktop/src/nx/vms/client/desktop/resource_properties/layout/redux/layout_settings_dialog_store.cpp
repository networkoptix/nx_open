#include "layout_settings_dialog_store.h"

#include <QtCore/QScopedValueRollback>

#include <nx/vms/client/desktop/common/redux/private_redux_store.h>

#include "layout_settings_dialog_state.h"
#include "layout_settings_dialog_state_reducer.h"

namespace nx::vms::client::desktop {

using State = LayoutSettingsDialogState;
using Reducer = LayoutSettingsDialogStateReducer;

struct LayoutSettingsDialogStore::Private:
    PrivateReduxStore<LayoutSettingsDialogStore, State>
{
    using PrivateReduxStore::PrivateReduxStore;
};

LayoutSettingsDialogStore::LayoutSettingsDialogStore(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
}

LayoutSettingsDialogStore::~LayoutSettingsDialogStore() = default;

const State& LayoutSettingsDialogStore::state() const
{
    return d->state;
}

void LayoutSettingsDialogStore::loadLayout(const QnLayoutResourcePtr& layout)
{
    d->executeAction([&](State state) { return Reducer::loadLayout(std::move(state), layout); });
}

void LayoutSettingsDialogStore::setLocked(bool value)
{
    d->executeAction([&](State state) { return Reducer::setLocked(std::move(state), value); });
}

void LayoutSettingsDialogStore::setLogicalId(int value)
{
    d->executeAction([&](State state) { return Reducer::setLogicalId(std::move(state), value); });
}

void LayoutSettingsDialogStore::resetLogicalId()
{
    d->executeAction([](State state) { return Reducer::resetLogicalId(std::move(state)); });
}

void LayoutSettingsDialogStore::generateLogicalId()
{
    d->executeAction([](State state) { return Reducer::generateLogicalId(std::move(state)); });
}

void LayoutSettingsDialogStore::setFixedSizeEnabled(bool value)
{
    d->executeAction(
        [&](State state) { return Reducer::setFixedSizeEnabled(std::move(state), value); });
}

void LayoutSettingsDialogStore::setFixedSizeWidth(int value)
{
    d->executeAction(
        [&](State state) { return Reducer::setFixedSizeWidth(std::move(state), value); });
}

void LayoutSettingsDialogStore::setFixedSizeHeight(int value)
{
    d->executeAction(
        [&](State state) { return Reducer::setFixedSizeHeight(std::move(state), value); });
}

void LayoutSettingsDialogStore::setBackgroundImageError(const QString& errorText)
{
    d->executeAction(
        [&](State state) { return Reducer::setBackgroundImageError(std::move(state), errorText); });
}

void LayoutSettingsDialogStore::clearBackgroundImage()
{
    d->executeAction([](State state) { return Reducer::clearBackgroundImage(std::move(state)); });
}

void LayoutSettingsDialogStore::setBackgroundImageOpacityPercent(int value)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::setBackgroundImageOpacityPercent(std::move(state), value);
        });
}

void LayoutSettingsDialogStore::setBackgroundImageWidth(int value)
{
    d->executeAction(
        [&](State state) { return Reducer::setBackgroundImageWidth(std::move(state), value); });
}

void LayoutSettingsDialogStore::setBackgroundImageHeight(int value)
{
    d->executeAction(
        [&](State state) { return Reducer::setBackgroundImageHeight(std::move(state), value); });
}

void LayoutSettingsDialogStore::setCropToMonitorAspectRatio(bool value)
{
    d->executeAction(
        [&](State state)
        {
            return Reducer::setCropToMonitorAspectRatio(std::move(state), value);
        });
}

void LayoutSettingsDialogStore::setKeepImageAspectRatio(bool value)
{
    d->executeAction(
        [&](State state) { return Reducer::setKeepImageAspectRatio(std::move(state), value); });
}

void LayoutSettingsDialogStore::startDownloading(const QString& targetPath)
{
    d->executeAction(
        [&](State state) { return Reducer::startDownloading(std::move(state), targetPath); });
}

void LayoutSettingsDialogStore::imageDownloaded()
{
    d->executeAction([](State state) { return Reducer::imageDownloaded(std::move(state)); });
}

void LayoutSettingsDialogStore::imageSelected(const QString& filename)
{
    d->executeAction(
        [&](State state) { return Reducer::imageSelected(std::move(state), filename); });
}

void LayoutSettingsDialogStore::startUploading()
{
    d->executeAction([](State state) { return Reducer::startUploading(std::move(state)); });
}

void LayoutSettingsDialogStore::imageUploaded(const QString& filename)
{
    d->executeAction(
        [&](State state) { return Reducer::imageUploaded(std::move(state), filename); });
}

void LayoutSettingsDialogStore::setPreview(const QImage& image)
{
    d->executeAction([&](State state) { return Reducer::setPreview(std::move(state), image); });
}

} // namespace nx::vms::client::desktop
