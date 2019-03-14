/* This file is a modified version of Qt's QProgressDialog.
 * Original copyright notice follows. */

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

#ifndef QN_PROGRESS_DIALOG_H
#define QN_PROGRESS_DIALOG_H

#include <QtCore/QScopedPointer>
#include <QtGui/QtEvents>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>

#include <ui/dialogs/common/dialog.h>

class QPushButton;
class QLabel;
class QProgressBar;
class QTimer;
class QnProgressDialogPrivate;

/**
 * A progress dialog that does not call <tt>QApplication::processEvents</tt>
 * when its value changes as the progress dialog from Qt does.
 *
 * This behavior is controlled via <tt>isEventProcessor</tt> property.
 */
class QnProgressDialog : public QnDialog {
    Q_OBJECT
    Q_PROPERTY(bool wasCanceled READ wasCanceled)
    Q_PROPERTY(int minimum READ minimum WRITE setMinimum)
    Q_PROPERTY(int maximum READ maximum WRITE setMaximum)
    Q_PROPERTY(int value READ value WRITE setValue)
    Q_PROPERTY(bool autoReset READ autoReset WRITE setAutoReset)
    Q_PROPERTY(bool autoClose READ autoClose WRITE setAutoClose)
    Q_PROPERTY(int minimumDuration READ minimumDuration WRITE setMinimumDuration)
    Q_PROPERTY(QString labelText READ labelText WRITE setLabelText)
    Q_PROPERTY(bool isEventProcessor READ isEventProcessor WRITE setEventProcessor)

    typedef QnDialog base_type;
public:
    explicit QnProgressDialog(QWidget *parent = 0, Qt::WindowFlags flags = Qt::Dialog);
    QnProgressDialog(const QString &labelText, const QString &cancelButtonText, int minimum, int maximum, QWidget *parent = 0, Qt::WindowFlags flags = Qt::Dialog);
    virtual ~QnProgressDialog();

    void setLabel(QLabel *label);
    void setCancelButton(QPushButton *button);
    void addButton(QPushButton *button, QDialogButtonBox::ButtonRole role);
    void setBar(QProgressBar *bar);

    bool wasCanceled() const;

    int minimum() const;
    int maximum() const;

    int value() const;

    QString labelText() const;
    int minimumDuration() const;

    void setAutoReset(bool reset);
    bool autoReset() const;
    void setAutoClose(bool close);
    bool autoClose() const;

    bool isEventProcessor() const;
    void setEventProcessor(bool isEventProcessor);

    using QDialog::open;
    void open(QObject *receiver, const char *member);

    void setAutoSize(bool autoSize);

Q_SIGNALS:
    void canceled();

public Q_SLOTS:
    void cancel();
    void reset();
    void setMaximum(int maximum);
    void setMinimum(int minimum);
    void setRange(int minimum, int maximum);
    void setValue(int progress);
    void setLabelText(const QString &text);
    void setCancelButtonText(const QString &text);
    void setMinimumDuration(int ms);
    void setInfiniteProgress();

private Q_SLOTS:
    void _q_disconnectOnClose();

protected:
    virtual void closeEvent(QCloseEvent *event) override;
    virtual void changeEvent(QEvent *event) override;
    virtual void showEvent(QShowEvent *event) override;

protected Q_SLOTS:
    void forceShow();

private:
    Q_DECLARE_PRIVATE(QnProgressDialog)

    QScopedPointer<QnProgressDialogPrivate> d_ptr;
};

#endif // QN_PROGRESS_DIALOG_H
