#include "franka_hardware/franka_desk_client.hpp"

#include <Poco/URI.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/HTTPMessage.h>
#include <Poco/JSON/Parser.h>
#include <Poco/Net/Context.h>
#include "Poco/StreamCopier.h"
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

bool FrankaDeskClient::login()
{
    // Step 1: Perform the initial GET requests
    Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, "/admin/login");
    
    //request.set("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.9");
    //request.set("Accept-Language", "en-US,en;q=0.9");
    //request.set("Sec-CH-UA", "\" Not;A Brand\";v=\"99\", \"Google Chrome\";v=\"97\", \"Chromium\";v=\"97\"");
    //request.set("Sec-CH-UA-Mobile", "?0");
    //request.set("Sec-CH-UA-Platform", "\"Linux\"");
    //request.set("Upgrade-Insecure-Requests", "1");

    Poco::Net::HTTPResponse response;
    _session->sendRequest(request);
    _session->receiveResponse(response);
    if (response.getStatus() != Poco::Net::HTTPResponse::HTTP_OK) {
        std::cerr << "Logging in to robot hand failed. Exiting." << std::endl;
        return false;
    }

    request.setURI("/admin/api/first-start");
    _session->sendRequest(request);
    _session->receiveResponse(response);
    if (response.getStatus() != Poco::Net::HTTPResponse::HTTP_OK) {
        std::cerr << "Logging in to robot hand failed. Exiting." << std::endl;
        return false;
    }

    request.setURI("/admin/api/startup-phase");
    _session->sendRequest(request);
    _session->receiveResponse(response);
    if (response.getStatus() != Poco::Net::HTTPResponse::HTTP_OK) {
        std::cerr << "Logging in to robot hand failed. Exiting." << std::endl;
        return false;
    }

    // Step 2: Perform POST request with login credentials

    Poco::Net::HTTPRequest PostRequest(Poco::Net::HTTPRequest::HTTP_POST, "/admin/api/login");
    //PostRequest.set("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.9");
    //PostRequest.set("Accept-Language", "en-US,en;q=0.9");
    //PostRequest.set("Sec-CH-UA", "\" Not;A Brand\";v=\"99\", \"Google Chrome\";v=\"97\", \"Chromium\";v=\"97\"");
    //PostRequest.set("Sec-CH-UA-Mobile", "?0");
    //PostRequest.set("Sec-CH-UA-Platform", "\"Linux\"");
    //PostRequest.set("Upgrade-Insecure-Requests", "1");

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
        std::cerr << "Logging in to robot hand failed. Exiting." << std::endl;
        return 1;
    }

    // Extract the auth cookie
    Poco::StreamCopier::copyToString(stream, auth_cookie);
    std::cout << "Auth Token: " << auth_cookie << std::endl;

    return true;
}

bool FrankaDeskClient::release_brakes()
{
    Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, "/admin/api/safety");
    request.set("Authorization",auth_cookie.c_str());
    request.setContentType("application/json");

    Poco::Net::HTTPResponse response;
    _session->sendRequest(request);
    _session->receiveResponse(response);
    if (response.getStatus() != Poco::Net::HTTPResponse::HTTP_OK) {
        std::cerr << "Logging in to robot hand failed. Exiting." << std::endl;
        return false;
    }

    Poco::Net::HTTPRequest PostRequest(Poco::Net::HTTPRequest::HTTP_POST, "/admin/api/control-token/request");
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
        std::cerr << "Logging in to robot hand failed. Exiting." << std::endl;
        return 1;
    }

    Poco::JSON::Parser parser;
    Poco::Dynamic::Var result = parser.parse(stream);
    Poco::JSON::Object::Ptr result_obj = result.extract<Poco::JSON::Object::Ptr>();
    control_auth_token = result_obj->get("token").toString();

    std::cout << "Token: " << control_auth_token << std::endl;

    return true;
}

bool FrankaDeskClient::engage_brakes()
{
    return true;
}

bool FrankaDeskClient::enable_fci()
{
    return true;
}

bool FrankaDeskClient::disable_fci()
{
    return true;
}