#ifndef mouse_ignore_helper_h_1645
#define mouse_ignore_helper_h_1645

class CLMouseIgnoreHelper
{
public:
    CLMouseIgnoreHelper();
    ~CLMouseIgnoreHelper();

    bool shouldIgnore() const;

    // function shouldIgnore will return true next ms 
    // if the function was called with bigger value followed by small value => small value ignored
    void ignoreNextMs(int ms);

    void reset();

private:
    QTime m_timeUpTo;
    

};


#endif //mouse_ignore_helper_h_1645