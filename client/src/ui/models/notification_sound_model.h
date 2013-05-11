#ifndef NOTIFICATION_SOUND_MODEL_H
#define NOTIFICATION_SOUND_MODEL_H

#include <QStandardItemModel>

class QnNotificationSoundModel : public QStandardItemModel
{
    Q_OBJECT
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

    void loadList(const QStringList &filenames);
    void addUploading(const QString &filename);
    void updateTitle(const QString &filename, const QString &title);
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
