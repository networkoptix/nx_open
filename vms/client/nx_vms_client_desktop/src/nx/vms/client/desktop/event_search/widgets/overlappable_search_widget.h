// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QWidget>

#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::desktop {

class AbstractSearchWidget;

/**
 * Wrapper around AbstractSearchWidget which functional mightn't be supported in case of some
 * condition.
 */
// TODO: #mmalofeev make this class extensible in case of new conditions appears.
class OverlappableSearchWidget: public QWidget
{
    Q_OBJECT

public:
    OverlappableSearchWidget(
        AbstractSearchWidget* searchWidget, //< Takes ownership.
        QWidget* parent = nullptr);
    ~OverlappableSearchWidget() override;

    AbstractSearchWidget* searchWidget() const;

    enum class Appearance
    {
        searchWidget, //< Actual search widget will be displayed.
        overlay //< Placeholder will be displayed.
    };
    void setAppearance(Appearance appearance);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
