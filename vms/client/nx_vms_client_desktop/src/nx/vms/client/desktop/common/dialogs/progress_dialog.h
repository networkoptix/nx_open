// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>
#include <QDialogButtonBox>

#include <ui/dialogs/common/dialog.h>
#include <nx/utils/impl_ptr.h>

class QAbstractButton;

namespace nx::vms::client::desktop {

/**
 * Dialog with a progress bar. By default uses range [0..100]. Supports customizing "Cancel" button
 * text and adding custom buttons. Does not call <tt>QApplication::processEvents</tt> when its value
 * changes as the progress dialog from Qt does.
 */
class ProgressDialog: public QnDialog
{
    Q_OBJECT
    using base_type = QnDialog;

public:
    explicit ProgressDialog(QWidget* parent = nullptr);
    virtual ~ProgressDialog();

    int minimum() const;
    int maximum() const;

    /** Set progress bar range. Zero range [0..0] will switch progress bar to a infinite mode. */
    void setRange(int minimum, int maximum);

    /** Switch progress bar to a infinite mode. Equivalent to setRange(0, 0). */
    void setInfiniteMode();

    int value() const;
    void setValue(int value);

    QString text() const;
    void setText(const QString& value);

    QString infoText() const;
    void setInfoText(const QString& value);

    QString cancelText() const;
    void setCancelText(const QString& value);

    void addCustomButton(QAbstractButton* button, QDialogButtonBox::ButtonRole buttonRole);

    bool wasCanceled() const;

signals:
    void canceled();

protected:
   virtual void reject() override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
