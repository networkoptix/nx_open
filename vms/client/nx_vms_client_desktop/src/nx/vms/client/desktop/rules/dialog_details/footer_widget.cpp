// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "footer_widget.h"

#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/ui/dialogs/week_time_schedule_dialog.h>
#include <ui/common/palette.h>
#include <utils/common/event_processors.h>

#include "ui_footer_widget.h"

namespace nx::vms::client::desktop::rules {

FooterWidget::FooterWidget(QWidget* parent):
    QWidget(parent),
    ui(new Ui::FooterWidget())
{
    ui->setupUi(this);
    setupIconsAndColors();

    connect(ui->addCommentButton, &QPushButton::clicked, this,
        [this]
        {
            ui->stackedWidget->setCurrentWidget(ui->commentEditorPage);
            ui->commentLineEdit->setFocus(Qt::OtherFocusReason);
        });

    connect(ui->commitCommentButton, &QPushButton::clicked, this,
        [this]
        {
            const auto comment = ui->commentLineEdit->text().trimmed();
            if (auto rule = m_displayedRule.lock())
                rule->setComment(comment);

            displayComment(comment);

            ui->stackedWidget->setCurrentWidget(ui->mainPage);
        });

    installEventHandler({ui->commentLineEdit}, {QEvent::FocusOut}, this,
        [this] { ui->stackedWidget->setCurrentWidget(ui->mainPage); });

    setupCommentLabelInteractions();

    connect(ui->setScheduleButton, &QPushButton::clicked, this,
        [this]
        {
            auto rule = m_displayedRule.lock();
            if (!rule)
                return;

            WeekTimeScheduleDialog dialog(this, /*isEmptyAllowed*/ false);
            dialog.setSchedule(rule->schedule());
            if (dialog.exec() != QDialog::Accepted)
                return;

            rule->setSchedule(dialog.schedule());
        });
}

// Non-inline destructor is required for member scoped pointers to forward declared classes.
FooterWidget::~FooterWidget()
{
}

void FooterWidget::setRule(std::weak_ptr<SimplifiedRule> rule)
{
    m_displayedRule = std::move(rule);

    if (auto rule = m_displayedRule.lock())
        displayComment(rule->comment());
}

// Makes comment label clickable like a push button. Comment label is highlighted on mouse hover.
void FooterWidget::setupCommentLabelInteractions()
{
    installEventHandler(
        {ui->commentDisplayLabel},
        {QEvent::MouseButtonPress, QEvent::MouseButtonRelease, QEvent::HoverEnter, QEvent::HoverLeave},
        this,
        [this, wasPressed = false](QObject* /*object*/, QEvent* event) mutable
        {
            if (!ui->addCommentButton->isEnabled())
                return;

            if (event->type() == QEvent::MouseButtonPress)
                wasPressed = true;

            if (event->type() == QEvent::MouseButtonRelease && wasPressed)
            {
                wasPressed = false;
                const auto label = ui->commentDisplayLabel;
                if (label->geometry().contains(label->mapFromGlobal(QCursor::pos())))
                {
                    ui->stackedWidget->setCurrentWidget(ui->commentEditorPage);
                    ui->commentLineEdit->setFocus(Qt::OtherFocusReason);
                }
            }

            if (event->type() == QEvent::HoverEnter)
            {
                setPaletteColor(
                    ui->commentDisplayLabel,
                    QPalette::WindowText,
                    QPalette().color(QPalette::BrightText));
            }

            if (event->type() == QEvent::HoverLeave)
            {
                setPaletteColor(
                    ui->commentDisplayLabel,
                    QPalette::WindowText,
                    QPalette().color(QPalette::Light));
            }
        });
}

void FooterWidget::setupIconsAndColors()
{
    ui->setScheduleButton->setIcon(qnSkin->pixmap("text_buttons/calendar.png"));
    ui->testActionButton->setIcon(qnSkin->pixmap("text_buttons/forward.png"));
    ui->commitCommentButton->setIcon(
        core::Skin::colorize(qnSkin->pixmap("text_buttons/ok.png"),
        QPalette().color(QPalette::BrightText)));

    setPaletteColor(
        ui->commentLineEdit,
        QPalette::PlaceholderText,
        QPalette().color(QPalette::Midlight));
}

void FooterWidget::displayComment(const QString& comment)
{
    const auto trimmedComment = comment.trimmed();
    ui->commentDisplayLabel->setText(trimmedComment);
    ui->commentLineEdit->setText(trimmedComment);
    if (!trimmedComment.isEmpty())
        ui->commentStackedWidget->setCurrentWidget(ui->commentDisplayPage);
    else
        ui->commentStackedWidget->setCurrentWidget(ui->commentButtonPage);
}

} // namespace nx::vms::client::desktop::rules
