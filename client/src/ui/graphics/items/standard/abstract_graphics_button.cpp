#include "abstract_graphics_button.h"
#include "abstract_graphics_button_p.h"

#include <QtCore/QEvent>
#include <QtCore/QPointer>

#include <QtGui/QAbstractItemView>
#ifndef QT_NO_ACCESSIBILITY
#  include <QtGui/QAccessible>
#endif
#include <QtGui/QAction>
#include <QtGui/QApplication>
#include <QtGui/QGraphicsSceneEvent>
#include <QtGui/QStyle>

#define AUTO_REPEAT_DELAY  300
#define AUTO_REPEAT_INTERVAL 100

/*!
    \class AbstractGraphicsButton

    \brief The AbstractGraphicsButton class is the abstract base class of
    button widgets, providing functionality common to buttons.

    \ingroup abstractwidgets

    This class implements an \e abstract button. Subclasses of this class handle
    user actions, and specify how the button is drawn.

    AbstractGraphicsButton provides support for both push buttons and checkable
    (toggle) buttons. Checkable buttons are implemented in the QRadioButton
    and QCheckBox classes. Push buttons are implemented in the
    QPushButton and QToolButton classes; these also provide toggle
    behavior if required.

    Any button can display a label containing text and an icon. setText()
    sets the text; setIcon() sets the icon. If a button is disabled, its label
    is changed to give the button a "disabled" appearance.

    If the button is a text button with a string containing an ampersand ('&'),
    AbstractGraphicsButton automatically creates a shortcut key. For example:

    \snippet doc/src/snippets/code/src_gui_widgets_qabstractbutton.cpp 0

    The \key Alt+C shortcut is assigned to the button, i.e., when the
    user presses \key Alt+C the button will call animateClick(). See
    the \l {QShortcut#mnemonic}{QShortcut} documentation for details
    (to display an actual ampersand, use '&&').

    You can also set a custom shortcut key using the setShortcut()
    function. This is useful mostly for buttons that do not have any
    text, because they have no automatic shortcut.

    \snippet doc/src/snippets/code/src_gui_widgets_qabstractbutton.cpp 1

    All of the buttons provided by Qt (QPushButton, QToolButton,
    QCheckBox, and QRadioButton) can display both \l text and \l{icon}{icons}.

    A button can be made the default button in a dialog are provided by
    QPushButton::setDefault() and QPushButton::setAutoDefault().

    AbstractGraphicsButton provides most of the states used for buttons:

    \list

    \o isDown() indicates whether the button is \e pressed down.

    \o isChecked() indicates whether the button is \e checked.  Only
    checkable buttons can be checked and unchecked (see below).

    \o isEnabled() indicates whether the button can be pressed by the
    user.

    \o setAutoRepeat() sets whether the button will auto-repeat if the
    user holds it down. \l autoRepeatDelay and \l autoRepeatInterval
    define how auto-repetition is done.

    \o setCheckable() sets whether the button is a toggle button or not.

    \endlist

    The difference between isDown() and isChecked() is as follows.
    When the user clicks a toggle button to check it, the button is first
    \e pressed then released into the \e checked state. When the user
    clicks it again (to uncheck it), the button moves first to the
    \e pressed state, then to the \e unchecked state (isChecked() and
    isDown() are both false).

    AbstractGraphicsButton provides four signals:

    \list 1

    \o pressed() is emitted when the left mouse button is pressed while
    the mouse cursor is inside the button.

    \o released() is emitted when the left mouse button is released.

    \o clicked() is emitted when the button is first pressed and then
    released, when the shortcut key is typed, or when click() or
    animateClick() is called.

    \o toggled() is emitted when the state of a toggle button changes.

    \endlist

    To subclass AbstractGraphicsButton, you must reimplement at least
    paintEvent() to draw the button's outline and its text or pixmap. It
    is generally advisable to reimplement sizeHint() as well, and
    sometimes hitButton() (to determine whether a button press is within
    the button). For buttons with more than two states (like tri-state
    buttons), you will also have to reimplement checkStateSet() and
    nextCheckState().

    \sa GraphicsButtonGroup
*/

AbstractGraphicsButtonPrivate::AbstractGraphicsButtonPrivate(QSizePolicy::ControlType type):
#ifndef QT_NO_SHORTCUT
      shortcutId(0),
#endif
      checkable(false), checked(false), autoRepeat(false), autoExclusive(false),
      down(false), blockRefresh(false), pressed(false),
#ifndef QT_NO_GRAPHICSBUTTONGROUP
      group(0),
#endif
      autoRepeatDelay(AUTO_REPEAT_DELAY),
      autoRepeatInterval(AUTO_REPEAT_INTERVAL),
      controlType(type)
{
}

#ifndef QT_NO_GRAPHICSBUTTONGROUP

class GraphicsButtonGroupPrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(GraphicsButtonGroup)

public:
    GraphicsButtonGroupPrivate() : exclusive(true) {}

    QList<AbstractGraphicsButton *> buttonList;
    QPointer<AbstractGraphicsButton> checkedButton;
    void detectCheckedButton();
    void notifyChecked(AbstractGraphicsButton *button);
    bool exclusive;
    QHash<AbstractGraphicsButton*, int> mapping;
};

GraphicsButtonGroup::GraphicsButtonGroup(QObject *parent)
    : QObject(*new GraphicsButtonGroupPrivate, parent)
{
}

GraphicsButtonGroup::~GraphicsButtonGroup()
{
    Q_D(GraphicsButtonGroup);
    for (int i = 0; i < d->buttonList.count(); ++i)
        d->buttonList.at(i)->d_func()->group = 0;
}

bool GraphicsButtonGroup::exclusive() const
{
    return d_func()->exclusive;
}

void GraphicsButtonGroup::setExclusive(bool exclusive)
{
    d_func()->exclusive = exclusive;
}

void GraphicsButtonGroup::addButton(AbstractGraphicsButton *button)
{
    addButton(button, -1);
}

void GraphicsButtonGroup::addButton(AbstractGraphicsButton *button, int id)
{
    Q_D(GraphicsButtonGroup);
    if (GraphicsButtonGroup *previous = button->d_func()->group)
        previous->removeButton(button);
    button->d_func()->group = this;
    d->buttonList.append(button);
    if (id == -1) {
        QList<int> ids = d->mapping.values();
        if (ids.isEmpty())
           d->mapping[button] = -2;
        else {
            qSort(ids);
            d->mapping[button] = ids.first()-1;
        }
    } else {
        d->mapping[button] = id;
    }

    if (d->exclusive && button->isChecked())
        button->d_func()->notifyChecked();
}

void GraphicsButtonGroup::removeButton(AbstractGraphicsButton *button)
{
    Q_D(GraphicsButtonGroup);
    if (d->checkedButton == button)
        d->detectCheckedButton();
    if (button->d_func()->group == this) {
        button->d_func()->group = 0;
        d->buttonList.removeAll(button);
        d->mapping.remove(button);
    }
}

QList<AbstractGraphicsButton*> GraphicsButtonGroup::buttons() const
{
    return d_func()->buttonList;
}

AbstractGraphicsButton *GraphicsButtonGroup::checkedButton() const
{
    return d_func()->checkedButton;
}

AbstractGraphicsButton *GraphicsButtonGroup::button(int id) const
{
    return d_func()->mapping.key(id);
}

int GraphicsButtonGroup::id(AbstractGraphicsButton *button) const
{
    return d_func()->mapping.value(button, -1);
}

void GraphicsButtonGroup::setId(AbstractGraphicsButton *button, int id)
{
    if (button && id != -1)
        d_func()->mapping[button] = id;
}

int GraphicsButtonGroup::checkedId() const
{
    Q_D(const GraphicsButtonGroup);
    return d->mapping.value(d->checkedButton, -1);
}

// detect a checked button other than the current one
void GraphicsButtonGroupPrivate::detectCheckedButton()
{
    AbstractGraphicsButton *previous = checkedButton;
    checkedButton = 0;
    if (exclusive)
        return;
    for (int i = 0; i < buttonList.count(); i++) {
        if (buttonList.at(i) != previous && buttonList.at(i)->isChecked()) {
            checkedButton = buttonList.at(i);
            return;
        }
    }
}

#endif // QT_NO_GRAPHICSBUTTONGROUP

QList<AbstractGraphicsButton *>AbstractGraphicsButtonPrivate::queryButtonList() const
{
#ifndef QT_NO_GRAPHICSBUTTONGROUP
    if (group)
        return group->d_func()->buttonList;
#endif

    QList<AbstractGraphicsButton*>candidates = qFindChildren<AbstractGraphicsButton *>(q_func()->parent());
    if (autoExclusive) {
        for (int i = candidates.count() - 1; i >= 0; --i) {
            AbstractGraphicsButton *candidate = candidates.at(i);
            if (!candidate->autoExclusive()
#ifndef QT_NO_GRAPHICSBUTTONGROUP
                || candidate->group()
#endif
                )
                candidates.removeAt(i);
        }
    }
    return candidates;
}

AbstractGraphicsButton *AbstractGraphicsButtonPrivate::queryCheckedButton() const
{
#ifndef QT_NO_GRAPHICSBUTTONGROUP
    if (group)
        return group->d_func()->checkedButton;
#endif

    Q_Q(const AbstractGraphicsButton);
    QList<AbstractGraphicsButton *> buttonList = queryButtonList();
    if (!autoExclusive || buttonList.count() == 1) // no group
        return 0;

    for (int i = 0; i < buttonList.count(); ++i) {
        AbstractGraphicsButton *b = buttonList.at(i);
        if (b->d_func()->checked && b != q)
            return b;
    }
    return checked  ? const_cast<AbstractGraphicsButton *>(q) : 0;
}

void AbstractGraphicsButtonPrivate::notifyChecked()
{
#ifndef QT_NO_GRAPHICSBUTTONGROUP
    Q_Q(AbstractGraphicsButton);
    if (group) {
        AbstractGraphicsButton *previous = group->d_func()->checkedButton;
        group->d_func()->checkedButton = q;
        if (group->d_func()->exclusive && previous && previous != q)
            previous->nextCheckState();
    } else
#endif
    if (autoExclusive) {
        if (AbstractGraphicsButton *b = queryCheckedButton())
            b->setChecked(false);
    }
}

void AbstractGraphicsButtonPrivate::moveFocus(int key)
{
    QList<AbstractGraphicsButton *> buttonList = queryButtonList();;
#ifndef QT_NO_GRAPHICSBUTTONGROUP
    bool exclusive = group ? group->d_func()->exclusive : autoExclusive;
#else
    bool exclusive = autoExclusive;
#endif
    QGraphicsWidget *f = q_func()->focusWidget(); // todo: get real focused item
    AbstractGraphicsButton *fb = qobject_cast<AbstractGraphicsButton *>(f);
    if (!fb || !buttonList.contains(fb))
        return;

    AbstractGraphicsButton *candidate = 0;
    int bestScore = -1;
    QRectF target = f->rect().translated(f->mapToScene(QPoint(0,0)));
    QPointF goal = target.center();

    Q_GUI_EXPORT extern bool qt_tab_all_widgets;
    uint focus_flag = qt_tab_all_widgets ? Qt::TabFocus : Qt::StrongFocus;

    for (int i = 0; i < buttonList.count(); ++i) {
        AbstractGraphicsButton *button = buttonList.at(i);
        if (button != f && button->window() == f->window() && button->isEnabled() && button->isVisible()
            && (autoExclusive || (button->focusPolicy() & focus_flag) == focus_flag)) {
            QRectF buttonRect = button->rect().translated(button->mapToScene(QPoint(0,0)));
            QPointF p = buttonRect.center();

            //Priority to widgets that overlap on the same coordinate.
            //In that case, the distance in the direction will be used as significant score,
            //take also in account orthogonal distance in case two widget are in the same distance.
            int score;
            if ((buttonRect.x() < target.right() && target.x() < buttonRect.right())
                  && (key == Qt::Key_Up || key == Qt::Key_Down)) {
                //one item's is at the vertical of the other
                score = ((int)qAbs(p.y() - goal.y()) << 16) + qAbs(p.x() - goal.x());
            } else if ((buttonRect.y() < target.bottom() && target.y() < buttonRect.bottom())
                        && (key == Qt::Key_Left || key == Qt::Key_Right) ) {
                //one item's is at the horizontal of the other
                score = ((int)qAbs(p.x() - goal.x()) << 16) + qAbs(p.y() - goal.y());
            } else {
                score = (1 << 30) + (p.y() - goal.y()) * (p.y() - goal.y()) + (p.x() - goal.x()) * (p.x() - goal.x());
            }

            if (score > bestScore && candidate)
                continue;

            switch(key) {
            case Qt::Key_Up:
                if (p.y() < goal.y()) {
                    candidate = button;
                    bestScore = score;
                }
                break;
            case Qt::Key_Down:
                if (p.y() > goal.y()) {
                    candidate = button;
                    bestScore = score;
                }
                break;
            case Qt::Key_Left:
                if (p.x() < goal.x()) {
                    candidate = button;
                    bestScore = score;
                }
                break;
            case Qt::Key_Right:
                if (p.x() > goal.x()) {
                    candidate = button;
                    bestScore = score;
                }
                break;
            }
        }
    }

    if (exclusive
#ifdef QT_KEYPAD_NAVIGATION
        && !QApplication::keypadNavigationEnabled()
#endif
        && candidate
        && fb->d_func()->checked
        && candidate->d_func()->checkable)
        candidate->click();

    if (candidate) {
        if (key == Qt::Key_Up || key == Qt::Key_Left)
            candidate->setFocus(Qt::BacktabFocusReason);
        else
            candidate->setFocus(Qt::TabFocusReason);
    }
}

void AbstractGraphicsButtonPrivate::fixFocusPolicy()
{
    Q_Q(AbstractGraphicsButton);

#ifndef QT_NO_GRAPHICSBUTTONGROUP
    if (!group && !autoExclusive)
#else
    if (!autoExclusive)
#endif
        return;

    QList<AbstractGraphicsButton *> buttonList = queryButtonList();
    for (int i = 0; i < buttonList.count(); ++i) {
        AbstractGraphicsButton *b = buttonList.at(i);
        if (!b->isCheckable())
            continue;
        b->setFocusPolicy((Qt::FocusPolicy) ((b == q || !q->isCheckable())
                                         ? (b->focusPolicy() | Qt::TabFocus)
                                         :  (b->focusPolicy() & ~Qt::TabFocus)));
    }
}

void AbstractGraphicsButtonPrivate::init()
{
    Q_Q(AbstractGraphicsButton);

    q->setAcceptHoverEvents(true);
    q->setFocusPolicy(Qt::FocusPolicy(q->style()->styleHint(QStyle::SH_Button_FocusPolicy, NULL, q)));
    q->setSizePolicy(QSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed, controlType));
//    q->setForegroundRole(QPalette::ButtonText);
//    q->setBackgroundRole(QPalette::Button);
}

void AbstractGraphicsButtonPrivate::refresh()
{
    Q_Q(AbstractGraphicsButton);

    if (blockRefresh)
        return;
    q->update();
#ifndef QT_NO_ACCESSIBILITY
    QAccessible::updateAccessibility(q, 0, QAccessible::StateChanged);
#endif
}

void AbstractGraphicsButtonPrivate::click()
{
    Q_Q(AbstractGraphicsButton);

    down = false;
    blockRefresh = true;
    bool changeState = true;
    if (checked && queryCheckedButton() == q) {
        // the checked button of an exclusive or autoexclusive group cannot be unchecked
#ifndef QT_NO_GRAPHICSBUTTONGROUP
        if (group ? group->d_func()->exclusive : autoExclusive)
#else
        if (autoExclusive)
#endif
            changeState = false;
    }

    QPointer<AbstractGraphicsButton> guard(q);
    if (changeState) {
        q->nextCheckState();
        if (!guard)
            return;
    }
    blockRefresh = false;
    refresh();
    q->update(); //flush paint event before invoking potentially expensive operation
    QApplication::flush();
    if (guard)
        emitReleased();
    if (guard)
        emitClicked();
}

void AbstractGraphicsButtonPrivate::emitClicked()
{
    Q_Q(AbstractGraphicsButton);
    QPointer<AbstractGraphicsButton> guard(q);
    Q_EMIT q->clicked(checked);
#ifndef QT_NO_GRAPHICSBUTTONGROUP
    if (guard && group) {
        Q_EMIT group->buttonClicked(group->id(q));
        if (guard && group)
            Q_EMIT group->buttonClicked(q);
    }
#endif
}

void AbstractGraphicsButtonPrivate::emitPressed()
{
    Q_Q(AbstractGraphicsButton);
    QPointer<AbstractGraphicsButton> guard(q);
    Q_EMIT q->pressed();
#ifndef QT_NO_GRAPHICSBUTTONGROUP
    if (guard && group) {
        Q_EMIT group->buttonPressed(group->id(q));
        if (guard && group)
            Q_EMIT group->buttonPressed(q);
    }
#endif
}

void AbstractGraphicsButtonPrivate::emitReleased()
{
    Q_Q(AbstractGraphicsButton);
    QPointer<AbstractGraphicsButton> guard(q);
    Q_EMIT q->released();
#ifndef QT_NO_GRAPHICSBUTTONGROUP
    if (guard && group) {
        Q_EMIT group->buttonReleased(group->id(q));
        if (guard && group)
            Q_EMIT group->buttonReleased(q);
    }
#endif
}


/*!
    Constructs an abstract button with a \a parent.
*/
AbstractGraphicsButton::AbstractGraphicsButton(QGraphicsItem *parent)
    : GraphicsWidget(*new AbstractGraphicsButtonPrivate, parent)
{
    Q_D(AbstractGraphicsButton);
    d->init();
}

/*!
    \internal
*/
AbstractGraphicsButton::AbstractGraphicsButton(AbstractGraphicsButtonPrivate &dd, QGraphicsItem *parent)
    : GraphicsWidget(dd, parent)
{
    Q_D(AbstractGraphicsButton);
    d->init();
}

/*!
    Destroys the button.
*/
AbstractGraphicsButton::~AbstractGraphicsButton()
{
#ifndef QT_NO_GRAPHICSBUTTONGROUP
    Q_D(AbstractGraphicsButton);
    if (d->group)
        d->group->removeButton(this);
#endif
}

/*!
    \property AbstractGraphicsButton::text
    \brief the text shown on the button

    If the button has no text, the text() function will return a an empty string.

    If the text contains an ampersand character ('&'), a shortcut is
    automatically created for it. The character that follows the '&' will
    be used as the shortcut key. Any previous shortcut will be
    overwritten, or cleared if no shortcut is defined by the text. See the
    \l {QShortcut#mnemonic}{QShortcut} documentation for details (to
    display an actual ampersand, use '&&').

    There is no default text.
*/
QString AbstractGraphicsButton::text() const
{
    return d_func()->text;
}

void AbstractGraphicsButton::setText(const QString &text)
{
    Q_D(AbstractGraphicsButton);
    if (d->text == text)
        return;

    d->text = text;
#ifndef QT_NO_SHORTCUT
    setShortcut(QKeySequence::mnemonic(text));
#endif
    d->sizeHint = QSizeF();
    update();
    updateGeometry();
#ifndef QT_NO_ACCESSIBILITY
    QAccessible::updateAccessibility(this, 0, QAccessible::NameChanged);
#endif
}

/*!
    \property AbstractGraphicsButton::icon
    \brief the icon shown on the button

    The icon's default size is defined by the GUI style, but can be
    adjusted by setting the \l iconSize property.
*/
QIcon AbstractGraphicsButton::icon() const
{
    return d_func()->icon;
}

void AbstractGraphicsButton::setIcon(const QIcon &icon)
{
    Q_D(AbstractGraphicsButton);
    d->icon = icon;
    d->sizeHint = QSizeF();
    update();
    updateGeometry();
}

/*!
    \property AbstractGraphicsButton::iconSize
    \brief the icon size used for this button.

    The default size is defined by the GUI style. This is a maximum
    size for the icons. Smaller icons will not be scaled up.
*/
QSize AbstractGraphicsButton::iconSize() const
{
    Q_D(const AbstractGraphicsButton);
    if (d->iconSize.isValid())
        return d->iconSize;
    const int e = style()->pixelMetric(QStyle::PM_ButtonIconSize, 0, this);
    return QSize(e, e);
}

void AbstractGraphicsButton::setIconSize(const QSize &size)
{
    Q_D(AbstractGraphicsButton);
    if (d->iconSize == size)
        return;

    d->iconSize = size;
    d->sizeHint = QSizeF();
    update();
    updateGeometry();
}

#ifndef QT_NO_SHORTCUT
/*!
    \property AbstractGraphicsButton::shortcut
    \brief the mnemonic associated with the button
*/
QKeySequence AbstractGraphicsButton::shortcut() const
{
    return d_func()->shortcut;
}

void AbstractGraphicsButton::setShortcut(const QKeySequence &key)
{
    Q_D(AbstractGraphicsButton);
    if (d->shortcutId != 0)
        releaseShortcut(d->shortcutId);
    d->shortcut = key;
    d->shortcutId = grabShortcut(key);
}
#endif // QT_NO_SHORTCUT

/*!
    \property AbstractGraphicsButton::checkable
    \brief whether the button is checkable

    By default, the button is not checkable.

    \sa checked
*/
bool AbstractGraphicsButton::isCheckable() const
{
    return d_func()->checkable;
}

void AbstractGraphicsButton::setCheckable(bool checkable)
{
    Q_D(AbstractGraphicsButton);
    if (d->checkable == checkable)
        return;

    d->checkable = checkable;
    d->checked = false;
}

/*!
    \property AbstractGraphicsButton::checked
    \brief whether the button is checked

    Only checkable buttons can be checked. By default, the button is unchecked.

    \sa checkable
*/
bool AbstractGraphicsButton::isChecked() const
{
    return d_func()->checked;
}

void AbstractGraphicsButton::setChecked(bool checked)
{
    Q_D(AbstractGraphicsButton);
    if (!d->checkable || d->checked == checked) {
        if (!d->blockRefresh)
            checkStateSet();
        return;
    }

    if (!checked && d->queryCheckedButton() == this) {
        // the checked button of an exclusive or autoexclusive group cannot be unchecked
#ifndef QT_NO_GRAPHICSBUTTONGROUP
        if (d->group ? d->group->d_func()->exclusive : d->autoExclusive)
            return;
        if (d->group)
            d->group->d_func()->detectCheckedButton();
#else
        if (d->autoExclusive)
            return;
#endif
    }

    QPointer<AbstractGraphicsButton> guard(this);

    d->checked = checked;
    if (!d->blockRefresh)
        checkStateSet();
    d->refresh();

    if (guard && checked)
        d->notifyChecked();
    if (guard)
        Q_EMIT toggled(checked);
}

/*!
    \property AbstractGraphicsButton::down
    \brief whether the button is pressed down

    If this property is true, the button is pressed down. The signals
    pressed() and clicked() are not emitted if you set this property
    to true. The default is false.
*/
bool AbstractGraphicsButton::isDown() const
{
    return d_func()->down;
}

void AbstractGraphicsButton::setDown(bool down)
{
    Q_D(AbstractGraphicsButton);
    if (d->down == down)
        return;

    d->down = down;
    d->refresh();
    if (d->autoRepeat && d->down)
        d->repeatTimer.start(d->autoRepeatDelay, this);
    else
        d->repeatTimer.stop();
}

/*!
    \property AbstractGraphicsButton::autoRepeat
    \brief whether autoRepeat is enabled

    If autoRepeat is enabled, then the pressed(), released(), and clicked() signals are emitted at
    regular intervals when the button is down. autoRepeat is off by default.
    The initial delay and the repetition interval are defined in milliseconds by \l
    autoRepeatDelay and \l autoRepeatInterval.

    Note: If a button is pressed down by a shortcut key, then auto-repeat is enabled and timed by the
    system and not by this class. The pressed(), released(), and clicked() signals will be emitted
    like in the normal case.
*/
bool AbstractGraphicsButton::autoRepeat() const
{
    return d_func()->autoRepeat;
}

void AbstractGraphicsButton::setAutoRepeat(bool autoRepeat)
{
    Q_D(AbstractGraphicsButton);
    if (d->autoRepeat == autoRepeat)
        return;
    d->autoRepeat = autoRepeat;
    if (d->autoRepeat && d->down)
        d->repeatTimer.start(d->autoRepeatDelay, this);
    else
        d->repeatTimer.stop();
}

/*!
    \property AbstractGraphicsButton::autoRepeatDelay
    \brief the initial delay of auto-repetition
    \since 4.2

    If \l autoRepeat is enabled, then autoRepeatDelay defines the initial
    delay in milliseconds before auto-repetition kicks in.

    \sa autoRepeat, autoRepeatInterval
*/
int AbstractGraphicsButton::autoRepeatDelay() const
{
    return d_func()->autoRepeatDelay;
}

void AbstractGraphicsButton::setAutoRepeatDelay(int autoRepeatDelay)
{
    Q_D(AbstractGraphicsButton);
    d->autoRepeatDelay = autoRepeatDelay;
}

/*!
    \property AbstractGraphicsButton::autoRepeatInterval
    \brief the interval of auto-repetition
    \since 4.2

    If \l autoRepeat is enabled, then autoRepeatInterval defines the
    length of the auto-repetition interval in millisecons.

    \sa autoRepeat, autoRepeatDelay
*/
int AbstractGraphicsButton::autoRepeatInterval() const
{
    return d_func()->autoRepeatInterval;
}

void AbstractGraphicsButton::setAutoRepeatInterval(int autoRepeatInterval)
{
    Q_D(AbstractGraphicsButton);
    d->autoRepeatInterval = autoRepeatInterval;
}

/*!
    \property AbstractGraphicsButton::autoExclusive
    \brief whether auto-exclusivity is enabled

    If auto-exclusivity is enabled, checkable buttons that belong to the
    same parent widget behave as if they were part of the same
    exclusive button group. In an exclusive button group, only one button
    can be checked at any time; checking another button automatically
    unchecks the previously checked one.

    The property has no effect on buttons that belong to a button
    group.

    autoExclusive is off by default, except for radio buttons.

    \sa QRadioButton
*/
bool AbstractGraphicsButton::autoExclusive() const
{
    return d_func()->autoExclusive;
}

void AbstractGraphicsButton::setAutoExclusive(bool autoExclusive)
{
    d_func()->autoExclusive = autoExclusive;
}

#ifndef QT_NO_GRAPHICSBUTTONGROUP
/*!
    Returns the group that this button belongs to.

    If the button is not a member of any GraphicsButtonGroup, this function
    returns 0.

    \sa GraphicsButtonGroup
*/
GraphicsButtonGroup *AbstractGraphicsButton::group() const
{
    return d_func()->group;
}
#endif // QT_NO_GRAPHICSBUTTONGROUP

/*!
    Performs an animated click: the button is pressed immediately, and
    released \a msec milliseconds later (the default is 100 ms).

    Calling this function again before the button was released will reset
    the release timer.

    All signals associated with a click are emitted as appropriate.

    This function does nothing if the button is \link setEnabled() disabled. \endlink

    \sa click()
*/
void AbstractGraphicsButton::animateClick(int msec)
{
    if (!isEnabled())
        return;
    Q_D(AbstractGraphicsButton);
    if (d->checkable && focusPolicy() & Qt::ClickFocus)
        setFocus();
    setDown(true);
    update(); //flush paint event before invoking potentially expensive operation
    QApplication::flush();
    if (!d->animateTimer.isActive())
        d->emitPressed();
    d->animateTimer.start(msec, this);
}

/*!
    Performs a click.

    All the usual signals associated with a click are emitted as appropriate.
    If the button is checkable, the state of the button is toggled.

    This function does nothing if the button is \link setEnabled() disabled. \endlink

    \sa animateClick()
*/
void AbstractGraphicsButton::click()
{
    if (!isEnabled())
        return;
    Q_D(AbstractGraphicsButton);
    QPointer<AbstractGraphicsButton> guard(this);
    d->down = true;
    d->emitPressed();
    if (guard) {
        d->down = false;
        nextCheckState();
        if (guard)
            d->emitReleased();
        if (guard)
            d->emitClicked();
    }
}

/*! \fn void AbstractGraphicsButton::toggle()

    Toggles the state of a checkable button.

    \sa checked
*/
void AbstractGraphicsButton::toggle()
{
    setChecked(!d_func()->checked);
}

/*!
    Returns true if \a pos is inside the clickable button rectangle;
    otherwise returns false.

    By default, the clickable area is the entire widget. Subclasses
    may reimplement this function to provide support for clickable
    areas of different shapes and sizes.
*/
bool AbstractGraphicsButton::hitButton(const QPointF &pos) const
{
    return rect().contains(pos);
}

/*!
    This virtual handler is called when setChecked() was called,
    unless it was called from within nextCheckState(). It allows
    subclasses to reset their intermediate button states.

    \sa nextCheckState()
 */
void AbstractGraphicsButton::checkStateSet()
{
}

/*!
    This virtual handler is called when a button is clicked. The
    default implementation calls setChecked(!isChecked()) if the button
    isCheckable(). It allows subclasses to implement intermediate button states.

    \sa checkStateSet()
*/
void AbstractGraphicsButton::nextCheckState()
{
    if (isCheckable())
        setChecked(!isChecked());
}

/*!
    \reimp
*/
bool AbstractGraphicsButton::event(QEvent *event)
{
    // as opposed to other widgets, disabled buttons accept mouse
    // events. This avoids surprising click-through scenarios
    if (!isEnabled()) {
        switch(event->type()) {
        case QEvent::TabletPress:
        case QEvent::TabletRelease:
        case QEvent::TabletMove:
        case QEvent::GraphicsSceneMousePress:
        case QEvent::GraphicsSceneMouseRelease:
        case QEvent::GraphicsSceneMouseDoubleClick:
        case QEvent::GraphicsSceneMouseMove:
        case QEvent::GraphicsSceneHoverEnter:
        case QEvent::GraphicsSceneHoverLeave:
        case QEvent::GraphicsSceneHoverMove:
        case QEvent::ContextMenu:
#ifndef QT_NO_WHEELEVENT
        case QEvent::GraphicsSceneWheel:
#endif
            return true;
        default:
            break;
        }
    }

#ifndef QT_NO_SHORTCUT
    if (event->type() == QEvent::Shortcut) {
        Q_D(AbstractGraphicsButton);
        QShortcutEvent *se = static_cast<QShortcutEvent *>(event);
        if (d->shortcutId != se->shortcutId())
            return false;

        if (!se->isAmbiguous()) {
            if (!d->animateTimer.isActive())
                animateClick();
        } else {
            if (focusPolicy() != Qt::NoFocus)
                setFocus(Qt::ShortcutFocusReason);
            window()->setAttribute(Qt::WA_KeyboardFocusChange);
        }
        return true;
    }
#endif

    return GraphicsWidget::event(event);
}

/*!
    \reimp
*/
void AbstractGraphicsButton::mousePressEvent(QGraphicsSceneMouseEvent *e)
{
    Q_D(AbstractGraphicsButton);
    if (e->button() == Qt::LeftButton && hitButton(e->pos())) {
        setDown(true);
        d->pressed = true;
        update(); //flush paint event before invoking potentially expensive operation
        QApplication::flush();
        d->emitPressed();
        e->accept();
    } else {
        e->ignore();
    }
}

/*!
    \reimp
*/
void AbstractGraphicsButton::mouseReleaseEvent(QGraphicsSceneMouseEvent *e)
{
    Q_D(AbstractGraphicsButton);
    d->pressed = false;

    if (e->button() != Qt::LeftButton || !d->down) {
        e->ignore();
        return;
    }

    if (hitButton(e->pos())) {
        d->repeatTimer.stop();
        d->click();
        e->accept();
    } else {
        setDown(false);
        e->ignore();
    }
}

/*!
    \reimp
*/
void AbstractGraphicsButton::mouseMoveEvent(QGraphicsSceneMouseEvent *e)
{
    Q_D(AbstractGraphicsButton);
    if (!(e->buttons() & Qt::LeftButton) || !d->pressed) {
        e->ignore();
        return;
    }

    if (hitButton(e->pos()) != d->down) {
        setDown(!d->down);
        update(); //flush paint event before invoking potentially expensive operation
        QApplication::flush();
        if (d->down)
            d->emitPressed();
        else
            d->emitReleased();
        e->accept();
    } else if (!hitButton(e->pos())) {
        e->ignore();
    }
}

/*!
    \reimp
*/
void AbstractGraphicsButton::keyPressEvent(QKeyEvent *e)
{
    Q_D(AbstractGraphicsButton);
    bool next = true;
    switch (e->key()) {
    case Qt::Key_Enter:
    case Qt::Key_Return:
        e->ignore();
        break;
    case Qt::Key_Select:
    case Qt::Key_Space:
        if (!e->isAutoRepeat()) {
            setDown(true);
            update(); //flush paint event before invoking potentially expensive operation
            QApplication::flush();
            d->emitPressed();
        }
        break;
    case Qt::Key_Up:
    case Qt::Key_Left:
        next = false;
        // fall through
    case Qt::Key_Right:
    case Qt::Key_Down:
#ifdef QT_KEYPAD_NAVIGATION
        if ((QApplication::keypadNavigationEnabled()
                && (e->key() == Qt::Key_Left || e->key() == Qt::Key_Right))
                || (!QApplication::navigationMode() == Qt::NavigationModeKeypadDirectional
                || (e->key() == Qt::Key_Up || e->key() == Qt::Key_Down))) {
            e->ignore();
            return;
        }
#endif
        QGraphicsWidget *pw;
        if (d->autoExclusive
#ifndef QT_NO_GRAPHICSBUTTONGROUP
            || d->group
#endif
#ifndef QT_NO_ITEMVIEWS
            || ((pw = parentWidget()) && qobject_cast<QAbstractItemView *>(pw->parentWidget()))
#endif
            ) {
            // ### Using qobject_cast to check if the parent is a viewport of
            // QAbstractItemView is a crude hack, and should be revisited and
            // cleaned up when fixing task 194373. It's here to ensure that we
            // keep compatibility outside QAbstractItemView.
            d->moveFocus(e->key());
            if (hasFocus()) // nothing happend, propagate
                e->ignore();
        } else {
            focusNextPrevChild(next);
        }
        break;
    case Qt::Key_Escape:
        if (d->down) {
            setDown(false);
            update(); //flush paint event before invoking potentially expensive operation
            QApplication::flush();
            d->emitReleased();
            break;
        }
        // fall through
    default:
        e->ignore();
    }
}

/*!
    \reimp
*/
void AbstractGraphicsButton::keyReleaseEvent(QKeyEvent *e)
{
    Q_D(AbstractGraphicsButton);

    if (!e->isAutoRepeat())
        d->repeatTimer.stop();

    switch (e->key()) {
    case Qt::Key_Select:
    case Qt::Key_Space:
        if (!e->isAutoRepeat() && d->down)
            d->click();
        break;
    default:
        e->ignore();
    }
}

/*!
    \reimp
*/
void AbstractGraphicsButton::timerEvent(QTimerEvent *e)
{
    Q_D(AbstractGraphicsButton);
    if (e->timerId() == d->repeatTimer.timerId()) {
        d->repeatTimer.start(d->autoRepeatInterval, this);
        if (d->down) {
            QPointer<AbstractGraphicsButton> guard(this);
            nextCheckState();
            if (guard)
                d->emitReleased();
            if (guard)
                d->emitClicked();
            if (guard)
                d->emitPressed();
        }
    } else if (e->timerId() == d->animateTimer.timerId()) {
        d->animateTimer.stop();
        d->click();
    }
}

/*!
    \reimp
*/
void AbstractGraphicsButton::focusInEvent(QFocusEvent *e)
{
#ifdef QT_KEYPAD_NAVIGATION
    if (!QApplication::keypadNavigationEnabled())
#endif
    d_func()->fixFocusPolicy();

    GraphicsWidget::focusInEvent(e);
}

/*!
    \reimp
*/
void AbstractGraphicsButton::focusOutEvent(QFocusEvent *e)
{
    if (e->reason() != Qt::PopupFocusReason)
        d_func()->down = false;

    GraphicsWidget::focusOutEvent(e);
}

/*!
    \reimp
*/
void AbstractGraphicsButton::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::EnabledChange) {
        if (!isEnabled())
            setDown(false);
    } else {
        d_func()->sizeHint = QSizeF();
    }

    GraphicsWidget::changeEvent(e);
}

/*!
    \fn void AbstractGraphicsButton::pressed()

    This signal is emitted when the button is pressed down.

    \sa released(), clicked()
*/

/*!
    \fn void AbstractGraphicsButton::released()

    This signal is emitted when the button is released.

    \sa pressed(), clicked(), toggled()
*/

/*!
    \fn void AbstractGraphicsButton::clicked(bool checked)

    This signal is emitted when the button is activated (i.e. pressed down
    then released while the mouse cursor is inside the button), when the
    shortcut key is typed, or when click() or animateClick() is called.
    Notably, this signal is \e not emitted if you call setDown(),
    setChecked() or toggle().

    If the button is checkable, \a checked is true if the button is
    checked, or false if the button is unchecked.

    \sa pressed(), released(), toggled()
*/

/*!
    \fn void AbstractGraphicsButton::toggled(bool checked)

    This signal is emitted whenever a checkable button changes its state.
    \a checked is true if the button is checked, or false if the button is
    unchecked.

    This may be the result of a user action, click() slot activation,
    or because setChecked() was called.

    The states of buttons in exclusive button groups are updated before this
    signal is emitted. This means that slots can act on either the "off"
    signal or the "on" signal emitted by the buttons in the group whose
    states have changed.

    For example, a slot that reacts to signals emitted by newly checked
    buttons but which ignores signals from buttons that have been unchecked
    can be implemented using the following pattern:

    \snippet doc/src/snippets/code/src_gui_widgets_qabstractbutton.cpp 2

    Button groups can be created using the GraphicsButtonGroup class, and updates
    to the button states monitored with the \l{GraphicsButtonGroup::buttonClicked()} signal.

    \sa checked, clicked()
*/
