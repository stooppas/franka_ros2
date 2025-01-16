#pragma once

#include <Poco/Net/HTTPSClientSession.h>
#include <memory>

class FrankaDeskClient
{
    public:
    FrankaDeskClient(std::string& ip);
    bool startup();
    bool shutdown();

    private:

    bool login();
    bool request_token();
    bool release_token();
    bool release_brakes();
    bool engage_brakes();
    bool enable_fci();
    bool home_gripper();
    bool logout();

    std::string _ip;
    std::string auth_cookie;
    std::string control_auth_token;
    std::shared_ptr<Poco::Net::HTTPSClientSession> _session;
};