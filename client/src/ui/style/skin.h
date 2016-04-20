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

    QIcon icon(const QString& name, const QString& checkedName = QString(), int numModes = -1, const QPair<QIcon::Mode, QString>* modes = nullptr);
    QIcon icon(const char* name, const char* checkedName = nullptr);
    QIcon icon(const QIcon& icon);

    QPixmap pixmap(const QString& name, const QSize& size = QSize(), Qt::AspectRatioMode aspectMode = Qt::IgnoreAspectRatio, Qt::TransformationMode mode = Qt::FastTransformation);
    QPixmap pixmap(const char* name, const QSize& size = QSize(), Qt::AspectRatioMode aspectMode = Qt::IgnoreAspectRatio, Qt::TransformationMode mode = Qt::FastTransformation);

    QMovie* newMovie(const QString& name, QObject* parent = nullptr);
    QMovie* newMovie(const char* name, QObject* parent = nullptr);

    static QStyle* newStyle(const QnGenericPalette& genericPalette);

private:
    void init(const QStringList& paths);

private:
    QStringList m_paths;
    QnNoptixIconLoader* m_iconLoader;
};

#define qnSkin (QnSkin::instance())

#endif // QN_SKIN_H
