// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <QtCore/QObject>

#include <core/resource/device_dependent_strings.h>

#include "action_fwd.h"

namespace nx::vms::client::desktop {

class WindowContext;

namespace menu {

class TextFactory: public QObject
{
public:
    /**
     * Constructor.
     *
     * \param parent                    Context-aware parent.
     */
    TextFactory(QObject* parent);

    /**
     * \param parameters                Action parameters.
     * \returns                         Actual text for the given action.
     */
    virtual QString text(const Parameters& parameters, WindowContext* context) const;
};

class DevicesNameTextFactory: public TextFactory
{
public:
    DevicesNameTextFactory(const QnCameraDeviceStringSet& stringSet, QObject* parent);
    virtual QString text(const Parameters& parameters, WindowContext* context) const override;
private:
    const QnCameraDeviceStringSet m_stringSet;
};

class FunctionalTextFactory: public TextFactory
{
public:
    using TextFunction = std::function<
        QString(const Parameters& parameters, WindowContext* context)>;

    FunctionalTextFactory(TextFunction&& textFunction, QObject* parent);
    virtual QString text(const Parameters& parameters, WindowContext* context) const override;

private:
    const TextFunction m_textFunction;

};

} // namespace menu
} // namespace nx::vms::client::desktop
