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
    assert(z.load() != 0); //5 有可能触发, 因为宽松内存序 不保证修改各个可见
    std::cout << "z value is " << z.load() << std::endl;
}


// 使用栅栏
//线程a运行write_x_then_y_fence，线程b运行read_y_then_x_fence.
//当线程b执行到5处时说明4已经结束，此时线程a看到y为true，那么线程a必然已经执行完3.
//尽管4和3我们采用的是std::memory_order_relaxed顺序，但是通过逻辑关系保证了3的结果同步给4，进而"3 happens-before 4"
//因为我们采用了栅栏std::atomic_fence所以，5处能保证6不会先于5写入内存，(memory_order_acquire保证其后的指令不会先于其写入内存)
//2处能保证1处的指令先于2写入内存，进而"1 happens-before 6", 1的结果会同步给 6
//所以"atomic_thread_fence"其实和"release-acquire"相似，都是保证memory_order_release之前的指令不会排到其后，memory_order_acquire之后的指令不会排到其之前。

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
