#pragma once
#include <condition_variable>
#include <mutex>
#include <queue>
#include <atomic>
#include <memory>

// ** **
// ** 这是有安全问题的单计数版本
// ** **
template<typename T>
class single_ref_stack {
public:
    single_ref_stack() {

    }

    ~single_ref_stack() {
        //循环出栈
        while (pop());
    }

    void push(T const& data) {
        auto new_node = ref_node(data);
        new_node._node_ptr->_next = head.load();
        while (!head.compare_exchange_weak(new_node._node_ptr->_next, new_node));
    }

    std::shared_ptr<T> pop() {
        ref_node old_head = head.load();
        for (;;) {
            //1 只要执行pop就对引用计数+1并更新到head中
            ref_node new_head;
            do {
                new_head = old_head;
                new_head._ref_count += 1;
            } while (!head.compare_exchange_weak(old_head, new_head));

            old_head = new_head;

            auto* node_ptr = old_head._node_ptr;
            if (node_ptr == nullptr) {
                return  std::shared_ptr<T>();
            }

            //2 比较head和old_head想等则交换否则说明head已经被其他线程更新
            if (head.compare_exchange_strong(old_head, node_ptr->_next)) {

                //要返回的值
                std::shared_ptr<T> res;
                //交换智能指针
                res.swap(node_ptr->_data);

                //增加的数量
                int increase_count = old_head._ref_count - 2;

                if (node_ptr->_dec_count.fetch_add(increase_count) == -increase_count) {
                    delete node_ptr;
                }

                return res;
            }
            else {
                if (node_ptr->_dec_count.fetch_sub(1) == 1) {
                    delete node_ptr;
                }
            }
        }
    }

private:
    struct ref_node;
    struct node {
        //1 数据域智能指针
        std::shared_ptr<T>  _data;
        //2  下一个节点
        ref_node _next;
        node(T const& data_) : _data(std::make_shared<T>(data_)) {}

        //减少的数量
        std::atomic<int>  _dec_count;
    };

    struct ref_node {
        // 引用计数
        int _ref_count;

        node* _node_ptr;
        ref_node(T const& data_) :_node_ptr(new node(data_)), _ref_count(1) {}

        ref_node() :_node_ptr(nullptr), _ref_count(0) {}
    };

    //头部节点
    std::atomic<ref_node> head;
};

void TestSingleRefStack() {
    single_ref_stack<int> single_ref_stack;
    std::set<int>  rmv_set;
    std::mutex set_mtx;

    static constexpr size_t pop_size = 10'000;
    static constexpr size_t push_size = 5 * pop_size;

    std::thread t1([&]() {
        auto now = std::chrono::steady_clock::now();
        for (int i = 0; i < push_size; i++) {
            single_ref_stack.push(i);
            //std::cout << "push data " << i << " success!" << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        std::chrono::duration<double> dif = (std::chrono::steady_clock::now() - now);
        std::cout << "pop end use time: " << dif.count() << " rmv_set size: " << rmv_set.size();

        });

    auto pop = [&]() {
        for (int i = 0; i < pop_size; )
        {
            auto head = single_ref_stack.pop();
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


void day17_SingleRefStack()
{
    TestSingleRefStack();
}

