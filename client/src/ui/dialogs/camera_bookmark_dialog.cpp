#include "camera_bookmark_dialog.h"
#include "ui_camera_bookmark_dialog.h"

#include <core/resource/camera_bookmark.h>

namespace {
    const int defaultTimeoutIdx = 3;
}

QnCameraBookmarkDialog::QnCameraBookmarkDialog(QWidget *parent) :
    base_type(parent),
    ui(new Ui::QnCameraBookmarkDialog)
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
}

QnCameraBookmarkDialog::~QnCameraBookmarkDialog() {}


void QnCameraBookmarkDialog::updateTagsList() {
    
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

QnCameraBookmarkTags QnCameraBookmarkDialog::tags() const {
    return m_allTags;
}

void QnCameraBookmarkDialog::setTags(const QnCameraBookmarkTags &tags) {
    m_allTags = tags;
    updateTagsList();
}

void QnCameraBookmarkDialog::loadData(const QnCameraBookmark &bookmark) {
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

void QnCameraBookmarkDialog::submitData(QnCameraBookmark &bookmark) const {
    bookmark.name = ui->nameLineEdit->text();
    bookmark.description = ui->descriptionTextEdit->toPlainText();
    bookmark.timeout = ui->timeoutComboBox->currentData().toLongLong();
    bookmark.tags = m_selectedTags;
}
