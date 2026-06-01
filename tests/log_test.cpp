#include "logger.h"

int main()
{
    setLogFd(1);    // 1 通常是 stdout（也可以不写）
    setLogLevel(1); // 1=DEBUG（trace看不到，因为 trace更低）

    LOG_TRACE("trace should NOT show");
    LOG_DEBUG("debug should show");
    LOG_INFO("info should show");
    LOG_WARN("warn should show");

    LOG_ERROR("error should show");

    // 下面这句会直接 abort，通常先注释掉
    // LOG_FATAL("fatal will abort");

    // errno相关的系统日志示例：先制造一个 errno
    // 例如：你也可以在 TCP/UDP 逻辑里触发 read/connect 错误后再看 LOG_SYSERR
    // LOG_SYSERR("sys error: %d", 123);

    return 0;
}