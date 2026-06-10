#include "http.hpp"
#include <signal.h>

std::string BuildRequest()
{
    std::stringstream request_stream;
    request_stream << "GET /search HTTP/1.1\r\nConnection: keep-alive\r\nContent-Length: 0 \r\n\r\n";
    request_stream << "GET /search HTTP/1.1\r\nConnection: keep-alive\r\nContent-Length: 0 \r\n\r\n";
    request_stream << "GET /search HTTP/1.1\r\nConnection: keep-alive\r\nContent-Length: 0 \r\n\r\n";
    request_stream << "GET /search HTTP/1.1\r\nConnection: keep-alive\r\nContent-Length: 0 \r\n\r\n";
    return request_stream.str();
}

int main()
{
    Socket client;
    client.BuildTcpMethodClient("127.0.0.1", 8080);
    while (1)
    {
        std::string send_request = BuildRequest();
        int s = client.Send(send_request.c_str(), send_request.size(), 0);
        if (s < 0)
        {
            LOG_ERROR("Send error");
            exit(1);
        }
        else if (s == 0)
        {
            LOG_DEBUG("Peer closed");
            break;
        }
        else
        {
            LOG_DEBUG("Send Successufl!");
        }

        sleep(1);

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
            LOG_DEBUG("Peer closed");
            break;
        }
        else
        {
            LOG_DEBUG("Receive Successufl,wait");
            // exit(0);
            std::cout << receive_response << std::endl;
            sleep(100);
        }
    }

    sleep(31);
    client.Close();
    return 0;
}
