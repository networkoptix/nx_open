#ifndef QN_SKIN_H
#define QN_SKIN_H

#include <QtCore/QHash>

#include <QtGui/QIcon>
#include <QtGui/QPixmap>
#include <QtGui/QMovie>

#include <utils/common/singleton.h>

class QStyle;

class QnSkin: public QObject, public Singleton<QnSkin> {
    Q_OBJECT

public:
    static const QIcon::Mode Normal = QIcon::Normal;
    static const QIcon::Mode Disabled = QIcon::Disabled;
    static const QIcon::Mode Active = QIcon::Active;
    static const QIcon::Mode Selected = QIcon::Selected;
    static const QIcon::Mode Pressed = static_cast<QIcon::Mode>(0xF);

    static const QIcon::State On = QIcon::On;
    static const QIcon::State Off = QIcon::Off;

    QnSkin(QObject *parent = NULL);
    QnSkin(const QStringList &paths, QObject *parent = NULL);
    virtual ~QnSkin();

    const QStringList &paths() const;

    QString path(const QString &name) const;
    QString path(const char *name) const;

    bool hasFile(const QString &name) const;
    bool hasFile(const char *name) const;

    QIcon icon(const QString &name, const QString &checkedName = QString());
    QIcon icon(const char *name, const char *checkedName = NULL);

    QPixmap pixmap(const QString &name, const QSize &size = QSize(), Qt::AspectRatioMode aspectMode = Qt::IgnoreAspectRatio, Qt::TransformationMode mode = Qt::FastTransformation);
    QPixmap pixmap(const char *name, const QSize &size = QSize(), Qt::AspectRatioMode aspectMode = Qt::IgnoreAspectRatio, Qt::TransformationMode mode = Qt::FastTransformation);
    QPixmap pixmap(const QIcon &icon, const QSize &size, QIcon::Mode mode = Normal, QIcon::State state = Off) const;
    QPixmap pixmap(const QIcon &icon, int w, int h, QIcon::Mode mode = Normal, QIcon::State state = Off) const;
    QPixmap pixmap(const QIcon &icon, int extent, QIcon::Mode mode = Normal, QIcon::State state = Off) const;

    QMovie *newMovie(const QString &name, QObject* parent = NULL);
    QMovie *newMovie(const char *name, QObject* parent = NULL);

    QStyle *newStyle();

private:
    void init(const QStringList &paths);

private:
    QStringList m_paths;
    QHash<QString, QIcon> m_iconByKey;
    QHash<qint64, QIcon> m_pressedIconByCacheKey;
};

#define qnSkin (QnSkin::instance())

#endif // QN_SKIN_H
