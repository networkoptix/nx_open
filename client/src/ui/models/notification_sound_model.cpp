#include "notification_sound_model.h"

QnNotificationSoundModel::QnNotificationSoundModel(QObject *parent) :
    QStandardItemModel(parent)
{
    init();
}

void QnNotificationSoundModel::init() {
    clear();
    m_loaded = false;
    QList<QStandardItem *> row;
    row << new QStandardItem(tr("<Downloading sound list...>"))
        << new QStandardItem(QString());
    appendRow(row);
}

void QnNotificationSoundModel::loadList(const QStringList &filenames) {
    clear();


    QList<QStandardItem *> row;
    row << new QStandardItem(tr("<No Sound>"))
        << new QStandardItem(QString());
    appendRow(row);

    m_loaded = true;
    foreach (QString filename, filenames)
        addDownloading(filename, true);
    emit listLoaded();
}

void QnNotificationSoundModel::addDownloading(const QString &filename, bool silent) {
    QList<QStandardItem *> row;
    row << new QStandardItem(tr("<Downloading sound...>"))
        << new QStandardItem(filename);
    //TODO: #GDM #Business append columns: duration, date added (?)
    appendRow(row);
    if (!silent)
        emit itemAdded(filename);
}

void QnNotificationSoundModel::addUploading(const QString &filename) {
    QList<QStandardItem *> row;
    row << new QStandardItem(tr("<Uploading sound...>"))
        << new QStandardItem(filename);
    //TODO: #GDM #Business append columns: duration, date added (?)
    appendRow(row);
    emit itemAdded(filename);
}

void QnNotificationSoundModel::updateTitle(const QString &filename, const QString &title) {
    if (!m_loaded)
        return;

    QList<QStandardItem*> items = findItems(filename, Qt::MatchExactly, FilenameColumn);
    if (items.size() == 0)
        return;
    QStandardItem* item = this->item(items[0]->row(), TitleColumn);
    if (!item)
        return;
    item->setText(title);
    sort(0);
    emit itemChanged(filename);
}

int QnNotificationSoundModel::rowByFilename(const QString &filename) const {
    if (!m_loaded)
        return 0; // Downloading items list

    QList<QStandardItem*> items = findItems(filename, Qt::MatchExactly, FilenameColumn);
    return (items.size() > 0) ? items[0]->row() : 0;
}

QString QnNotificationSoundModel::filenameByRow(int row) const {
    if (!m_loaded)
        return QString();

    QStandardItem* item = this->item(row, FilenameColumn);
    return item ? item->text() : QString();
}

QString QnNotificationSoundModel::titleByFilename(const QString &filename) const {
    int row = rowByFilename(filename);
    QStandardItem* item = this->item(row, TitleColumn);
    return item ? item->text() : QString();
}

bool QnNotificationSoundModel::loaded() const {
    return m_loaded;
}

void QnNotificationSoundModel::sort(int column, Qt::SortOrder order) {
    if (rowCount() > 0) {
        QList<QStandardItem*> noSoundRow = takeRow(0);
        QStandardItemModel::sort(column, order);
        insertRow(0, noSoundRow);
    } else {
        QStandardItemModel::sort(column, order);
    }
}
