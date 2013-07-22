/**********************************************************
* 15 jul 2013
* a.kolesnikov
***********************************************************/

#ifndef GLOBAL_SETTINGS_H
#define GLOBAL_SETTINGS_H


namespace Qn
{
    //!#todo: use QSettings instead. This will require some refactor in all executable modules using libcommon
    class GlobalSettings
    {
    public:
        GlobalSettings();

        //!Local port listened for http requests. 0 if no listener is present
        void setHttpPort( int httpPortVal );
        int httpPort() const;

        static GlobalSettings* instance();

    private:
        int m_httpPort;
    };
}

#endif  //GLOBAL_SETTINGS_H
