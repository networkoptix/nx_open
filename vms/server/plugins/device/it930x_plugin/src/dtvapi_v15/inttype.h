#ifndef __INTTYPE_H__
#define __INTTYPE_H__


/**
 * The type defination of SnrTable.
 */
typedef struct {
    Dword errorCount;
    Dword snr;
    double errorRate;
} SnrTable;


/**
 * The type defination of Statistic.
 */
typedef struct {
    Word abortCount;
    Dword postVitBitCount;
    Dword postVitErrorCount;
    /** float point */
    Dword softBitCount;
    Dword softErrorCount;
    Dword preVitBitCount;
    Dword preVitErrorCount;
    double snr;
} ChannelStatistic;


/**
 * The type defination of AgcVoltage.
 */
typedef struct {
    double doSetVolt;
    double doPuUpVolt;
} AgcVoltage;


#endif