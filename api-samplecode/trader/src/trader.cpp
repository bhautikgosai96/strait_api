#include "ThostFtdcTraderApi.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h>    // To handle SIGPIPE
#endif

static const char *TRADE_SERVER_URI = "tcp://119.136.26.197:40905";
static const char *BROKER_ID = "SF001";
static const char *USER_ID = "sfuser1";
static const char *USER_PASSWORD = "8*MvhY!C";
static const char *APP_ID = "app_id";
static const char *AUTH_CODE = "auth_code";
static const char *INVESTOR_ID = USER_ID;  // Investor id is mapped to user id in ATP

void doSleep(unsigned int millis);
void printCurrTime();

class Logger {
public:
    static void info(const char *format, ...) {
        va_list args;
        va_start(args, format);
        printCurrTime();
        vfprintf(stdout, format, args);
        putchar('\n');
        va_end(args);
    }
};

class TestTraderClient : public CThostFtdcTraderSpi {
public:
    TestTraderClient(CThostFtdcTraderApi *pTraderApi)
        : tradeApi(pTraderApi),
        requestID(0),
        orderRef(0),
        logonState(UNINITIALIZED_STATE) {}
    ~TestTraderClient() {}
protected:
    virtual void OnFrontConnected() {
        Logger::info("[INFO] [%s:%3d]: Front connected.", __FUNCTION__, __LINE__);
        doLogin();
    }
    virtual void OnFrontDisconnected(int reason) {
        logonState = LOGON_ABORTED;
        Logger::info("[WARN] [%s:%3d]: Front disconnected: reasonCode=%d.", __FUNCTION__, __LINE__, reason);
        doSleep(3000);  // Better to have a delay before next retry of connecting
    }
    virtual void OnRspUserLogin(CThostFtdcRspUserLoginField *login, CThostFtdcRspInfoField *status, int requestID, bool isLast) {
        if (status != NULL && status->ErrorID != 0) {
            Logger::info("[ERROR] [%s:%3d]: Failed to login, errorID=%d, errorMsg=%s", __FUNCTION__, __LINE__,
                status->ErrorID, status->ErrorMsg);
            logonState = LOGON_FAILED;
            return;
        }
        if (login == NULL) {
            Logger::info("[ERROR] [%s:%3d]: Invalid server response, got null pointer of login info.", __FUNCTION__, __LINE__);    // Should not happen
            logonState = LOGON_FAILED;
            return;
        }
        this->sessionID = login->SessionID;
        this->frontID = login->FrontID;
        this->orderRef = atoi(login->MaxOrderRef);
        Logger::info("[INFO] [%s:%3d]: Login succeed: brokerID=%s, userID=%s, sessionID=%d, tradingDay=%s.", __FUNCTION__, __LINE__,
            login->BrokerID, login->UserID, login->SessionID, login->TradingDay);
        logonState = LOGON_SUCCEED;
        // Do other things, e.g. add an order
        //insertOrder("CU3M-LME", true, 3900.0, 5);
    }
    virtual void OnRspAuthenticate(CThostFtdcRspAuthenticateField *authenticate, CThostFtdcRspInfoField *status, int requestID, bool isLast) {
        if (status != NULL && status->ErrorID != 0) {
            Logger::info("[ERROR] [%s:%3d]: Failed to authenticate, errorID=%d, errorMsg=%s", __FUNCTION__, __LINE__,
                status->ErrorID, status->ErrorMsg);
            return;
        }
        if (authenticate == NULL) {
            Logger::info("[ERROR] [%s:%3d]: Invalid server response, got null pointer of authentication info.", __FUNCTION__, __LINE__);
            return;
        }
        Logger::info("[INFO] [%s:%3d]: Authenticate succeed: brokerID=%s, userID=%s, appID=%s, appType=%c.", __FUNCTION__, __LINE__,
            authenticate->BrokerID, authenticate->UserID, authenticate->AppID, authenticate->AppType);
        doLogin();
    }
    virtual void OnRspOrderInsert(CThostFtdcInputOrderField *order, CThostFtdcRspInfoField *status, int requestID, bool isLast) {
        if (status != NULL && status->ErrorID != 0) {
            if (order == NULL) {
                Logger::info("[ERROR] [%s:%3d]: Invalid server response, got null pointer of input order.", __FUNCTION__, __LINE__);    // Should not happen
                return;
            }
            Logger::info("[WARN] [%s:%3d]: Failed to add new order: OrderRef=%s, instrumentID=%s, direction=%s, volumeTotalOriginal=%d, limitPrice=%f, errorID=%d, errorMsg=%s", __FUNCTION__, __LINE__,
                order->OrderRef, order->InstrumentID, order->Direction == THOST_FTDC_D_Buy ? "buy" : "sell", order->VolumeTotalOriginal, order->LimitPrice, status->ErrorID, status->ErrorMsg);
        }
    }
    virtual void OnRspOrderAction(CThostFtdcInputOrderActionField *action, CThostFtdcRspInfoField *status, int requestID, bool isLast) {
        if (status != NULL && status->ErrorID != 0) {
            if (action == NULL) {
                Logger::info("[ERROR] [%s:%3d]: Invalid server response, got null pointer of input order action.", __FUNCTION__, __LINE__);    // Should not happen
                return;
            }
            Logger::info("[WARN] [%s:%3d]: Failed to %s order: orderRef=%s, orderSysID=%s, instrumentID=%s, volumeChange=%d, limitPrice=%f, errorID=%d, errorMsg=%s", __FUNCTION__, __LINE__,
                action->ActionFlag == THOST_FTDC_AF_Delete ? "delete" : "modify", action->OrderRef, action->OrderSysID, action->InstrumentID, action->VolumeChange, action->LimitPrice, status->ErrorID, status->ErrorMsg);
        }
    }
    virtual void OnErrRtnOrderAction(CThostFtdcOrderActionField *action, CThostFtdcRspInfoField *status) {
        if (status != NULL && status->ErrorID != 0) {
            if (action == NULL) {
                Logger::info("[ERROR] [%s:%3d]: Invalid server response, got null pointer of order action.", __FUNCTION__, __LINE__);    // Should not happen
                return;
            }
            Logger::info("[WARN] [%s:%3d]: Failed to %s order: instrumentID=%s, orderLocalID=%s, volumeChange=%d, limitPrice=%f, errorID=%d, errorMsg=%s", __FUNCTION__, __LINE__,
                action->ActionFlag == THOST_FTDC_AF_Delete ? "delete" : "modify", action->InstrumentID, action->OrderLocalID, action->VolumeChange, action->LimitPrice, status->ErrorID, status->ErrorMsg);
        }
    }
    virtual void OnRtnOrder(CThostFtdcOrderField *order) {
        if (order != NULL) {
            Logger::info("[INFO] [%s:%3d]: Order status: orderRef=%s, orderLocalID=%s, sessionID=%d, frontID=%d, instrumentID=%s, direction=%s, volumeTotalOriginal=%d, limitPrice=%f, volumeTraded=%d, orderStatus=%c.", __FUNCTION__, __LINE__,
                order->OrderRef, order->OrderLocalID, order->SessionID, order->FrontID, order->InstrumentID, order->Direction == THOST_FTDC_D_Buy ? "buy" : "sell", order->VolumeTotalOriginal, order->LimitPrice, order->VolumeTraded, order->OrderStatus);
        }
    }
    virtual void OnRtnTrade(CThostFtdcTradeField *trade) {
        if (trade != NULL) {
            Logger::info("[INFO] [%s:%3d]: Traded order: orderRef=%s, orderLocalID=%s, instrumentID=%s, direction=%s, volume=%d, price=%f, tradeDate=%s.", __FUNCTION__, __LINE__,
                trade->OrderRef, trade->OrderLocalID, trade->InstrumentID, trade->Direction == THOST_FTDC_D_Buy ? "buy" : "sell", trade->Volume, trade->Price, trade->TradeDate);
        }
    }
    virtual void OnRspQryTrade(CThostFtdcTradeField *trade, CThostFtdcRspInfoField *status, int requestID, bool isLast) {
        if (status != NULL && status->ErrorID != 0) {
            Logger::info("[WARN] [%s:%3d]: Failed to query trade info: errorID=%d, errorMsg=%s", __FUNCTION__, __LINE__,
                status->ErrorID, status->ErrorMsg);
            return;
        }
        if (trade == NULL) {
            if (status == NULL) {
                Logger::info("[ERROR] [%s:%3d]: Invalid server response, got null pointer of trade info.", __FUNCTION__, __LINE__);    // Should not happen
                return;
            }
            if (!isLast) {
                Logger::info("[ERROR] [%s:%3d]: Invalid server response, got null pointer of trade info.", __FUNCTION__, __LINE__);    // Should not happen
                return;
            }
            Logger::info("[INFO] [%s:%3d]: No matched trade info found.", __FUNCTION__, __LINE__);
            return;
        }
        Logger::info("[INFO] [%s:%3d]: Filled order: orderLocalID=%s, instrumentID=%s, direction=%s, volume=%d, price=%f, tradeDate=%s.", __FUNCTION__, __LINE__,
            trade->OrderLocalID, trade->InstrumentID, trade->Direction == THOST_FTDC_D_Buy ? "buy" : "sell", trade->Volume, trade->Price, trade->TradeDate);
    }
    virtual void OnRspQryOrder(CThostFtdcOrderField *order, CThostFtdcRspInfoField *status, int requestID, bool isLast) {
        if (status != NULL && status->ErrorID != 0) {
            Logger::info("[WARN] [%s:%3d]: Failed to query order info: errorID=%d, errorMsg=%s", __FUNCTION__, __LINE__,
                status->ErrorID, status->ErrorMsg);
            return;
        }
        if (order == NULL) {
            if (status == NULL) {
                Logger::info("[ERROR] [%s:%3d]: Invalid server response, got null pointer of order info.", __FUNCTION__, __LINE__);    // Should not happen
                return;
            }
            if (!isLast) {
                Logger::info("[ERROR] [%s:%3d]: Invalid server response, got null pointer of order info.", __FUNCTION__, __LINE__);    // Should not happen
                return;
            }
            Logger::info("[INFO] [%s:%3d]: No matched order info found.", __FUNCTION__, __LINE__);
            return;
        }
        Logger::info("[INFO] [%s:%3d]: Order info: orderRef=%s, orderLocalID=%s, sessionID=%d, frontID=%d, instrumentID=%s, direction=%s, volumeTotalOriginal=%d, limitPrice=%f, OrderStatus=%c.", __FUNCTION__, __LINE__,
            order->OrderRef, order->OrderLocalID, order->SessionID, order->FrontID, order->InstrumentID, order->Direction == THOST_FTDC_D_Buy ? "buy" : "sell", order->VolumeTotalOriginal, order->LimitPrice, order->OrderStatus);
    }
    virtual void OnRspQryInstrument(CThostFtdcInstrumentField *inst, CThostFtdcRspInfoField *status, int requestID, bool isLast) {
        if (status != NULL && status->ErrorID != 0) {
            Logger::info("[WARN] [%s:%3d]: Failed to query instrument info: errorID=%d, errorMsg=%s", __FUNCTION__, __LINE__,
                status->ErrorID, status->ErrorMsg);
            return;
        }
        if (inst == NULL) {
            if (status == NULL) {
                Logger::info("[ERROR] [%s:%3d]: Invalid server response, got null pointer of instrument info.", __FUNCTION__, __LINE__);    // Should not happen
                return;
            }
            if (!isLast) {
                Logger::info("[ERROR] [%s:%3d]: Invalid server response, got null pointer of instrument info.", __FUNCTION__, __LINE__);    // Should not happen
                return;
            }
            Logger::info("[INFO] [%s:%3d]: No matched instrument info found.", __FUNCTION__, __LINE__);
            return;
        }
        Logger::info("[INFO] [%s:%3d]: Instrument info: instrumentID=%s, exchangeID=%s, InstrumentName=%s, ExchangeInstID=%s, ProductID=%s, DeliveryYear=%d, DeliveryMonth=%d, MaxMarketOrderVolume=%d, MinMarketOrderVolume=%d, MaxLimitOrderVolume=%d, MinLimitOrderVolume=%d, VolumeMultiple=%d, PriceTick=%f, isTrading=%s, expireDate=%s, StartDelivDate=%s, EndDelivDate=%s, UnderlyingInstrID=%s, StrikePrice=%f, volumeMultiple=%d, exchangeInstID=%s.", __FUNCTION__, __LINE__,
            inst->InstrumentID, inst->ExchangeID, inst->InstrumentName, inst->ExchangeInstID, inst->ProductID,  inst->DeliveryYear, inst->DeliveryMonth, inst->MaxMarketOrderVolume, inst->MinMarketOrderVolume, inst->MaxLimitOrderVolume, inst->MinLimitOrderVolume, inst->VolumeMultiple, inst->PriceTick, inst->IsTrading != 0 ? "true" : "false", inst->ExpireDate, inst->StartDelivDate, inst->EndDelivDate, inst->UnderlyingInstrID, inst->StrikePrice, inst->VolumeMultiple, inst->ExchangeInstID);
    }
    virtual void OnRspQryInvestorPosition(CThostFtdcInvestorPositionField *position, CThostFtdcRspInfoField *status, int requestID, bool isLast) {
        if (status != NULL && status->ErrorID != 0) {
            Logger::info("[WARN] [%s:%3d]: Failed to query investor position info: errorID=%d, errorMsg=%s", __FUNCTION__, __LINE__,
                status->ErrorID, status->ErrorMsg);
            return;
        }
        if (position == NULL) {
            if (status == NULL) {
                Logger::info("[ERROR] [%s:%3d]: Invalid server response, got null pointer of investor position info.", __FUNCTION__, __LINE__);    // Should not happen
                return;
            }
            if (!isLast) { 
                Logger::info("[ERROR] [%s:%3d]: Invalid server response, got null pointer of investor position info.", __FUNCTION__, __LINE__);    // Should not happen
                return;
            }
            Logger::info("[INFO] [%s:%3d]: No matched investor position info found.", __FUNCTION__, __LINE__);
            return;
        }
        Logger::info("[INFO] [%s:%3d]: Investor position info: instrumentID=%s, openCost=%f, positionCost=%f, position=%d, ydPosition=%d, closeProfit=%f.", __FUNCTION__, __LINE__,
            position->InstrumentID, position->OpenCost, position->PositionCost, position->Position, position->YdPosition, position->CloseProfit);
    }

     virtual void OnRspQryTradingAccount(CThostFtdcTradingAccountField *account, CThostFtdcRspInfoField *status, int requestID, bool isLast) {
        if (status != NULL && status->ErrorID != 0) {
            Logger::info("[WARN] [%s:%3d]: Failed to query investor trading account info: errorID=%d, errorMsg=%s", __FUNCTION__, __LINE__,
                status->ErrorID, status->ErrorMsg);
            return;
        }
        if (account == NULL) {
            if (status == NULL) {
                Logger::info("[ERROR] [%s:%3d]: Invalid server response, got null pointer of investor trading account info.", __FUNCTION__, __LINE__);    // Should not happen
                return;
            }
            if (!isLast) { 
                Logger::info("[ERROR] [%s:%3d]: Invalid server response, got null pointer of investor trading account info.", __FUNCTION__, __LINE__);    // Should not happen
                return;
            }
            Logger::info("[INFO] [%s:%3d]: No matched investor trading account info found.", __FUNCTION__, __LINE__);
            return;
        }
        Logger::info("[INFO] [%s:%3d]: Investor trading account info: BrokerID=%s, AccountID=%s, PreBalance=%f, Deposit=%f, Withdraw=%f, FrozenMargin=%f.", __FUNCTION__, __LINE__,
            account->BrokerID, account->AccountID, account->PreBalance, account->Deposit, account->Withdraw, account->FrozenMargin);
    }
    // Overwrite other api(s)
    // ...
public:
    void insertOrder(const char *instrumentID, bool isBuy, double price, int volume) {
        ensureLogon();
        const int requestID = nextRequestID();
        CThostFtdcInputOrderField field;
        memset(&field, 0, sizeof(field));
        strcpy(field.BrokerID, BROKER_ID);
        strcpy(field.InvestorID, INVESTOR_ID);
        strcpy(field.UserID, USER_ID);
        strcpy(field.InstrumentID, instrumentID);
        sprintf(field.OrderRef, "%d", nextOrderRef());
        field.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
        field.Direction = isBuy ? THOST_FTDC_D_Buy : THOST_FTDC_D_Sell;  // Direction
        field.CombOffsetFlag[0] = THOST_FTDC_OF_Open;
        field.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
        //field.CombHedgeFlag[1] = '9';  // Enable this line to create market maker order
        //field.CombHedgeFlag[2] = '2';  // Enable this line to set T+1 session flag for non market maker order(currently for HKEX only)
        field.LimitPrice = price;            // Price
        field.VolumeTotalOriginal = volume;  // Volume
        field.TimeCondition = THOST_FTDC_TC_GFD;
        strcpy(field.GTDDate, "");
        field.VolumeCondition = THOST_FTDC_VC_AV;
        field.MinVolume = 0;
        field.ContingentCondition = THOST_FTDC_CC_Immediately;
        field.StopPrice = 0.0;
        field.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;
        field.IsAutoSuspend = 0;
        strcpy(field.BusinessUnit, "customized-data");  // Store customized data if necessary (needs to be a string)
        field.RequestID = requestID;
        field.UserForceClose = 0;
        field.IsSwapOrder = 0;
        int rtnCode = tradeApi->ReqOrderInsert(&field, requestID);
        if (rtnCode != 0) {
            Logger::info("[ERROR] [%s:%3d]: Request failed: code=%d.", __FUNCTION__, __LINE__, rtnCode);
        } else {
            Logger::info("[INFO] [%s:%3d]: Requested to add new order: instrumentID=%s, direction=%s, volume=%d, price=%f.", __FUNCTION__, __LINE__,
                instrumentID, isBuy ? "buy" : "sell", volume, price);
        }
    }
    void replaceOrder(const char *orderRef, double price, int volume) {
        ensureLogon();
        CThostFtdcInputOrderActionField field;
        memset(&field, 0, sizeof(field));
        strcpy(field.BrokerID, BROKER_ID);
        strcpy(field.InvestorID, INVESTOR_ID);
        strcpy(field.UserID, USER_ID);

        // Use FrontID+SessionID+OrderRef to locate the order:
        field.FrontID = frontID;        // FrontID can be got from CThostFtdcOrderField.FrontID
        field.SessionID = sessionID;    // SessionID can be got from CThostFtdcOrderField.SessionID
        strcpy(field.OrderRef, orderRef);

        // Or use ExchangeID+OrderSysID to locate the order:
        //strcpy(field.ExchangeID, "LME");
        //strcpy(field.OrderSysID, "AAAAAAA");

        field.ActionFlag = THOST_FTDC_AF_Modify;
        field.LimitPrice = price;       // Updated price
        field.VolumeChange = volume;    // Updated remaining volume

        int rtnCode = tradeApi->ReqOrderAction(&field, nextRequestID());
        if (rtnCode != 0) {
            Logger::info("[ERROR] [%s:%3d]: Request failed: code=%d.", __FUNCTION__, __LINE__, rtnCode);
        } else {
            Logger::info("[INFO] [%s:%3d]: Requested to modify order: orderRef=%s, frontID=%d, sessionID=%d, newPrice=%f, newVolume=%d.", __FUNCTION__, __LINE__,
                field.OrderRef, field.FrontID, field.SessionID, field.LimitPrice, field.VolumeChange);
        }
    }
    void cancelOrder(const char *orderRef) {
        ensureLogon();
        CThostFtdcInputOrderActionField field;
        memset(&field, 0, sizeof(field));
        strcpy(field.BrokerID, BROKER_ID);
        strcpy(field.InvestorID, INVESTOR_ID);
        strcpy(field.UserID, USER_ID);

        // Use FrontID+SessionID+OrderRef to locate the order:
        field.FrontID = frontID;        // FrontID can be got from CThostFtdcOrderField.FrontID
        field.SessionID = sessionID;    // SessionID can be got from CThostFtdcOrderField.SessionID
        strcpy(field.OrderRef, orderRef);

        // Or use ExchangeID+OrderSysID to locate the order:
        //strcpy(field.ExchangeID, "LME");
        //strcpy(field.OrderSysID, "AAAAAAA");

        field.ActionFlag = THOST_FTDC_AF_Delete;    // Delete

        int rtnCode = tradeApi->ReqOrderAction(&field, nextRequestID());
        if (rtnCode != 0) {
            Logger::info("[ERROR] [%s:%3d]: Request failed: code=%d.", __FUNCTION__, __LINE__, rtnCode);
        } else {
            Logger::info("[INFO] [%s:%3d]: Requested to cancel order: orderRef=%s, frontID=%d, sessionID=%d.", __FUNCTION__, __LINE__,
                field.OrderRef, field.FrontID, field.SessionID);
        }
    }
    void queryTrade(const char *instrumentID) {
        ensureLogon();
        CThostFtdcQryTradeField field;
        memset(&field, 0, sizeof(field));
        strcpy(field.BrokerID, BROKER_ID);
        strcpy(field.InvestorID, INVESTOR_ID);
        strcpy(field.InstrumentID, instrumentID);
        strcpy(field.ExchangeID, "");
        strcpy(field.TradeID, "");
        strcpy(field.TradeTimeStart, "");    // The Format of TradeTimeStart: HH:mm:ss, e.g: 09:30:00
        strcpy(field.TradeTimeEnd, "");      // The Format of TradeTimeEnd  : HH:mm:ss, e.g: 15:00:00
        int rtnCode = tradeApi->ReqQryTrade(&field, nextRequestID());
        if (rtnCode != 0) {
            Logger::info("[ERROR] [%s:%3d]: Request failed: code=%d.", __FUNCTION__, __LINE__, rtnCode);
        } else {
            Logger::info("[INFO] [%s:%3d]: Requested to query trade info: brokerID=%s, investorID=%s, instrumentID=%s, exchangeID=%s.", __FUNCTION__, __LINE__,
                field.BrokerID, field.InvestorID, field.InstrumentID, field.ExchangeID);
        }
    }
    void queryOrder(const char *instrumentID) {
        ensureLogon();
        CThostFtdcQryOrderField field;
        memset(&field, 0, sizeof(field));
        strcpy(field.BrokerID, BROKER_ID);
        strcpy(field.InvestorID, INVESTOR_ID);
        strcpy(field.InstrumentID, instrumentID);
        strcpy(field.ExchangeID, "");
        strcpy(field.OrderSysID, "");
        strcpy(field.InsertTimeStart, "");    // The Format of TradeTimeStart: HH:mm:ss, e.g: 09:30:00
        strcpy(field.InsertTimeEnd, "");      // The Format of TradeTimeEnd  : HH:mm:ss, e.g: 15:00:00
        int rtnCode = tradeApi->ReqQryOrder(&field, nextRequestID());
        if (rtnCode != 0) {
            Logger::info("[ERROR] [%s:%3d]: Request failed: code=%d.", __FUNCTION__, __LINE__, rtnCode);
        } else {
            Logger::info("[INFO] [%s:%3d]: Requested to query order info: brokerID=%s, investorID=%s, instrumentID=%s, exchangeID=%s.", __FUNCTION__, __LINE__,
                field.BrokerID, field.InvestorID, field.InstrumentID, field.ExchangeID);
        }
    }
    void queryInstrument(const char *instrumentID) {
        ensureLogon();
        CThostFtdcQryInstrumentField field;
        memset(&field, 0, sizeof(field));
        strcpy(field.ExchangeID, "");
        //strcpy(field.ExchangeInstID, "EVM");  // Enable this line to get CThostFtdcInstrumentField.VolumeMultiple with original currency
        strcpy(field.InstrumentID, instrumentID);
        strcpy(field.ProductID, "");
        int rtnCode = tradeApi->ReqQryInstrument(&field, nextRequestID());
        if (rtnCode != 0) {
            Logger::info("[ERROR] [%s:%3d]: Request failed: code=%d.", __FUNCTION__, __LINE__, rtnCode);
        } else {
            Logger::info("[INFO] [%s:%3d]: Requested to query instrument info: exchangeID=%s, instrumentID=%s, productID=%s.", __FUNCTION__, __LINE__,
                field.ExchangeID, field.InstrumentID, field.ProductID);
        }
    }
    void queryInvestorPosition(const char *instrumentID) {
        ensureLogon();
        CThostFtdcQryInvestorPositionField field;
        memset(&field, 0, sizeof(field));
        strcpy(field.BrokerID, BROKER_ID);
        strcpy(field.InvestorID, INVESTOR_ID);
        strcpy(field.InstrumentID, instrumentID);
        int rtnCode = tradeApi->ReqQryInvestorPosition(&field, nextRequestID());
        if (rtnCode != 0) {
            Logger::info("[ERROR] [%s:%3d]: Request failed: code=%d.", __FUNCTION__, __LINE__, rtnCode);
        } else {
            Logger::info("[INFO] [%s:%3d]: Requested to query investor position info: brokerID=%s, investorID=%s, instrumentID=%s.", __FUNCTION__, __LINE__,
                field.BrokerID, field.InvestorID, field.InstrumentID);
        }
    }
    void queryTradingAccount() {
        ensureLogon();
        CThostFtdcQryTradingAccountField field;
        memset(&field, 0, sizeof(field));
        strcpy(field.BrokerID, BROKER_ID);
        strcpy(field.InvestorID, INVESTOR_ID);
        strcpy(field.CurrencyID, "");  
                int rtnCode = tradeApi->ReqQryTradingAccount(&field, nextRequestID());
        if (rtnCode != 0) {
            Logger::info("[ERROR] [%s:%3d] Request failed: code=%d.", __FUNCTION__, __LINE__, rtnCode);
        } else {
            Logger::info("[INFO] [%s:%3d] Requested to query trading account info: brokerID=%s, investorID=%s.", __FUNCTION__, __LINE__,
                field.BrokerID, field.InvestorID);
        }
    }
    void ensureLogon() {
        const int MAX_ATTEMPT_TIMES = 100;
        int tryTimes = 0;
        while (logonState == UNINITIALIZED_STATE && tryTimes++ < MAX_ATTEMPT_TIMES) {
            doSleep(200);
        }
        if (logonState != LOGON_SUCCEED) {
            Logger::info("[ERROR] [%s:%3d]: Trade logon failed.", __FUNCTION__, __LINE__);
        }
    }
private:
    void doLogin() {
        CThostFtdcReqUserLoginField field;
        memset(&field, 0, sizeof(field));
        strcpy(field.BrokerID, BROKER_ID);
        strcpy(field.UserID, USER_ID);
        strcpy(field.Password, USER_PASSWORD);
        int rtnCode = tradeApi->ReqUserLogin(&field, nextRequestID());
        if (rtnCode != 0) {
            Logger::info("[ERROR] [%s:%3d]: Request failed: code=%d.", __FUNCTION__, __LINE__, rtnCode);
        } else {
            Logger::info("[INFO] [%s:%3d]: Requested to login: brokerID=%s, userID=%s, password=%s.", __FUNCTION__, __LINE__,
                field.BrokerID, field.UserID, field.Password);
        }
    }
    void doAuthenticate() {
        CThostFtdcReqAuthenticateField field;
        memset(&field, 0, sizeof(field));
        strcpy(field.BrokerID, BROKER_ID);
        strcpy(field.UserID, USER_ID);
        strcpy(field.AppID, APP_ID);
        strcpy(field.AuthCode, AUTH_CODE);
        int rtnCode = tradeApi->ReqAuthenticate(&field, nextRequestID());
        if (rtnCode != 0) {
            Logger::info("[ERROR] [%s:%3d]: Request failed: code=%d.", __FUNCTION__, __LINE__, rtnCode);
        } else {
            Logger::info("[INFO] [%s:%3d]: Requested to authenticate: brokerID=%s, userID=%s, appID=%s, authCode=%s.", __FUNCTION__, __LINE__,
                field.BrokerID, field.UserID, field.AppID, field.AuthCode);
        }
    }
    int nextRequestID() { return ++requestID; }
    int nextOrderRef() { return orderRef++; }
private:
    enum LogonState {
        LOGON_SUCCEED = 0,
        LOGON_FAILED = 1,
        LOGON_ABORTED = 2,
        UNINITIALIZED_STATE = 3
    };
    volatile LogonState logonState;

    CThostFtdcTraderApi *tradeApi;
    int requestID;
    int frontID;
    int sessionID;
    int orderRef;
};

void doSleep(unsigned int millis) {
#ifdef _WIN32
    Sleep(millis);
#else
    usleep(millis*1000);
#endif
}

void printCurrTime() {
#ifdef _WIN32
    SYSTEMTIME localTime;
    GetLocalTime(&localTime);
    printf("[%d-%02d-%02d %02d:%02d:%02d.%03d] ", localTime.wYear, localTime.wMonth, localTime.wDay, localTime.wHour, localTime.wMinute, localTime.wSecond, localTime.wMilliseconds);
#else
    struct timeval timeVal;
    gettimeofday(&timeVal, NULL);
    struct tm* time;
    time = localtime(&timeVal.tv_sec);
    printf("[%d-%02d-%02d %02d:%02d:%02d.%03d] ", (time->tm_year + 1900), (time->tm_mon + 1), time->tm_mday, time->tm_hour, time->tm_min, time->tm_sec, timeVal.tv_usec / 1000);
#endif
}

int main() {
#ifndef _WIN32
    // To ignore SIGPIPE
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, 0);
#endif
    const char* flowPath = "trader-0";
    CThostFtdcTraderApi *pTraderApi = CThostFtdcTraderApi::CreateFtdcTraderApi(flowPath);  // Distinct flow path is needed for each api instance
    TestTraderClient *tradeClient = new TestTraderClient(pTraderApi);
    pTraderApi->RegisterSpi(tradeClient);
    pTraderApi->RegisterFront((char *)TRADE_SERVER_URI);

    pTraderApi->SubscribePrivateTopic(THOST_TERT_RESTART);
    pTraderApi->SubscribePublicTopic(THOST_TERT_RESTART);

    Logger::info("[INFO] [%s:%3d]: Initial client: serverUri=%s, brokerID=%s, userID=%s, version=%s.", __FUNCTION__, __LINE__,
        TRADE_SERVER_URI, BROKER_ID, USER_ID, CThostFtdcTraderApi::GetApiVersion());
    
    pTraderApi->Init();  // Start connecting

    doSleep(1000);
    // tradeClient->queryInstrument("GC2202-CME");
    // doSleep(5000);
    // tradeClient->queryInstrument("GC2203-CME");
    // doSleep(5000);
    tradeClient->queryInstrument("GC2204-CME");
    doSleep(5000);
    tradeClient->insertOrder("GC2204-CME", true, 3900.0, 3);
    doSleep(1000);
    tradeClient->queryOrder("GC2204-CME");
    // doSleep(1000);
    // tradeClient->replaceOrder("1", 3950.0, 5);
    // doSleep(1000);
    // tradeClient->cancelOrder("1");
    // doSleep(1000);
    tradeClient->queryTrade("GC2204-CME");
    doSleep(1000);
    tradeClient->queryInvestorPosition("GC2204-CME");
    doSleep(1000);
    tradeClient->queryTradingAccount();
    doSleep(1000);
    // Destroy the instance and release resources
    pTraderApi->RegisterSpi(NULL);
    pTraderApi->Release();

    delete tradeClient;

    return 0;
}
