#include "franka_hardware/franka_desk_client.hpp"

#include <Poco/URI.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/HTTPMessage.h>
#include <Poco/JSON/Parser.h>
#include <Poco/Net/Context.h>
#include "Poco/StreamCopier.h"
#include <rclcpp/rclcpp.hpp>
#include <sstream>

FrankaDeskClient::FrankaDeskClient(std::string& ip):_ip(ip)
{
    std::string base_url = "https://" + _ip;

    const Poco::Net::Context::Ptr context = new Poco::Net::Context(
            Poco::Net::Context::CLIENT_USE, "", "", "",
            Poco::Net::Context::VERIFY_NONE, 9, false,
            "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");

    Poco::URI uri(base_url);
    _session = std::make_shared<Poco::Net::HTTPSClientSession>(uri.getHost(), uri.getPort(),context);
    //_session->setTimeout(Poco::Timespan(30, 0)); // Set timeout
}

bool FrankaDeskClient::startup()
{
    if(!login())
    {
        return false;
    }

    if(!request_token())
    {
        return false;
    }

    if(!release_brakes())
    {
        return false;
    }

    if(!enable_fci())
    {
        return false;
    }

    return true;
}

bool FrankaDeskClient::shutdown()
{
    if(!engage_brakes())
    {
        return false;
    }

    if(!release_token())
    {
        return false;
    }

    if(!logout())
    {
        return false;
    }

    return true;
}

bool FrankaDeskClient::login()
{
    RCLCPP_INFO(rclcpp::get_logger("FrankaDeskClient"),"Starting Login");

    Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, "/admin/login");

    Poco::Net::HTTPResponse response;
    _session->sendRequest(request);
    _session->receiveResponse(response);
    if (response.getStatus() != Poco::Net::HTTPResponse::HTTP_OK) {
        RCLCPP_ERROR(rclcpp::get_logger("FrankaDeskClient"),"Logging in to robot hand failed");
        return false;
    }

    request.setURI("/admin/api/first-start");
    _session->sendRequest(request);
    _session->receiveResponse(response);
    if (response.getStatus() != Poco::Net::HTTPResponse::HTTP_OK) {
        RCLCPP_ERROR(rclcpp::get_logger("FrankaDeskClient"),"Logging in to robot hand failed");
        return false;
    }

    request.setURI("/admin/api/startup-phase");
    _session->sendRequest(request);
    _session->receiveResponse(response);
    if (response.getStatus() != Poco::Net::HTTPResponse::HTTP_OK) {
        RCLCPP_ERROR(rclcpp::get_logger("FrankaDeskClient"),"Logging in to robot hand failed");
        return false;
    }

    Poco::Net::HTTPRequest PostRequest(Poco::Net::HTTPRequest::HTTP_POST, "/admin/api/login");

    Poco::JSON::Object login_body;
    login_body.set("login", "ILT");
    login_body.set("password", "MTkxLDI1MiwxOTcsMjA4LDEwLDIzOCw3NCwzMCwxNSwxOTcsMTQ5LDQzLDE1MSw1MiwxNTYsMjI3LDk3LDQwLDExMiw3OSw4NSwyNDQsNDgsODYsMjEsMSwyNTEsMjIyLDI4LDI1MywyMzIsNTM=");

    std::ostringstream login_body_stream;
    login_body.stringify(login_body_stream);
    std::string login_body_str = login_body_stream.str();

    PostRequest.setContentType("application/json");
    PostRequest.set("Content-Length", std::to_string(login_body_str.length()));

    _session->sendRequest(PostRequest) << login_body_str;
    std::istream &stream = _session->receiveResponse(response);

    // Check if login was successful
    if (response.getStatus() != Poco::Net::HTTPResponse::HTTP_OK) {
        RCLCPP_ERROR(rclcpp::get_logger("FrankaDeskClient"),"Logging in to robot hand failed");
        return false;
    }

    // Extract the auth cookie
    Poco::StreamCopier::copyToString(stream, auth_cookie);
    std::cout << "Auth Token: " << auth_cookie << std::endl;

    return true;
}

bool FrankaDeskClient::request_token()
{
    RCLCPP_INFO(rclcpp::get_logger("FrankaDeskClient"),"Requesting Token");

    Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, "/admin/api/safety");
    request.set("Authorization",auth_cookie.c_str());
    request.setContentType("application/json");

    Poco::Net::HTTPResponse response;
    _session->sendRequest(request);
    _session->receiveResponse(response);
    if (response.getStatus() != Poco::Net::HTTPResponse::HTTP_OK) {
        RCLCPP_ERROR(rclcpp::get_logger("FrankaDeskClient"),"Request Token failed");
        return false;
    }

    Poco::Net::HTTPRequest PostRequest(Poco::Net::HTTPRequest::HTTP_POST, "/admin/api/control-token/request?force");
    PostRequest.set("Authorization",auth_cookie.c_str());

    Poco::JSON::Object control_token_request;
    control_token_request.set("requestedBy", "ILT");

    std::ostringstream control_token_request_stream;
    control_token_request.stringify(control_token_request_stream);
    std::string control_token_request_str = control_token_request_stream.str();

    PostRequest.setContentType("application/json");
    PostRequest.set("Content-Length", std::to_string(control_token_request_str.length()));

    _session->sendRequest(PostRequest) << control_token_request_str;
    std::istream &stream = _session->receiveResponse(response);

    // Check if login was successful
    if (response.getStatus() != Poco::Net::HTTPResponse::HTTP_OK) {
        RCLCPP_ERROR(rclcpp::get_logger("FrankaDeskClient"),"Request Token failed");
        return false;
    }

    Poco::JSON::Parser parser;
    Poco::Dynamic::Var result = parser.parse(stream);
    Poco::JSON::Object::Ptr result_obj = result.extract<Poco::JSON::Object::Ptr>();
    control_auth_token = result_obj->get("token").toString();

    return true;
}

bool FrankaDeskClient::release_token()
{
    RCLCPP_INFO(rclcpp::get_logger("FrankaDeskClient"),"Releasing Token");

    Poco::Net::HTTPRequest PostRequest(Poco::Net::HTTPRequest::HTTP_DELETE, "/admin/api/control-token");
    PostRequest.set("Authorization",auth_cookie.c_str());

    Poco::JSON::Object control_token_request;
    control_token_request.set("token", control_auth_token);

    std::ostringstream control_token_request_stream;
    control_token_request.stringify(control_token_request_stream);
    std::string control_token_request_str = control_token_request_stream.str();

    PostRequest.setContentType("application/json");
    PostRequest.set("Content-Length", std::to_string(control_token_request_str.length()));

    _session->sendRequest(PostRequest) << control_token_request_str;
        Poco::Net::HTTPResponse response;

    std::istream &stream = _session->receiveResponse(response);

    // Check if login was successful
    if (response.getStatus() != Poco::Net::HTTPResponse::HTTP_OK) {
        RCLCPP_ERROR(rclcpp::get_logger("FrankaDeskClient"),"Releasing Token failed");
        return false;
    }

    control_auth_token = "";

    return true;
}

bool FrankaDeskClient::logout()
{
    RCLCPP_INFO(rclcpp::get_logger("FrankaDeskClient"),"Logging out");

    Poco::Net::HTTPRequest PostRequest(Poco::Net::HTTPRequest::HTTP_POST, "/admin/api/logout");
    PostRequest.set("authorization",auth_cookie.c_str());

    _session->sendRequest(PostRequest);
    Poco::Net::HTTPResponse response;
    _session->receiveResponse(response);

    if (response.getStatus() != Poco::Net::HTTPResponse::HTTP_OK) {
        RCLCPP_ERROR(rclcpp::get_logger("FrankaDeskClient"),"Logging out failed");
        return false;
    }
    
    return true;
}

/*
bool FrankaDeskClient::get_active_token_id()
{

}

bool FrankaDeskClient::is_active_token()
{
    return true;
}
*/

bool FrankaDeskClient::release_brakes()
{
    RCLCPP_INFO(rclcpp::get_logger("FrankaDeskClient"),"Releasing brakes");

    Poco::Net::HTTPRequest PostRequest(Poco::Net::HTTPRequest::HTTP_POST, "/desk/api/robot/open-brakes");
    PostRequest.set("authorization",auth_cookie.c_str());
    PostRequest.set("X-Control-Token",control_auth_token.c_str());

    Poco::JSON::Object force;
    force.set("force", "true");
    std::ostringstream force_stream;
    force.stringify(force_stream);
    std::string force_str = force_stream.str();

    PostRequest.setContentType("application/json");
    PostRequest.set("Content-Length", std::to_string(force_str.length()));

    _session->sendRequest(PostRequest) << force_str;
    Poco::Net::HTTPResponse response;
    _session->receiveResponse(response);

    if (response.getStatus() != Poco::Net::HTTPResponse::HTTP_OK) {
        RCLCPP_ERROR(rclcpp::get_logger("FrankaDeskClient"),"Releasing brakes failed");
        return false;
    }

    return true;
}

bool FrankaDeskClient::engage_brakes()
{
    RCLCPP_INFO(rclcpp::get_logger("FrankaDeskClient"),"Engaging brakes");

    Poco::Net::HTTPRequest PostRequest(Poco::Net::HTTPRequest::HTTP_POST, "/desk/api/robot/close-brakes");
    PostRequest.set("authorization",auth_cookie.c_str());
    PostRequest.set("X-Control-Token",control_auth_token.c_str());

    Poco::JSON::Object force;
    force.set("force", "true");
    std::ostringstream force_stream;
    force.stringify(force_stream);
    std::string force_str = force_stream.str();

    PostRequest.setContentType("application/json");
    PostRequest.set("Content-Length", std::to_string(force_str.length()));

    _session->sendRequest(PostRequest) << force_str;
    Poco::Net::HTTPResponse response;
    _session->receiveResponse(response);

    if (response.getStatus() != Poco::Net::HTTPResponse::HTTP_OK) {
        RCLCPP_ERROR(rclcpp::get_logger("FrankaDeskClient"),"Activating brakes failed");
        return false;
    }
    
    return true;
}

bool FrankaDeskClient::enable_fci()
{
    RCLCPP_INFO(rclcpp::get_logger("FrankaDeskClient"),"Enabling FCi");

    Poco::Net::HTTPRequest PostRequest(Poco::Net::HTTPRequest::HTTP_POST, "/admin/api/control-token/fci");
    PostRequest.set("authorization",auth_cookie.c_str());

    Poco::JSON::Object control_token;
    control_token.set("token", control_auth_token);
    std::ostringstream control_token_stream;
    control_token.stringify(control_token_stream);
    std::string control_token_str = control_token_stream.str();

    PostRequest.setContentType("application/json");
    PostRequest.set("Content-Length", std::to_string(control_token_str.length()));

    _session->sendRequest(PostRequest) << control_token_str;
    Poco::Net::HTTPResponse response;
    _session->receiveResponse(response);

    if (response.getStatus() != Poco::Net::HTTPResponse::HTTP_OK) {
        RCLCPP_ERROR(rclcpp::get_logger("FrankaDeskClient"),"Activating Fci failed");
        return false;
    }
    
    return true;
}

bool FrankaDeskClient::home_gripper()
{
    RCLCPP_INFO(rclcpp::get_logger("FrankaDeskClient"),"Homing Gripper");

    Poco::Net::HTTPRequest PostRequest(Poco::Net::HTTPRequest::HTTP_POST, "/desk/api/gripper/homing");
    PostRequest.set("authorization",auth_cookie.c_str());
    PostRequest.set("X-Control-Token",control_auth_token.c_str());

    _session->sendRequest(PostRequest);
    Poco::Net::HTTPResponse response;
    _session->receiveResponse(response);

    if (response.getStatus() != Poco::Net::HTTPResponse::HTTP_OK) {
        RCLCPP_ERROR(rclcpp::get_logger("FrankaDeskClient"),"Homing Gripper failed");
        return false;
    }

    return true;
}