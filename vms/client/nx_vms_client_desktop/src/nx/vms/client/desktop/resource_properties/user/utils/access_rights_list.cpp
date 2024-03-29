// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "access_rights_list.h"

#include <QtQml/QtQml>

#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/common/html/html.h>

namespace nx::vms::client::desktop {

namespace {

static const QString kIconsDir = "image://svg/skin/user_settings/access_rights/";

} // namespace

using namespace nx::vms::api;

AccessRightsList::AccessRightsList(QObject* parent):
    QObject(parent),
    m_items(
        []()
        {
            QVector<AccessRightDescriptor> result({
                AccessRightDescriptor{
                    .accessRight = AccessRight::view,
                    .name = tr("View Live"),
                    .description = common::html::bold(tr("View Live") + ".") + " " +
                        tr("Allows users to access a resource and view live footage from a camera."),
                    .icon = QUrl(kIconsDir + "view_live_24x20.svg")},
                AccessRightDescriptor{
                    .accessRight = AccessRight::audio,
                    .name = tr("Play Sound"),
                    .description = common::html::bold(tr("Access Audio") + ".") + " " +
                        tr("Allows users to access an audio stream from a Device. "
                            "Used in combination with View Live and/or View Archive."),
                    .icon = QUrl(kIconsDir + "sound_24x20.svg")},
                AccessRightDescriptor{
                    .accessRight = AccessRight::viewArchive,
                    .name = tr("View Archive"),
                    .description = common::html::bold(tr("View Archive")),
                    .icon = QUrl(kIconsDir + "view_archive_24x20.svg")},
                AccessRightDescriptor{
                    .accessRight = AccessRight::exportArchive,
                    .name = tr("Export Archive"),
                    .description = common::html::bold(tr("Export Archive")),
                    .icon = QUrl(kIconsDir + "export_archive_24x20.svg")},
                AccessRightDescriptor{
                    .accessRight = AccessRight::viewBookmarks,
                    .name = tr("View Bookmarks"),
                    .description = common::html::bold(tr("View Bookmarks")),
                    .icon = QUrl(kIconsDir + "view_bookmarks_24x20.svg")},
                AccessRightDescriptor{
                    .accessRight = AccessRight::manageBookmarks,
                    .name = tr("Manage Bookmarks"),
                    .description = common::html::bold(tr("Manage Bookmarks")),
                    .icon = QUrl(kIconsDir + "manage_bookmarks_24x20.svg")},
                AccessRightDescriptor{
                    .accessRight = AccessRight::userInput,
                    .name = tr("User Input"),
                    .description = common::html::bold(tr("User Input") + ".") + " " +
                        tr("Allows user to control PTZ, use 2-Way Audio, Soft Triggers and I/O buttons."),
                    .icon = QUrl(kIconsDir + "user_input_24x20.svg")},
                AccessRightDescriptor{
                    .accessRight = AccessRight::edit,
                    .name = tr("Edit Settings"),
                    .description = common::html::bold(tr("Edit Settings") + ".") + " " +
                        tr("Depending on the resource type it either allows user to modify device settings"
                        " or to control video wall."),
                    .icon = QUrl(kIconsDir + "edit_settings_24x20.svg")}});

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
