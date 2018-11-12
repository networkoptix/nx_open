////////////////////////////////////////////////////////////
// 31 jan 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef INTELCPUDESCRIPTION_H
#define INTELCPUDESCRIPTION_H

#include <QString>

#include <plugins/videodecoders/stree/resourcecontainer.h>


/*!
    Provides resources:\n
    - cpuString
    - cpuFamily
    - cpuModel
    - cpuStepping
*/
class IntelCPUDescription
:
    public nx::utils::stree::AbstractResourceReader
{
public:
    IntelCPUDescription();

    //!Implementation of AbstractResourceReader::get
    virtual bool get( int resID, QVariant* const value ) const;

private:
    QString m_cpuString;
    unsigned int m_cpuFamily;
    unsigned int m_cpuModel;
    unsigned int m_cpuStepping;

    void readCPUInfo();
};

#endif  //INTELCPUDESCRIPTION_H
