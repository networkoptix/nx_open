// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/vms/common/system_health/message_type.h>

class NX_VMS_CLIENT_DESKTOP_API QnSystemHealthStringsHelper: public QObject
{
    Q_OBJECT
public:
    using MessageType = nx::vms::common::system_health::MessageType;

    /** Text that is used where the most short common title is required, e.g. in settings. */
    static QString messageShortTitle(MessageType messageType);

    /** Text that is in notifications. */
    static QString messageNotificationTitle(MessageType messageType, const QSet<QnResourcePtr>& resources);

    /** Text that is used where the full description is required, e.g. in notification hints. */
    static QString messageTooltip(MessageType messageType, const QSet<QnResourcePtr>& resources);

    /** Text that used in description of notifications. */
    static QString messageDescription(MessageType messageType);

    // TODO: #vbutkevich move all formatting to widget side.
    /** Generates elided resource text for informers */
    static QString resourceText(
        const QStringList& resources, int maxWidth = 50, int maxResourcesLines = 3);
};
