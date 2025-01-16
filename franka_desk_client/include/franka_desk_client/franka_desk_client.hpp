#pragma once

#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <memory>
#include <rclcpp/rclcpp.hpp>

class FrankaDeskClient: public rclcpp::Node
{
    public:
    FrankaDeskClient();
    bool startup();
    bool shutdown();

    private:

    bool login();
    bool request_token();
    bool release_token();
    bool release_brakes();
    bool engage_brakes();
    bool enable_fci();
    bool disable_fci();
    bool home_gripper();
    bool logout();
    bool reset_errors();
    std::string get_active_token();

    Poco::Net::NameValueCollection cookies;
    std::string control_auth_token;
    std::string token_id;
    std::shared_ptr<Poco::Net::HTTPSClientSession> _session;
};