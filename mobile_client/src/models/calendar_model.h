#pragma once

#include <QtCore/QAbstractListModel>
#include <QtCore/QScopedPointer>
#include <QtCore/QDate>

class QnCalendarModelPrivate;
class QnCameraChunkProvider;

class QnCalendarModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(int year READ year WRITE setYear NOTIFY yearChanged)
    Q_PROPERTY(int month READ month WRITE setMonth NOTIFY monthChanged)
    Q_PROPERTY(bool mondayIsFirstDay READ mondayIsFirstDay WRITE setMondayIsFirstDay NOTIFY mondayIsFirstDayChanged)
    Q_PROPERTY(QnCameraChunkProvider* chunkProvider READ chunkProvider WRITE setChunkProvider NOTIFY chunkProviderChanged)

public:

    enum Roles {
        DateRole = Qt::UserRole + 1,
        DayRole,
        HasArchiveRole
    };

    QnCalendarModel(QObject *parent = nullptr);
    ~QnCalendarModel();

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex &index, int role) const override;

    virtual QHash<int, QByteArray> roleNames() const override;

    int year() const;
    void setYear(int year);

    int month() const;
    void setMonth(int month);

    bool mondayIsFirstDay() const;
    void setMondayIsFirstDay(bool mondayIsFirstDay);

    QnCameraChunkProvider *chunkProvider() const;
    void setChunkProvider(QnCameraChunkProvider *chunkProvider);

signals:
    void yearChanged();
    void monthChanged();
    void mondayIsFirstDayChanged();
    void chunkProviderChanged();

private:
    QScopedPointer<QnCalendarModelPrivate> d;
};
