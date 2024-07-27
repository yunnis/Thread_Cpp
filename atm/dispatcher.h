// header.h
#pragma once
// Code placed here is included only once per translation unit
#include "message.h"
#include <iostream>

namespace messaging
{
    // 关闭队列的消息
    class close_queue {};

    class dispatcher
    {
    private:
        queue* q;
        bool chained;
        dispatcher(dispatcher const&) = delete;
        dispatcher& operator=(dispatcher const&) = delete;

        template<typename Dispatcher, typename Msg, typename Func>
        friend class TemplateDispatcher;

        void wait_and_dispatch()
        {
            while (true)
            {
                auto msg = q->wait_and_pop();
                dispatch(msg);
            }
        }

        bool dispatch(std::shared_ptr<message_base> const& msg)
        {
            if (dynamic_cast<wrapped_message<close_queue>*>(msg.get()))
            {
                throw close_queue();
            }

            return false;
        }
    public:
        dispatcher(dispatcher&& other) : q(other.q), chained(other.chained)
        {
            // 上游的消息分发者不再等待消息
            other.chained = true;
        }
        explicit dispatcher(queue* q_) : q(q_), chained(false) {}

        template<typename Message, typename Func, typename dispatcher>
        TemplateDispatcher<dispatcher, Message, Func> handle(Func&& f, std::string info_msg)
        {
            return TemplateDispatcher<dispatcher, Message, Func>(q, this, std::forward<Func>(f), info_msg);
        }

        ~dispatcher() noexcept(false)
        {
            if (!chained)
            {
                wait_and_dispatch();
            }
        }
    };

    // sender receiver 公用一套queue
    class sender
    {
        queue* q;
    public:
        sender() : q(nullptr) {}

        explicit sender(queue* q_) : q(q_) {}

        template<typename Message>
        void send(Message const& msg)
        {
            if (q)
            {
                q->push(msg);
            }
        }
    };
    class receiver
    {
        // 完全拥有队列
        queue q;
    public:
        // receiver对象允许 隐式转换为sender对象, 前者拥有的队列被后者引用
        operator sender()
        {
            return sender(&q);
        }
        // 队列上的等待行为会创建一个dispatcher对象
        dispatcher wait()
        {
            return dispatcher(&q);
        }
    };


    template<typename PreviousDispatcher, typename Msg, typename Func>
    class TemplateDispatcher
    {
        std::string _msg;
        queue* q;
        PreviousDispatcher* prev;
        Func f;
        bool chained;
        TemplateDispatcher(TemplateDispatcher const&) = delete;
        TemplateDispatcher& operator= (TemplateDispatcher const&) = delete;

        template<typename Dispatcher, typename OtherMsg, typename OtherFunc>
        //①根据类模板TemplateDispatcher<>具现化而成的各种类型互为友类
        friend class TemplateDispatcher;

        void wait_and_dispatch()
        {
            while (true)
            {
                auto msg = q->wait_and_pop();
                // 消息处理妥善 则跳出
                if (dispatch(msg))
                {
                    break;
                }
            }
        }

        bool dispatch(std::shared_ptr<message_base> const& msg)
        {
            // 查验消息类别并调用对应的处理函数
            if (wrapped_message<Msg>* wrapper = dynamic_cast<wrapped_message<Msg>*>(msg.get()))
            {
                f(wrapper->contents);
                return true;
            }
            else
            {
                // 衔接前一个dispatcher对象, 形成连锁调用
                return prev->dispatch(msg);
            }
        }
    public:
        TemplateDispatcher(TemplateDispatcher&& other) : q(other.q), prev(other.prev),
            f(std::move(other.f)), chained(other.chained), _msg(other._msg)
        {
            other.chained = true;
        }
        TemplateDispatcher(queue* q_, PreviousDispatcher* prev_, Func&& f_, std::string msg) :
            q(q_), prev(prev_), f(std::forward<Func>(f_)), chained(false), _msg(msg)
        {
            prev_->chained = true;
        }

        // 链接成链的方式引入更多的处理函数
        template<typename OtherMsg, typename OtherFunc>
        TemplateDispatcher<TemplateDispatcher, OtherMsg, OtherFunc>
            handle(OtherFunc&& of, std::string info_msg)
        {
            std::cout << "Dispatcher  handle msg is " << info_msg << std::endl;
            return TemplateDispatcher<TemplateDispatcher, OtherMsg, OtherFunc>
                (q, this, std::forward<OtherFunc>(of), info_msg);
        }

        // 该类的析构函数的异常行为描述也是noexecpt(false)
        ~TemplateDispatcher() noexcept(false)
        {
            if (!chained)
            {
                wait_and_dispatch();
            }

            //std::cout << "~TemplateDispatcher  msg is " << _msg << std::endl;
        }
    };
}