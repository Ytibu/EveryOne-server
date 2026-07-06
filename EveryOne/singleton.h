#pragma once

// Meyer's Singleton: 线程安全的局部静态对象，返回引用而非 shared_ptr
// - 自动 lazy-init（C++11 保证静态局部变量初始化的线程安全）
// - 避免 static shared_ptr 跨编译单元析构顺序问题
// - 零堆分配，无引用计数开销
template<typename T>
class Singleton {
protected:
    Singleton() = default;
    Singleton(const Singleton<T>&) = delete;
    Singleton& operator=(const Singleton<T>& st) = delete;

    virtual ~Singleton() = default;

public:
    static T& GetInstance()
    {
        static T instance;
        return instance;
    }
};
