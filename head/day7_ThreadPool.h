#include <atomic>
#include <condition_variable>
#include <future>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#pragma once

class ThreadPool 
{
public:
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator = (const ThreadPool&) = delete;

    // 单例
    static ThreadPool& instance()
    {
        static ThreadPool ins;
        return ins;
    }

    using Task = std::packaged_task<void()>;
    ~ThreadPool()
    {
        stop();
    }

    template<class F, class... Args>
    auto commit(F&& f, Args&&... args) -> std::future<decltype(f(args...))> {
        using RetType = decltype(f(args...));
        if (stop_.load())
            return std::future<RetType>{};
        // 生成一个无参的函数
        auto task = std::make_shared<std::packaged_task<RetType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));

        std::future<RetType> ret = task->get_future();
        {
            std::lock_guard<std::mutex> cv_mt(cv_mt_);
            tasks_.emplace([task] { (*task)(); });
        }
        cv_lock_.notify_one();

        return ret;
    }

    int idelThreadCount()
    {
        return thread_num_;
    }

private:
    ThreadPool(unsigned int num = 5) : stop_(false)
    {
        thread_num_ = num < 1 ? 1 : num;
        start();
    }

    void start()
    {
        for (int i = 0; i < thread_num_; ++i)
        {
            pool_.emplace_back([this]() {
                while (!this->stop_.load())
                {
                    Task task;
                    {
                        std::unique_lock<std::mutex> cv_mt(cv_mt_);
                        this->cv_lock_.wait(cv_mt, [this]() {
                            return this->stop_.load() || !this->tasks_.empty();
                            });
                        if (this->tasks_.empty())
                        {
                            return;
                        }

                        task = std::move(this->tasks_.front());
                        this->tasks_.pop();
                    }
                    --this->thread_num_;
                    task();
                    ++this->thread_num_;
                }
                });
        }
    }

    void stop()
    {
        stop_.store(true);
        cv_lock_.notify_all();
        for (auto& td : pool_)
        {
            if (td.joinable())
            {
                std::cout << "join able thread " << td.get_id() << std::endl;
                td.join();
            }
        }
    }
private:
    std::mutex  cv_mt_;
    std::condition_variable cv_lock_;
    std::atomic_bool stop_;
    std::atomic_int thread_num_;
    std::queue<Task> tasks_;
    std::vector<std::thread> pool_;
};

void ThreadPoolTest()
{
    int m;
    ThreadPool::instance().commit([](int& m) {
        m = 1024;
        std::cout << "inner set m to " << m << std::endl;
        std::cout << "m addr is " << &m << std::endl;
        }, m);

    std::this_thread::sleep_for(std::chrono::seconds(3));
    std::cout << "outer m is " << m << std::endl;
    std::cout << "outer m addr is " << &m << std::endl;

    ThreadPool::instance().commit([](int& m) {
        m = 1024;
        std::cout << "inner set m to " << m << std::endl;
        std::cout << "m addr is " << &m << std::endl;
        }, std::ref(m));

    std::this_thread::sleep_for(std::chrono::seconds(3));
    std::cout << "outer m is " << m << std::endl;
    std::cout << "outer m addr is " << &m << std::endl;

}



// using thread pool quick sort
template <typename T>
void quick_sort(T arr[], int start, int end)
{
    if (start >= end)
    {
        return;
    }
    auto key = arr[start];
    auto left = start;
    auto right = end;
    while (left < right)
    {
        while (arr[right] >= key && left < right)            --right;
        while (arr[left] <= key && left < right)             ++left;
        std::swap(arr[left], arr[right]);
    }

    // 左右相遇 在判断一次
    if (arr[left] < key)
    {
        std::swap(arr[left], arr[start]);
    }

    quick_sort(arr, start, left - 1);
    quick_sort(arr, left + 1, end);
}

    constexpr int SIZE = 10'000;  // 0.124341
void test_quick_sort()
{
    std::vector<int> v(SIZE, 0);
    for (int i = 0; i < SIZE; ++i)
    {
        v[i] = std::rand() % SIZE;
    }

    //constexpr int SIZE = 1'0000'0000; // 栈溢出
    //constexpr int SIZE = 5000'0000; // 55.506
    //constexpr int SIZE = 1000'0000; // 3.03629
    //constexpr int SIZE = 100'0000;  // 0.124341
    //constexpr int SIZE = 10'0000;   // 0.0099723
    //constexpr int SIZE = 1'0000;    // 0.0008479
    //constexpr int SIZE = 1000;      // 0.000075
    //constexpr int SIZE = 100;       // 0.0000157
    //constexpr int SIZE = 10;        // 0.0000137
    //constexpr int SIZE = 1;         // 0.0000077
    auto beginTime{ std::chrono::steady_clock::now() };
    quick_sort(v.data(), 0, SIZE-1);
    auto endTime{ std::chrono::steady_clock::now() };
    std::chrono::duration<double> duration{endTime - beginTime};
    std::cout << "vector quick sort count " << SIZE << " use time " << duration.count() << std::endl;

    //for (auto i : v)
    //{
    //    std::cout << i << " ";
    //}
}

template <typename T>
std::list<T> sequential_quick_sort(std::list<T> input)
{
    if (input.empty())
    {
        return input;
    }
    std::list<T> result;
    // 1 input 第一个元素放入 result, 并从input中删除
    result.splice(result.begin(), input, input.begin());
    // 2 取result的第一个元素, 用这个元素做切割, 切割input中的列表
    T const& pivot = *result.begin();
    // std::partition : 将容器或数组中的元素按指定条件分区
    // 满足的元素排在不满足的之前
    auto divide_point = std::partition(input.begin(), input.end(),
        [&](T const& t) {return t < pivot; });
    // 4 小于pivot的元素放到 lower_part 中
    std::list<T> lower_part;
    lower_part.splice(lower_part.end(), input, input.begin(), divide_point);
    // 5 lower_part 调用本身 返回一个新的有序序列
    auto new_lower(sequential_quick_sort(std::move(lower_part)));
    // 6 剩余的input列表调用本身, 处理大于divide_point的部分
    auto new_higher(sequential_quick_sort(std::move(input)));
    // 7 lower 和 higher 都已排好序 需要合并
    result.splice(result.end(), new_higher);
    result.splice(result.begin(), new_lower);
    return result;
}
template <typename T>
std::list<T> parallel_quick_sort(std::list<T> input)
{
    if (input.empty())
    {
        return input;
    }
    std::list<T> result;
    // 1 input 第一个元素放入 result, 并从input中删除
    result.splice(result.begin(), input, input.begin());
    // 2 取result的第一个元素, 用这个元素做切割, 切割input中的列表
    T const& pivot = *result.begin();
    // std::partition : 将容器或数组中的元素按指定条件分区
    // 满足的元素排在不满足的之前
    auto divide_point = std::partition(input.begin(), input.end(),
        [&](T const& t) {return t < pivot; });
    // 4 小于pivot的元素放到 lower_part 中
    std::list<T> lower_part;
    lower_part.splice(lower_part.end(), input, input.begin(), divide_point);

    // 5 lower_part 是副本, 所以并行安全
    std::future<std::list<T>> new_lower(
        std::async(&parallel_quick_sort<T>, std::move(lower_part)));

    auto new_higher(parallel_quick_sort(std::move(input)));
    result.splice(result.end(), new_higher);
    result.splice(result.begin(), new_lower.get());

    return result;
}
template<typename T>
std::list<T> thread_pool_quick_sort(std::list<T> input)
{
    if (input.empty())
    {
        return input;
    }
    std::list<T> result;
    // 1 input 第一个元素放入 result, 并从input中删除
    result.splice(result.begin(), input, input.begin());
    // 2 取result的第一个元素, 用这个元素做切割, 切割input中的列表
    T const& pivot = *result.begin();
    // std::partition : 将容器或数组中的元素按指定条件分区
    // 满足的元素排在不满足的之前
    auto divide_point = std::partition(input.begin(), input.end(),
        [&](T const& t) {return t < pivot; });
    // 4 小于pivot的元素放到 lower_part 中
    std::list<T> lower_part;
    lower_part.splice(lower_part.end(), input, input.begin(), divide_point);

    // 这种做法线程任务太多, 应该将数据分成线程数 的个数, 每个线程去处理一块, 而不是疯狂给线程加任务
    auto new_lower = ThreadPool::instance().commit(&parallel_quick_sort<T>, std::move(lower_part));

    auto new_higher(parallel_quick_sort(std::move(input)));
    result.splice(result.end(), new_higher);
    result.splice(result.begin(), new_lower.get());

    return result;
}

void test_sequential_quick_sort()
{
    //constexpr int SIZE = 100'0000;  // 7.29227
    //constexpr int SIZE = 1'000;     // 0.006505
    //constexpr int SIZE = 1'00;      // 0.0007345
    std::list<int> input;
    for (int i = 0; i < SIZE; ++i)
    {
        input.push_back(std::rand() % SIZE);
    }
    auto beginTime{ std::chrono::steady_clock::now() };
    auto res = sequential_quick_sort(input);
    auto endTime{ std::chrono::steady_clock::now() };
    std::chrono::duration<double> duration{endTime - beginTime};
    std::cout << "list sequential quick sort count " << SIZE << " use time " << duration.count() << std::endl;
}
void test_parallel_quick_sort()
{
    //constexpr int SIZE = 100'0000;  // too long
    //constexpr int SIZE = 1'000;     // 0.100614
    //constexpr int SIZE = 1'00;      // 0.0121737
    std::list<int> input;
    for (int i = 0; i < SIZE; ++i)
    {
        input.push_back(std::rand() % SIZE);
    }
    auto beginTime{ std::chrono::steady_clock::now() };
    auto res = parallel_quick_sort(input);
    auto endTime{ std::chrono::steady_clock::now() };
    std::chrono::duration<double> duration{endTime - beginTime};
    std::cout << "list parallel quick sort count " << SIZE << " use time " << duration.count() << std::endl;
}
void test_thread_pool_quick_sort()
{
    //constexpr int SIZE = 100'0000;  // 7.29227
    //constexpr int SIZE = 1'000;     // 0.042984
    //constexpr int SIZE = 1'00;      // 0.0121737
    std::list<int> input;
    for (int i = 0; i < SIZE; ++i)
    {
        input.push_back(std::rand() % SIZE);
    }
    auto beginTime{ std::chrono::steady_clock::now() };
    auto res = thread_pool_quick_sort(input);
    auto endTime{ std::chrono::steady_clock::now() };
    std::chrono::duration<double> duration{endTime - beginTime};
    std::cout << "list thread pool quick sort count " << SIZE << " use time " << duration.count() << std::endl;
}


void day7_pool()
{
    //ThreadPoolTest();
    test_quick_sort();
    test_sequential_quick_sort();
    //test_parallel_quick_sort();
    test_thread_pool_quick_sort();
}

