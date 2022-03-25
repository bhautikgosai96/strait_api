#include "ThostFtdcMdApi.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <iostream>
#include <fstream>
#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h> // To handle SIGPIPE
#endif

static const char *MD_SERVER_URI = "tcp://119.136.26.197:41213";
static const char *BROKER_ID = "SF001";
static const char *USER_ID = "sfuser1";
static const char *USER_PASSWORD = "8*MvhY!C";

void doSleep(unsigned int millis);
void printCurrTime();

static std::ofstream myfile;
class Logger
{
public:
    static void info(const char *format, ...)
    {
        va_list args;
        va_start(args, format);
        printCurrTime();
        vfprintf(stdout, format, args);
        putchar('\n');
        va_end(args);
    }
};

class TestMdClient : public CThostFtdcMdSpi
{
public:
    TestMdClient(CThostFtdcMdApi *pMdApi)
        : mdApi(pMdApi),
          requestID(1) {}
    ~TestMdClient() {}

protected:
    virtual void OnFrontConnected()
    {
        Logger::info("[INFO] [%s:%3d]: Front connected.", __FUNCTION__, __LINE__);
        doLogin();
    }
    virtual void OnFrontDisconnected(int reason)
    {
        Logger::info("[WARN] [%s:%3d]: Front disconnected: reasonCode=%d.", __FUNCTION__, __LINE__, reason);
        doSleep(3000); // Better to have a delay before next retry of connecting
    }
    virtual void OnRspUserLogin(CThostFtdcRspUserLoginField *login, CThostFtdcRspInfoField *status, int requestID, bool isLast)
    {
        if (status != NULL && status->ErrorID != 0)
        {
            Logger::info("[ERROR] [%s:%3d]: Failed to login: errorID=%d, errorMsg=%s", __FUNCTION__, __LINE__,
                         status->ErrorID, status->ErrorMsg);
            return;
        }
        if (login == NULL)
        {
            Logger::info("[ERROR] [%s:%3d]: Invalid server response, got null pointer of login info.", __FUNCTION__, __LINE__); // Should not happen
            return;
        }
        Logger::info("[INFO] [%s:%3d]: Login succeed: brokerID=%s, userID=%s, sessionID=%d, tradingDay=%s.", __FUNCTION__, __LINE__,
                     login->BrokerID, login->UserID, login->SessionID, login->TradingDay);
        // Do other things, e.g. subscribe contract
        //subscribeContract("CU3M-LME");
    }
    virtual void OnRspSubMarketData(CThostFtdcSpecificInstrumentField *inst, CThostFtdcRspInfoField *status, int requestID, bool isLast)
    {
        if (status != NULL && status->ErrorID != 0)
        {
            Logger::info("[ERROR] [%s:%3d]: Failed to subscribe market data: instrumentID=%s, errorID=%d, errorMsg=%s", __FUNCTION__, __LINE__,
                         inst == NULL ? "NULL" : inst->InstrumentID, status->ErrorID, status->ErrorMsg);
            return;
        }
        if (inst == NULL)
        {
            Logger::info("[ERROR] [%s:%3d]: Invalid server response, got null pointer of instrument info.", __FUNCTION__, __LINE__); // Should not happen
            return;
        }
        Logger::info("[INFO] [%s:%3d]: Subscribed market data: instrumentID=%s.", __FUNCTION__, __LINE__, inst->InstrumentID);
    }
    virtual void OnRspUnSubMarketData(CThostFtdcSpecificInstrumentField *inst, CThostFtdcRspInfoField *status, int requestID, bool isLast)
    {
        if (status != NULL && status->ErrorID != 0)
        {
            Logger::info("[ERROR] [%s:%3d]: Failed to unsubscribe market data: instrumentID=%s, errorID=%d, errorMsg=%s", __FUNCTION__, __LINE__,
                         inst == NULL ? "NULL" : inst->InstrumentID, status->ErrorID, status->ErrorMsg);
            return;
        }
        if (inst == NULL)
        {
            Logger::info("[ERROR] [%s:%3d]: Invalid server response, got null pointer of instrument info.", __FUNCTION__, __LINE__); // Should not happen
            return;
        }
        Logger::info("[INFO] [%s:%3d]: Unsubscribed market data: instrumentID=%s.", __FUNCTION__, __LINE__, inst->InstrumentID);
    }
    virtual void OnRtnDepthMarketData(CThostFtdcDepthMarketDataField *data)
    {
        if (data == NULL)
        {
            Logger::info("[ERROR] [%s:%3d]: Invalid server response, got null pointer of market data info.", __FUNCTION__, __LINE__); // Should not happen
            return;
        }

        // Handle market data
        myfile << data->InstrumentID << ", " << data->ExchangeID << ", " << data->TradingDay << ", " << data->LastPrice << ", " << data->PreSettlementPrice << ", " << data->PreClosePrice << ", " << data->OpenPrice << ", " << data->HighestPrice << ", " << data->LowestPrice << ", " << data->Volume << ", " << data->OpenInterest << ", " << data->UpperLimitPrice << ", " << data->LowerLimitPrice << ", " << data->BidPrice1 << ", " << data->BidVolume1 << ", " << data->AskPrice1 << ", " << data->AskVolume1 << ", " << data->BidPrice2 << ", " << data->BidVolume2 << ", " << data->AskPrice2 << ", " << data->AskVolume2 << ", " << data->BidPrice3 << ", " << data->BidVolume3 << ", " << data->AskPrice3 << ", " << data->AskVolume3 << ", " << data->BidPrice4 << ", " << data->BidVolume4 << ", " << data->AskPrice4 << ", " << data->AskVolume4 << ", " << data->BidPrice5 << ", " << data->BidVolume5 << ", " << data->AskPrice5 << ", " << data->AskVolume5 << ", " << data->ActionDay << ", " << data->UpdateTime << std::endl;

        Logger::info("[INFO] [%s:%3d]: MarketData: instrumentID=%s, exchangeID=%s, tradingDay=%s, lastPrice=%f, PreSettlementPrice=%f, PreClosePrice=%f, OpenPrice=%f, HighestPrice=%f, LowestPrice=%f, Volume=%d, OpenInterest=%d, UpperLimitPrice=%d, LowerLimitPrice=%d, bidPrice1=%f, bidVolume1=%d, askPrice1=%f, askVolume1=%d, bidPrice2=%f, bidVolume2=%d, askPrice2=%f, askVolume2=%d, bidPrice3=%f, bidVolume3=%d, askPrice3=%f, askVolume3=%d, bidPrice4=%f, bidVolume4=%d, askPrice4=%f, askVolume4=%d, bidPrice5=%f, bidVolume5=%d, askPrice5=%f, askVolume5=%d, ActionDay=%s, UpdateTime=%s.", __FUNCTION__, __LINE__,
                     data->InstrumentID, data->ExchangeID, data->TradingDay, data->LastPrice, data->PreSettlementPrice, data->PreClosePrice, data->OpenPrice, data->HighestPrice, data->LowestPrice, data->Volume, data->OpenInterest, data->UpperLimitPrice, data->LowerLimitPrice, data->BidPrice1, data->BidVolume1, data->AskPrice1, data->AskVolume1, data->BidPrice2, data->BidVolume2, data->AskPrice2, data->AskVolume2, data->BidPrice3, data->BidVolume3, data->AskPrice3, data->AskVolume3, data->BidPrice4, data->BidVolume4, data->AskPrice4, data->AskVolume4, data->BidPrice5, data->BidVolume5, data->AskPrice5, data->AskVolume5, data->ActionDay, data->UpdateTime);
    }

public:
    void subscribeContract(char *instrumentID)
    {
        char *instrumentIDs[] = {"ES2206-CME", "ES2203-CME", "NQ2203-CME", "NQ2206-CME"};
        //char *instrumentIDs[] = {instrumentID};
        int rtnCode = mdApi->SubscribeMarketData(instrumentIDs, sizeof(instrumentIDs) / sizeof(char *));
        if (rtnCode != 0)
        {
            Logger::info("[ERROR] [%s:%3d]: Request failed: code=%d.", __FUNCTION__, __LINE__, rtnCode);
        }
        else
        {
            Logger::info("[INFO] [%s:%3d]: Requested to subscribe market data: instrumentID=%s.", __FUNCTION__, __LINE__, instrumentID);
        }
    }
    void unsubscribeContract(char *instrumentID)
    {
        char *instrumentIDs[] = {"ES2206-CME", "ES2203-CME", "NQ2203-CME", "NQ2206-CME"};
        //char *instrumentIDs[] = {instrumentID};
        int rtnCode = mdApi->UnSubscribeMarketData(instrumentIDs, sizeof(instrumentIDs) / sizeof(char *));
        if (rtnCode != 0)
        {
            Logger::info("[ERROR] [%s:%3d]: Request failed: code=%d.", __FUNCTION__, __LINE__, rtnCode);
        }
        else
        {
            Logger::info("[INFO] [%s:%3d]: Requested to unsubscribe market data: instrumentID=%s.", __FUNCTION__, __LINE__, instrumentID);
        }
    }
    void doLogout() {
            CThostFtdcUserLogoutField field;
            memset(&field, 0, sizeof(field));
            strcpy(field.BrokerID, BROKER_ID);
            strcpy(field.UserID, USER_ID);
            int rtnCode = mdApi->ReqUserLogout(&field, nextRequestID());
            if (rtnCode != 0) {
                Logger::info("[ERROR] [%s:%3d] Request failed: code=%d.", __FUNCTION__, __LINE__, rtnCode);
            } else {
                Logger::info("[INFO] [%s:%3d]: Requested to logout: brokerID=%s, userID=%s.", __FUNCTION__, __LINE__,
                    field.BrokerID, field.UserID);
            }
    }
private:
    void doLogin()
    {
        CThostFtdcReqUserLoginField field;
        memset(&field, 0, sizeof(field));
        strcpy(field.BrokerID, BROKER_ID);
        strcpy(field.UserID, USER_ID);
        strcpy(field.Password, USER_PASSWORD);
        int rtnCode = mdApi->ReqUserLogin(&field, nextRequestID());
        if (rtnCode != 0)
        {
            Logger::info("[ERROR] [%s:%3d]: Request failed: code=%d.", __FUNCTION__, __LINE__, rtnCode);
        }
    }
    int nextRequestID() { return requestID++; }

private:
    CThostFtdcMdApi *mdApi;
    int requestID;
};

void doSleep(unsigned int millis)
{
#ifdef _WIN32
    Sleep(millis);
#else
    usleep(millis * 1000);
#endif
}

void printCurrTime()
{
#ifdef _WIN32
    SYSTEMTIME localTime;
    GetLocalTime(&localTime);
    printf("[%d-%02d-%02d %02d:%02d:%02d.%03d] ", localTime.wYear, localTime.wMonth, localTime.wDay, localTime.wHour, localTime.wMinute, localTime.wSecond, localTime.wMilliseconds);
#else
    struct timeval timeVal;
    gettimeofday(&timeVal, NULL);
    struct tm *time;
    time = localtime(&timeVal.tv_sec);
    printf("[%d-%02d-%02d %02d:%02d:%02d.%03d] ", (time->tm_year + 1900), (time->tm_mon + 1), time->tm_mday, time->tm_hour, time->tm_min, time->tm_sec, timeVal.tv_usec / 1000);
#endif
}

int main()
{
#ifndef _WIN32
    // To ignore SIGPIPE
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, 0);
#endif
    CThostFtdcMdApi *pMdApi = CThostFtdcMdApi::CreateFtdcMdApi();
    TestMdClient *mdClient = new TestMdClient(pMdApi);

    myfile.open("price.csv", std::ios_base::app);

    pMdApi->RegisterSpi(mdClient);
    pMdApi->RegisterFront((char *)MD_SERVER_URI);

    Logger::info("[INFO] [%s:%3d]: Initial client: serverUri=%s, brokerID=%s, userID=%s version=%s.", __FUNCTION__, __LINE__,
                 MD_SERVER_URI, BROKER_ID, USER_ID, CThostFtdcMdApi::GetApiVersion());
    pMdApi->Init(); // Start connecting

    doSleep(5000);
    //mdClient->doLogout();
    mdClient->subscribeContract("ES2206-CME");
    while(true){
        doSleep(10000);
    }
    //doSleep(4294967295);
     mdClient->unsubscribeContract("ES2206-CME");
    doSleep(1000);
    // Destroy the instance and release resources
    pMdApi->RegisterSpi(NULL);
    pMdApi->Release();

    delete mdClient;

    return 0;
}
