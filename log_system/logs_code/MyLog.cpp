#include "MyLog.hpp"

namespace mylog{
    AsyncLogger::ptr GetLogger(const std::string &name)
    {
        return LoggerManager::GetInstance().GetLogger(name);
    }

    AsyncLogger::ptr DefaultLogger()
    {
        return LoggerManager::GetInstance().DefaultLogger();
    }

    void StopAllLoggers()
    {
        LoggerManager::GetInstance().StopAll();
    }
}