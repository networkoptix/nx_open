#pragma once

#include <QByteArray>
#include <QDir>

class NX_VMS_CLIENT_DESKTOP_API SvgIconColorer
{
private:
    static const int kColorValueStringLength = 7;

public:
    SvgIconColorer(const QByteArray& sourceIconData, const QString& iconName):
        m_sourceIconData(sourceIconData),
        m_iconName(iconName)
    {
        initializeDump();
        dumpIconIfNeeded(sourceIconData);
    }

    QByteArray makeNormalIcon() const { return makeIcon(); }
    QByteArray makeDisabledIcon() const { return makeIcon("dark14", "dark17", /*FIXME*/"windowText", "_disabled"); }
    QByteArray makeSelectedIcon() const { return makeIcon("light4", "light1", "light", "_selected"); }
    QByteArray makeActiveIcon() const { return makeIcon("brand_core", "brand_l2", "light14", "_active"); }
    QByteArray makeErrorIcon() const { return makeIcon("red_l2", "red_l3", /*FIXME*/"windowText", "_error"); }
    //< TODO: Perhaps we need to do something with our coloring mechanics
    // since our designers have started to use svg icons in Text Buttons,
    // whose Disabled state uses opacity and Error state is not specified.

private:
    void initializeDump();
    void dumpIconIfNeeded(const QByteArray& data, const QString& suffix = QString()) const;
    QByteArray makeIcon() const;
    QString colorStringValue(const QString& name) const;
    QByteArray makeIcon(
        const QString& primaryColorName,
        const QString& secondaryColorName,
        const QString& windowTextColorName,
        const QString& suffix) const;
    QByteArray substituteColors(const QMap<QString, QString>& colorSubstitutions) const;  //< Default #XXXXXX to updated #YYYYYY.
    int getNextColorPos(int startPos) const;

private:
    bool m_dump = false;
    QDir m_dumpDirectory;
    QByteArray m_sourceIconData;
    QString m_iconName;
};
