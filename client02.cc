#include "http.hpp"

std::string BuildRequest()
{
    std::stringstream request_stream;
    request_stream << "GET /hello HTTP/1.1\r\nConnection: keep-alive\r\nContent-Length: 0 \r\n\r\n";
    return request_stream.str();
}

int main()
{
    Socket client;
    client.BuildTcpMethodClient("127.0.0.1", 8080);
    std::string send_request = BuildRequest();
    std::string test;
    std::cout << test << std::endl;
    // int s = client.SendWithNoBlock(send_request.c_str(), send_request.size());
    while (1)
    {
        int s = client.Send(send_request.c_str(), send_request.size(), 0);
        if (s < 0)
        {
            LOG_ERROR("Send error");
            exit(1);
        }
        else if (s == 0)
        {
            LOG_DEBUG("Try Again");
        }
        else
        {
            LOG_DEBUG("Send Successufl!");
        }
        std::string receive_response;
        receive_response.resize(4096);
        int r = client.Receive(&(receive_response[0]), 4096, 0);
        if (r < 0)
        {
            LOG_ERROR("Receive error");
            exit(1);
        }
        else if (r == 0)
        {
            LOG_DEBUG("Try Again");
        }
        else
        {
            LOG_DEBUG("Receive Successufl!");
            // LOG_DEBUG("Recieve Response:\r\n%s\r\n", receive_response.c_str());
            std::cout << receive_response.c_str() << std::endl;
        }
        sleep(100);
    }
    client.Close();
    while (1)
    {
        sleep(1);
    }

    return 0;
}
