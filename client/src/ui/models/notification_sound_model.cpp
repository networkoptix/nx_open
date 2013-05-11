#include "notification_sound_model.h"

QnNotificationSoundModel::QnNotificationSoundModel(QObject *parent) :
    QStandardItemModel(parent),
    m_loaded(false)
{
}

void QnNotificationSoundModel::loadList(const QStringList &filenames) {
    clear();
    m_loaded = true;
    foreach (QString filename, filenames) {
        QList<QStandardItem *> row;
        row << new QStandardItem(tr("Downloading sound..."))
            << new QStandardItem(filename);
        //TODO: #GDM append columns: duration, date added (?)
        appendRow(row);
    }
    emit listLoaded();
}

void QnNotificationSoundModel::updateTitle(const QString &filename, const QString &title) {
    QList<QStandardItem*> items = findItems(filename, Qt::MatchExactly, FilenameColumn);
    if (items.size() == 0) {
        return;
        //TODO: #GDM boolean flag in the model, clear on logoff
    }
    QStandardItem* item = this->item(items[0]->row(), TitleColumn);
    if (!item)
        return;
    item->setText(title);
    emit itemChanged(filename);
}


int QnNotificationSoundModel::rowByFilename(const QString &filename) const {
    QList<QStandardItem*> items = findItems(filename, Qt::MatchExactly, FilenameColumn);
    if (items.size() == 0) {
        return -1;
        //TODO: #GDM boolean flag in the model, clear on logoff
        //TODO: #GDM what if sound was removed?
    }
    return items[0]->row();
}

QString QnNotificationSoundModel::filenameByRow(int row) const {
    //TODO: #GDM check if model is already loaded
    QModelIndex idx = index(row, FilenameColumn);
    if (!idx.isValid())
        return QString();

    return itemFromIndex(idx)->text();
}


QString QnNotificationSoundModel::titleByFilename(const QString &filename) const {
    QList<QStandardItem*> items = findItems(filename, Qt::MatchExactly, FilenameColumn);
    if (items.size() == 0) {
        return tr("Downloading sound list...");
        //TODO: #GDM boolean flag in the model, clear on logoff
        //TODO: #GDM what if sound was removed?
    }
    QStandardItem* item = items[0];
    return this->item(item->row(), TitleColumn)->text();
}
