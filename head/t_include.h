#pragma once
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <numeric>
#include <mutex>
#include <stack>
#include <shared_mutex>
#include <map>
#include <set>
#include <queue>
#include <future>
#include <cassert>


extern std::mutex mtx1;
extern std::mutex mtx;
extern int shared_data;

class MyClass
{
public :
    MyClass(int i) : _data(i) {}
    friend std::ostream& operator<<(std::ostream& os, const MyClass& mc)
    {
        os << mc._data;
        return os;
    }
private:
    int _data;
};

