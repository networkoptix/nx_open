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
        if (m_tags.contains(link))
            m_tags.removeOne(link);
        else
            m_tags.append(link.trimmed());
        m_tags.removeDuplicates();
        ui->tagsLineEdit->setText(m_tags.join(lit(", ")));
        updateTagsList();
    });

    connect(ui->tagsLineEdit, &QLineEdit::textEdited, this, [this](const QString &text) {
        m_tags = ui->tagsLineEdit->text().split(QRegExp(lit("[ ,]")), QString::SkipEmptyParts);
        m_tags.removeDuplicates();
        updateTagsList();
    });
}

QnCameraBookmarkDialog::~QnCameraBookmarkDialog() {}


void QnCameraBookmarkDialog::updateTagsList() {
    
    QString html = lit("<html><body><center>");
    QString tag = lit("<a style=\"text-decoration:none;\" href=\"%1\"><font style=\"font-size:%2px;\">%3</font><\a><span style=\"text-decoration:none;\">  </span>");
    QString tagFilter = lit("<a style=\"text-decoration:none;\" href=\"%1\"><font style=\"font-size:%2px;color:#009933\">%3</font><\a><span style=\"text-decoration:none;\">  </span>");

    QStringList keys = m_tagsUsage;
    int min_value = 65353;
    int max_value = 0;
    int min_size = 0;
    int max_size = 6;
    int spread = 1;
    float step = 1.0;

    foreach(QString key, keys) {
        int value = 1;
        if (value < min_value) min_value = value;
        if (value > max_value) max_value = value;
    }

    foreach(QString key, keys) {
        int value = (min_size + max_size) / 2 + min_size;
        if (m_tags.contains(key)) {
            html.append(tagFilter.arg(key).arg(QString::number(12 + (value * 3))).arg(key));
        } else {
            html.append(tag.arg(key).arg(QString::number(12 + (value * 3))).arg(key));
        }
    }

    html.append(lit("</center></body></html>"));

    ui->tagsListLabel->setText(html);
    update();
}

QnCameraBookmarkTagsUsage QnCameraBookmarkDialog::tagsUsage() const {
    return m_tagsUsage;
}

void QnCameraBookmarkDialog::setTagsUsage(const QnCameraBookmarkTagsUsage &tagsUsage) {
    m_tagsUsage = tagsUsage;
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

    m_tags = bookmark.tags;
    m_tags.removeDuplicates();
    ui->tagsLineEdit->setText(m_tags.join(lit(", ")));
}

void QnCameraBookmarkDialog::submitData(QnCameraBookmark &bookmark) const {
    bookmark.name = ui->nameLineEdit->text();
    bookmark.description = ui->descriptionTextEdit->toPlainText();
    bookmark.timeout = ui->timeoutComboBox->currentData().toLongLong();
    bookmark.tags = m_tags;
}
