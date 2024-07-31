#include "t_include.h"

#pragma once

template<typename T>
class threadsafe_forwardlist
{
    struct node
    {
        std::mutex m;
        std::shared_ptr<T> data;
        std::unique_ptr<node> next;
        node() :next() {};
        node(T const& value) :data(std::make_shared<T>(value)) {};
    };

    node head;
public:
    threadsafe_forwardlist() {}
    ~threadsafe_forwardlist() { remove_if([](node const&) {return true; }); }
    threadsafe_forwardlist(const threadsafe_forwardlist&) = delete;
    threadsafe_forwardlist& operator=(const threadsafe_forwardlist&) = delete;

    template<typename Predicate>
    void remove_if(Predicate p)
    {
        node* current = &head;
        std::unique_lock<std::mutex> lk(head.m);
        while (node* const next = current->next.get())
        {
            std::unique_lock<std::mutex> next_lk(next->m);
            if (p(*next->data))
            {
                std::unique_ptr<node> old_next = std::move(current->next);
                current->next = std::move(next->next);
                next_lk.unlock();
            }
            else
            {
                lk.unlock();
                current = next;
                lk = std::move(next_lk);
            }
        }
    }

    void push_front(T const& value)
    {
        std::unique_ptr<node> new_node(new node(value));
        std::unique_lock<std::mutex> lk(head.m);
        new_node->next = std::move(head.next);
        head.next = std::move(new_node);
    }

    template<typename Predicate>
    std::shared_ptr<T> find_first_if(Predicate p)
    {
        node* current = &head;
        std::unique_lock<std::mutex> lk(head.m);
        while (node* const next = current->next.get())
        {
            std::unique_lock<std::mutex> next_lk(next->m);
            lk.unlock();
            if (p(*next->data))
            {
                return next->data;
            }
            current = next;
            lk = std::move(next_lk);
        }
        return std::shared_ptr<T>();
    }

    template<typename Function>
    void for_each(Function f)
    {
        node* cur = &head;
        std::unique_lock<std::mutex> lk(head.m);
        while (node* next = cur->next.get())
        {
            std::unique_lock<std::mutex> next_lk(next->m);
            lk.unlock();
            f(*next->data);
            cur = next;
            lk = std::move(next_lk);
        }
    }
};

void TestThreadSafeForwardList()
{
    std::set<int> removeSet;
    threadsafe_forwardlist<MyClass> thread_safe_forwardlist;
    std::thread t1([&]() {
        for (unsigned int i = 0; i < 100; ++i)
        {
            MyClass mc(i);
            thread_safe_forwardlist.push_front(mc);
        }
        });

    std::thread t2([&]() {
        for (unsigned int i=0;i<100;)
        {
            auto find_res = thread_safe_forwardlist.find_first_if([&](auto& mc) {
                return mc.GetData() == i;
                });
            if (find_res == nullptr)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            removeSet.insert(i);
            ++i;
        }
        });

    t1.join();
    t2.join();


    std::this_thread::sleep_for(std::chrono::seconds(3));
    std::cout << __FUNCTION__ << "removeSet size " << removeSet.size()<<std::endl;
}

void day16_forwardlist()
{
    TestThreadSafeForwardList();
}
