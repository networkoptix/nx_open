#ifndef QN_SKIN_H
#define QN_SKIN_H

#include <QtCore/QHash>

#include <QtGui/QIcon>
#include <QtGui/QPixmap>
#include <QtGui/QMovie>

#include <nx/utils/singleton.h>

#include "icon.h"

class QStyle;
class QnNoptixIconLoader;
class QnGenericPalette;

class QnSkin: public QObject, public Singleton<QnSkin>
{
    Q_OBJECT

public:
    QnSkin(QObject* parent = nullptr);
    QnSkin(const QStringList& paths, QObject* parent = nullptr);
    virtual ~QnSkin();

    const QStringList& paths() const;

    QString path(const QString& name) const;
    QString path(const char* name) const;

    bool hasFile(const QString& name) const;
    bool hasFile(const char* name) const;


    QIcon icon(const QString& name,
        const QString& checkedName = QString(),
        const QnIcon::SuffixesList* suffixes = nullptr);
    QIcon icon(const char* name, const char* checkedName = nullptr);
    QIcon icon(const QIcon& icon);

    /// @brief Loads pixmap with appropriate size according to current hidpi settings
    QPixmap pixmap(const char* name,
        const QSize& size = QSize(),
        Qt::AspectRatioMode aspectMode = Qt::IgnoreAspectRatio,
        Qt::TransformationMode mode = Qt::FastTransformation);

    QPixmap pixmap(const QString& name,
        const QSize& size = QSize(),
        Qt::AspectRatioMode aspectMode = Qt::IgnoreAspectRatio,
        Qt::TransformationMode mode = Qt::FastTransformation);


    QMovie* newMovie(const QString& name, QObject* parent = nullptr);
    QMovie* newMovie(const char* name, QObject* parent = nullptr);

    static QSize maximumSize(const QIcon& icon, QIcon::Mode mode = QIcon::Normal,
        QIcon::State state = QIcon::Off, QWindow* window = nullptr);

    static QPixmap maximumSizePixmap(const QIcon& icon, QIcon::Mode mode = QIcon::Normal,
        QIcon::State state = QIcon::Off, QWindow* window = nullptr);

    static QStyle* newStyle(const QnGenericPalette& genericPalette);

private:
    void init(const QStringList& paths);

    QPixmap getPixmapInternal(const QString &name, const QSize& size = QSize(), Qt::AspectRatioMode aspectMode = Qt::IgnoreAspectRatio, Qt::TransformationMode mode = Qt::FastTransformation);

private:
    QStringList m_paths;
    QnNoptixIconLoader* m_iconLoader;
};

#define qnSkin (QnSkin::instance())

#endif // QN_SKIN_H
