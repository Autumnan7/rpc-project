#include <iostream>
#include <string>
#include <sys/sysinfo.h>

#include "../include/logger.h"
#include "../include/tcp/tcp_client.h"

void tcp_client_worker(TcpClient &tcp_client, int loop_time)
{
    LOG_INFO("Client: Start connecting...");
    tcp_client.connect("127.0.0.1", 8888);
    LOG_INFO("Client: Connected successfully!");

    char buf[2048];
    for (int i = 0; i < loop_time; ++i)
    {
        const char *msg = "ping";
        LOG_INFO("Client: %dth send %s", i, msg);

        ssize_t s_n = tcp_client.send(msg, 4);
        if (s_n <= 0)
        {
            LOG_ERROR("Send failed");
            break;
        }

        ssize_t r_n = tcp_client.recv(buf, 2047);
        if (r_n > 0)
        {
            buf[r_n] = '\0';
            LOG_INFO("Client: %dth recv %s (size: %ld)", i, buf, r_n);
        }
        else
        {
            LOG_ERROR("Recv failed or closed");
            break;
        }
    }
}

int main()
{
    LOG_INFO("---------------");
    LOG_INFO("TEST TCP CLIENT");
    LOG_INFO("---------------");

    // Default: ping-pong
    TcpClient tcp_client;
    int loop_time = 10;
    minico::co_go([&tcp_client, &loop_time]()
                  { tcp_client_worker(tcp_client, loop_time); });
    minico::sche_join();
    return 0;
}