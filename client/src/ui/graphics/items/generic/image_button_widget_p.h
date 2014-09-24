#ifndef QN_IMAGE_BUTTON_WIDGET_P_H
#define QN_IMAGE_BUTTON_WIDGET_P_H

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
        TRY(CHECKED);
        return 0;
    case CHECKED_HOVERED:
        TRY(CHECKED | HOVERED);
        TRY(CHECKED);
        return 0;
    case CHECKED_DISABLED_PRESSED:
        TRY(CHECKED | DISABLED | PRESSED);
        /* Fall through. */
    case CHECKED_DISABLED:
        TRY(CHECKED | DISABLED);
        TRY(CHECKED);
        return 0;
    case HOVERED_DISABLED_PRESSED:
        TRY(HOVERED | DISABLED | PRESSED);
        /* Fall through. */
    case HOVERED_DISABLED:
        TRY(HOVERED | DISABLED);
        TRY(DISABLED);
        return 0;
    case CHECKED_HOVERED_PRESSED:
        TRY(CHECKED | HOVERED | PRESSED);
        /* Fall through. */
    case CHECKED_PRESSED:
        TRY(CHECKED | PRESSED);
        /* Fall through. */
    case CHECKED:
        TRY(CHECKED);
        return 0;
    case HOVERED:
        TRY(HOVERED);
        return 0;
    case DISABLED_PRESSED:
        TRY(DISABLED | PRESSED);
        /* Fall through. */
    case DISABLED:
        TRY(DISABLED);
        return 0;
    case HOVERED_PRESSED:
        TRY(HOVERED | PRESSED);
        /* Fall through. */
    case PRESSED:
        TRY(PRESSED);
        return 0;
    case 0:
        return 0;
    default:
        return 0;
#undef TRY
    }
}

#endif // QN_IMAGE_BUTTON_WIDGET_P_H
