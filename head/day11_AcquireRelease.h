#include "t_include.h"

// �ڴ��� ���������˳��, ֻ������cpu�Բ����Ŀɼ���, 
// memory_order_seq_cst ȫ��һ����˳��, ÿ���޸��궼֪ͨ����cpu, �����߳̿��������в�������һ��һ�µ�˳��
// �������Ĵ�һЩ, ����С, ʵ�ʽ���ʹ��, ����ͨ��ʹ�� ͬ��ģ�� 

// memory_order_relaxed ֻ�ܱ�֤������ԭ���Ժ�˳���޸ĵ�һ����, �޷�ʵ�� synchronizes-with �Ĺ�ϵ
// �����Ⱥ�ִ��, ��һ��cpu���ܿ�����ִ�еĽ��

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
    // �Ͳ����ry��д��, rx��д��
    std::thread t1([&]() {
        rx.store(true, std::memory_order_relaxed);
        ry.store(true, std::memory_order_relaxed);
        });
    // ��ʹrx,ry���޸���, t2 �п��ܿ���ry����, rxû��
    std::thread t2([&]() {
        while (!ry.load(std::memory_order_relaxed));
        assert(rx.load(std::memory_order_relaxed));
        });

    t1.join();
    t2.join();
}


// Acquire-Release
// load ʹ��memory_order_acquire , ��Ϊ acquire ����
// store ʹ��memory_order_release , ��Ϊ release ����
// read-modify-write ʹ�� ʹ��memory_order_acq_rel
// Acquire-Release ����ʵ�� synchronizes-with �Ĺ�ϵ, acquire ������ͬһ��ԭ�ӱ����϶�ȡ release д���ֵ
//  ����� Release ���� synchronizes-with ��� Acquire ����
// Ӱ��������Ż�, ָ������ŵ� acquire ֮ǰ, release֮��

void TestReleaseAcquire()
{
    std::atomic<bool> rx, ry;
    std::thread t1([&]() {
        // 1 sequence-before 2
        rx.store(true, std::memory_order_relaxed); // 1
        ry.store(true, std::memory_order_release); // 2
        // release ֮ǰ�� store ���������֮ǰ�� store д��
        });
    // 2 synchronizes-with 3
    std::thread t2([&]() {
        // 3 sequence-before 4
        while (!ry.load(std::memory_order_acquire)); // 3
        assert(rx.load(std::memory_order_relaxed));  // 4
        });
    // ���� 4 �ܿ��� 1,2���޸�

    t1.join();
    t2.join();
}

// ����̶߳�ͬһ��ԭ�ӱ��� ִ�� release, һ���̶߳������ acquire, ֻ��һ�Ի��� synchronizes-with ��ϵ
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
    // 2,3 �� 4 ֻ��һ���ܹ���ͬ����ϵ
    // ����3,4 ͬ����ϵʱ, ���Իᴥ��
    std::thread t3([&]() {
        while (!yd.load(std::memory_order_acquire)); // 4
        assert(xd.load(std::memory_order_acquire) == 1); // 5
        });

    t1.join();
    t2.join();
    t3.join();
}

// ������ֻ��acquire ���� release ����д���ֵ�Ź���synchronizes-with ��ϵ, release-sequence Ҳ��������
// release-sequence:
// ԭ�ӱ���M release ����A��ɺ�, ������M�ϵ���������, �������
//   1. ͬһ�߳��ϵ�д����
//   2. �����̵߳� read-modify-write ����
// ����, �����Щ����Ϊ�� release ����Ϊ�׵� release-sequence, �������ֲ��������������ڴ���
// һ��qcquire������ͬ��ԭ�ӱ����϶����� release-sequence ������ֵ, Ҳ�� synchronizes-with ��ϵ
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
        // ��Ȼt1 t2 ˳��ȷ��, ����t3 ����flag == 2 ���ܿ���
        while (flag.load(std::memory_order_acquire) < 2); // 4
        assert(data.at(0) == 42);//5
        });

    t1.join();
    t2.join();
    t3.join();

}

// memory_order_consume ����load, consume ��������ͬһ��ԭ�ӱ����� release ����д���ֵ, ������Ϊ�׵� release sequence д
// ���ֵ, �� release ���� dependency-ordered before ��� consume ����
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
        // 1 ��
        if (_b_init.load(std::memory_order_acquire))
        {
            return single;
        }
        // 2 ��
        s_mutex.lock();
        // 3 ��
        if (_b_init.load(std::memory_order_relaxed))
        {
            s_mutex.unlock();
            return single;
        }
        // 4��
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

