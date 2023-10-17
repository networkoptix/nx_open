// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>

#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <nx/reflect/instrument.h>
#include <nx/utils/serialization/qt_core_types.h>
#include <nx/vms/client/desktop/menu/action.h>
#include <nx/vms/client/desktop/menu/action_parameters.h>

namespace nx::vms::client::desktop {
namespace joystick {

static const menu::IDType kModifierActionId = menu::RaiseCurrentItemAction;

/**
 * Describes location of field bits in HID report.
 *
 * Supposed data format:
 * Subsequence = first[-last]
 * FieldLocation = Subsequence(,Subsequence)*
 * where first and last are 0-indexed positions of the first and the last bit in bit subsequence;
 * last could be omitted, in this case the used subsequence would consist of only one bit.
 *
 * All subsequences are concatenated in the given order.
 * The resulting bit vector is interpreted as the field value.
 */
using FieldLocation = QString;

struct ActionDescriptor
{
    /** Action ID. */
    menu::IDType id = menu::IDType::NoAction;

    /** Configurable action parameters. */
    std::map<QString, QString> parameters;

    // In the future we may want to specify some conditions when this action should be executed.
    // A possible and backward-compatitible way to do so is to add some field like "state" here,
    // which should specify button state, applied modifiers and so on.
};
NX_REFLECTION_INSTRUMENT(ActionDescriptor, (id)(parameters));

struct AxisDescriptor
{
    /** Field location in the HID report. */
    FieldLocation bits;

    /**
     * Boundary values on the given axis.
     * C language convention is used, therefore hex values are allowed (e.g. 0xff).
     * Min value could exceed max value, in this case the axis would be inverted.
     */
    QString min;
    QString max;

    /** Neutral point on the given axis. Could be omitted, (min+max)/2 is used in this case. */
    QString mid;
    /** Values in [mid-bounce, mid+bounce] interval are interpreted as neutral position. */
    QString bounce;

    /** Values in [0.0, 1.0] interval are interpreted as position factor. */
    QString sensitivity;
};
NX_REFLECTION_INSTRUMENT(AxisDescriptor, (bits)(min)(max)(mid)(bounce)(sensitivity));

struct ButtonDescriptor
{
    /** Field location in the HID report. */
    FieldLocation bits;

    /** Name of the button (e.g. text written near it on the device). */
    QString name;

    /**
     * List of actions associated with the button.
     *
     * Currently, may contain one or two actions only. If the list contains two actions, the first
     * of them is used when no modifier buttons are pressed, and the second is used with modifier.
     * If only one action is specified, it's used for any modifier state.
     *
     * Only one action could be assigned to a modifier button. It is called on button release if
     * no more actions were executed while this modifier button was pressed.
     */
    QList<ActionDescriptor> actions;
};
NX_REFLECTION_INSTRUMENT(ButtonDescriptor, (bits)(name)(actions));

struct JoystickDescriptor
{
    /**
     * Vendor id and product id as hex numbers, concatenated.
     * Used to match found devices with appropriate descriptors.
     */
    QString id;

    /** Manufacturer name, optional. */
    QString manufacturer;

    /** Model name, optional. */
    QString model;

    /** Number of bits in HID report. Stored as a string, so all values in config are uniform. */
    QString reportSize;

    /** X-Axis description. */
    AxisDescriptor xAxis;

    /** Y-Axis description. */
    AxisDescriptor yAxis;

    /** Z-Axis (rotation) description. */
    AxisDescriptor zAxis;

    /** Buttons. */
    QList<ButtonDescriptor> buttons;
};
NX_REFLECTION_INSTRUMENT(JoystickDescriptor,
    (id)(manufacturer)(model)(reportSize)(xAxis)(yAxis)(zAxis)(buttons));

} // namespace joystick
} // namespace nx::vms::client::desktop
