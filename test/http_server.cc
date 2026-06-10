#include "http.hpp"

void HelloCallback(Request *request, Response *response)
{
    LOG_DEBUG("Hello business start");
    sleep(1);
    LOG_DEBUG("Hello business end");

    response->SetBody("Hello response");
    response->SetHeaders("Content-Type", "text/plain");
}

void SearchCallback(Request *request, Response *response)
{
    LOG_DEBUG("Request Search Module");
    return;
}

void HeadCallback(Request *request, Response *response)
{
    return;
}
void OptionsCallback(Request *request, Response *response)
{
    return;
}
void TraceCallback(Request *request, Response *response)
{
    return;
}
void PutCallback(Request *request, Response *response)
{
    return;
}
void DeleteCallback(Request *request, Response *response)
{
    return;
}
void PostCallback(Request *request, Response *response)
{
    return;
}
void PatchCallback(Request *request, Response *response)
{
    return;
}
void SlowCallback(Request *request, Response *response)
{
    LOG_DEBUG("Slow business start");
    sleep(40);
    LOG_DEBUG("Slow business end");

    response->SetBody("slow response");
    response->SetHeaders("Content-Type", "text/plain");
}

int main()
{
    HttpServer httpserver(8080);
    httpserver.SetThreadPollCount(5);
    httpserver.SetStaticResourcePath("/home/wyf/learning-code/OneThreadOneLoopServer/version1/www");
    httpserver.SetGetRouteTable("/hello", HelloCallback);
    httpserver.SetGetRouteTable("/slow", SlowCallback);
    httpserver.SetGetRouteTable("/search", SearchCallback);
    httpserver.SetHeadRouteTable("/api/head", HeadCallback);
    httpserver.SetOptionsRouteTable("/api/options", OptionsCallback);
    httpserver.SetTraceRouteTable("/api/trace", TraceCallback);
    httpserver.SetPutRouteTable("/api/put", PutCallback);
    httpserver.SetDeleteRouteTable("/api/delete", DeleteCallback);
    httpserver.SetPostRouteTable("/api/post", PostCallback);
    httpserver.SetPatchRouteTable("/api/patch", PatchCallback);
    httpserver.Start();
    return 0;
}
