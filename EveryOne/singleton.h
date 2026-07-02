#pragma once

#include <memory>
#include <mutex>

template<typename T>
class Singleton {
protected:
    Singleton() = default;
    Singleton(const Singleton<T>&) = delete;
    Singleton& operator=(const Singleton<T>& st) = delete;

    virtual ~Singleton() = default;  // 保持 protected，支持多态

    static std::shared_ptr<T> _instance;

public:
    static std::shared_ptr<T> GetInstance()
    {
        static std::once_flag s_flag;
        std::call_once(s_flag, [&]() {
            _instance = std::shared_ptr<T>(new T);
            });
        return _instance;
    }

};

template<typename T>
std::shared_ptr<T> Singleton<T>::_instance = nullptr;
