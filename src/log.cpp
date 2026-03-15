#include "log.h"
#include <iostream>

namespace minico
{
    // Logger 类的 Instance() 函数，返回一个 Logger 对象的引用。
    // Logger类的实现，使用了单例模式，确保全局只有一个Logger实例。
    Logger &Logger::Instance()
    {
        static Logger logger;
        return logger;
    }

    void Logger::Info(const std::string &msg)
    {
        std::cout << "[INFO] " << msg << std::endl;
    }

    void Logger::Error(const std::string &msg)
    {
        std::cerr << "[ERROR] " << msg << std::endl;
    }
}