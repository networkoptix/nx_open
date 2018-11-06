#ifndef NOTIFICATION_SOUND_MODEL_H
#define NOTIFICATION_SOUND_MODEL_H

#include <QtGui/QStandardItemModel>

class QnNotificationSoundModel : public QStandardItemModel
{
    Q_OBJECT

    typedef QStandardItemModel base_type;
public:
    enum Columns {
        TitleColumn,
        FilenameColumn,

        ColumnCount
    };

    explicit QnNotificationSoundModel(QObject *parent = 0);

    int rowByFilename(const QString &filename) const;
    QString filenameByRow(int row) const;

    QString titleByFilename(const QString &filename) const;

    void init();
    void loadList(const QStringList &filenames);

    void addDownloading(const QString &filename, bool silent = false);
    void addUploading(const QString &filename);

    void updateTitle(const QString &filename, const QString &title);

    bool loaded() const;

    virtual Qt::ItemFlags flags(const QModelIndex &index) const override;

    // Override standard sort to avoid '<No Sound>' item from being sorted
    virtual void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

signals:
    void listUnloaded();
    void listLoaded();

    void itemAdded(const QString &filename);
    void itemChanged(const QString &filename);
    void itemRemoved(const QString &filename);
private:
    bool m_loaded;

};

#endif // NOTIFICATION_SOUND_MODEL_H
