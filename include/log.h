#pragma once
#include <string>
#include <memory>
namespace minico
{

    class Logger
    {
    public:
        // 日志单例
        // 静态变量：在函数内部定义一个静态变量logger，确保它只会被创建一次，并且在整个程序运行期间保持存在，不需要创建对象
        // 就可以调用
        // 修饰符  返回类型  函数名   参数列表
        // 返回 Logger类的引用
        static Logger &Instance();

        void Info(const std::string &msg);
        void Error(const std::string &msg);

    private:
        // 私有构造函数，禁止外部实例化
        // 只有在logger类的内部才能创建实例，外部无法直接创建Logger对象。
        Logger() = default;
    };

}
