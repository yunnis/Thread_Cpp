#include "t_include.h"

void use_lock()
{
    while (true)
    {
        {
            std::lock_guard<std::mutex> lk_gurad(mtx1);
            //mtx1.lock();
            ++shared_data;
            std::cout << "cur thread is " << std::this_thread::get_id() << std::endl;
            std::cout << "share data is " << shared_data << std::endl;
            //mtx1.unlock();
        }
        std::this_thread::sleep_for(std::chrono::microseconds(10));
    }
}
void test_lock()
{
    std::thread t1(use_lock);
    std::thread t2([]() {
        while (true)
        {
            {
                std::lock_guard<std::mutex> lk_gurad(mtx1);
                //mtx1.lock();
                --shared_data;
                std::cout << "cur thread is " << std::this_thread::get_id() << std::endl;
                std::cout << "share data is " << shared_data << std::endl;
                //mtx1.unlock();
            }

            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
        });

    t1.join();
    t2.join();
}

template<typename T>
class threadsafe_stack1
{
private:
    std::stack<T> data;
    // 读操作可以声明为const 但是要加锁, 所以用mutable
    mutable std::mutex m;

public:
    threadsafe_stack1() {}
    threadsafe_stack1(const threadsafe_stack1& other)
    {
        std::lock_guard<std::mutex> l(other.m);
        data = other.data;
    }
    threadsafe_stack1& operator= (const threadsafe_stack1&) = delete;

    void push(T new_value)
    {
        std::lock_guard<std::mutex> l(m);
        data.push(std::move(new_value));
    }

    // error code
    T pop()
    {
        std::lock_guard<std::mutex> l(m);
        auto element = data.top();
        data.pop();
        return element;
    }

    // dangours
    bool empty() const
    {
        std::lock_guard<std::mutex> l(m);
        return data.empty();
    }
};
void test_threadsafe_stack1()
{
    threadsafe_stack1<int> safe_stack;
    safe_stack.push(1);

    std::thread t1([&safe_stack]() {
        if (!safe_stack.empty())
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            safe_stack.pop();
        }
        });

    std::thread t2([&safe_stack]() {
        if (!safe_stack.empty())
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            safe_stack.pop();
        }
        });

    t1.join();
    t2.join();
}
// 1 use exception
struct empty_stack :std::exception
{
    const char* what() const throw();
};
template<typename T>
class threadsafe_stack
{
private:
    std::stack<T> data;
    // 读操作可以声明为const 但是要加锁, 所以用mutable
    mutable std::mutex m;

public:
    threadsafe_stack() {}
    threadsafe_stack(const threadsafe_stack& other)
    {
        std::lock_guard<std::mutex> l(other.m);
        data = other.data;
    }
    threadsafe_stack& operator= (const threadsafe_stack&) = delete;

    void push(T new_value)
    {
        std::lock_guard<std::mutex> l(m);
        data.push(std::move(new_value));
    }

    // error code
    T pop()
    {
        std::lock_guard<std::mutex> l(m);
        if (data.empty())
        {
            throw empty_stack();
        }
        auto element = data.top();
        data.pop();
        return element;
    }
    // use shared_ptr, if T is too big
    std::shared_ptr<T> pop()
    {
        std::lock_guard<std::mutex> l(m);
        if (data.empty())
        {
            throw nullptr;
        }
        std::shared_ptr<T> const res(std::make_shared<T>(data.top()));
        data.pop();
        return res;
    }
    // use input param, if T is too big
    void pop(T& value)
    {
        std::lock_guard<std::mutex> l(m);
        if (data.empty())
        {
            throw empty_stack();
        }
        value = data.top();
        data.pop();
    }

    // dangours
    bool empty() const
    {
        std::lock_guard<std::mutex> l(m);
        return data.empty();
    }
};

// deadlock
std::mutex  t_lock1;
std::mutex  t_lock2;
int m_1 = 0;
int m_2 = 1;
void dead_lock1() {
    while (true) {
        std::cout << "dead_lock1 begin " << std::endl;
        t_lock1.lock();
        m_1 = 1024;
        t_lock2.lock();
        m_2 = 2048;
        t_lock2.unlock();
        t_lock1.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        std::cout << "dead_lock2 end " << std::endl;
    }
}
void dead_lock2() {
    while (true) {
        std::cout << "dead_lock2 begin " << std::endl;
        t_lock2.lock();
        m_2 = 2048;
        t_lock1.lock();
        m_1 = 1024;
        t_lock1.unlock();
        t_lock2.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        std::cout << "dead_lock2 end " << std::endl;
    }
}
void test_dead_lock()
{
    std::thread t1(dead_lock1);
    std::thread t2(dead_lock2);
    t1.join();
    t2.join();
}

void atomic_lock1() {
    std::cout << "lock1 begin lock" << std::endl;
    t_lock1.lock();
    m_1 = 1024;
    t_lock1.unlock();
    std::cout << "lock1 end lock" << std::endl;
}
void atomic_lock2() {
    std::cout << "lock2 begin lock" << std::endl;
    t_lock2.lock();
    m_2 = 2048;
    t_lock2.unlock();
    std::cout << "lock2 end lock" << std::endl;
}
void safe_lock1() {
    while (true) {
        atomic_lock1();
        atomic_lock2();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}
void safe_lock2() {
    while (true) {
        atomic_lock2();
        atomic_lock1();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}
void test_safe_lock() {
    std::thread t1(safe_lock1);
    std::thread t2(safe_lock2);
    t1.join();
    t2.join();
}

// lock multi mutex
class som_big_object
{
public:
    som_big_object(int data) : _data(data) {}
    som_big_object(const som_big_object& b2) : _data(b2._data) {}
    som_big_object(const som_big_object&& b2) noexcept : _data(std::move(b2._data)) {};

    som_big_object& operator=(const som_big_object& b2)
    {
        if (&b2 == this)
        {
            return *this;
        }

        _data = b2._data;
        return *this;
    } 
    som_big_object& operator=(const som_big_object&& b2)
    {
        if (&b2 == this)
        {
            return *this;
        }

        _data = std::move(b2._data);
        return *this;
    }

    friend std::ostream& operator << (std::ostream& os, const som_big_object& big_obj)
    {
        os << big_obj._data;
        return os;
    }

    // swap friend: swap(b1, b2)
    friend void swap(som_big_object& b1, som_big_object& b2)
    {
        som_big_object tmp = std::move(b1);
        b1 = std::move(b2);
        b2 = std::move(tmp);
    }
private:
    int _data;
};
class big_object_mgr
{
public:
    big_object_mgr(int data = 0) : _obj(data) {};
    void printinfo()
    {
        std::cout << " cur obj data is " << _obj << std::endl;
    }

    friend void danger_swap(big_object_mgr& objm1, big_object_mgr& objm2);
    friend void safe_swap(big_object_mgr& objm1, big_object_mgr& objm2);
    friend void safe_swap_scope(big_object_mgr& objm1, big_object_mgr& objm2);

private:
    std::mutex _mtx;
    som_big_object _obj;
};
void danger_swap(big_object_mgr& objm1, big_object_mgr& objm2)
{
    std::cout << "thread [" << std::this_thread::get_id() << " ] begin" << std::endl;
    if (&objm1 == &objm2)
    {
        return;
    }
    std::lock_guard<std::mutex> lk1(objm1._mtx);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::lock_guard<std::mutex> lk2(objm2._mtx);
    swap(objm1._obj, objm2._obj);
    std::cout << "thread [" << std::this_thread::get_id() << " ] end" << std::endl;

}
void safe_swap(big_object_mgr& objm1, big_object_mgr& objm2)
{
    std::cout << "thread [" << std::this_thread::get_id() << " ] begin" << std::endl;
    if (&objm1 == &objm2)
    {
        return;
    }

    std::lock(objm1._mtx, objm2._mtx);
    // only unlock()
    std::lock_guard<std::mutex> guard1(objm1._mtx, std::adopt_lock);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::lock_guard<std::mutex> guard2(objm2._mtx, std::adopt_lock);
    swap(objm1._obj, objm2._obj);
    std::cout << "thread [" << std::this_thread::get_id() << " ] end" << std::endl;
}
void safe_swap_scope(big_object_mgr& objm1, big_object_mgr& objm2)
{
    std::cout << "thread [" << std::this_thread::get_id() << " ] begin" << std::endl;
    if (&objm1 == &objm2)
    {
        return;
    }

    std::scoped_lock guard(objm1._mtx, objm2._mtx);
    swap(objm1._obj, objm2._obj);
    std::cout << "thread [" << std::this_thread::get_id() << " ] end" << std::endl;

}
void test_danger_swap()
{
    big_object_mgr objm1(1);
    big_object_mgr objm2(2);

    std::thread t1(danger_swap, std::ref(objm1), std::ref(objm2));
    std::thread t2(danger_swap, std::ref(objm2), std::ref(objm1));
    t1.join();
    t2.join();

    objm1.printinfo();
    objm2.printinfo();
}
void test_swap()
{
    big_object_mgr objm1(1);
    big_object_mgr objm2(2);

    std::thread t1(safe_swap, std::ref(objm1), std::ref(objm2));
    std::thread t2(safe_swap, std::ref(objm2), std::ref(objm1));
    t1.join();
    t2.join();

    objm1.printinfo();
    objm2.printinfo();
}
void test_scope_swap()
{
    big_object_mgr objm1(1);
    big_object_mgr objm2(2);

    std::thread t1(safe_swap_scope, std::ref(objm1), std::ref(objm2));
    std::thread t2(safe_swap_scope, std::ref(objm2), std::ref(objm1));
    t1.join();
    t2.join();

    objm1.printinfo();
    objm2.printinfo();
}


// hierarchical lock
class hierarchical_mutex
{
public:
    explicit hierarchical_mutex(unsigned long value) : _hierarchy_value(value),
        _previous_hierarchy_value(0) {}

    hierarchical_mutex(const hierarchical_mutex&) = delete;
    hierarchical_mutex& operator= (const hierarchical_mutex&) = delete;

    void lock()
    {
        check_for_hierarchy_violation();
        _internal_mutex.lock();
        update_hierarachy_value();
    }

    void unlock()
    {
        if (_this_thread_hierarchy_value != _hierarchy_value)
        {
            throw std::logic_error("mutex hierarchy violated");
        }

        _this_thread_hierarchy_value = _previous_hierarchy_value;
        _internal_mutex.unlock();
    }

    bool try_lock()
    {
        check_for_hierarchy_violation();
        if (!_internal_mutex.try_lock())
        {
            return false;
        }

        update_hierarachy_value();
        return true;
    }

private:
    void check_for_hierarchy_violation()
    {
        if (_this_thread_hierarchy_value <= _hierarchy_value)
        {
            throw std::logic_error("mutex hierarchy violated");
        }
    }

    void update_hierarachy_value()
    {
        _previous_hierarchy_value = _this_thread_hierarchy_value;
        _this_thread_hierarchy_value = _hierarchy_value;
    }
private:
    std::mutex _internal_mutex;
    // cur
    unsigned long const _hierarchy_value;
    // last
    unsigned long _previous_hierarchy_value;
    static thread_local unsigned long _this_thread_hierarchy_value;
};
thread_local unsigned long hierarchical_mutex::_this_thread_hierarchy_value = 0xffff'ffff;
void test_hierarchy_lock()
{
    hierarchical_mutex hmtx1(1000);
    hierarchical_mutex hmtx2(500);

    std::thread t1([&hmtx1, &hmtx2]() {
        hmtx1.lock();
        hmtx2.lock();

        hmtx2.unlock();
        hmtx1.unlock();
        });

    std::thread t2([&hmtx1, &hmtx2]() {
        hmtx2.lock();
        hmtx1.lock();

        hmtx1.unlock();
        hmtx2.unlock();
        });

    t1.join();
    t2.join();
}


void day3()
{
    //test_lock();
    //test_threadsafe_stack1();
    //test_dead_lock();
    //test_safe_lock();
    //test_danger_swap();
    //test_swap();
    //test_scope_swap();
    test_hierarchy_lock();
}

