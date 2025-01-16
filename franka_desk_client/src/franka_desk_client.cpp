#include "franka_desk_client/franka_desk_client.hpp"

#include <Poco/URI.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/HTTPMessage.h>
#include <Poco/JSON/Parser.h>
#include <Poco/Net/Context.h>
#include "Poco/StreamCopier.h"
#include <sstream>

const std::string username = "controller";
const std::string password = "MTY5LDExOSw4OCwxNTUsOTQsMTUsMSwyMTcsMjU1LDE5LDEyMSw0NywxNTksMTYwLDIwMiwyNDYsMTgyLDExMSwxODUsMjQ5LDQwLDE4NywxMzYsMTcyLDEyMCwxNzMsNzQsMTMwLDEyMSw5OSwzMSwyNDQ=";

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  auto client = std::make_shared<FrankaDeskClient>();
  rclcpp::shutdown();
  return 0;
}


FrankaDeskClient::FrankaDeskClient(): Node("franka_desk_client")
{
    this->declare_parameter("robot_ip","");
    std::string ip = this->get_parameter("robot_ip").as_string();

    if(ip == "")
    {
        return;
    }

    std::string base_url = "https://" + ip;

    const Poco::Net::Context::Ptr context = new Poco::Net::Context(
            Poco::Net::Context::CLIENT_USE, "", "", "",
            Poco::Net::Context::VERIFY_NONE, 9, false,
            "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");

    Poco::URI uri(base_url);
    _session = std::make_shared<Poco::Net::HTTPSClientSession>(uri.getHost(), uri.getPort(),context);
    _session->setKeepAlive(true);
    //_session->setTimeout(Poco::Timespan(30, 0)); // Set timeout

    if(startup()){
        
        RCLCPP_INFO(rclcpp::get_logger("FrankaDeskClient"),"Sucessfully started Robot");

        rclcpp::Rate r(10);
        while(rclcpp::ok())
        {
            r.sleep();
        }

        shutdown();
    }

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

    rclcpp::Rate r(1);
    uint8_t timeout_ctr = 0;
    while(rclcpp::ok())
    {
        std::string active_token = get_active_token();
        RCLCPP_INFO(rclcpp::get_logger("FrankaDeskClient"),"Waiting for Token... Grant Access by Pressing physical Button");
        if(active_token == token_id)
        {
            break;
        }
        else if(timeout_ctr++ > 60)
        {
            return false;
        }
        else
        {
            r.sleep();
        }
    }

    if(!release_brakes())
    {
        return false;
    }
    
    if(!home_gripper())
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
    if(!disable_fci())
    {

    }

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
    login_body.set("login", username);
    login_body.set("password", password);

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
    std::string auth_cookie;
    Poco::StreamCopier::copyToString(stream, auth_cookie);
    cookies.set("authorization",auth_cookie);

    return true;
}

bool FrankaDeskClient::request_token()
{
    RCLCPP_INFO(rclcpp::get_logger("FrankaDeskClient"),"Requesting Token");

    Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, "/admin/api/safety");
    request.setCookies(cookies);
    request.setContentType("application/json");

    Poco::Net::HTTPResponse response;
    _session->sendRequest(request);
    _session->receiveResponse(response);
    if (response.getStatus() != Poco::Net::HTTPResponse::HTTP_OK) {
        RCLCPP_ERROR(rclcpp::get_logger("FrankaDeskClient"),"Get Safety Failed");
        return false;
    }

    Poco::Net::HTTPRequest PostRequest(Poco::Net::HTTPRequest::HTTP_POST, "/admin/api/control-token/request?force");
    PostRequest.setCookies(cookies);

    Poco::JSON::Object control_token_request;
    control_token_request.set("requestedBy", username);

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
    token_id = result_obj->get("id").toString();

    return true;
}

bool FrankaDeskClient::release_token()
{
    RCLCPP_INFO(rclcpp::get_logger("FrankaDeskClient"),"Releasing Token");

    Poco::Net::HTTPRequest PostRequest(Poco::Net::HTTPRequest::HTTP_DELETE, "/admin/api/control-token");
    PostRequest.setCookies(cookies);

    Poco::JSON::Object control_token_request;
    control_token_request.set("token", control_auth_token.c_str());

    std::ostringstream control_token_request_stream;
    control_token_request.stringify(control_token_request_stream);
    std::string control_token_request_str = control_token_request_stream.str();

    PostRequest.setContentType("application/json");
    PostRequest.set("Content-Length", std::to_string(control_token_request_str.length()));

    _session->sendRequest(PostRequest) << control_token_request_str;
    Poco::Net::HTTPResponse response;
    _session->receiveResponse(response);

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
    PostRequest.setCookies(cookies);
    PostRequest.set("X-Control-Token",control_auth_token.c_str());

    _session->sendRequest(PostRequest);
    Poco::Net::HTTPResponse response;
    _session->receiveResponse(response);

    if (response.getStatus() != Poco::Net::HTTPResponse::HTTP_OK) {
        RCLCPP_ERROR(rclcpp::get_logger("FrankaDeskClient"),"Logging out failed");
        return false;
    }
    
    return true;
}

bool FrankaDeskClient::release_brakes()
{
    RCLCPP_INFO(rclcpp::get_logger("FrankaDeskClient"),"Releasing brakes");

    /*
    Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, "/admin/api/robot/shutdown-position-error");
    request.setCookies(cookies);
    request.set("X-Control-Token",control_auth_token.c_str());

    Poco::Net::HTTPResponse response;
    _session->sendRequest(request);
    _session->receiveResponse(response);
    if (response.getStatus() != Poco::Net::HTTPResponse::HTTP_OK) {
        RCLCPP_ERROR(rclcpp::get_logger("FrankaDeskClient"),"Position Error Shutdown failed");
        return false;
    }
    */

    Poco::Net::HTTPRequest PostRequest(Poco::Net::HTTPRequest::HTTP_POST, "/desk/api/robot/open-brakes");
    PostRequest.setCookies(cookies);
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

    if(response.getStatus() == Poco::Net::HTTPResponse::HTTP_LOCKED)
    {
        RCLCPP_INFO(rclcpp::get_logger("FrankaDeskClient"),"Brakes already released");
    }
    else if (response.getStatus() != Poco::Net::HTTPResponse::HTTP_OK) {
        RCLCPP_ERROR(rclcpp::get_logger("FrankaDeskClient"),"Releasing brakes failed");
        return false;
    }

    return true;
}

bool FrankaDeskClient::engage_brakes()
{
    RCLCPP_INFO(rclcpp::get_logger("FrankaDeskClient"),"Engaging brakes");

    Poco::Net::HTTPRequest PostRequest(Poco::Net::HTTPRequest::HTTP_POST, "/desk/api/robot/close-brakes");
    PostRequest.setCookies(cookies);
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

    if(response.getStatus() == Poco::Net::HTTPResponse::HTTP_LOCKED)
    {
        RCLCPP_INFO(rclcpp::get_logger("FrankaDeskClient"),"Brakes already engaged");
    }
    else if (response.getStatus() != Poco::Net::HTTPResponse::HTTP_OK) {
        RCLCPP_ERROR(rclcpp::get_logger("FrankaDeskClient"),"Engaging brakes failed");
        return false;
    }
    
    return true;
}

std::string FrankaDeskClient::get_active_token()
{
    Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, "/admin/api/control-token");
    request.setCookies(cookies);
    request.set("X-Control-Token",control_auth_token.c_str());

    Poco::Net::HTTPResponse response;
    _session->sendRequest(request);
    std::istream& stream = _session->receiveResponse(response);

    if (response.getStatus() != Poco::Net::HTTPResponse::HTTP_OK) {
        RCLCPP_ERROR(rclcpp::get_logger("FrankaDeskClient"),"Get Active Token failed");
        return "";
    }

    Poco::JSON::Parser parser;
    Poco::Dynamic::Var result = parser.parse(stream);
    Poco::JSON::Object::Ptr result_obj = result.extract<Poco::JSON::Object::Ptr>();
    return result_obj->get("activeToken").extract<Poco::JSON::Object::Ptr>()->get("id");
}

bool FrankaDeskClient::enable_fci()
{
    RCLCPP_INFO(rclcpp::get_logger("FrankaDeskClient"),"Enabling FCi");

    Poco::Net::HTTPRequest PostRequest(Poco::Net::HTTPRequest::HTTP_POST, "/admin/api/control-token/fci");
    PostRequest.setCookies(cookies);
    PostRequest.set("X-Control-Token",control_auth_token.c_str());

    Poco::JSON::Object obj;
    obj.set("token", control_auth_token);
    std::stringstream ss; 
    obj.stringify(ss);

    PostRequest.setContentType("application/json");
    PostRequest.setContentLength(ss.str().size());

    std::ostream& myOStream = _session->sendRequest(PostRequest);
    obj.stringify(myOStream);

    //_session->sendRequest(PostRequest) << control_token_str;
    Poco::Net::HTTPResponse response;
    _session->receiveResponse(response);

    if (response.getStatus() != Poco::Net::HTTPResponse::HTTP_OK) {
        RCLCPP_ERROR(rclcpp::get_logger("FrankaDeskClient"),"Activating FCi failed, response: %i",response.getStatus());
        return false;
    }
    
    return true;
}

bool FrankaDeskClient::reset_errors()
{
    Poco::Net::HTTPRequest PostRequest(Poco::Net::HTTPRequest::HTTP_POST, "/desk/api/robot/reset-errors");
    PostRequest.setCookies(cookies);
    PostRequest.set("X-Control-Token",control_auth_token.c_str());

    PostRequest.setContentType("application/json");
    PostRequest.setContentLength(0);

    _session->sendRequest(PostRequest);
    Poco::Net::HTTPResponse response;
    std::istream &stream = _session->receiveResponse(response);

    std::string str;
    Poco::StreamCopier::copyToString(stream, str);
    std::cout << str << std::endl;

    if (response.getStatus() != Poco::Net::HTTPResponse::HTTP_OK) {
        RCLCPP_ERROR(rclcpp::get_logger("FrankaDeskClient"),"Reset Error Failed");
        return false;
    }

    return true;
}

bool FrankaDeskClient::disable_fci()
{
    RCLCPP_INFO(rclcpp::get_logger("FrankaDeskClient"),"Disabling FCi");

    Poco::Net::HTTPRequest PostRequest(Poco::Net::HTTPRequest::HTTP_DELETE, "/admin/api/control-token/fci");
    PostRequest.setCookies(cookies);
    PostRequest.set("X-Control-Token",control_auth_token.c_str());

    Poco::JSON::Object control_token;
    control_token.set("token", control_auth_token.c_str());
    std::ostringstream control_token_stream;
    control_token.stringify(control_token_stream);
    std::string control_token_str = control_token_stream.str();

    PostRequest.setContentType("application/json");
    PostRequest.set("Content-Length", std::to_string(control_token_str.length()));

    _session->sendRequest(PostRequest) << control_token_str;
    Poco::Net::HTTPResponse response;
    _session->receiveResponse(response);

    if(response.getStatus() == Poco::Net::HTTPResponse::HTTP_LOCKED)
    {
        RCLCPP_INFO(rclcpp::get_logger("FrankaDeskClient"),"FCi already disabled");
    }
    else if (response.getStatus() != Poco::Net::HTTPResponse::HTTP_OK) {
        RCLCPP_ERROR(rclcpp::get_logger("FrankaDeskClient"),"Disabling FCi failed");
        return false;
    }
    
    return true;
}

bool FrankaDeskClient::home_gripper()
{
    RCLCPP_INFO(rclcpp::get_logger("FrankaDeskClient"),"Homing Gripper");

    Poco::Net::HTTPRequest PostRequest(Poco::Net::HTTPRequest::HTTP_POST, "/desk/api/gripper/homing");
    PostRequest.setCookies(cookies);
    PostRequest.set("X-Control-Token",control_auth_token.c_str());

    PostRequest.setContentType("application/json");
    PostRequest.setContentLength(0);

    _session->sendRequest(PostRequest);
    Poco::Net::HTTPResponse response;
    _session->receiveResponse(response);

    if (response.getStatus() != Poco::Net::HTTPResponse::HTTP_OK) {
        RCLCPP_ERROR(rclcpp::get_logger("FrankaDeskClient"),"Homing Gripper failed");
        return false;
    }

    return true;
}