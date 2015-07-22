#include "bookmark_widget.h"
#include "ui_bookmark_widget.h"

#include <core/resource/camera_bookmark.h>

namespace {
    const int defaultTimeoutIdx = 3;
}

QnBookmarkWidget::QnBookmarkWidget(QWidget *parent /*= 0*/):
    QWidget(parent),
    ui(new Ui::BookmarkWidget)
{
    ui->setupUi(this);

    connect(ui->tagsListLabel, &QLabel::linkActivated, this, [this](const QString &link) {
        if (m_selectedTags.contains(link))
            m_selectedTags.removeOne(link);
        else
            m_selectedTags.append(link.trimmed());
        m_selectedTags.removeDuplicates();
        ui->tagsLineEdit->setText(m_selectedTags.join(lit(", ")));
        updateTagsList();
    });

    connect(ui->tagsLineEdit, &QLineEdit::textEdited, this, [this](const QString &text) {
        m_selectedTags = text.split(QRegExp(lit("[ ,]")), QString::SkipEmptyParts);
        m_selectedTags.removeDuplicates();
        updateTagsList();
    });

    QDateTime start = QDateTime::fromMSecsSinceEpoch(0);
    ui->timeoutComboBox->clear();
    ui->timeoutComboBox->addItem(tr("Do not lock archive"), 0);
    ui->timeoutComboBox->addItem(tr("1 month"), start.addMonths(1).toMSecsSinceEpoch());
    ui->timeoutComboBox->addItem(tr("3 month"), start.addMonths(3).toMSecsSinceEpoch());
    ui->timeoutComboBox->addItem(tr("6 month"), start.addMonths(6).toMSecsSinceEpoch());
    ui->timeoutComboBox->addItem(tr("year"), start.addYears(1).toMSecsSinceEpoch());
    ui->timeoutComboBox->setCurrentIndex(defaultTimeoutIdx);
}

QnBookmarkWidget::~QnBookmarkWidget() {}

QnCameraBookmarkTags QnBookmarkWidget::tags() const {
    return m_allTags;
}

void QnBookmarkWidget::setTags(const QnCameraBookmarkTags &tags) {
    m_allTags = tags;
    updateTagsList();
}

void QnBookmarkWidget::loadData(const QnCameraBookmark &bookmark) {
    ui->nameLineEdit->setText(bookmark.name);
    ui->descriptionTextEdit->setPlainText(bookmark.description);

    QDateTime start = QDateTime::fromMSecsSinceEpoch(bookmark.startTimeMs);
    ui->timeoutComboBox->clear();
    ui->timeoutComboBox->addItem(tr("Do not lock archive"), 0);
    ui->timeoutComboBox->addItem(tr("1 month"), start.addMonths(1).toMSecsSinceEpoch() - bookmark.startTimeMs);
    ui->timeoutComboBox->addItem(tr("3 month"), start.addMonths(3).toMSecsSinceEpoch() - bookmark.startTimeMs);
    ui->timeoutComboBox->addItem(tr("6 month"), start.addMonths(6).toMSecsSinceEpoch() - bookmark.startTimeMs);
    ui->timeoutComboBox->addItem(tr("year"), start.addYears(1).toMSecsSinceEpoch() - bookmark.startTimeMs);

    int timeoutIdx = ui->timeoutComboBox->findData(bookmark.timeout);
    ui->timeoutComboBox->setCurrentIndex(timeoutIdx < 0 ? defaultTimeoutIdx : timeoutIdx);

    m_selectedTags = bookmark.tags;
    m_selectedTags.removeDuplicates();
    ui->tagsLineEdit->setText(m_selectedTags.join(lit(", ")));

    updateTagsList();
}

void QnBookmarkWidget::submitData(QnCameraBookmark &bookmark) const {
    bookmark.name = ui->nameLineEdit->text();
    bookmark.description = ui->descriptionTextEdit->toPlainText();
    bookmark.timeout = ui->timeoutComboBox->currentData().toLongLong();
    bookmark.tags = m_selectedTags;
}

void QnBookmarkWidget::updateTagsList() {
    QString html = lit("<html><body><center>%1</center></body></html>");
    QString unusedTag = lit("<a style=\"text-decoration:none;\" href=\"%1\">%1<\a><span style=\"text-decoration:none;\"> </span>");
    QString usedTag = lit("<a style=\"text-decoration:none;\" href=\"%1\"><font style=\"color:#009933\">%1</font><\a><span style=\"text-decoration:none;\"> </span>");

    QString tags;
    foreach(const QString &tag, m_allTags) {
        if (m_selectedTags.contains(tag)) {
            tags.append(usedTag.arg(tag));
        } else {
            tags.append(unusedTag.arg(tag));
        }
    }

    ui->tagsListLabel->setText(html.arg(tags));
    update();
}
