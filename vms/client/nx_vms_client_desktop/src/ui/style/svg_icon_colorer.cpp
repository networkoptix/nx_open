#include "svg_icon_colorer.h"


#include <QFileInfo>

#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <nx/vms/client/desktop/ini.h>

using namespace nx::vms::client::desktop;

void SvgIconColorer::initializeDump()
{
    const QString directoryPath(ini().dumpGeneratedIconsTo);
    if (directoryPath.isEmpty())
        return;

    const QFileInfo basePath = QDir(directoryPath).absoluteFilePath(m_iconName);

    m_dumpDirectory = basePath.absoluteDir();
    m_iconName = basePath.baseName();
    m_dump = m_dumpDirectory.mkpath(m_dumpDirectory.path());
}

void SvgIconColorer::dumpIconIfNeeded(const QByteArray& data, const QString& suffix) const
{
    static const QString svgExtension(".svg");

    if (!m_dump)
        return;

    QString filename = m_dumpDirectory.absoluteFilePath(m_iconName + suffix + svgExtension);
    QFile f(filename);
    if (!f.open(QIODevice::WriteOnly))
        return;

    f.write(data);
    f.close();
}

QByteArray SvgIconColorer::makeIcon() const
{
    QByteArray result = substituteColors(colorTheme()->getColorSubstitutions());

    dumpIconIfNeeded(result);

    return result;
}

QString SvgIconColorer::colorStringValue(const QString& name) const
{
    return colorTheme()->color(name).name();
}

QByteArray SvgIconColorer::makeIcon(const QString& primaryColorName,
        const QString& secondaryColorName,
        const QString& suffix) const
{
    static const QString basePrimaryColor = QColor("#a5b7c0").name(); //< Value of light10 in default customization.
    static const QString baseSecondaryColor = QColor("#e1e7eA").name(); //< Value of light4 in default customization.

    QByteArray result = substituteColors(QMap<QString, QString>{
        {basePrimaryColor, colorStringValue(primaryColorName)},
        {baseSecondaryColor, colorStringValue(secondaryColorName)}});

    dumpIconIfNeeded(result, suffix);

    return result;
}

QByteArray SvgIconColorer::substituteColors(const QMap<QString, QString>& colorSubstitutions) const
{
    QByteArray result = m_sourceIconData;

    for (int pos = 0; pos < m_sourceIconData.size(); ++pos)
    {
        pos = getNextColorPos(pos);
        if (pos != m_sourceIconData.size())
        {
            QString currentColor = QString::fromLatin1(
                m_sourceIconData.begin() + pos,
                kColorValueStringLength).toLower();

            if (colorSubstitutions.contains(currentColor))
            {
                result.replace(
                    pos,
                    kColorValueStringLength,
                    colorSubstitutions[currentColor].toLatin1());
            }

            pos += kColorValueStringLength;
        }
    }

    return result;
}

int SvgIconColorer::getNextColorPos(int startPos) const
{
    for (; startPos < m_sourceIconData.size(); ++startPos)
    {
        if (m_sourceIconData[startPos] == '#')
        {
            return m_sourceIconData.size() - startPos >= kColorValueStringLength
                ? startPos
                : m_sourceIconData.size();
        }
    }

    return startPos;
}
