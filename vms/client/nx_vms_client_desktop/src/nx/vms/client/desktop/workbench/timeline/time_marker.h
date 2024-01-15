// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QTimeZone>
#include <QtGui/QColor>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/desktop/workbench/timeline/timeline_globals.h>
#include <nx/vms/client/desktop/workbench/widgets/bubble_tooltip.h>

namespace nx::vms::client::desktop {

class TextEditLabel;

} // namespace nx::vms::client::desktop

namespace nx::vms::client::desktop::workbench::timeline {

/**
 * Tooltip used for timeline Time Marker and Live Preview. Shows two lines of text with
 * specified date and time.
 */
class TimeMarker: public BubbleToolTip
{
    Q_OBJECT
    using base_type = BubbleToolTip;
    using Mode = TimeMarkerMode;

public:
    struct TimeContent
    {
        std::chrono::milliseconds position{};
        QTimeZone timeZone{QTimeZone::LocalTime};
        bool isTimestamp = true;
        std::chrono::milliseconds localFileLength{}; //< Should be 0 if archive is not local file.
        bool showDate = true;
        bool showMilliseconds = false;
    };

    explicit TimeMarker(QnWorkbenchContext* context, QObject* parent = nullptr);
    virtual ~TimeMarker() override;

    bool isShown() const;
    void setShown(bool value);

    QPointF pointerPos() const;

    Mode mode() const;
    void setMode(Mode value);

    /** Set date and time shown in text lines. */
    void setTimeContent(const TimeContent& value);

    /**
     * @param value If `TimeMarkerDisplay::full`, then both date and time are always shown.
     *     If `TimeMarkerDisplay::compact`, then either date or time is shown, depending on scale.
     *     If `TimeMarkerDisplay::automatic`, then either full or compact mode is chosen,
     *         depending on available width.
     */
    void setTimeMarkerDisplay(TimeMarkerDisplay value);

    /**
     * Point tooltip tail according to x, but align bottom edge of the tooltip to bottomY.
     * @param pointerX X coordinate for tooltip pointer tip, in global coordinate system.
     * @param bottomY Y coordinate to align tooltip bottom edge, in global coordinate system.
     * @param minX, maxX Horizontal boundaries for the body of the tooltip.
     *     Ignored if mode is not Mode::normal or if minX == maxX.
     *     Specified in global coordinate system.
     */
    void setPosition(int pointerX, int bottomY, int minX = 0, int maxX = 0);

protected:
    explicit TimeMarker(const QUrl& sourceUrl, QnWorkbenchContext* context,
        QObject* parent = nullptr);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop::workbench::timeline
