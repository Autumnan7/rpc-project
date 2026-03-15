#include <iostream>
#include "log.h"
int main()
{
    std::cout << "minico-RPC bootstrap success\n";
    minico::Logger::Instance().Info("This is an info message.");
    minico::Logger::Instance().Error("This is an error message.");
    return 0;
}