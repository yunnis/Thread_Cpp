#include "t_include.h"

void thread_work1(std::string str)
{
    std::cout << "str is " << str << std::endl;
}

class background_task
{
public:
    void operator()()
    {
        std::cout << "background_task called" << std::endl;
    }
};

struct func
{
    func(int& i) : _i(i) {}

    int& _i;

    void operator()()
    {
        for (int i = 0; i < 3; ++i)
        {
            _i = i;
            std::cout << "_i is " << _i << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
};

void oops()
{
    int local_state = 0;
    func myfunc(local_state);
    std::thread t(myfunc);
    // 隐患, 访问了其他线程的局部变量
    t.detach();
}
void oops_use_join()
{
    int local_state = 0;
    func myfunc(local_state);
    std::thread t(myfunc);
    t.join();
}
void oops_catch_exception()
{
    int local_state = 0;
    func myfunc(local_state);
    std::thread t(myfunc);
    try
    {
        // do someting
    }
    catch (std::exception* e)
    {
    	// 核心线程继续做自己的事 (写DB 写日志...)
        t.join();
        throw;
    }

    t.join();
}

class thread_guard
{
public:
    explicit thread_guard(std::thread& t) : _t(t) {}
    ~thread_guard()
    {
        if (_t.joinable())
        {
            _t.join();
        }
    }

    thread_guard(const thread_guard&) = delete;
    thread_guard& operator=(const thread_guard&) = delete;

private:
    std::thread& _t;
};
void auto_guard()
{
    int local_state = 0;
    func myfunc(local_state);
    std::thread t(myfunc);
    thread_guard g(t);
    // do someting
    std::cout << "auto guard finished " << std::endl;
}

void print_str(int i, std::string const& s)
{
    std::cout << " i : " << i << " str: " << s << std::endl;
}
void danger_oops(int som_param)
{
    char buffer[1024];
    sprintf_s(buffer, "%i", som_param);
    // 线程内部将char* 转为std::string
    // 先作为变量存着, 调用的时候可能不存在
    std::thread t(print_str, 3, buffer);
    t.detach();
    std::cout << "danger opps finished" << std::endl;
}
void safe_oops(int som_param)
{
    char buffer[1024];
    sprintf_s(buffer, "%i", som_param);
    std::thread t(print_str, 3, std::string(buffer));
    t.detach();
    std::cout << "safe opps finished" << std::endl;
}
void change_param(int& param)
{
    ++param;
}
void ref_oops(int som_param)
{
    std::cout << "before change param is " << som_param << std::endl;
    std::thread t(change_param, std::ref(som_param));
    t.join();
    std::cout << "after change param is " << som_param << std::endl;
}

class X
{
public:
    void do_lengthy_work()
    {
        std::cout << __FUNCTION__ << std::endl;
    }
};
void bind_class_oops()
{
    X x;
    std::thread t(&X::do_lengthy_work, &x);
    t.join();
}

void deal_unique(std::unique_ptr<int> p)
{
    std::cout << "before unique ptr is " << *p << std::endl;
    (*p)++;
    std::cout << "after unique ptr is " << *p << std::endl;
}
void move_oops()
{
    auto p = std::make_unique<int>(100);
    std::thread t(deal_unique, std::move(p));
    t.join();
    // p 已经不能再使用了, 已经被move废弃
}

void day1()
{
    auto str = "Hello World!\n";
    // 普通函数
    std::thread t1(thread_work1, str);
    t1.join();

    // 仿函数
    // t2 是一个函数对象 而不是一个线程
    std::thread x2(background_task());
    // 可以通过加一层() 或者使用初始化列表
    std::thread t2{background_task()};
    t2.join();

    // lambda
    std::thread t3([](std::string str)
        {
            std::cout << "lambda str is " << str << std::endl;
        }, str);
    t3.join();

    // detach
    oops();
    // 防止主线程退出太快
    std::this_thread::sleep_for(std::chrono::seconds(1));
    // 解决方案: 1传智能指针 2使用join
    oops_use_join();
    // catch
    oops_catch_exception();
    // raii guard
    auto_guard();



}
