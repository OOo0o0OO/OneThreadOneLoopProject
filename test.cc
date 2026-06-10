#include "server.hpp"

int main()
{
    Buffer buffer;
    std::string str1("Hello!");

    buffer.Write(str1);
    std::cout << buffer.Read(str1.size()) << std::endl;
    buffer.Reset();
    std::string str2("Hallo");
    buffer.Write(str2);
    std::cout << buffer.Read(str2.size()) << std::endl;
    buffer.Reset();
    buffer.Write(std::string("Bonjour\nBonjour"));
    std::cout << buffer.ReadLine() << std::endl;

    LOG_DEBUG("x=%d y=%d", 10, 20);
    LOG_INFO("连接建立 fd=%d", 5);
    LOG_ERROR("read 失败 errno=%d", errno);

    return 0;
}