// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <array>

#include <QtWidgets/QWidget>
#include <QtCore/QVariant>

#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::desktop {

class ScheduleCellPainter;

/**
 * Widget that provides graphical input of an schedule with breakdown by each hour for each day
 * of week. Representation of the grid control is locale / system configuration aware, this applies
 * on which day will be displayed as a first day of week and time format. It acts as controller and
 * container for the user defined data stored as QVariant values. Stored data interpretation and
 * representation is delegated to some <tt>ScheduleCellPainter</tt> descendant instance.
 */
class ScheduleGridWidget: public QWidget
{
    Q_OBJECT
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)

    using base_type = QWidget;

public:
    static constexpr int kDaysInWeek = 7;
    static constexpr int kHoursInDay = 24;

    /**
     * @return Array with all Qt::DayOfWeek enumeration values to be able iterate over them without
     *     type casting. No other purposes implied.
     */
    static constexpr inline std::array<Qt::DayOfWeek, kDaysInWeek> daysOfWeek()
    {
        return {Qt::Monday, Qt::Tuesday, Qt::Wednesday, Qt::Thursday, Qt::Friday,
            Qt::Saturday, Qt::Sunday};
    }

public:
    explicit ScheduleGridWidget(QWidget* parent = nullptr);
    virtual ~ScheduleGridWidget() override;

    /**
     * Brush is the user defined value which will be set to schedule grid cells as result of
     * interaction with widget via mouse.
     * @return Current brush value, <tt>QVariant()</tt> by default.
     */
    QVariant brush() const;

    /**
     * Sets new brush value. Brush value updating and validation is solely caller responsibility.
     * @param value Any data suitable for a specific implementation, may be null <tt>QVariant</tt>
     *     as well.
     */
    void setBrush(const QVariant& value);

    /**
     * Sets cell painter.
     */
    void setCellPainter(ScheduleCellPainter* cellPainter);

    /**
     * @param day Constant from <tt>Qt::DayOfWeek</tt> enumeration. Value is not associated with
     *     certain schedule grid row since day which will be represented as the first day of week
     *     is locale / system preferences dependent.
     * @param hour Value from [0, 23] range.
     * @return Data from schedule grid cell associated with given day of week and hour.
     *     <tt>QVariant()</tt> by default.
     */
    QVariant cellData(Qt::DayOfWeek day, int hour) const;

    /**
     * Sets user data to the schedule grid cell associated with given day of week and hour.
     * @param day Constant from <tt>Qt::DayOfWeek</tt> enumeration. Value is not associated with
     *     certain schedule grid row since day which will be represented as the first day of week
     *     is locale / system preferences dependent.
     * @param hour Value from [0, 23] range.
     * @param value Any value, implementation dependent. Comparison operators should be registered
     *     via <tt>QMetaType::registerComparators</tt> for custom types, change notifications
     *     won't be working otherwise.
     */
    void setCellData(Qt::DayOfWeek day, int hour, const QVariant& value);

    /**
     * Fills whole schedule grid with the same value.
     */
    void fillGridData(const QVariant& value);

    /**
     * Fills whole schedule grid with null values. Equivalent to <tt>fillGridData(QVariant())</tt>
     * call.
     */
    void resetGridData();

    /**
     * @return True if schedule grid isn't user interactable, false otherwise. Schedule grid is
     *     not read only by default.
     */
    bool isReadOnly() const;

    /**
     * Sets whether schedule grid should be user interactable or not. Stored data may be altered
     *     programmatically disregarding read only state.
     */
    void setReadOnly(bool value);

    /**
     * Active grid widget contains actual tasks. Widget may be set inactive e.g. when we are
     * editing several cameras with different schedules.
     * @return Whether schedule grid is currently active or not.
     */
    bool isActive() const;

    /**
     * Sets whether schedule grid should be active or not.
     */
    void setActive(bool value);

signals:
    /**
     * Data input is done by mouse, but widget became non-interactive if keyboard modifier applied.
     * This feature may be used for implementing custom actions performed on grid cells clicking.
     * For example, applying data associated with clicked grid cells as active brush.
     */
    void cellClicked(Qt::DayOfWeek day, int hour, Qt::KeyboardModifiers modifiers);

    /**
     * Signal is emitted whenever the grid data changes.
     */
    void gridDataChanged();

    /**
     * Signal is emitted whenever the grid data is edited by user, it's not emitted when the grid
     * data is changed programmatically.
     */
    void gridDataEdited();

public:
    virtual bool hasHeightForWidth() const override;
    virtual int heightForWidth(int width) const override;
    virtual QSize minimumSizeHint() const override;

protected:
    virtual void mouseMoveEvent(QMouseEvent* event) override;
    virtual void mousePressEvent(QMouseEvent* event) override;
    virtual void mouseReleaseEvent(QMouseEvent* event) override;
    virtual void leaveEvent(QEvent* event) override;
    virtual void paintEvent(QPaintEvent* event) override;
    virtual void resizeEvent(QResizeEvent* event) override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
