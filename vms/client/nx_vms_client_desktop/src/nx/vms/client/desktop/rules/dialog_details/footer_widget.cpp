// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "footer_widget.h"

#include "ui_footer_widget.h"

#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <ui/common/palette.h>
#include <ui/dialogs/week_time_schedule_dialog.h>
#include <utils/common/event_processors.h>

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
        [this] { ui->stackedWidget->setCurrentWidget(ui->mainPage); });

    installEventHandler({ui->commentLineEdit}, {QEvent::FocusOut}, this,
        [this] { ui->stackedWidget->setCurrentWidget(ui->mainPage); });

    connect(ui->commentLineEdit, &QLineEdit::textEdited, this,
        [this](const QString& text)
        {
            const auto comment = text.trimmed();

            displayComment(comment);

            if (auto rule = displayedRule.lock())
                rule->setComment(comment);
        });

    setupCommentLabelInteractions();

    connect(ui->setScheduleButton, &QPushButton::clicked, this,
        [this]
        {
            auto rule = displayedRule.lock();
            if (!rule)
                return;

            QnWeekTimeScheduleDialog dialog(this);
            dialog.setScheduleTasks(rule->schedule());
            if (!dialog.exec())
                return;

            rule->setSchedule(dialog.scheduleTasks().toUtf8());
        });
}

// Non-inline destructor is required for member scoped pointers to forward declared classes.
FooterWidget::~FooterWidget()
{
}

void FooterWidget::setRule(std::weak_ptr<SimplifiedRule> rule)
{
    displayedRule = rule;

    updateUi();
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
        Skin::colorize(qnSkin->pixmap("text_buttons/ok.png"),
        QPalette().color(QPalette::BrightText)));

    setPaletteColor(
        ui->commentLineEdit,
        QPalette::PlaceholderText,
        QPalette().color(QPalette::Midlight));
}

void FooterWidget::updateUi()
{
    auto rule = displayedRule.lock();
    if (!rule)
        return;

    displayComment(rule->comment());
}

void FooterWidget::displayComment(const QString& comment)
{
    ui->commentDisplayLabel->setText(comment.trimmed());
    if (!ui->commentDisplayLabel->text().isEmpty())
        ui->commentStackedWidget->setCurrentWidget(ui->commentDisplayPage);
    else
        ui->commentStackedWidget->setCurrentWidget(ui->commentButtonPage);
}

} // namespace nx::vms::client::desktop::rules
