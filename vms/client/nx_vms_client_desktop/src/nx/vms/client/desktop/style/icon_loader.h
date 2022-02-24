// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QSet>
#include <QtGui/QIcon>

#include "icon.h"
#include "svg_icon_colorer.h"

namespace nx::vms::client::desktop {

class Skin;

class IconLoader: public QObject
{
    Q_OBJECT
    typedef QObject base_type;

public:
    IconLoader(QObject* parent = nullptr);
    virtual ~IconLoader();

    QIcon polish(const QIcon& icon);
    QIcon load(
        const QString& name,
        const QString& checkedName = QString(),
        const QnIcon::Suffixes* suffixes = nullptr,
        const SvgIconColorer::IconSubstitutions& svgColorSubstitutions =
            SvgIconColorer::kDefaultIconSubstitutions);

    static const QnIcon::Suffixes kDefaultSuffixes;

private:
    static QIcon loadPixmapIconInternal(
        Skin* skin,
        const QString& name,
        const QString& checkedName,
        const QnIcon::Suffixes* suffixes);

    static QIcon loadSvgIconInternal(
        Skin* skin,
        const QString& name,
        const QString& checkedName,
        const QnIcon::Suffixes* suffixes,
        const SvgIconColorer::IconSubstitutions& substitutions);

private:
    QHash<QString, QIcon> m_iconByKey;
    QSet<qint64> m_cacheKeys;
};

} // namespace nx::vms::client::desktop
