#include "t_include.h"

#pragma once

namespace{
std::atomic<bool> x, y;
std::atomic<int> z;

void write_x()
{
    x.store(true, std::memory_order_release); //1
}
void write_y()
{
    y.store(true, std::memory_order_release); //2
}
void read_x_then_y()
{
    while (!x.load(std::memory_order_acquire));
    if (y.load(std::memory_order_acquire))   //3
        ++z;
}
void read_y_then_x()
{
    while (!y.load(std::memory_order_acquire));
    if (x.load(std::memory_order_acquire))   //4
        ++z;
}

void TestAR()
{
    x = false;
    y = false;
    z = 0;
    std::thread a(write_x);
    std::thread b(write_y);
    std::thread c(read_x_then_y);
    std::thread d(read_y_then_x);
    a.join();
    b.join();
    c.join();
    d.join();
    assert(z.load() != 0); //5 �п��ܴ���, ��Ϊ�����ڴ��� ����֤�޸ĸ����ɼ�
    std::cout << "z value is " << z.load() << std::endl;
}


// ʹ��դ��
//�߳�a����write_x_then_y_fence���߳�b����read_y_then_x_fence.
//���߳�bִ�е�5��ʱ˵��4�Ѿ���������ʱ�߳�a����yΪtrue����ô�߳�a��Ȼ�Ѿ�ִ����3.
//����4��3���ǲ��õ���std::memory_order_relaxed˳�򣬵���ͨ���߼���ϵ��֤��3�Ľ��ͬ����4������"3 happens-before 4"
//��Ϊ���ǲ�����դ��std::atomic_fence���ԣ�5���ܱ�֤6��������5д���ڴ棬(memory_order_acquire��֤����ָ���������д���ڴ�)
//2���ܱ�֤1����ָ������2д���ڴ棬����"1 happens-before 6", 1�Ľ����ͬ���� 6
//����"atomic_thread_fence"��ʵ��"release-acquire"���ƣ����Ǳ�֤memory_order_release֮ǰ��ָ����ŵ����memory_order_acquire֮���ָ����ŵ���֮ǰ��

void write_x_then_y_fence()
{
    x.store(true, std::memory_order_relaxed);  //1
    std::atomic_thread_fence(std::memory_order_release);  //2
    y.store(true, std::memory_order_relaxed);  //3
}

void read_y_then_x_fence()
{
    while (!y.load(std::memory_order_relaxed));  //4
    std::atomic_thread_fence(std::memory_order_acquire); //5
    if (x.load(std::memory_order_relaxed))  //6
        ++z;
}

void TestFence()
{
    x = false;
    y = false;
    z = 0;
    std::thread a(write_x_then_y_fence);
    std::thread b(read_y_then_x_fence);
    a.join();
    b.join();
    assert(z.load() != 0);   //7
}
}
void day13()
{
    TestAR();
    TestFence();
}
