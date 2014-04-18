#include "add_camera_bookmark_dialog.h"
#include "ui_add_camera_bookmark_dialog.h"

QnAddCameraBookmarkDialog::QnAddCameraBookmarkDialog(QWidget *parent) :
    base_type(parent),
    ui(new Ui::QnAddCameraBookmarkDialog)
{
    ui->setupUi(this);
}

QnAddCameraBookmarkDialog::~QnAddCameraBookmarkDialog() {}


void QnAddCameraBookmarkDialog::updateTagsList() {
    /*
    QString html = "<html><body><center>";
    QString tag = "<a style=\"text-decoration:underline;\" href=\"%1\"><font style=\"font-size:%2px;\">%3</font><\a><span style=\"text-decoration:none;\">  </span>";
    QString tagFilter = "<a style=\"text-decoration:underline;\" href=\"%1\"><font style=\"font-size:%2px;color:#009933\">%3</font><\a><span style=\"text-decoration:none;\">  </span>";

    QMap<QString, int> occurences;
    QList<QString> keys = occurences.keys();
    int min_value = 65353;
    int max_value = 0;
    int min_size = 0;
    int max_size = 6;
    int spread = 1;
    float step = 1.0;

    foreach(QString key, keys) {
        int value = occurences.value(key, 0);
        if (value < min_value) min_value = value;
        if (value > max_value) max_value = value;
    }
    spread = max_value - min_value;
    if (spread == 0) spread = 1;
    step = ((float)(max_size - min_size)) / ((float)spread);

    foreach(QString key, keys) {
        int value = min_size + ((occurences.value(key, 0) - min_value) * step);
        if (m_filter->tags().contains(key)) {
            html.append(tagFilter.arg(key).arg(QString::number(12 + (value * 3))).arg(key));
        } else {
            html.append(tag.arg(key).arg(QString::number(12 + (value * 3))).arg(key));
        }
    }

    html.append("</center></body></html>");

    setHtml(html);
    update();*/
}
