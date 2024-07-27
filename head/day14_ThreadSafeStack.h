#include "t_include.h"
#include "day14_ThreadSafeQue.h"

namespace {
    struct empty_stack : std::exception
    {
        const char* what() const throw();
    };

    template<typename T>
    class threadsafe_stack
    {
    private:
        std::stack<T> data;
        mutable std::mutex m;
    public:
        threadsafe_stack() {}
        threadsafe_stack(const threadsafe_stack& other)
        {
            std::lock_guard<std::mutex> lock(other.m);
            data = other.data;
        }

        threadsafe_stack& operator=(const threadsafe_stack&) = delete;

        void push(T new_value)
        {
            std::lock_guard<std::mutex> lock(m);
            data.push(new_value); // 1 内存不足 push失败 stack处理了, 不会导致已有数据异常
        }

        std::shared_ptr<T> pop()
        {
            std::lock_guard<std::mutex> lk(m);
            if (data.empty())
            {
                throw empty_stack();
            }
            std::shared_ptr<T> const res(std::make_shared<T>(std::move(data.top()))); // 潜在风险: 堆空间不足
            data.pop();
            return res;
        }
        void pop(T& value)
        {
            std::lock_guard<std::mutex> lk(m);
            if (data.empty())
            {
                throw empty_stack();
            }
            value = std::move(data.top());
            data.pop();
        }

        bool empty()
        {
            std::lock_guard<std::mutex> lk(m);
            return (data.empty());
        }
    };

    // 解决空时抛异常
    template<typename T>
    class threadsafe_stack_waitable
    {
    private:
        std::stack<T> data;
        mutable std::mutex m;
        std::condition_variable cv;
    public:
        threadsafe_stack_waitable() {}
        threadsafe_stack_waitable(const threadsafe_stack_waitable& other)
        {
            std::lock_guard<std::mutex> lk(m);
            data = other.data;
        }
        threadsafe_stack_waitable& operator=(const threadsafe_stack_waitable&) = delete;

        void push(T new_value)
        {
            std::lock_guard<std::mutex> lk(m);
            data.push(std::move(new_value));
            cv.notify_one();
        }

        std::shared_ptr<T> wait_and_pop()
        {
            std::unique_lock<std::mutex> lk(m);
            cv.wait(lk, [this]() {
                return !data.empty();
                });

            std::shared_ptr<T> const res(std::make_shared<T>(std::move(data.top()))); //  还是会内存不足, 或者move有问题
            data.pop();
            return res;
        }

        void wait_and_pop(T& value)
        {
            std::unique_lock<std::mutex> lk(m);
            cv.wait(lk, [this]() {
                return !data.empty();
                });
            value = std::move(data.top());
            data.pop();
        }

        bool empty()
        {
            std::lock_guard<std::mutex> lk(m);
            return data.empty();
        }

        std::shared_ptr<T> try_pop()
        {
            std::lock_guard<std::mutex> lk(m);
            if (data.empty())
            {
                return std::shared_ptr<T>();
            }
            std::shared_ptr<T> const res(std::make_shared<T>(std::move(data.top()))); // 潜在风险: 堆空间不足
            // 如果上面出现异常, 这个可能会pop不出去
            data.pop();
            return res;
        }
        bool try_pop(T& value)
        {
            std::lock_guard<std::mutex> lk(m);
            if (data.empty())
            {
                return false;
            }
            value = std::move(data.top());
            data.pop();
            return true;
        }

    };
}

void TestThreadSafeStack()
{
    std::mutex m_count;
    auto printS = [&m_count](std::string s, std::shared_ptr<int> p) {
        std::lock_guard<std::mutex> lk(m_count);
        std::cout << s << " pop data , data is " << *p << std::endl;
    };
    threadsafe_stack_waitable<int> s ;

    std::thread consumer1(
        [&]() {
            while (1)
            {
                std::shared_ptr<int> d = s.wait_and_pop();
                printS("consumer1", d);
            }
        }
    );
    std::thread consumer2(
        [&]() {
            while (1)
            {
                std::shared_ptr<int> d = s.wait_and_pop();
                printS("consumer2", d);
            }
        }
    );
    std::thread produce(
        [&]() {
            for (int i = 0; i < 100; ++i)
            {
                s.push(i);
            }
        }
    );

    consumer1.join();
    consumer2.join();
    produce.join();

}

void day14_ThreadSafeStack()
{
    //TestThreadSafeStack();

    //TestThreadSafeQue();
    TestThreadSafeQueHt();
}
