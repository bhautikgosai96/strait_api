For the client who wants trade the future products of China exchanges, it is recommended to upgrade the CTP library to v6.3.15.

The v6.3.15 version api has a new feature than the other version apis, v6.3.15 will collect client's system information and encrypt it and finally 
delivery to CFMMC (China Futures Market Monitoring Center), according to the requirement of CFMMC. CFMMC will decrypt the client system information
and validate if there's any unexpected behaviour(e.g. rig the market) with the client. If the client's sytem information is not correctly collected,
client may be not allowed trading in the China exchanges.

There's only one additional function need to be called against the normal version api: ReqAuthenticate() needs to be called before the logon request.
Client needs to apply BrokerID, UserID, AppID and AuthCode properly to the ReqAuthenticate() request, and check the returned error code with OnRspAuthenticate()
callback function, if the error code is 0(success), client can then do logon. With the successfully authentication, the client's system info will be
collected silently and will be automatically transfered to the CTP server with the logon request, there's no difference with the usage of logon and
any other apis against the old version api.

To do the authentication, client need to contact to the ATP Support Team to request for a valid AuthCode. Each AuthCode is banded with BrokerID, UserID,
and AppID. AppID is provided by the client as an identifier of the client in CFMMC, the format of AppID is [clientname_clientapplicationname_clientapplicationversion],
e.g. abcdef_ghijkl_1.6.0. AppID is required for the ATP Support Team to generate an AuthCode.

Please refer to the sample code project for the details of authentication and contact the Support Team if there's any questions about the upgradation.

