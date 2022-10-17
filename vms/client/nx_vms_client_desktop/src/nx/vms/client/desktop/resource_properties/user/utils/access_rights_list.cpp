// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "access_rights_list.h"

#include <QtQml/QtQml>

namespace nx::vms::client::desktop {

namespace {

static const QString kIconsDir = "image://svg/skin/user_settings/access_rights/";

} // namespace

using namespace nx::vms::api;

AccessRightsList::AccessRightsList(QObject* parent):
    QObject(parent),
    m_items({
        AccessRightDescriptor{
            .accessRight = AccessRight::viewLive,
            .name = tr("View Live"),
            .description = tr("Can view live footage"),
            .icon = QUrl(kIconsDir + "view_live.svg")},
        AccessRightDescriptor{
            .accessRight = AccessRight::listenToAudio,
            .name = tr("Listen to Audio"),
            .description = tr("Can listen to audio from devices"),
            .icon = QUrl(kIconsDir + "listen_to_audio.svg")},
        AccessRightDescriptor{
            .accessRight = AccessRight::viewArchive,
            .name = tr("View Archive"),
            .description = tr("Can view archive footage"),
            .icon = QUrl(kIconsDir + "view_archive.svg")},
        AccessRightDescriptor{
            .accessRight = AccessRight::exportArchive,
            .name = tr("Export Archive"),
            .description = tr("Can export parts of archive"),
            .icon = QUrl(kIconsDir + "export_archive.svg")},
        AccessRightDescriptor{
            .accessRight = AccessRight::viewBookmarks,
            .name = tr("View Bookmarks"),
            .description = tr("Can view bookmarks"),
            .icon = QUrl(kIconsDir + "view_bookmarks.svg")},
        AccessRightDescriptor{
            .accessRight = AccessRight::manageBookmarks,
            .name = tr("Manage Bookmarks"),
            .description = tr("Can modify bookmarks"),
            .icon = QUrl(kIconsDir + "manage_bookmarks.svg")},
        AccessRightDescriptor{
            .accessRight = AccessRight::userInput,
            .name = tr("User Input"),
            .description =
                tr("Can change camera PTZ state, use Soft Triggers, 2-Way Audio and I/O buttons"),
            .icon = QUrl(kIconsDir + "user_input.svg")},
        AccessRightDescriptor{
            .accessRight = AccessRight::editSettings,
            .name = tr("Edit Settings"),
            .description = tr("Can edit device settings"),
            .icon = QUrl(kIconsDir + "edit_settings.svg")}})
{
    QQmlEngine::setObjectOwnership(this, QQmlEngine::CppOwnership);
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
