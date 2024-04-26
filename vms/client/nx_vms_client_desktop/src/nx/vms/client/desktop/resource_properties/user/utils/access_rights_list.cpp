// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "access_rights_list.h"

#include <QtQml/QtQml>

#include <api/helpers/access_rights_helper.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/common/html/html.h>

namespace nx::vms::client::desktop {

using namespace nx::vms::api;

AccessRightsList::AccessRightsList(QObject* parent):
    QObject(parent),
    m_items(
        []()
        {
            QVector<AccessRightDescriptor> result({
                AccessRightDescriptor{
                    .accessRight = AccessRight::view,
                    .name = common::AccessRightHelper::name(AccessRight::view),
                    .description = common::html::bold(tr("View Live.")) + " " +
                        tr("Allows users to access a resource and view live footage from a camera."),
                    .icon = QUrl("24x20/Solid/live.svg")},
                AccessRightDescriptor{
                    .accessRight = AccessRight::audio,
                    .name = common::AccessRightHelper::name(AccessRight::audio),
                    .description = common::html::bold(tr("Access Audio") + ".") + " " +
                        tr("Allows users to access an audio stream from a Device. "
                            "Used in combination with View Live and/or View Archive."),
                    .icon = QUrl("24x20/Solid/audio.svg")},
                AccessRightDescriptor{
                    .accessRight = AccessRight::viewArchive,
                    .name = common::AccessRightHelper::name(AccessRight::viewArchive),
                    .description = common::html::bold(tr("View Archive")),
                    .icon = QUrl("24x20/Solid/video.svg")},
                AccessRightDescriptor{
                    .accessRight = AccessRight::exportArchive,
                    .name = common::AccessRightHelper::name(AccessRight::exportArchive),
                    .description = common::html::bold(tr("Export Archive")),
                    .icon = QUrl("24x20/Solid/video_export.svg")},
                AccessRightDescriptor{
                    .accessRight = AccessRight::viewBookmarks,
                    .name = common::AccessRightHelper::name(AccessRight::viewBookmarks),
                    .description = common::html::bold(tr("View Bookmarks")),
                    .icon = QUrl("24x20/Solid/bookmarks.svg")},
                AccessRightDescriptor{
                    .accessRight = AccessRight::manageBookmarks,
                    .name = common::AccessRightHelper::name(AccessRight::manageBookmarks),
                    .description = common::html::bold(tr("Manage Bookmarks")),
                    .icon = QUrl("24x20/Solid/bookmark_edit.svg")},
                AccessRightDescriptor{
                    .accessRight = AccessRight::userInput,
                    .name = common::AccessRightHelper::name(AccessRight::userInput),
                    .description = common::html::bold(tr("User Input.")) + " " +
                        tr("Allows user to control PTZ, use 2-Way Audio, Soft Triggers and I/O buttons."),
                    .icon = QUrl("24x20/Solid/userinput.svg")},
                AccessRightDescriptor{
                    .accessRight = AccessRight::edit,
                    .name = common::AccessRightHelper::name(AccessRight::edit),
                    .description = common::html::bold(tr("Edit Settings.")) + " " +
                        tr("Depending on the resource type it either allows user to modify device settings"
                        " or to control video wall."),
                    .icon = QUrl("24x20/Solid/camera_edit.svg")}});

            return result;
        }()),
    m_indexLookup(
        [this]()
        {
            QHash<AccessRight, int> result;
            for (int index = 0; index < m_items.size(); ++index)
                result.insert(m_items[index].accessRight, index);
            return result;
        }())
{
    QQmlEngine::setObjectOwnership(this, QQmlEngine::CppOwnership);
}

AccessRightDescriptor AccessRightsList::get(AccessRight accessRight) const
{
    const auto it = m_indexLookup.find(accessRight);
    return NX_ASSERT(it != m_indexLookup.cend()) ? m_items[*it] : AccessRightDescriptor{};
}

AccessRightsList* AccessRightsList::instance()
{
    static AccessRightsList staticInstance;
    return &staticInstance;
}

void AccessRightsList::registerQmlTypes()
{
    qRegisterMetaType<QVector<AccessRightDescriptor>>();

    qmlRegisterUncreatableType<AccessRightDescriptor>("nx.vms.client.desktop", 1, 0,
        "AccessRightDescriptor", "Cannot create an instance of AccessRightDescriptor");

    qmlRegisterSingletonType<AccessRightsList>(
        "nx.vms.client.desktop", 1, 0, "AccessRightsList",
        [](QQmlEngine* /*qmlEngine*/, QJSEngine* /*jsEngine*/) -> QObject*
        {
            return instance();
        });
}

QVector<AccessRightDescriptor> AccessRightsList::items() const
{
    return m_items;
}

} // namespace nx::vms::client::desktop
