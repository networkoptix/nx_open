// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "image_button_widget.h"

template<class Container, class ValidityChecker>
QnImageButtonWidget::StateFlags findValidState(QnImageButtonWidget::StateFlags flags, const Container &container, const ValidityChecker &isValid) {
    /* Some compilers don't allow expressions in case labels, so we have to
        * precalculate them. */
    enum LocalStateFlag {
        CHECKED = QnImageButtonWidget::Checked,
        HOVERED = QnImageButtonWidget::Hovered,
        DISABLED = QnImageButtonWidget::Disabled,
        PRESSED = QnImageButtonWidget::Pressed
    };

    const LocalStateFlag
    CHECKED_HOVERED_DISABLED_PRESSED =  LocalStateFlag (CHECKED | HOVERED | DISABLED | PRESSED),
    CHECKED_HOVERED_DISABLED =          LocalStateFlag (CHECKED | HOVERED | DISABLED),
    CHECKED_HOVERED =                   LocalStateFlag (CHECKED | HOVERED),
    CHECKED_DISABLED =                  LocalStateFlag (CHECKED | DISABLED),
    HOVERED_DISABLED =                  LocalStateFlag (HOVERED | DISABLED),
    CHECKED_HOVERED_PRESSED =           LocalStateFlag (CHECKED | HOVERED | PRESSED),
    CHECKED_DISABLED_PRESSED =          LocalStateFlag (CHECKED | DISABLED | PRESSED),
    HOVERED_DISABLED_PRESSED =          LocalStateFlag (HOVERED | DISABLED | PRESSED),
    CHECKED_PRESSED =                   LocalStateFlag (CHECKED | PRESSED),
    HOVERED_PRESSED =                   LocalStateFlag (HOVERED | PRESSED),
    DISABLED_PRESSED =                  LocalStateFlag (DISABLED | PRESSED);

    switch(flags) {
#define TRY(FLAGS)                                                              \
        if(isValid(container[(FLAGS)]))                                         \
            return static_cast<QnImageButtonWidget::StateFlags>(FLAGS);
    case CHECKED_HOVERED_DISABLED_PRESSED:
        TRY(CHECKED | HOVERED | DISABLED | PRESSED);
        /* Fall through. */
    case CHECKED_HOVERED_DISABLED:
        TRY(CHECKED | HOVERED | DISABLED);
        TRY(CHECKED | DISABLED);
        TRY(DISABLED);  /// Note: disabled state is always presented due to QIcon usage
        return {};
    case CHECKED_HOVERED:
        TRY(CHECKED | HOVERED);
        TRY(CHECKED);
        return {};
    case CHECKED_DISABLED_PRESSED:
        TRY(CHECKED | DISABLED | PRESSED);
        /* Fall through. */
    case CHECKED_DISABLED:
        TRY(CHECKED | DISABLED);
        TRY(DISABLED);  /// Note: disabled state is always presented due to QIcon usage
        return {};
    case HOVERED_DISABLED_PRESSED:
        TRY(HOVERED | DISABLED | PRESSED);
        /* Fall through. */
    case HOVERED_DISABLED:
        TRY(HOVERED | DISABLED);
        TRY(DISABLED);  /// Note: disabled state is always presented due to QIcon usage
        return {};
    case CHECKED_HOVERED_PRESSED:
        TRY(CHECKED | HOVERED | PRESSED);
        /* Fall through. */
    case CHECKED_PRESSED:
        TRY(CHECKED | PRESSED);
        /* Fall through. */
    case CHECKED:
        TRY(CHECKED);
        return {};
    case HOVERED:
        TRY(HOVERED);
        return {};
    case DISABLED_PRESSED:
        TRY(DISABLED | PRESSED);
        /* Fall through. */
    case DISABLED:
        TRY(DISABLED);
        return {};
    case HOVERED_PRESSED:
        TRY(HOVERED | PRESSED);
        /* Fall through. */
    case PRESSED:
        TRY(PRESSED);
        return {};
    case 0:
        return {};
    default:
        return {};
#undef TRY
    }
}
