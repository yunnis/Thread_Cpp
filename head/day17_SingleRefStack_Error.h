#pragma once
#include <condition_variable>
#include <mutex>
#include <queue>
#include <atomic>
#include <memory>

// ** **
// ** 这是有安全问题的单计数版本
// ** **
#pragma once
#include <atomic>

template<typename T>
class single_ref_stack {
public:
    single_ref_stack() :head(nullptr) {

    }

    ~single_ref_stack() {
        //循环出栈
        while (pop());
    }

    void push(T const& data) {
        auto new_node = new ref_node(data);
        new_node->next = head.load();
        while (!head.compare_exchange_weak(new_node->next, new_node));
    }


    std::shared_ptr<T> pop() {
        ref_node* old_head = head.load();
        for (;;) {
            if (!old_head) {
                return std::shared_ptr<T>();
            }
            //1 只要执行pop就对引用计数+1
            ++(old_head->_ref_count);
            //2 比较head和old_head想等则交换否则说明head已经被其他线程更新
            if (head.compare_exchange_strong(old_head, old_head->_next)) {
                auto cur_count = old_head->_ref_count.load();
                auto new_count;
                //3  循环重试保证引用计数安全更新
                do {
                    //4 减去本线程增加的1次和初始的1次
                    new_count = cur_count - 2;
                } while (!old_head->_ref_count.compare_exchange_weak(cur_count, new_count));

                //返回头部数据
                std::shared_ptr<T> res;
                //5  交换数据
                res.swap(old_head->_data);

                if (old_head->_ref_count == 0) {
                    delete old_head;
                }

                return res;
            }
            else {
                if (old_head->_ref_count.fetch_sub(1) == 1) {
                    delete old_head;
                }
            }
        }
    }

    // 错误:
    //1 假设线程1比线程2先执行，线程1在2处执行比较交换后head会被更新为新的值。线程2执行比较交换操作会失败，则进入else处理, old_head会被更新为新的head值， 此时old_head的引用计数为1则会被线程2误删，因为线程2此时读到的old_head正是新的head指向的数据。而且没有弹出和修改head的值。这样其他线程pop头部元素时会崩溃。

    //2 线程1和线程2都执行完0处代码，二者读取的old_head值相同。假设线程1比线程2先执行，线程2因未抢占到cpu的时间片停顿在1处，线程1按次序依次执行最后执行到3处将node_ptr删除。而且现在的head已经指向了新的栈顶元素即old_head的下一个元素。此时线程2抢占到时间片，执行1处代码又将old_head更新为head当前值了，只不过引用计数加了1变为2，但是指向的是下一个节点，所以这种情况下进入仍会进入if条件，对新的old_head节点删除。这种情况倒是正常。

    //3 还是假设线程1和线程2都执行完0处代码，线程1抢先执行完5处。准备执行6处时，线程2抢占CPU执行了7处代码，尽管会被while比较old_head和head不同而重试，进而更新old_head。但是线程2的do逻辑中第一次的old_head和线程1的old_head指向的是同一个，线程2修改了old_head中的引用计数，导致线程1执行6处代码时不会进入if逻辑。又因为线程2在2处之后while会不断重试，线程2的head已经和old_head指向不同了，导致线程2也不会回收old_head内部节点指向的数据，导致内存泄漏。

    //这就告诉我们当我们设计pop逻辑的时候尽量不要存储指针，存储指针意味着存在多个线程操作同一块内存的情况。
private:
    struct ref_node {
        //1 数据域智能指针
        std::shared_ptr<T>  _data;
        //2 引用计数
        std::atomic<int> _ref_count;
        //3  下一个节点
        ref_node* _next;
        ref_node(T const& data_) : _data(std::make_shared<T>(data_)),
            _ref_count(1), _next(nullptr) {}
    };

    //头部节点
    std::atomic<ref_node*> head;
};