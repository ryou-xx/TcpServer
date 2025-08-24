#pragma once

/*
** 由该类派生的子类默认无法使用拷贝构造函数和赋值运算符，需要显示定义
*/
class noncopyable
{   
public:
    noncopyable(const noncopyable &) = delete;
    noncopyable& operator=(const noncopyable&) = delete;
protected:
    noncopyable() = default;
    ~noncopyable() = default;
};
