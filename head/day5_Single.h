#include "t_include.h"

// 各种单例模式

// 局部静态变量
// C++11 之前存在多线程不安全, 之后厂商优化编译器保证线程安全
class Single2
{
private:
    Single2() {}
    Single2(const Single2&) = delete;
    Single2& operator= (const Single2&) = delete;
public:
    static Single2& GetInst()
    {
        static Single2 single;
        return single;
    }
};
void test_single2()
{
    std::cout << "s1 addr " << &Single2::GetInst() << std::endl;
    std::cout << "s2 addr " << &Single2::GetInst() << std::endl;
}

// 饿汉式(不好, 并不推荐, 因为遵守规则难)
// 主线程启动后, 其他线程启动钱, 由主线程初始化单例资源, 避免触发初始化
class Single2Hungry
{
    static Single2Hungry* single;
private:
    Single2Hungry() {}
    Single2Hungry(const Single2Hungry&) = delete;
    Single2Hungry& operator=(const Single2Hungry&) = delete;
public:
    static Single2Hungry* GetInst()
    {
        if (single == nullptr)
        {
            single = new Single2Hungry();
        }
        return single;
    }
};
// 饿汉式初始化
Single2Hungry* Single2Hungry::single = Single2Hungry::GetInst();
void thread_func_s2(int i)
{
    std::cout << "this thread " << i << std::endl;
    std::cout << "addr is " << Single2Hungry::GetInst() << std::endl;
}
void test_single2hungry()
{
    std::cout << " s1 addr is " << Single2Hungry::GetInst() << std::endl;
    std::cout << " s2 addr is " << Single2Hungry::GetInst() << std::endl;
    for (int i = 0; i < 3; ++i)
    {
        std::thread tid(thread_func_s2, i);
        tid.join();
    }
}

// 如果用户期望使用懒汉式就要加锁
// 但回收指针同样存在问题, 多重释放 or 不知道哪个指针释放
class SinglePoint
{
    static SinglePoint* single;
    static std::mutex s_mutex;
private:
    SinglePoint() {}
    SinglePoint(const SinglePoint&) = delete;
    SinglePoint& operator= (const SinglePoint&) = delete;
public:
    static SinglePoint* GetInst()
    {
        if (single != nullptr)
        {
            return single;
        }

        s_mutex.lock();
        if (single != nullptr)
        {
            s_mutex.unlock();
            return single;
        }
        single = new SinglePoint();
        s_mutex.unlock();
        return single;
    }
};
SinglePoint* SinglePoint::single = nullptr;
std::mutex SinglePoint::s_mutex;
void thread_func_lazy(int i)
{
    std::cout << "this is lazy thread " << i << std::endl;
    std::cout << "inst is " << SinglePoint::GetInst() << std::endl;
}
void test_singlelazy()
{
    for (int i = 0; i < 3; ++i)
    {
        std::thread tid(thread_func_lazy, i);
        tid.join();
    }
}


// 使用智能指针回收
class SingleAuto
{
    static std::shared_ptr<SingleAuto> single;
    static std::mutex s_mutex;
private:
    SingleAuto() {}
    SingleAuto(const SingleAuto&) = delete;
    SingleAuto& operator= (const SingleAuto&) = delete;
public:
    ~SingleAuto()
    {
        std::cout << "single auto delete success" << std::endl;
    }
    static std::shared_ptr<SingleAuto> GetInst()
    {
        if (single != nullptr)
        {
            return single;
        }

        s_mutex.lock();
        if (single != nullptr)
        {
            s_mutex.unlock();
            return single;
        }
        single = std::shared_ptr<SingleAuto>(new SingleAuto);
        s_mutex.unlock();
        return single;
    }
};
std::shared_ptr<SingleAuto> SingleAuto::single = nullptr;
std::mutex SingleAuto::s_mutex;
void test_singleauto()
{
    auto sp1 = SingleAuto::GetInst();
    auto sp2 = SingleAuto::GetInst();
    std::cout << "sp1 is " << sp1 << std::endl;
    std::cout << "sp2 is " << sp2 << std::endl;
    // 可以调用删除裸指针, 有潜在隐患
    delete sp1.get();
    // 将delete设置为私有函数
}

// 使用智能指针回收 且不能手动delete返回的函数
class SingleAutoSafe;
class SafeDeletor
{
public:
    void operator()(SingleAutoSafe* sf)
    {
        std::cout << "this is safe deleter operator()" << std::endl;
        delete sf;
    }
};
class SingleAutoSafe
{
    static std::shared_ptr<SingleAutoSafe> single;
    static std::mutex s_mutex;
private:
    SingleAutoSafe() {}
    SingleAutoSafe(const SingleAutoSafe&) = delete;
    SingleAutoSafe& operator= (const SingleAutoSafe&) = delete;
    ~SingleAutoSafe()
    {
        std::cout << "single autoSafe delete success" << std::endl;
    }
    //定义友元类，通过友元类调用该类析构函数
    friend class SafeDeletor;
public:
    static std::shared_ptr<SingleAutoSafe> GetInst()
    {
        // 双检测也并不稳定, 因为new 有3步, 1 分配空间 2 构造 3 赋值
        // 在某些编译器优化可能先赋值 再构造
        // 所以即使这里检测通过 返回的也是无法正常使用的地址
        if (single != nullptr)
        {
            return single;
        }

        s_mutex.lock();
        if (single != nullptr)
        {
            s_mutex.unlock();
            return single;
        }
        single = std::shared_ptr<SingleAutoSafe>(new SingleAutoSafe, SafeDeletor());
        s_mutex.unlock();
        return single;
    }
};
std::shared_ptr<SingleAutoSafe> SingleAutoSafe::single = nullptr;
std::mutex SingleAutoSafe::s_mutex;

void test_singleautosafe()
{
    auto sp1 = SingleAutoSafe::GetInst();
    auto sp2 = SingleAutoSafe::GetInst();
    std::cout << "sp1 is " << sp1 << std::endl;
    std::cout << "sp2 is " << sp2 << std::endl;
    // 私有析构 无法手动delete
    //delete sp1.get();
}

// c++ 11 call_once
class SingletonOnce {
private:
    SingletonOnce() = default;
    SingletonOnce(const SingletonOnce&) = delete;
    SingletonOnce& operator = (const SingletonOnce& st) = delete;
    static std::shared_ptr<SingletonOnce> _instance;
public:
    static std::shared_ptr<SingletonOnce> GetInstance() {
        static std::once_flag s_flag;
        std::call_once(s_flag, [&]() {
            _instance = std::shared_ptr<SingletonOnce>(new SingletonOnce);
            });
        return _instance;
    }
    void PrintAddress() {
        std::cout << _instance.get() << std::endl;
    }
    virtual ~SingletonOnce() {
        std::cout << "this is singleton destruct" << std::endl;
    }
};
std::shared_ptr<SingletonOnce> SingletonOnce::_instance = nullptr;
void TestSingle() {
    std::thread t1([]() {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        SingletonOnce::GetInstance()->PrintAddress();
        });
    std::thread t2([]() {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        SingletonOnce::GetInstance()->PrintAddress();
        });
    t1.join();
    t2.join();
}





void day5()
{
    test_single2();
    test_single2hungry();
    test_singlelazy();
}


//为了让单例更加通用，可以做成模板类
template <typename T>
class Singleton {
protected:
    Singleton() = default;
    Singleton(const Singleton<T>&) = delete;
    Singleton& operator=(const Singleton<T>& st) = delete;
    static std::shared_ptr<T> _instance;
public:
    static std::shared_ptr<T> GetInstance() {
        static std::once_flag s_flag;
        std::call_once(s_flag, [&]() {
            _instance = std::shared_ptr<T>(new T);
            });
        return _instance;
    }
    void PrintAddress() {
        std::cout << _instance.get() << std::endl;
    }
    ~Singleton() {
        std::cout << "this is singleton destruct" << std::endl;
    }
};
template <typename T>
std::shared_ptr<T> Singleton<T>::_instance = nullptr;

//想使用单例类，可以继承上面的模板，我们在网络编程中逻辑单例类用的就是这种方式
class LogicSystem :public Singleton<LogicSystem>
{
    friend class Singleton<LogicSystem>;
public:
    ~LogicSystem() {}
private:
    LogicSystem() {}
};

