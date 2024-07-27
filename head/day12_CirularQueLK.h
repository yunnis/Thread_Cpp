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
    //�������Ϊ10
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

// �����汾
//bool std::atomic<T>::compare_exchange_weak(T& expected, T desired);
//bool std::atomic<T>::compare_exchange_strong(T& expected, T desired);
//compare_exchange_strong��Ƚ�ԭ�ӱ���atomic<T>��ֵ��expected��ֵ�Ƿ���ȣ���������ִ�н���������
// ��atomic<T>��ֵ��Ϊdesired���ҷ���true, ����expected��ֵ�޸�Ϊbool������ֵ�����ҷ���false.
//compare_exchange_weak���ܱ�compare_exchange_strong��һЩ�������ܱ�֤atomic<T>��ֵ��expected��ֵ���ʱҲ����������
// �ܿ���ԭ�ӱ�����Ԥ��ֵ���Ҳ�᷵��false������ʹ��Ҫ���ѭ��ʹ�á�

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
        //�����ڲ�Ԫ�ص���������
        while (_head != _tail) {
            //std::allocator<T>::destroy(_data + _head);
            _head = (_head + 1) % _max_size;
        }
        //���û��ղ���
        std::allocator<T>::deallocate(_data, _max_size);
    }
    //���Ӻ���

    bool pop(T& val) {

        size_t h;
        do
        {
            h = _head.load();  //1 ��
            //�ж�ͷ����β��ָ���Ƿ��غϣ�����غ������Ϊ��
            if (h == _tail.load())
            {
                std::cout << "circular que empty ! " << std::endl;
                return false;
            }

            //�ж������ʱҪ��ȡ�����ݺ�tail_update�Ƿ�һ�£����һ��˵��β������δ������
            if (h == _tail_update.load())
            {
                return false;
            }
            val = _data[h]; // 2��

        } while (!_head.compare_exchange_strong(h,
            (h + 1) % _max_size)); //3 ��
        std::cout << "pop data success, data is " << val << std::endl;
        return true;
    }

    bool push(const T& val)
    {
        size_t t;
        do
        {
            t = _tail.load();  //1
            //�ж϶����Ƿ���
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
