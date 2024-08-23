// Thread_C.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "day1.h"
#include "day2.h"
#include "day3.h"
#include "day4.h"
#include "day5_Single.h"
#include "day6_condition_variable.h"
#include "day7_future_promise_async.h"
#include "day8_csp.h"
#include "day11_AcquireRelease.h"
#include "day12_CirularQueLK.h"
#include "day13_fence.h"
#include "day14_ThreadSafeStack.h"
#include "day15_ThreadSafeLookupTable.h"
#include "day16_ThreadSafeForwardList.h"
#include "day16_ThreadSafeList.h"
#include "day17_LockFreeStack.h"
#include "day17_HazardPointStack.h"
#include "day17_SingleRefStack.h"
#include "day17_RefCountStack.h"

#include "atm.h"


 std::mutex mtx;
 std::mutex mtx1;
 int shared_data = 100;

 void DeadLock() {
     std::mutex  mtx;
     std::cout << "DeadLock begin " << std::endl;
     std::future<void>  futures;
     std::lock_guard<std::mutex>  dklock(mtx);
     {
         futures = std::async(std::launch::async, [&mtx]() {
             std::cout << "std::async called " << std::endl;
             std::lock_guard<std::mutex>  dklock(mtx);
             std::cout << "async working...." << std::endl;
             });
     }

     std::cout << "DeadLock end " << std::endl;
 }

int main()
{
    //day1();
    //day2();
    //day3();
    //day4();
    //day5();
    //day6();
    //day7();
    //day7_pool();
    //day8();
    //RunATM();
    //DeadLock();
    //day11();
    //day12();
    //day13();
    //day14_ThreadSafeStack();
    //day15();
    //day16_forwardlist();
    //day16_list();
    //day17_LockFreeStack();
    //day17_HazardPointStack();
    //day17_SingleRefStack();
    day17_RefCountStack();
}

// 运行程序: Ctrl + F5 或调试 >“开始执行(不调试)”菜单
// 调试程序: F5 或调试 >“开始调试”菜单

// 入门使用技巧: 
//   1. 使用解决方案资源管理器窗口添加/管理文件
//   2. 使用团队资源管理器窗口连接到源代码管理
//   3. 使用输出窗口查看生成输出和其他消息
//   4. 使用错误列表窗口查看错误
//   5. 转到“项目”>“添加新项”以创建新的代码文件，或转到“项目”>“添加现有项”以将现有代码文件添加到项目
//   6. 将来，若要再次打开此项目，请转到“文件”>“打开”>“项目”并选择 .sln 文件
