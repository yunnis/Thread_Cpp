#include "t_include.h"

#pragma once

template<typename T>
class lock_free_stack
{
    struct node
    {
        std::shared_ptr<T> data;
        node* next;
        node(T const& data_) :data(std::make_shared<T>(data_)) {}
    };

    std::atomic<node*> head;
    std::atomic<node*> to_be_deleted;
    std::atomic<int> threads_in_pop;

private:
    lock_free_stack(const lock_free_stack&) = delete;
    lock_free_stack& operator=(const lock_free_stack&) = delete;

public:
    lock_free_stack() {}

    void  push(const T& data)
    {
        auto new_node = new node(data);
        do
        {
            new_node->next = head.load();
        } while (!head.compare_exchange_strong(new_node->next, new_node));
        // 书中表示用weak即可, 可以提高效率
    }

    // 无法回收被pop的结点
    //void pop(T& value)
    //{
    //    do
    //    {
    //        node* old_head = head.load();
    //    } while (!head.compare_exchange_weak(old_head, old_head->next));
    //    value = old_head->data;
    //}
    std::shared_ptr<T> pop()
    {
        ++threads_in_pop;
        node* old_head = nullptr;
        do
        {
            old_head = head.load();
            if (old_head == nullptr)
            {
                --threads_in_pop;
                return nullptr;
            }
        } while (!head.compare_exchange_weak(old_head, old_head->next));

        std::shared_ptr<T> res;
        if (old_head)
        {
            res.swap(old_head->data);
        }
        try_reclaim(old_head);
        return res;
    }

    void try_reclaim(node* old_head)
    {
        if (threads_in_pop == 1)
        {
            node* nodes_to_delete = to_be_deleted.exchange(nullptr);
            if (!--threads_in_pop)
            {
                delete_nodes(nodes_to_delete);
            }
            else if (nodes_to_delete)
            {
                // 还原待删列表
                chain_pending_nodes(nodes_to_delete);
            }

            delete old_head;
        }
        else
        {
            chain_pending_node(old_head);
            --threads_in_pop;
        }
    }


    static void delete_nodes(node* nodes)
    {
        while (nodes)
        {
            node* next = nodes->next;
            delete nodes;
            nodes = next;
        }
    }

    void chain_pending_node(node* n)
    {
        chain_pending_nodes(n, n);
    }

    void chain_pending_nodes(node* first, node* last)
    {
        last->next = to_be_deleted;
        while (!to_be_deleted.compare_exchange_weak(last->next, first));
    }

    void chain_pending_nodes(node* nodes)
    {
        node* last = nodes;
        while (node* const next = last->next)
        {
            last = next;
        }
        chain_pending_nodes(nodes, last);
    }
};


void TestLockFreeStack()
{
    lock_free_stack<int> lk_free_stack;
    std::set<int> rmv_set;
    std::mutex set_mtx;

    static constexpr size_t pop_size = 10'000;
    static constexpr size_t push_size = 5 * pop_size;

    std::thread t1([&]() {
        for (int i = 0; i < push_size; ++i)
        {
            lk_free_stack.push(i);
            //std::cout << "push data " << i << "success\n";
        }
        });
    auto pop = [&]() {
        for (int i = 0; i < pop_size; )
        {
            auto head = lk_free_stack.pop();
            if (!head)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            std::lock_guard<std::mutex> lock(set_mtx);
            rmv_set.insert(*head);
            //std::cout << "pop data " << i << "success\n";
            ++i;
        }
    };
    std::thread t2(pop);
    std::thread t3(pop);
    std::thread t4(pop);
    std::thread t5(pop);
    std::thread t6(pop);

    t1.join();
    t2.join();
    t3.join();
    t4.join();
    t5.join();
    t6.join();

    assert(rmv_set.size() == push_size);
}

void day17_LockFreeStack()
{
    TestLockFreeStack();
}
