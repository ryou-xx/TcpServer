#pragma once

#include <iostream>
#include <string>

class Timestamp
{
public:
    Timestamp();
    explicit Timestamp(int64_t secondsSinceEpoch);
    static Timestamp now();
    std::string toString() const;  // 将sencodsSinceEpoch转化为对应的本地时间

private:
    int64_t secondsSinceEopch_;
};