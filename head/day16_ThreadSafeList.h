#include "t_include.h"

#pragma once

template<typename T>
class threadsafe_list
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
    node* last_node_ptr;
    std::mutex last_ptr_mutex;
public:
    threadsafe_list() { last_node_ptr = &head; }
    ~threadsafe_list() { remove_if([](node const&) {return true; }); }
    threadsafe_list(const threadsafe_list&) = delete;
    threadsafe_list& operator=(const threadsafe_list&) = delete;

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
                // 删除最后一个结点
                if (current->next == nullptr)
                {
                    std::lock_guard<std::mutex> last_lk(last_ptr_mutex);
                    last_node_ptr = current;
                }
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
    template<typename Predicate>
    bool remove_first(Predicate p)
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
                //判断删除的是否为最后一个节点
                if (current->next == nullptr) {
                    std::lock_guard<std::mutex> last_lk(last_ptr_mutex);
                    last_node_ptr = current;
                }
                next_lk.unlock();

                return true;
            }

            lk.unlock();
            current = next;
            lk = std::move(next_lk);
        }

        return false;
    }

    void push_front(T const& value)
    {
        std::unique_ptr<node> new_node(new node(value));
        std::unique_lock<std::mutex> lk(head.m);
        new_node->next = std::move(head.next);
        head.next = std::move(new_node);
        // 最后一个结点
        if (head.next->next == nullptr)
        {
            std::lock_guard<std::mutex> llk(last_ptr_mutex);
            last_node_ptr = head.next.get();
        }
    }
    void push_back(T const& value)
    {
        std::unique_ptr<node> new_node(new node(value));
        std::lock(last_node_ptr->m, last_ptr_mutex);
        std::unique_lock<std::mutex> lk(last_node_ptr->m, std::adopt_lock);
        std::unique_lock<std::mutex> llk(last_ptr_mutex, std::adopt_lock);
        last_node_ptr->next = std::move(new_node);
        last_node_ptr = last_node_ptr->next.get();
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

    //从断言处查询满足条件的第一个点之前插入
    template<typename Predicate>
    void insert_if(Predicate p, T const& value)
    {
        node* current = &head;
        std::unique_lock<std::mutex> lk(head.m);
        while (node* const next = current->next.get())
        {
            std::unique_lock<std::mutex> next_lk(next->m);
            if (p(*(next->data)))
            {
                std::unique_ptr<node> new_node(new node(value));

                //缓存当前节点的下一个节点作为旧节点
                auto old_next = std::move(current->next);

                //新节点指向当前节点的下一个节点
                new_node->next = std::move(old_next);
                //当前节点的下一个节点更新为新节点
                current->next = std::move(new_node);
                return;
            }
            lk.unlock();
            current = next;
            lk = std::move(next_lk);
        }
    }
};

void TestThreadSafeList()
{
    threadsafe_list<MyClass> thread_safe_list;

    std::thread t1([&]()
        {
            for (int i = 0; i < 20000; i++)
            {
                MyClass mc(i);
                thread_safe_list.push_front(mc);
                std::cout << "push front " << i << " success" << std::endl;
            }
        });

    std::thread t2([&]()
        {
            for (int i = 20000; i < 40000; i++)
            {
                MyClass mc(i);
                thread_safe_list.push_back(mc);
                std::cout << "push back " << i << " success" << std::endl;
            }
        });

    std::thread t3([&]()
        {
            for (int i = 0; i < 40000; )
            {
                bool rmv_res = thread_safe_list.remove_first([&](const MyClass& mc)
                    {
                        return mc.GetData() == i;
                    });

                if (!rmv_res)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }

                i++;
            }
        });

    t1.join();
    t2.join();
    t3.join();

    std::cout << "begin for each print...." << std::endl;
    thread_safe_list.for_each([](const MyClass& mc)
        {
            std::cout << "for each print " << mc << std::endl;
        });
    std::cout << "end for each print...." << std::endl;
}
void day16_list()
{
    TestThreadSafeList();
}
