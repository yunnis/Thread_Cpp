#include "t_include.h"

// pop 和 push 使用同一个mutex, 但是操作并没有那么强的相关, pop最前的, push 在最后
// 所以头尾不等就可以单独操作
template<typename T>
class threadsafe_queue_ptr
{
private:
    mutable std::mutex mut;
    std::queue<std::shared_ptr<T>> data_queue;
    std::condition_variable data_cond;
public:
    threadsafe_queue_ptr() {};
    void wait_and_pop(T& value)
    {
        std::unique_lock<std::mutex> lk(mut);
        data_cond.wait(lk, [this] {return !data_queue.empty(); });
        value = std::move(*data_queue.front());
        data_queue.pop();
    }
    bool try_pop(T& value)
    {
        std::lock_guard<std::mutex> lk(mut);
        if (data_queue.empty())
        {
            return false;
        }
        value = std::move(*data_queue.front());
        data_queue.pop();
        return true;
    }
    std::shared_ptr<T> wait_and_pop()
    {
        std::unique_lock<std::mutex> lk(mut);
        data_cond.wait(lk, [&]() {
            return !data_queue.empty();
            });
        std::shared_ptr<T> res = data_queue.front();
        data_queue.pop();
        return res;
    }
    std::shared_ptr<T> try_pop()
    {
        std::lock_guard<std::mutex> lk(mut);
        if (data_queue.empty())
            return std::shared_ptr<T>();
        std::shared_ptr<T> res = data_queue.front();   // ⇽-- - ④
        data_queue.pop();
        return res;
    }
    void push(T& value)
    {
        std::shared_ptr<T> data(std::make_shared<T>(std::move(value)));
        std::lock_guard<std::mutex> lk(mut);
        data_queue.push(data);
        data_cond.notify_one();
    }
    bool empty() const
    {
        std::lock_guard<std::mutex> lk(mut);
        return data_queue.empty();
    }
};

// pop 和 push 使用不同的mutex
template<typename T>
class threadsafe_queue_ht
{
private:
    struct node
    {
        std::shared_ptr<T> data;
        std::unique_ptr<node> next;
    };
    std::mutex head_mutex;
    std::unique_ptr<node> head;
    std::mutex tail_mutex;
    node* tail;
    std::condition_variable data_cond;

    node* get_tail()
    {
        std::lock_guard<std::mutex> lk(tail_mutex);
        return tail;
    }
    std::unique_ptr<node> pop_head()
    {
        std::unique_ptr<node> old_head = std::move(head);
        head = std::move(old_head->next);
        return old_head;
    }
    std::unique_lock<std::mutex> wait_for_data()
    {
        std::unique_lock<std::mutex> head_lock(head_mutex);
        data_cond.wait(head_lock, [&]() {return head.get() != get_tail(); });
        return std::move(head_lock);
    }

    std::unique_ptr<node> wait_pop_head()
    {
        std::unique_lock<std::mutex> head_lock(wait_for_data());
        return pop_head();
    }
    std::unique_ptr<node> wait_pop_head(T& value)
    {
        std::unique_lock<std::mutex> head_lock(wait_for_data());
        value = std::move(*head->data);
        return pop_head();
    }
    std::unique_ptr<node> try_pop_head()
    {
        std::lock_guard<std::mutex> head_lock(head_mutex);
        if (head.get() == get_tail())
        {
            return std::unique_ptr<node>();
        }
        return pop_head();
    }
    std::unique_ptr<node> try_pop_head(T& value)
    {
        std::lock_guard<std::mutex> head_lock(head_mutex);
        if (head.get() == get_tail())
        {
            return std::unique_ptr<node>();
        }
        value = std::move(*head->data);
        return pop_head();
    }

public:
    threadsafe_queue_ht() : head(new node), tail(head.get()) {};
    threadsafe_queue_ht(const threadsafe_queue_ht&) = delete;
    threadsafe_queue_ht& operator=(const threadsafe_queue_ht&) = delete;

    std::shared_ptr<T> wait_and_pop() //  <------3
    {
        std::unique_ptr<node> const old_head = wait_pop_head();
        return old_head->data;
    }

    void wait_and_pop(T& value)  //  <------4
    {
        std::unique_ptr<node> const old_head = wait_pop_head(value);
    }
    std::shared_ptr<T> try_pop()
    {
        std::unique_ptr<node> old_head = try_pop_head();
        return old_head ? old_head->data : std::shared_ptr<T>();
    }
    bool try_pop(T& value)
    {
        std::unique_ptr<node> const old_head = try_pop_head(value);
        return old_head;
    }
    bool empty()
    {
        std::lock_guard<std::mutex> head_lock(head_mutex);
        return (head.get() == get_tail());
    }
    void push(T new_value)
    {
        std::shared_ptr<T> new_data(std::make_shared<T>(std::move(new_value)));
        std::unique_ptr<node> p(new node);
        {
            std::lock_guard<std::mutex> tl(tail_mutex);
            tail->data = new_data;
            node* const new_tail = p.get();
            tail->next = std::move(p);
            tail = new_tail;
        }

        data_cond.notify_one();
    }

};


void TestThreadSafeQue()
{
    std::mutex m_count;
    auto printS = [&m_count](std::string s, std::shared_ptr<int> p) {
        std::lock_guard<std::mutex> lk(m_count);
        std::cout << s << " pop data , data is " << *p << std::endl;
    };

    threadsafe_queue_ptr<int> safe_que;
    std::thread consumer1(
        [&]()
        {
            for (;;)
            {
                std::shared_ptr<int> data = safe_que.wait_and_pop();
                printS("consumer1", data);
            }
        }
    );

    std::thread consumer2([&]()
        {
            for (;;)
            {
                std::shared_ptr<int> data = safe_que.wait_and_pop();
                printS("consumer2", data);
            }
        });

    std::thread producer([&]()
        {
            for (int i = 0; i < 100; i++)
            {
                int mc(i);
                safe_que.push((mc));
            }
        });

    consumer1.join();
    consumer2.join();
    producer.join();
}


void TestThreadSafeQueHt()
{
    std::mutex m_count;
    auto printS = [&m_count](std::string s, std::shared_ptr<int> p) {
        std::lock_guard<std::mutex> lk(m_count);
        std::cout << s << " pop data , data is " << *p << std::endl;
    };

    threadsafe_queue_ht<int> safe_que;
    std::thread consumer1(
        [&]()
        {
            for (;;)
            {
                std::shared_ptr<int> data = safe_que.wait_and_pop();
                printS("consumer1", data);
            }
        }
    );

    std::thread consumer2([&]()
        {
            for (;;)
            {
                std::shared_ptr<int> data = safe_que.wait_and_pop();
                printS("consumer2", data);
            }
        });

    std::thread producer([&]()
        {
            for (int i = 0; i < 100; i++)
            {
                int mc(i);
                safe_que.push(std::move(mc));
            }
        });

    consumer1.join();
    consumer2.join();
    producer.join();
}



