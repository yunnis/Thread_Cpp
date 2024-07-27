#include "t_include.h"


template<typename T, size_t Cap>
class CircularQueLK : private std::allocator<T>
{
public:
    CircularQueLK() : _max_size(Cap + 1), _data(std::allocator<T>::allocate(_max_size)), _head(0), _tail(0) {}
    CircularQueLK(const CircularQueLK&) = delete;
    CircularQueLK& operator = (const CircularQueLK&) = delete;

    ~CircularQueLK()
    {
        std::lock_guard<std::mutex> lk(_mtx);
        while (_head != _tail)
        {
            //std::allocator<T>::destroy(_data + _head);
            _head = (_head + 1) % _max_size;
        }
        std::allocator<T>::deallocate(_data, _max_size);
    }

    template<typename ...Args>
    bool emplace(Args&&... args)
    {
        std::lock_guard<std::mutex> lock(_mtx);
        if ((_tail + 1) % _max_size == _head)
        {
            std::cout << "circular que full" << std::endl;
            return false;
        }
        //
        //std::allocator<T>::construct(_data + _tail, std::forward<Args>(args)...);
        new (_data + _tail)T(std::forward<Args>(args)...);

        _tail = (_tail + 1) % _max_size;
        return true;
    }

    bool push(const T& val)
    {
        std::cout << "called push const T& " << std::endl;
        return emplace(val);
    }
    bool push(const T&& val)
    {
        std::cout << "called push const T&& " << std::endl;
        return emplace(std::move(val));
    }

    bool pop(T& val)
    {
        std::lock_guard<std::mutex> lk(_mtx);
        if (_head==_tail)
        {
            std::cout << "circular que empty" << std::endl;
            return false;
        }
        val = std::move(_data[_head]);
        _head = (_head + 1) % _max_size;
        return true;
    }
private:
    size_t _max_size;
    T* _data;
    size_t _head;
    size_t _tail;
    std::mutex _mtx;
};

void TestCircularQue() {
    //最大容量为10
    CircularQueLK<int, 5> cq_lk;
    int mc1(1);
    int mc2(2);
    cq_lk.push(mc1);
    cq_lk.push(std::move(mc2));
    for (int i = 3; i <= 5; i++) {
        int mc(i);
        auto res = cq_lk.push(mc);
        if (res == false) {
            break;
        }
    }

    cq_lk.push(mc2);

    for (int i = 0; i < 5; i++) {
        int mc1;
        auto res = cq_lk.pop(mc1);
        if (!res) {
            break;
        }
        std::cout << "pop success, " << mc1 << std::endl;
    }

    auto res = cq_lk.pop(mc1);
}

// 无所版本
//bool std::atomic<T>::compare_exchange_weak(T& expected, T desired);
//bool std::atomic<T>::compare_exchange_strong(T& expected, T desired);
//compare_exchange_strong会比较原子变量atomic<T>的值和expected的值是否相等，如果相等则执行交换操作，
// 将atomic<T>的值换为desired并且返回true, 否则将expected的值修改为bool变量的值，并且返回false.
//compare_exchange_weak功能比compare_exchange_strong弱一些，他不能保证atomic<T>的值和expected的值相等时也会做交换，
// 很可能原子变量和预期值相等也会返回false，所以使用要多次循环使用。

template<typename T, size_t Cap>
class CircularQueLight : private std::allocator<T>
{
public:
    CircularQueLight() :_max_size(Cap + 1),
        _data(std::allocator<T>::allocate(_max_size))
        , _head(0), _tail(0), _tail_update(0) {}

    CircularQueLight(const CircularQueLight&) = delete;
    CircularQueLight& operator = (const CircularQueLight&) volatile = delete;
    CircularQueLight& operator = (const CircularQueLight&) = delete;
    ~CircularQueLight()
    {
        //调用内部元素的析构函数
        while (_head != _tail) {
            //std::allocator<T>::destroy(_data + _head);
            _head = (_head + 1) % _max_size;
        }
        //调用回收操作
        std::allocator<T>::deallocate(_data, _max_size);
    }
    //出队函数

    bool pop(T& val) {

        size_t h;
        do
        {
            h = _head.load();  //1 处
            //判断头部和尾部指针是否重合，如果重合则队列为空
            if (h == _tail.load())
            {
                std::cout << "circular que empty ! " << std::endl;
                return false;
            }

            //判断如果此时要读取的数据和tail_update是否一致，如果一致说明尾部数据未更新完
            if (h == _tail_update.load())
            {
                return false;
            }
            val = _data[h]; // 2处

        } while (!_head.compare_exchange_strong(h,
            (h + 1) % _max_size)); //3 处
        std::cout << "pop data success, data is " << val << std::endl;
        return true;
    }

    bool push(const T& val)
    {
        size_t t;
        do
        {
            t = _tail.load();  //1
            //判断队列是否满
            if ((t + 1) % _max_size == _head.load())
            {
                std::cout << "circular que full ! " << std::endl;
                return false;
            }



        } while (!_tail.compare_exchange_strong(t,
            (t + 1) % _max_size));  //3

        _data[t] = val; //2
        size_t tailup;
        do
        {
            tailup = t;

        } while (!_tail_update.compare_exchange_strong(tailup,
            (tailup + 1) % _max_size));

        std::cout << "called push data success " << val << std::endl;
        return true;
    }

private:
    size_t _max_size;
    T* _data;
    std::atomic<size_t>  _head;
    std::atomic<size_t> _tail;
    std::atomic<size_t> _tail_update;
};

void TestCircularQueLight()
{
    CircularQueLight<int, 3> cq_seq;
    for (int i = 0; i < 4; i++)
    {
        int mc1(i);
        auto res = cq_seq.push(mc1);
        if (!res)
        {
            break;
        }
    }

    for (int i = 0; i < 4; i++)
    {
        int mc1;
        auto res = cq_seq.pop(mc1);
        if (!res)
        {
            break;
        }

        std::cout << "pop success, " << mc1 << std::endl;
    }

    for (int i = 0; i < 4; i++)
    {
        int mc1(i);
        auto res = cq_seq.push(mc1);
        if (!res)
        {
            break;
        }
    }

    for (int i = 0; i < 4; i++)
    {
        int mc1;
        auto res = cq_seq.pop(mc1);
        if (!res)
        {
            break;
        }

        std::cout << "pop success, " << mc1 << std::endl;
    }
}

void day12()
{
    TestCircularQue();
    TestCircularQueLight();
}
