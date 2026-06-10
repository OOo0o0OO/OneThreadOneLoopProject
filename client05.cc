#include "http.hpp"
#include <signal.h>

std::string BuildRequest(const std::string path)
{
    std::stringstream request_stream;
    request_stream << "GET " << path << " HTTP/1.1\r\nConnection: keep-alive\r\nContent-Length: 0 \r\n\r\n";
    return request_stream.str();
}

int main()
{
    signal(SIGCHLD, SIG_IGN);
    for (int i = 0; i < 10; i++)
    {
        pid_t pid = fork();
        if (pid < 0)
        {
            LOG_ERROR("Fork error");
            exit(1);
        }
        // 子进程
        else if (pid == 0)
        {
            Socket client;
            client.BuildTcpMethodClient("127.0.0.1", 8080);
            // std::string path = (i == 0 ? "/slow" : "/hello");
            std::string path = (i < 5 ? "/slow" : (i < 7 ? "/hello" : "/search"));
            std::string send_request = BuildRequest(path);
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
                LOG_DEBUG("Try Again");
            }
            else
            {
                LOG_DEBUG("Receive Successufl,wait");
                // exit(0);
                std::cout << receive_response << std::endl;
                sleep(100);
            }

            LOG_DEBUG("=================================");
            sleep(2);
            client.Close();
            exit(0);
        }
        // 父进程
        sleep(1);
    }
    // 主进程等待
    while (1)
    {
        sleep(1);
    }

    return 0;
}
