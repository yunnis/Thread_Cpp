#include "t_include.h"
#include "day7_ThreadPool.h"

#pragma once

// async 异步执行函数的模板函数, 返回一个 future 对象, 用于获取

// async
// 一个异步任务
std::string fetchDataFromDB(std::string query)
{
// 模拟长耗时
    std::this_thread::sleep_for((std::chrono::seconds(3)));
    return "Data: " + query;
}
void use_async()
{
    std::future<std::string> resultFromDB = std::async(std::launch::async, fetchDataFromDB, "GetItems");

    std::cout << "Doing something else..." << std::endl;

    // 阻塞获取数据
    std::string dbData = resultFromDB.get();
    std::cout << dbData << std::endl;
}


int my_task()
{
    std::this_thread::sleep_for(std::chrono::seconds(3));
    std::cout << "task run 3s" << std::endl;
    return 37;
}
// packaged_task 是一个可调用目标, 包装一个任务, 可以在另一个线程运行, 可以使用 future 获取返回值或异常
// 与async 不同的是, 自己可以指定线程, 比如配合线程池
void use_package()
{
    // 包装任务的 packaged_task 对象 
    std::packaged_task<int()> task(my_task);
    // 获取与任务关联的 future 对象
    std::future<int> ret = task.get_future();

    // 另一个线程执行任务
    std::thread t1(std::move(task));
    t1.detach();

    int value = ret.get();
    std::cout << "task result is " << value << std::endl;
}


// promise 用与在某一线程中设置某个值或异常 future 在另一线程中获取这个值或异常
// 不需要通过return 可以在中间就返回出来
void set_value(std::promise<int> prom)
{
    std::this_thread::sleep_for(std::chrono::seconds(3));
    prom.set_value(10);
    std::cout << " promise set value success, value : " << 10 << std::endl;
}
void use_promise()
{
    std::promise<int> prom;
    // 获取与 promise 关联的 future 对象
    std::future<int> fut = prom.get_future();
    // 新线程中设置promise的值
    std::thread t(set_value, std::move(prom));
    // future 没有拷贝
    //std::thread t(set_value, (prom));
    // 主线程中获取 future 的值
    std::cout << "waiting ffor the thread to set the value...\n";
    std::cout << "value sset by the thread : " << fut.get() << '\n';
    t.join();
}

void set_exception(std::promise<void> prom)
{
    try
    {
        throw std::runtime_error("an error occurred!");
    }
    catch (...)
    {
        prom.set_exception(std::current_exception());
    }
}
void use_promise_exception()
{
    std::promise<void> prom;
    std::future<void> fut = prom.get_future();
    std::thread t(set_exception, std::move(prom));
    // 可以设置异常 但是一定一定要捕获异常, 要不然外面的线程就崩了
    try {
        std::cout << "waiting for the thread to set the exception....\n";
        fut.get();
    }
    catch (const std::exception& e)
    {
        std::cout << "exception  set by the thread : " << e.what() << '\n';
    }

    t.join();
}

// shared_future 多个线程等待一个执行结果
void myFunction(std::promise<int>&& promise)
{
    std::this_thread::sleep_for(std::chrono::seconds(1));
    promise.set_value(37);
}
void threadFunction(std::shared_future<int> future)
{
    try {
        int res = future.get();
        std::cout << "res : " << res << std::endl;
    }
    catch (const std::future_error& e)
    {
        std::cout << "future error: " << e.what() << std::endl;
    }
}
void use_shared_future()
{
    std::promise<int> prom;
    // 隐式转换成 shared_future
    std::shared_future<int> fut = prom.get_future();
    std::thread t1(myFunction, std::move(prom));

    std::thread t2(threadFunction, fut);
    std::thread t3(threadFunction, fut);

    t1.join();
    t2.join();
    t3.join();
}
void use_shared_future_error()
{
    std::promise<int> prom;
    // 隐式转换成 shared_future
    std::shared_future<int> fut = prom.get_future();
    std::thread t1(myFunction, std::move(prom));

    std::thread t2(threadFunction, std::move(fut));
    std::thread t3(threadFunction, std::move(fut));

    t1.join();
    t2.join();
    t3.join();
}

// 如果
void may_throw()
{
    // 这里我们抛出一个异常。在实际的程序中，这可能在任何地方发生。
    throw std::runtime_error("Oops, something went wrong!");
}
int catch_async_exception()
{
    // 创建一个异步任务
    std::future<void> result(std::async(std::launch::async, may_throw));
    try
    {
        // 获取结果（如果在获取结果时发生了异常，那么会重新抛出这个异常）
        result.get();
    }
    catch (const std::exception& e)
    {
        // 捕获并打印异常
        std::cerr << "Caught exception: " << e.what() << std::endl;
    }
    return 0;
}


void day7()
{
    //use_async();
    //use_package();
    //use_promise();
    //use_promise_exception();
    //use_shared_future();
    //use_shared_future_error();
    catch_async_exception();
}


