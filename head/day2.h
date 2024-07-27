#include "t_include.h"

void some_function()
{
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
void some_other_function()
{
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
void dangerous_use()
{
    std::thread t1(some_function);
    // 转移给t2, t1无效
    std::thread t2 = std::move(t1);
    //
    t1 = std::thread(some_other_function);
    std::thread t3;
    t3 = std::move(t2);
    t1 = std::move(t3);
    std::this_thread::sleep_for(std::chrono::seconds(2000));
}

std::thread f()
{
    return std::thread(some_function);
}
std::thread g()
{
    auto t = std::thread(some_function);
    return t;
}

void param_function(int a)
{
    while (true)
    {
        std::cout << "param is " << a << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

class joining_thread
{
    std::thread _t;
public:
    joining_thread() noexcept = default;
    template<typename Callable, typename... Args>
    explicit joining_thread(Callable&& func, Args&& ...args)
        : _t(std::forward<Callable>(func), std::forward<Args>(args)...) {}
    explicit joining_thread(std::thread t) noexcept
        : _t(std::move(t)) {}
    joining_thread(joining_thread&& other) noexcept
        : _t(std::move(other._t)) {}
    joining_thread& operator=(joining_thread&& other) noexcept
    {
        if (joinable())
        {
            join();
        }

        _t = std::move(other._t);
        return *this;
    }

    joining_thread& operator=(std::thread other) noexcept
    {
        if (joinable())
        {
            join();
        }

        _t = std::move(other);
        return *this;
    }

    ~joining_thread() noexcept
    {
        if (joinable())
        {
            join();
        }
    }

    void swap(joining_thread& other) noexcept
    {
        _t.swap(other._t);
    }

    std::thread::id get_id()const noexcept
    {
        return _t.get_id();
    }
    bool joinable()
    {
        return _t.joinable();
    }
    void join()
    {
        _t.join();
    }
    void detach()
    {
        _t.detach();
    }

    std::thread& as_thread() noexcept
    {
        return _t;
    }
    const std::thread& as_thread() const noexcept
    {
        return _t;
    }
};

void use_jointhread()
{
    joining_thread j1([](int index) {
        for (int i = 0; i < index; ++i)
        {
            std::cout << "in thread id " << std::this_thread::get_id() << " cur index is " << i << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        }, 10);

    joining_thread j2(std::thread( [](int index) {
        for (int i = 0; i < index; ++i)
        {
            std::cout << "in thread id " << std::this_thread::get_id() << " cur index is " << i << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        }, 10) );

    joining_thread j3(std::thread( [](int index) {
        for (int i = 0; i < index; ++i)
        {
            std::cout << "in thread id " << std::this_thread::get_id() << " cur index is " << i << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        }, 10) );

    auto t1 = std::thread([](int index) {
        for (int i = 0; i < index; ++i)
        {
            std::cout << "in thread id " << std::this_thread::get_id() << " cur index is " << i << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        }, 10);

    j2 = std::move(t1);
    // 上面调完会join(), 所以下面要等t1 结束才会执行
    j1 = std::move(j3);
}

void use_vector()
{
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i)
    {
        // 使用的完美转发
        threads.emplace_back(param_function, i);
        //threads.push_back(param_function, i);
    }

    for (auto& entry : threads)
    {
        entry.join();
    }
}

class x
{
    // 禁止栈
    x() = default;
    ~x() = default;
    // 禁止堆
    void* operator new(size_t) { return nullptr; };
public:
    static x* cc()
    {
        return new x;
    }
};

template<typename Iterator, typename T>
struct accumulate_block
{
    void operator()(Iterator first, Iterator last, T& result)
    {
        result = std::accumulate(first, last, result);
    }
};
template<typename Iterator, typename T>
T parallel_accumulate(Iterator first, Iterator last, T init)
{
    unsigned int const length = (unsigned int)std::distance(first, last);
    if (!length)
    {
        return init;
    }
    unsigned int const min_per_thread = 25;
    unsigned int const max_threads =
        (length + min_per_thread - 1) / min_per_thread;    //⇽-- - ②
    unsigned int const hardware_threads =
        std::thread::hardware_concurrency();
    unsigned int const num_threads =
        std::min(hardware_threads != 0 ? hardware_threads : 2, max_threads);    //⇽-- - ③
    unsigned int const block_size = length / num_threads;    //⇽-- - ④
    std::vector<T> results(num_threads);
    std::vector<std::thread>  threads(num_threads - 1);   // ⇽-- - ⑤  主线程-1
    Iterator block_start = first;
    for (unsigned long i = 0; i < (num_threads - 1); ++i)
    {
        Iterator block_end = block_start;
        std::advance(block_end, block_size);    //⇽-- - ⑥
        threads[i] = std::thread(//⇽-- - ⑦
            accumulate_block<Iterator, T>(),
            block_start, block_end, std::ref(results[i]));
        block_start = block_end;    //⇽-- - ⑧
    }
    accumulate_block<Iterator, T>()(
        block_start, last, results[num_threads - 1]);    //⇽-- - ⑨
    for (auto& entry : threads)
        entry.join();    //⇽-- - ⑩
    return std::accumulate(results.begin(), results.end(), init);    //⇽-- - ⑪
}
void use_parallel_acc()
{
    std::vector<int> vec(10000);
    for (int i = 0; i < 10000; ++i)
    {
        vec.push_back(i);
    }

    int sum = 0;
    sum = parallel_accumulate<std::vector<int>::iterator, int>(vec.begin(), vec.end(), sum);

    std::cout << " sum is " << sum << std::endl;
}

void day2()
{
    //dangerous_use();
    //use_jointhread();
    use_parallel_acc();
}
