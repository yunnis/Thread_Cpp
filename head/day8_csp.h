#include "t_include.h"
#include "day7_ThreadPool.h"

// actor 模型:
// 消除共享状态, 每次处理一条消息, actor 内部安全, 不用考虑加锁
// 例如 消息队列, 多个投递, 一个处理
// 有点指向性 A->B

// csp 模型: communication sequential process
// 描述两个独立的并发实体通过共享通信 channel 进行通信的并发模型, go天然支持
// A -> channel -> B


template <typename T>
class Channel
{
private:
    std::queue<T> queue_;
    std::mutex mtx_;
    std::condition_variable cv_producer_;
    std::condition_variable cv_consumer_;
    size_t capacity_;
    bool close_ = false;

public:
    Channel(size_t capacity = 0) : capacity_(capacity) {}

    bool send(T value)
    {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_producer_.wait(lock, [this]() {
            return (capacity_ == 0 && queue_.empty()) || queue_.size() < capacity_ || close_;
            });
        if (close_)
        {
            return false;
        }

        queue_.push(value);
        cv_consumer_.notify_one();
        return true;
    }

    bool receive(T& value)
    {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_consumer_.wait(lock, [this]() {return !queue_.empty() || close_; });
        if (close_ && queue_.empty())
        {
            return false;
        }

        value = queue_.front();
        queue_.pop();
        cv_producer_.notify_one();
        return true;
    }
    
    void close()
    {
        std::unique_lock<std::mutex> lock(mtx_);
        close_ = true;
        cv_producer_.notify_all();
        cv_consumer_.notify_all();
    }
};

void test_channel()
{
    Channel<int> ch(10);
    std::thread produce([&]() {
        //std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        for (int i = 0; i < 5; i++)
        {
            ch.send(i);
            std::cout << "send : " << i << std::endl;
        }
        ch.close();
        });

    std::thread consumer([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        int val;
        while (ch.receive(val))
        {
            std::cout << "receive: " << val << std::endl;
        }
        });
    produce.join();
    consumer.join();
}

void day8()
{
    test_channel();
}



