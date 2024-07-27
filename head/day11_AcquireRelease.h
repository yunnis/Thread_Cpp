#include "t_include.h"

// 内存序 不代表调用顺序, 只是其他cpu对操作的可见性, 
// memory_order_seq_cst 全局一致性顺序, 每次修改完都通知其他cpu, 所有线程看到的所有操作都有一个一致的顺序
// 性能消耗大一些, 比锁小, 实际较少使用, 无锁通常使用 同步模型 

// memory_order_relaxed 只能保证操作的原子性和顺序修改的一致性, 无法实现 synchronizes-with 的关系
// 比如先后执行, 另一个cpu可能看到后执行的结果

std::atomic<bool> x, y;
std::atomic<int> z;

void write_x_then_y()
{
    x.store(true, std::memory_order_seq_cst);
    y.store(true, std::memory_order_seq_cst);
}
void read_y_then_x()
{
    while (!y.load(std::memory_order_seq_cst))
    {
        std::cout << "y load false" << std::endl;
    }

    if (x.load(std::memory_order_seq_cst))
    {
        ++z;
    }
}

void TestOrderSeqCst()
{
    std::thread t1(write_x_then_y);
    std::thread t2(read_y_then_x);
    t1.join();
    t2.join();
    assert(z.load() != 0);
}

void TestOrderRelaxed()
{
    std::atomic<bool> rx, ry;
    // sequenc before
    // 低层可能ry先写入, rx后写入
    std::thread t1([&]() {
        rx.store(true, std::memory_order_relaxed);
        ry.store(true, std::memory_order_relaxed);
        });
    // 即使rx,ry都修改了, t2 中可能看到ry改了, rx没改
    std::thread t2([&]() {
        while (!ry.load(std::memory_order_relaxed));
        assert(rx.load(std::memory_order_relaxed));
        });

    t1.join();
    t2.join();
}


// Acquire-Release
// load 使用memory_order_acquire , 称为 acquire 操作
// store 使用memory_order_release , 称为 release 操作
// read-modify-write 使用 使用memory_order_acq_rel
// Acquire-Release 可以实现 synchronizes-with 的关系, acquire 操作在同一个原子变量上读取 release 写入的值
//  则这个 Release 操作 synchronizes-with 这个 Acquire 操作
// 影响编译器优化, 指令不能重排到 acquire 之前, release之后

void TestReleaseAcquire()
{
    std::atomic<bool> rx, ry;
    std::thread t1([&]() {
        // 1 sequence-before 2
        rx.store(true, std::memory_order_relaxed); // 1
        ry.store(true, std::memory_order_release); // 2
        // release 之前的 store 编译器会把之前的 store 写入
        });
    // 2 synchronizes-with 3
    std::thread t2([&]() {
        // 3 sequence-before 4
        while (!ry.load(std::memory_order_acquire)); // 3
        assert(rx.load(std::memory_order_relaxed));  // 4
        });
    // 所以 4 能看到 1,2的修改

    t1.join();
    t2.join();
}

// 多个线程对同一个原子变量 执行 release, 一个线程对其进行 acquire, 只有一对会有 synchronizes-with 关系
void ReleasAcquireDanger()
{
    std::atomic<int> xd{0}, yd{ 0 };
    std::atomic<int> zd{0};

    std::thread t1([&]() {
        xd.store(1, std::memory_order_release); // 1
        yd.store(1, std::memory_order_release); // 2
        });
    std::thread t2([&]() {
        yd.store(2, std::memory_order_release); // 3
        });
    // 2,3 和 4 只有一对能构成同步关系
    // 所以3,4 同步关系时, 断言会触发
    std::thread t3([&]() {
        while (!yd.load(std::memory_order_acquire)); // 4
        assert(xd.load(std::memory_order_acquire) == 1); // 5
        });

    t1.join();
    t2.join();
    t3.join();
}

// 并不是只有acquire 读到 release 操作写入的值才构成synchronizes-with 关系, release-sequence 也可以做到
// release-sequence:
// 原子变量M release 操作A完成后, 接下来M上的其他操作, 如果是由
//   1. 同一线程上的写操作
//   2. 任意线程的 read-modify-write 操作
// 构成, 测成这些操作为以 release 操作为首的 release-sequence, 以上两种操作可以是任意内存序
// 一个qcquire操作在同个原子变量上读到该 release-sequence 操作的值, 也是 synchronizes-with 关系
void ReleaseSequence()
{
    std::vector<int> data;
    std::atomic<int> flag{0};

    std::thread t1([&]() {
        data.push_back(42); // 1
        flag.store(1, std::memory_order_release); // 2
        });
    std::thread t2([&]() {
        int expected = 1;
        // flag == expected ? expected = flag; return false : flag = 2; return true;
        while (!flag.compare_exchange_strong(expected, 2, std::memory_order_relaxed)) // 3
            expected = 1;
        });
    std::thread t3([&]() {
        // 虽然t1 t2 顺序不确定, 但是t3 读到flag == 2 就能看到
        while (flag.load(std::memory_order_acquire) < 2); // 4
        assert(data.at(0) == 42);//5
        });

    t1.join();
    t2.join();
    t3.join();

}

// memory_order_consume 用于load, consume 操作读到同一个原子变量的 release 操作写入的值, 或以其为首的 release sequence 写
// 入的值, 则 release 操作 dependency-ordered before 这个 consume 操作
void ConsumeDependency()
{
    std::atomic<std::string*> ptr;
    int data;

    std::thread t1([&]() {
        std::string* p = new std::string("Hello World"); // (1)
        data = 42; // (2)
        ptr.store(p, std::memory_order_release); // (3)
        });

    std::thread t2([&]() {
        std::string* p2;
        while (!(p2 = ptr.load(std::memory_order_consume))); // (4)
        assert(*p2 == "Hello World"); // (5)
        assert(data == 42); // (6)
        });

    t1.join();
    t2.join();
}

class SingleMemoryModel
{
private:
    SingleMemoryModel()
    {
    }
    SingleMemoryModel(const SingleMemoryModel&) = delete;
    SingleMemoryModel& operator=(const SingleMemoryModel&) = delete;
public:
    ~SingleMemoryModel()
    {
        std::cout << "single auto delete success " << std::endl;
    }
    static std::shared_ptr<SingleMemoryModel> GetInst()
    {
        // 1 处
        if (_b_init.load(std::memory_order_acquire))
        {
            return single;
        }
        // 2 处
        s_mutex.lock();
        // 3 处
        if (_b_init.load(std::memory_order_relaxed))
        {
            s_mutex.unlock();
            return single;
        }
        // 4处
        single = std::shared_ptr<SingleMemoryModel>(new SingleMemoryModel);
        _b_init.store(true, std::memory_order_release);
        s_mutex.unlock();
        return single;
    }
private:
    static std::shared_ptr<SingleMemoryModel> single;
    static std::mutex s_mutex;
    static std::atomic<bool> _b_init;
};

std::shared_ptr<SingleMemoryModel> SingleMemoryModel::single = nullptr;
std::mutex SingleMemoryModel::s_mutex;
std::atomic<bool> SingleMemoryModel::_b_init = false;



void day11()
{
    for (int i = 0; i < 10000; ++i)
    {
        //TestOrderSeqCst();
        //TestOrderRelaxed();
        //TestReleaseAcquire();
    }
    //ReleaseSequence();
    ConsumeDependency();
}

