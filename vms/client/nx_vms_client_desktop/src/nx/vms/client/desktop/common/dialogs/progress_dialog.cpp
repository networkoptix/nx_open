// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "progress_dialog.h"

#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>

#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/style/helper.h>

#include <ui/widgets/common/elided_label.h>
#include <ui/widgets/common/dialog_button_box.h>
#include <ui/widgets/word_wrapped_label.h>

#include <nx/utils/log/assert.h>

namespace nx::vms::client::desktop {

namespace {

static constexpr int kDefaultRangeMinimum = 0;
static constexpr int kDefaultRangeMaximum = 100;

/* Minimum text label width. */
static constexpr int kMinimumTextLabelWidth = 150;

} // namespace

struct ProgressDialog::Private
{
    ProgressDialog* const q;
    bool wasCanceled = false;

    struct
    {
        QProgressBar* progressBar = nullptr;
        QnElidedLabel* textLabel = nullptr;
        QnWordWrappedLabel* infoLabel = nullptr;
        QDialogButtonBox* buttonBox = nullptr;
        QPushButton* cancelButton = nullptr;
    } ui;

    Private(ProgressDialog* parent):
        q(parent)
    {
        setupUi();
    }

    void setupUi()
    {
        ui.textLabel = new QnElidedLabel(q);
        ui.textLabel->setMinimumWidth(kMinimumTextLabelWidth);
        ui.textLabel->setForegroundRole(QPalette::Light);

        ui.progressBar = new QProgressBar(q);
        ui.progressBar->setRange(kDefaultRangeMinimum, kDefaultRangeMaximum);

        ui.buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel, q);
        ui.cancelButton = ui.buttonBox->button(QDialogButtonBox::Cancel);
        setWarningButtonStyle(ui.cancelButton);

        QFrame* line = new QFrame(q);
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Sunken);

        QVBoxLayout* contentLayout = new QVBoxLayout();
        contentLayout->addWidget(ui.textLabel);

        ui.infoLabel = new QnWordWrappedLabel(q);
        ui.infoLabel->setVisible(false);
        contentLayout->addWidget(ui.infoLabel, 1);
        contentLayout->addWidget(ui.progressBar);
        contentLayout->setContentsMargins(style::Metrics::kDefaultTopLevelMargins);

        auto layout = new QVBoxLayout();
        layout->addLayout(contentLayout);
        layout->addWidget(line);
        layout->addWidget(ui.buttonBox);
        layout->setSizeConstraint(QLayout::SetFixedSize);
        q->setLayout(layout);
    }
};

ProgressDialog::ProgressDialog(QWidget* parent):
    base_type(parent, Qt::Dialog),
    d(new Private(this))
{
    connect(d->ui.buttonBox, &QDialogButtonBox::rejected, this, &ProgressDialog::reject);
}

ProgressDialog::~ProgressDialog()
{
}

int ProgressDialog::minimum() const
{
    return d->ui.progressBar->minimum();
}

int ProgressDialog::maximum() const
{
    return d->ui.progressBar->maximum();
}

void ProgressDialog::setRange(int minimum, int maximum)
{
    d->ui.progressBar->setRange(minimum, maximum);
}

void ProgressDialog::setInfiniteMode()
{
    setRange(0, 0);
}

int ProgressDialog::value() const
{
    return d->ui.progressBar->value();
}

void ProgressDialog::setValue(int value)
{
    d->ui.progressBar->setValue(value);
}

QString ProgressDialog::text() const
{
    return d->ui.textLabel->text();
}

void ProgressDialog::setText(const QString& value)
{
    d->ui.textLabel->setText(value);
}

QString ProgressDialog::infoText() const
{
    return d->ui.infoLabel->text();
}

void ProgressDialog::setInfoText(const QString& value)
{
    d->ui.infoLabel->setText(value);
    d->ui.infoLabel->setVisible(!value.isEmpty());
}

QString ProgressDialog::cancelText() const
{
    return d->ui.cancelButton->text();
}

void ProgressDialog::setCancelText(const QString& value)
{
    d->ui.cancelButton->setText(value);
}

void ProgressDialog::addCustomButton(
    QAbstractButton* button,
    QDialogButtonBox::ButtonRole buttonRole)
{
    d->ui.buttonBox->addButton(button, buttonRole);
}

bool ProgressDialog::wasCanceled() const
{
    return d->wasCanceled;
}

void ProgressDialog::reject()
{
    d->wasCanceled = true;
    emit canceled();
    base_type::reject();
}

} // namespace nx::vms::client::desktop
