#include "t_include.h"

void use_unique()
{
    std::unique_lock<std::mutex> lock(mtx);
    std::cout << "lock success" << std::endl;
    ++shared_data;
    // ** 手动解锁 (也可手动解锁
    lock.unlock();
}

void owns_lock()
{
    std::unique_lock<std::mutex> lock(mtx);
    ++shared_data;
    if (lock.owns_lock())
    {
        std::cout << "own lock " << std::endl;
    }
    else
    {
        std::cout << " dont own lock " << std::endl;
    }

    lock.unlock();
    if (lock.owns_lock())
    {
        std::cout << "own lock " << std::endl;
    }
    else
    {
        std::cout << " dont own lock " << std::endl;
    }
}

void defer_lock()
{
    std::unique_lock<std::mutex> lock(mtx, std::defer_lock);
    lock.lock();
    lock.unlock();
}

void use_own_defer()
{
    std::unique_lock<std::mutex> lock(mtx);
    if (lock.owns_lock())
    {
        std::cout << "main thread own lock " << std::endl;
    }
    else
    {
        std::cout << "main thread dont own lock " << std::endl;
    }

    std::thread t([]() {
        std::unique_lock<std::mutex> lock(mtx, std::defer_lock);
        if (lock.owns_lock())
        {
            std::cout << "thread own lock " << std::endl;
        }
        else
        {
            std::cout << "thread dont own lock " << std::endl;
        }
        lock.lock();

        if (lock.owns_lock())
        {
            std::cout << "thread own lock " << std::endl;
        }
        else
        {
            std::cout << "thread dont own lock " << std::endl;
        }

        lock.unlock();

        });

    t.join();
}

void use_own_adopt()
{
    mtx.lock();
    std::unique_lock<std::mutex> lock(mtx, std::adopt_lock);
    if (lock.owns_lock())
    {
        std::cout << "  own lock " << std::endl;
    }
    else
    {
        std::cout << "  dont own lock " << std::endl;
    }
    lock.unlock();
}

int d4_a = 10;
int d4_b = 99;
std::mutex  d4_mtx1;
std::mutex  d4_mtx2;
void safe_swap() {
    std::lock(d4_mtx1, d4_mtx2);
    std::unique_lock<std::mutex> lock1(d4_mtx1, std::adopt_lock);
    std::unique_lock<std::mutex> lock2(d4_mtx2, std::adopt_lock);
    std::swap(d4_a, d4_b);
    //错误用法
    //d4_mtx1.unlock();
    //d4_mtx2.unlock();
}
void safe_swap2() {
    std::unique_lock<std::mutex> lock1(d4_mtx1, std::defer_lock);
    std::unique_lock<std::mutex> lock2(d4_mtx2, std::defer_lock);
    //需用lock1,lock2加锁
    std::lock(lock1, lock2);
    //错误用法
    //std::lock(d4_mtx1, d4_mtx2);
    std::swap(d4_a, d4_b);
}

std::unique_lock<std::mutex> get_lock()
{
    std::unique_lock<std::mutex> lock(mtx);
    ++shared_data;
    return lock;
}
void use_return()
{
    std::unique_lock<std::mutex> lock(get_lock());
    ++shared_data;
}

// 共享锁
// 17: shared
// 14: shared_time
// 11: 没有
class DNService {
public:
    DNService() {}
    //读操作采用共享锁
    std::string QueryDNS(std::string dnsname) {
        std::shared_lock<std::shared_mutex> shared_locks(_shared_mtx);
        auto iter = _dns_info.find(dnsname);
        if (iter != _dns_info.end()) {
            return iter->second;
        }
        return "";
    }
    //写操作采用独占锁
    void AddDNSInfo(std::string dnsname, std::string dnsentry) {
        std::lock_guard<std::shared_mutex>  guard_locks(_shared_mtx);
        _dns_info.insert(std::make_pair(dnsname, dnsentry));
    }
private:
    std::map<std::string, std::string> _dns_info;
    mutable std::shared_mutex  _shared_mtx;
};

void day4()
{
    //use_unique();
    //owns_lock();
    //defer_lock();
    //use_own_defer();
    //use_own_adopt();
    //safe_swap();
    //safe_swap2();
    use_return();
}

