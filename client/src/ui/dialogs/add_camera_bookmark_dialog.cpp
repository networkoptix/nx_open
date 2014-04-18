#include "add_camera_bookmark_dialog.h"
#include "ui_add_camera_bookmark_dialog.h"

QnAddCameraBookmarkDialog::QnAddCameraBookmarkDialog(QWidget *parent) :
    base_type(parent),
    ui(new Ui::QnAddCameraBookmarkDialog)
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

QnAddCameraBookmarkDialog::~QnAddCameraBookmarkDialog() {}


void QnAddCameraBookmarkDialog::updateTagsList() {
    
    QString html = lit("<html><body><center>");
    QString tag = lit("<a style=\"text-decoration:none;\" href=\"%1\"><font style=\"font-size:%2px;\">%3</font><\a><span style=\"text-decoration:none;\">  </span>");
    QString tagFilter = lit("<a style=\"text-decoration:none;\" href=\"%1\"><font style=\"font-size:%2px;color:#009933\">%3</font><\a><span style=\"text-decoration:none;\">  </span>");

    QList<QString> keys = m_tagsUsage.keys();
    int min_value = 65353;
    int max_value = 0;
    int min_size = 0;
    int max_size = 6;
    int spread = 1;
    float step = 1.0;

    foreach(QString key, keys) {
        int value = m_tagsUsage[key];
        if (value < min_value) min_value = value;
        if (value > max_value) max_value = value;
    }
    spread = max_value - min_value;
    if (spread == 0) spread = 1;
    step = ((float)(max_size - min_size)) / ((float)spread);

    foreach(QString key, keys) {
        int value = min_size + ((m_tagsUsage[key] - min_value) * step);
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

QHash<QString, int> QnAddCameraBookmarkDialog::tagsUsage() const {
    return m_tagsUsage;
}

void QnAddCameraBookmarkDialog::setTagsUsage(const QHash<QString, int> tagsUsage) {
    m_tagsUsage = tagsUsage;
    updateTagsList();
}

void QnAddCameraBookmarkDialog::loadData(const QnCameraBookmark &bookmark) {
    ui->nameLineEdit->setText(bookmark.name);
    ui->descriptionTextEdit->setPlainText(bookmark.description);

    m_tags = bookmark.tags;
    m_tags.removeDuplicates();
    ui->tagsLineEdit->setText(m_tags.join(lit(", ")));
}

void QnAddCameraBookmarkDialog::submitData(QnCameraBookmark &bookmark) const {
    bookmark.name = ui->nameLineEdit->text();
    bookmark.description = ui->descriptionTextEdit->toPlainText();
    bookmark.tags = m_tags;
}
