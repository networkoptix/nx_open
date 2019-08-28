#pragma once

#include <QtCore/QDate>
#include <QtCore/QLocale>
#include <QtCore/QAbstractListModel>

#include <nx/utils/impl_ptr.h>

class QnCameraChunkProvider;

class QnCalendarModel: public QAbstractListModel
{
    Q_OBJECT
    using base_type = QAbstractListModel;

    Q_PROPERTY(int year READ year WRITE setYear
        NOTIFY yearChanged)
    Q_PROPERTY(int month READ month WRITE setMonth
        NOTIFY monthChanged)
    Q_PROPERTY(QLocale locale READ locale WRITE setLocale
        NOTIFY localeChanged)
    Q_PROPERTY(QnCameraChunkProvider* chunkProvider READ chunkProvider WRITE setChunkProvider
        NOTIFY chunkProviderChanged)
    Q_PROPERTY(qint64 currentPosition READ currentPosition WRITE setCurrentPosition
        NOTIFY currentPositionChanged)
    Q_PROPERTY(qint64 displayOffset READ displayOffset WRITE setDisplayOffset
        NOTIFY displayOffsetChanged)

public:
    enum Roles
    {
        DayStartTimeRole = Qt::UserRole + 1,
        IsCurrentRole,
        HasArchiveRole
    };

    QnCalendarModel(QObject* parent = nullptr);
    virtual ~QnCalendarModel() override;

public: // Properties and invokables.
    int year() const;
    void setYear(int year);

    int month() const;
    void setMonth(int month);

    QLocale locale() const;
    void setLocale(const QLocale& locale);

    QnCameraChunkProvider* chunkProvider() const;
    void setChunkProvider(QnCameraChunkProvider* chunkProvider);

    qint64 currentPosition() const;
    void setCurrentPosition(qint64 value);

    qint64 displayOffset() const;
    void setDisplayOffset(qint64 value);

public: // Overrides section.
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex& index, int role) const override;

    virtual QHash<int, QByteArray> roleNames() const override;

signals:
    void yearChanged();
    void monthChanged();
    void chunkProviderChanged();
    void localeChanged();
    void currentPositionChanged();
    void displayOffsetChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};
