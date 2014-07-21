/****************************************************************************
**
** Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the QtGui module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "progress_dialog.h"

#include "qshortcut.h"
#include "qpainter.h"
#include "qdrawutil.h"
#include "qprogressbar.h"
#include "qapplication.h"
#include "qstyle.h"
#include "qpushbutton.h"
#include "qcursor.h"
#include "qtimer.h"
#include "qelapsedtimer.h"
#include <limits.h>

#include <QtCore/QPointer>
#include <QtWidgets/QVBoxLayout>

#if defined(QT_SOFTKEYS_ENABLED)
#include <qaction.h>
#endif
#ifdef Q_WS_S60
#include <QtGui/qdesktopwidget.h>
#endif

#include <ui/widgets/elided_label.h>
#include <ui/widgets/dialog_button_box.h>
#include <ui/widgets/progress_widget.h>


// If the operation is expected to take this long (as predicted by
// progress time), show the progress dialog.
static const int defaultShowTime = 4000;
// Wait at least this long before attempting to make a prediction.
static const int minWaitTime = 50;
// Maximum width of the label when auto-size is disabled.
static const int maxLabelFixedWidth = 400;

class QnProgressDialogPrivate
{
    Q_DECLARE_PUBLIC(QnProgressDialog)

public:
    QnProgressDialogPrivate() : label(0), cancel(0), bar(0),
        shown_once(false),
        cancellation_flag(false),
        showTime(defaultShowTime),
#ifndef QT_NO_SHORTCUT
        escapeShortcut(0),
#endif
#ifdef QT_SOFTKEYS_ENABLED
        cancelAction(0),
#endif
        useDefaultCancelText(false),
        processEvents(false)
    {
    }

    virtual ~QnProgressDialogPrivate() {}

    void init(const QString &labelText, const QString &cancelText, int min, int max);
    void retranslateStrings();
    void _q_disconnectOnClose();
    void showInfiniteProgress(bool enable);

    QnProgressDialog *q_ptr;

    QLabel *label;
    QPushButton *cancel;
    QProgressBar *bar;
    QnProgressWidget *infiniteProgress;
    QnDialogButtonBox *buttonBox;
    QVBoxLayout *layout;

    QTimer *forceTimer;
    bool shown_once;
    bool cancellation_flag;
    QElapsedTimer starttime;
#ifndef QT_NO_CURSOR
    QCursor parentCursor;
#endif
    int showTime;
    bool autoClose;
    bool autoReset;
    bool forceHide;
#ifndef QT_NO_SHORTCUT
    QShortcut *escapeShortcut;
#endif
#ifdef QT_SOFTKEYS_ENABLED
    QAction *cancelAction;
#endif
    bool useDefaultCancelText;
    QPointer<QObject> receiverToDisconnectOnClose;
    QByteArray memberToDisconnectOnClose;
    bool processEvents;
};

void QnProgressDialogPrivate::init(const QString &labelText, const QString &cancelText,
                                  int min, int max)
{
    Q_Q(QnProgressDialog);

    autoClose = true;
    autoReset = true;
    forceHide = false;
    
    label = new QnElidedLabel(q);
    label->setText(labelText);
    int align = q->style()->styleHint(QStyle::SH_ProgressDialog_TextLabelAlignment, 0, q);
    label->setAlignment(Qt::Alignment(align));

    bar = new QProgressBar(q);
    bar->setRange(min, max);

    infiniteProgress = new QnProgressWidget(q);

    buttonBox = new QnDialogButtonBox(QDialogButtonBox::NoButton, Qt::Horizontal, q);

    QHBoxLayout *hlayout = new QHBoxLayout();
    hlayout->addWidget(infiniteProgress);
    hlayout->addWidget(label, 1);

    layout = new QVBoxLayout();
    layout->addLayout(hlayout);
    layout->addWidget(bar);
    layout->addWidget(buttonBox);
    layout->setSizeConstraint(QLayout::SetFixedSize);
    q->setLayout(layout);

    infiniteProgress->hide();

    forceTimer = new QTimer(q);
    QObject::connect(forceTimer, SIGNAL(timeout()), q, SLOT(forceShow()));

    if (useDefaultCancelText) {
        retranslateStrings();
    } else {
        q->setCancelButtonText(cancelText);
    }

    QObject::connect(q, SIGNAL(canceled()), q, SLOT(cancel()));
}

void QnProgressDialogPrivate::retranslateStrings()
{
    Q_Q(QnProgressDialog);
    if (useDefaultCancelText) {
        q->setCancelButtonText(QnProgressDialog::tr("Cancel"));
        useDefaultCancelText = true;
    }
}

void QnProgressDialogPrivate::_q_disconnectOnClose()
{
    Q_Q(QnProgressDialog);
    if (receiverToDisconnectOnClose) {
        QObject::disconnect(q, SIGNAL(canceled()), receiverToDisconnectOnClose,
                            memberToDisconnectOnClose);
        receiverToDisconnectOnClose = 0;
    }
    memberToDisconnectOnClose.clear();
}

void QnProgressDialogPrivate::showInfiniteProgress(bool enable)
{
    infiniteProgress->setVisible(enable);
    bar->setVisible(!enable);
}

/*!
  \class QnProgressDialog
  \brief The QnProgressDialog class provides feedback on the progress of a slow operation.
  \ingroup standard-dialogs


  A progress dialog is used to give the user an indication of how long
  an operation is going to take, and to demonstrate that the
  application has not frozen. It can also give the user an opportunity
  to abort the operation.

  A common problem with progress dialogs is that it is difficult to know
  when to use them; operations take different amounts of time on different
  hardware.  QnProgressDialog offers a solution to this problem:
  it estimates the time the operation will take (based on time for
  steps), and only shows itself if that estimate is beyond minimumDuration()
  (4 seconds by default).

  Use setMinimum() and setMaximum() or the constructor to set the number of
  "steps" in the operation and call setValue() as the operation
  progresses. The number of steps can be chosen arbitrarily. It can be the
  number of files copied, the number of bytes received, the number of
  iterations through the main loop of your algorithm, or some other
  suitable unit. Progress starts at the value set by setMinimum(),
  and the progress dialog shows that the operation has finished when
  you call setValue() with the value set by setMaximum() as its argument.

  The dialog automatically resets and hides itself at the end of the
  operation.  Use setAutoReset() and setAutoClose() to change this
  behavior. Note that if you set a new maximum (using setMaximum() or
  setRange()) that equals your current value(), the dialog will not
  close regardless.

  There are two ways of using QnProgressDialog: modal and modeless.

  Compared to a modeless QnProgressDialog, a modal QnProgressDialog is simpler
  to use for the programmer. Do the operation in a loop, call \l setValue() at
  intervals, and check for cancellation with wasCanceled(). For example:

  \snippet doc/src/snippets/dialogs/dialogs.cpp 3

  A modeless progress dialog is suitable for operations that take
  place in the background, where the user is able to interact with the
  application. Such operations are typically based on QTimer (or
  QObject::timerEvent()), QSocketNotifier, or QUrlOperator; or performed
  in a separate thread. A QProgressBar in the status bar of your main window
  is often an alternative to a modeless progress dialog.

  You need to have an event loop to be running, connect the
  canceled() signal to a slot that stops the operation, and call \l
  setValue() at intervals. For example:

  \snippet doc/src/snippets/dialogs/dialogs.cpp 4
  \codeline
  \snippet doc/src/snippets/dialogs/dialogs.cpp 5
  \codeline
  \snippet doc/src/snippets/dialogs/dialogs.cpp 6

  In both modes the progress dialog may be customized by
  replacing the child widgets with custom widgets by using setLabel(),
  setBar(), and setCancelButton().
  The functions setLabelText() and setCancelButtonText()
  set the texts shown.

  \image plastique-progressdialog.png A progress dialog shown in the Plastique widget style.

  \sa QDialog, QProgressBar, {fowler}{GUI Design Handbook: Progress Indicator},
      {Find Files Example}, {Pixelator Example}
*/


/*!
  Constructs a progress dialog.

  Default settings:
  \list
  \i The label text is empty.
  \i The cancel button text is (translated) "Cancel".
  \i minimum is 0;
  \i maximum is 100
  \endlist

  The \a parent argument is dialog's parent widget. The widget flags, \a f, are
  passed to the QDialog::QDialog() constructor.

  \sa setLabelText(), setCancelButtonText(), setCancelButton(),
  setMinimum(), setMaximum()
*/

QnProgressDialog::QnProgressDialog(QWidget *parent, Qt::WindowFlags f): 
    QDialog(parent, f),
    d_ptr(new QnProgressDialogPrivate())
{
    Q_D(QnProgressDialog);
    d->useDefaultCancelText = true;
    d->q_ptr = this;
    d->init(QString(), QString(), 0, 100);
}

/*!
  Constructs a progress dialog.

   The \a labelText is the text used to remind the user what is progressing.

   The \a cancelButtonText is the text to display on the cancel button.  If 
   QString() is passed then no cancel button is shown.

   The \a minimum and \a maximum is the number of steps in the operation for
   which this progress dialog shows progress.  For example, if the
   operation is to examine 50 files, this value minimum value would be 0,
   and the maximum would be 50. Before examining the first file, call
   setValue(0). As each file is processed call setValue(1), setValue(2),
   etc., finally calling setValue(50) after examining the last file.

   The \a parent argument is the dialog's parent widget. The parent, \a parent, and
   widget flags, \a f, are passed to the QDialog::QDialog() constructor.

  \sa setLabelText(), setLabel(), setCancelButtonText(), setCancelButton(),
  setMinimum(), setMaximum()
*/

QnProgressDialog::QnProgressDialog(const QString &labelText,
                                 const QString &cancelButtonText,
                                 int minimum, int maximum,
                                 QWidget *parent, Qt::WindowFlags f): 
    QDialog(parent, f),
    d_ptr(new QnProgressDialogPrivate())
{
    Q_D(QnProgressDialog);
    d->q_ptr = this;
    d->init(labelText, cancelButtonText, minimum, maximum);
}


/*!
  Destroys the progress dialog.
*/

QnProgressDialog::~QnProgressDialog()
{
}

/*!
  \fn void QnProgressDialog::canceled()

  This signal is emitted when the cancel button is clicked.
  It is connected to the cancel() slot by default.

  \sa wasCanceled()
*/


/*!
  Sets the label to \a label.
  The label becomes owned by the progress dialog and will be deleted when
  necessary, so do not pass the address of an object on the stack.

  \sa setLabelText()
*/

void QnProgressDialog::setLabel(QLabel *label)
{
    Q_D(QnProgressDialog);
    delete d->label;
    d->label = label;
    if (!label)
        return;
    if (label->parentWidget() != this)
        label->setParent(this, 0);
    label->show();
}


/*!
  \property QnProgressDialog::labelText
  \brief the label's text

  The default text is an empty string.
*/

QString QnProgressDialog::labelText() const
{
    Q_D(const QnProgressDialog);
    if (d->label)
        return d->label->text();
    return QString();
}

void QnProgressDialog::setLabelText(const QString &text)
{
    Q_D(QnProgressDialog);
    if (d->label)
        d->label->setText(text);
}


/*!
  Sets the cancel button to the push button, \a cancelButton. The
  progress dialog takes ownership of this button which will be deleted
  when necessary, so do not pass the address of an object that is on
  the stack, i.e. use new() to create the button.  If 0 is passed then
  no cancel button will be shown.

  \sa setCancelButtonText()
*/

void QnProgressDialog::setCancelButton(QPushButton *cancelButton)
{
    Q_D(QnProgressDialog);
    if (d->cancel)
        delete d->cancel;
    d->cancel = cancelButton;
    if (cancelButton) {
        cancelButton->setParent(this, 0);
        d->buttonBox->addButton(cancelButton, QDialogButtonBox::RejectRole);

        connect(d->cancel, SIGNAL(clicked()), this, SIGNAL(canceled()));
#ifndef QT_NO_SHORTCUT
        d->escapeShortcut = new QShortcut(Qt::Key_Escape, this, SIGNAL(canceled()));
#endif
    } else {
#ifndef QT_NO_SHORTCUT
        delete d->escapeShortcut;
        d->escapeShortcut = 0;
#endif
    }
    if (cancelButton)
#if !defined(QT_SOFTKEYS_ENABLED)
        cancelButton->show();
#else
    {
        d->cancelAction = new QAction(cancelButton->text(), cancelButton);
        d->cancelAction->setSoftKeyRole(QAction::NegativeSoftKey);
        connect(d->cancelAction, SIGNAL(triggered()), this, SIGNAL(canceled()));
        addAction(d->cancelAction);
    }
#endif
}

void QnProgressDialog::addButton(QPushButton *button, QDialogButtonBox::ButtonRole role) {
    Q_D(QnProgressDialog);

    d->buttonBox->addButton(button, role);
}

/*!
  Sets the cancel button's text to \a cancelButtonText.  If the text
  is set to QString() then it will cause the cancel button to be 
  hidden and deleted.

  \sa setCancelButton()
*/

void QnProgressDialog::setCancelButtonText(const QString &cancelButtonText)
{
    Q_D(QnProgressDialog);
    d->useDefaultCancelText = false;

    if (!cancelButtonText.isNull()) {
        if (d->cancel) {
            d->cancel->setText(cancelButtonText);
#ifdef QT_SOFTKEYS_ENABLED
            d->cancelAction->setText(cancelButtonText);
#endif
        } else {
            setCancelButton(new QPushButton(cancelButtonText, this));
        }
    } else {
        setCancelButton(0);
    }
}


/*!
  Sets the progress bar widget to \a bar.
  The progress dialog takes ownership of the progress \a bar which
  will be deleted when necessary, so do not use a progress bar
  allocated on the stack.
*/

void QnProgressDialog::setBar(QProgressBar *bar)
{
    Q_D(QnProgressDialog);
    if (!bar) {
        qWarning("QnProgressDialog::setBar: Cannot set a null progress bar");
        return;
    }
#ifndef QT_NO_DEBUG
    if (value() > 0)
        qWarning("QnProgressDialog::setBar: Cannot set a new progress bar "
                  "while the old one is active");
#endif
    delete d->bar;
    d->bar = bar;
}


/*!
  \property QnProgressDialog::wasCanceled
  \brief whether the dialog was canceled
*/

bool QnProgressDialog::wasCanceled() const
{
    Q_D(const QnProgressDialog);
    return d->cancellation_flag;
}


/*!
    \property QnProgressDialog::maximum
    \brief the highest value represented by the progress bar

    The default is 0.

    \sa minimum, setRange()
*/

int QnProgressDialog::maximum() const
{
    Q_D(const QnProgressDialog);
    return d->bar->maximum();
}

void QnProgressDialog::setMaximum(int maximum)
{
    Q_D(QnProgressDialog);
    d->bar->setMaximum(maximum);
}

/*!
    \property QnProgressDialog::minimum
    \brief the lowest value represented by the progress bar

    The default is 0.

    \sa maximum, setRange()
*/

int QnProgressDialog::minimum() const
{
    Q_D(const QnProgressDialog);
    return d->bar->minimum();
}

void QnProgressDialog::setMinimum(int minimum)
{
    Q_D(QnProgressDialog);
    d->bar->setMinimum(minimum);
}

/*!
    Sets the progress dialog's minimum and maximum values
    to \a minimum and \a maximum, respectively.

    If \a maximum is smaller than \a minimum, \a minimum becomes the only
    legal value.

    If the current value falls outside the new range, the progress
    dialog is reset with reset().

    \sa minimum, maximum
*/
void QnProgressDialog::setRange(int minimum, int maximum)
{
    Q_D(QnProgressDialog);
    d->bar->setRange(minimum, maximum);
}


/*!
  Resets the progress dialog.
  The progress dialog becomes hidden if autoClose() is true.

  \sa setAutoClose(), setAutoReset()
*/

void QnProgressDialog::reset()
{
    Q_D(QnProgressDialog);
#ifndef QT_NO_CURSOR
    if (value() >= 0) {
        if (parentWidget())
            parentWidget()->setCursor(d->parentCursor);
    }
#endif
    if (d->autoClose || d->forceHide)
        hide();
    d->bar->reset();
    d->cancellation_flag = false;
    d->shown_once = false;
    d->forceTimer->stop();

    /*
        I wish we could disconnect the user slot provided to open() here but
        unfortunately reset() is usually called before the slot has been invoked.
        (reset() is itself invoked when canceled() is emitted.)
    */
    if (d->receiverToDisconnectOnClose)
        QMetaObject::invokeMethod(this, "_q_disconnectOnClose", Qt::QueuedConnection);
}

/*!
  Resets the progress dialog. wasCanceled() becomes true until
  the progress dialog is reset.
  The progress dialog becomes hidden.
*/

void QnProgressDialog::cancel()
{
    Q_D(QnProgressDialog);
    d->forceHide = true;
    reset();
    d->forceHide = false;
    d->cancellation_flag = true;
}


int QnProgressDialog::value() const
{
    Q_D(const QnProgressDialog);
    return d->bar->value();
}

/*!
  \property QnProgressDialog::value
  \brief the current amount of progress made.

  For the progress dialog to work as expected, you should initially set
  this property to 0 and finally set it to
  QnProgressDialog::maximum(); you can call setValue() any number of times
  in-between.

  \warning If the progress dialog is modal
    (see QnProgressDialog::QnProgressDialog()),
    setValue() calls QApplication::processEvents(), so take care that
    this does not cause undesirable re-entrancy in your code. For example,
    don't use a QnProgressDialog inside a paintEvent()!

  \sa minimum, maximum
*/
void QnProgressDialog::setValue(int progress)
{
    Q_D(QnProgressDialog);

    d->showInfiniteProgress(false);

    if (progress == d->bar->value()
        || (d->bar->value() == -1 && progress == d->bar->maximum()))
        return;

    d->bar->setValue(progress);

    if (d->shown_once) {
        if (isModal() && d->processEvents)
            QApplication::processEvents();
    } else {
        if (progress == 0) {
            d->starttime.start();
            d->forceTimer->start(d->showTime);
            return;
        } else {
            bool need_show;
            int elapsed = d->starttime.elapsed();
            if (elapsed >= d->showTime) {
                need_show = true;
            } else {
                if (elapsed > minWaitTime) {
                    int estimate;
                    int totalSteps = maximum() - minimum();
                    int myprogress = progress - minimum();
                    if ((totalSteps - myprogress) >= INT_MAX / elapsed)
                        estimate = (totalSteps - myprogress) / myprogress * elapsed;
                    else
                        estimate = elapsed * (totalSteps - myprogress) / myprogress;
                    need_show = estimate >= d->showTime;
                } else {
                    need_show = false;
                }
            }
            if (need_show) {
                show();
                d->shown_once = true;
            }
        }
#ifdef Q_OS_MAC
        QApplication::flush();
#endif
    }

    if (progress == d->bar->maximum() && d->autoReset)
        reset();
}

/*!
    \property QnProgressDialog::minimumDuration
    \brief the time that must pass before the dialog appears

    If the expected duration of the task is less than the
    minimumDuration, the dialog will not appear at all. This prevents
    the dialog popping up for tasks that are quickly over. For tasks
    that are expected to exceed the minimumDuration, the dialog will
    pop up after the minimumDuration time or as soon as any progress
    is set.

    If set to 0, the dialog is always shown as soon as any progress is
    set. The default is 4000 milliseconds.
*/
void QnProgressDialog::setMinimumDuration(int ms)
{
    Q_D(QnProgressDialog);
    d->showTime = ms;
    if (d->bar->value() == 0) {
        d->forceTimer->stop();
        d->forceTimer->start(ms);
    }
}

void QnProgressDialog::setInfiniteProgress()
{
    Q_D(QnProgressDialog);
    d->showInfiniteProgress(true);
}

int QnProgressDialog::minimumDuration() const
{
    Q_D(const QnProgressDialog);
    return d->showTime;
}


/*!
  \reimp
*/

void QnProgressDialog::closeEvent(QCloseEvent *e)
{
    emit canceled();
    base_type::closeEvent(e);
}

/*!
  \property QnProgressDialog::autoReset
  \brief whether the progress dialog calls reset() as soon as value() equals maximum()

  The default is true.

  \sa setAutoClose()
*/

void QnProgressDialog::setAutoReset(bool b)
{
    Q_D(QnProgressDialog);
    d->autoReset = b;
}

bool QnProgressDialog::autoReset() const
{
    Q_D(const QnProgressDialog);
    return d->autoReset;
}

/*!
  \property QnProgressDialog::autoClose
  \brief whether the dialog gets hidden by reset()

  The default is true.

  \sa setAutoReset()
*/

void QnProgressDialog::setAutoClose(bool close)
{
    Q_D(QnProgressDialog);
    d->autoClose = close;
}

bool QnProgressDialog::autoClose() const
{
    Q_D(const QnProgressDialog);
    return d->autoClose;
}

/*!
  \reimp
*/

void QnProgressDialog::showEvent(QShowEvent *e)
{
    Q_D(QnProgressDialog);
    base_type::showEvent(e);
    d->forceTimer->stop();
}

void QnProgressDialog::changeEvent(QEvent *event) {
    base_type::changeEvent(event);

    if(event->type() == QEvent::LanguageChange)
        d_func()->retranslateStrings();
}

/*!
  Shows the dialog if it is still hidden after the algorithm has been started
  and minimumDuration milliseconds have passed.

  \sa setMinimumDuration()
*/

void QnProgressDialog::forceShow()
{
    Q_D(QnProgressDialog);
    d->forceTimer->stop();
    if (d->shown_once || d->cancellation_flag)
        return;

    show();
    d->shown_once = true;
}

/*!
    \since 4.5
    \overload

    Opens the dialog and connects its accepted() signal to the slot specified
    by \a receiver and \a member.

    The signal will be disconnected from the slot when the dialog is closed.
*/
void QnProgressDialog::open(QObject *receiver, const char *member)
{
    Q_D(QnProgressDialog);
    connect(this, SIGNAL(canceled()), receiver, member);
    d->receiverToDisconnectOnClose = receiver;
    d->memberToDisconnectOnClose = member;
    base_type::open();
}

void QnProgressDialog::setAutoSize(bool autoSize)
{
    Q_D(QnProgressDialog);

    if (QnElidedLabel *label = qobject_cast<QnElidedLabel*>(d->label))
        label->setElideMode(autoSize ? Qt::ElideNone : Qt::ElideMiddle);

    d->label->setFixedWidth(autoSize ? -1 : qMax(d->label->sizeHint().width(), maxLabelFixedWidth));
}

void QnProgressDialog::_q_disconnectOnClose() {
    d_func()->_q_disconnectOnClose();
}

bool QnProgressDialog::isEventProcessor() const {
    return d_func()->processEvents;
}

void QnProgressDialog::setEventProcessor(bool isEventProcessor) {
    d_func()->processEvents = isEventProcessor;
}
