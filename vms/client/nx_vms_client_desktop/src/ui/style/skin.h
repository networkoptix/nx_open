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
    QPixmap pixmap(const char* name, bool correctDevicePixelRatio = true);

    QPixmap pixmap(const QString& name, bool correctDevicePixelRatio = true);

    QMovie* newMovie(const QString& name, QObject* parent = nullptr);
    QMovie* newMovie(const char* name, QObject* parent = nullptr);

    static QSize maximumSize(const QIcon& icon, QIcon::Mode mode = QIcon::Normal,
        QIcon::State state = QIcon::Off);

    static QPixmap maximumSizePixmap(const QIcon& icon, QIcon::Mode mode = QIcon::Normal,
        QIcon::State state = QIcon::Off, bool correctDevicePixelRatio = true);

    static QStyle* newStyle(const QnGenericPalette& genericPalette);

    static bool isHiDpi();

    static QPixmap colorize(const QPixmap& source, const QColor& color);

private:
    void init(const QStringList& paths);

    QPixmap getPixmapInternal(const QString &name);

private:
    QStringList m_paths;
    QnNoptixIconLoader* m_iconLoader;
};

#define qnSkin (QnSkin::instance())

#endif // QN_SKIN_H
