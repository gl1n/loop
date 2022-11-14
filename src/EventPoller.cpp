#include "Poller/EventPoller.h"
#include "Network/Socket.h"
#include "Thread/TaskExecutor.h"
#include "Thread/ThreadPool.h"
#include "Thread/semaphore.h"
#include "Utils/TimeTicker.h"
#include "Utils/log.h"
#include "Utils/util.h"
#include <cstring>
#include <errno.h>
#include <exception>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <sys/epoll.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <utility>

// 将边缘触发设置成默认
#define toEpoll(event)                                                         \
  (((event)&EpollRead) ? EPOLLIN : 0) |                                        \
      (((event)&EpollWrite) ? EPOLLOUT : 0) |                                  \
      (((event)&EpollError) ? (EPOLLHUP | EPOLLERR) : 0) |                     \
      (((event)&EpollLT) ? 0 : EPOLLET)

namespace loop {

constexpr unsigned EPOLL_SIZE = 1024;

EventPoller::EventPoller() {
  //边缘触发需要设置成nonblocked
  SockUtil::setNoBlocked(_pipe.get_read_fd());
  SockUtil::setNoBlocked(_pipe.get_write_fd());

  // epoll
  _epoll_fd = epoll_create(EPOLL_SIZE);
  if (_epoll_fd == -1) {
    std::string err_str = "创建epoll文件描述符失败:";
    throw std::runtime_error(err_str + std::strerror(errno));
  }

  //这一步的目的没搞懂
  if (addEvent(_pipe.get_read_fd(), EpollRead | EpollWrite, [](int event) {}) ==
      -1) {
    throw std::runtime_error("epoll添加管道失败");
  }

  // _logger = Logger::Instance().shared_from_this();
}

EventPoller::~EventPoller() {
  shutdown();
  wait();
  if (_epoll_fd != -1) {
    close(_epoll_fd);
    _epoll_fd = -1;
  }
  _loop_thread_id = std::this_thread::get_id();
  onPipeEvent();
  InfoL << this;
}

int EventPoller::addEvent(int fd, int event, const PollEventCB &cb) {
  Ticker ticker(5, WarnL, true);
  if (!cb) {
    WarnL << "PollEventCB 为空!";
    return -1;
  }

  std::lock_guard<std::mutex> lck(_mutex_event_map);
  struct epoll_event ev = {0};
  ev.events = (toEpoll(event)) | EPOLLEXCLUSIVE; // EPOLLEXECLUSIVE避免惊群效应?
  ev.data.fd = fd;
  int ret = epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, fd, &ev);
  if (ret == 0) {
    _event_map.emplace(fd, std::make_shared<PollEventCB>(cb));
  }

  return ret;
}

int EventPoller::delEvent(int fd, const PollDelCB &delCb) {
  Ticker ticker(5, WarnL, true);
  //没有delCb就创建一个默认的delCb
  if (!delCb) {
    const_cast<PollDelCB &>(delCb) = [](bool) {};
  }

  int ret0 = epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, fd, NULL);
  int ret1 = 0;
  {
    std::lock_guard<std::mutex> lck(_mutex_event_map);
    ret1 = _event_map.erase(fd);
  }
  bool success = ret0 == 0 && ret1 > 0;
  delCb(success);
  return success ? 0 : -1;
}

int EventPoller::modifyEvent(int fd, int event) {
  Ticker ticker(5, WarnL, true);
  struct epoll_event ev = {0};
  ev.events = toEpoll(event);
  ev.data.fd = fd;
  return epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, fd, &ev);
}

bool EventPoller ::async(Task &&task, bool may_sync) {
  return async_l(std::move(task), may_sync, false);
}

bool EventPoller::async_first(Task &&task, bool may_sync) {
  return async_l(std::move(task), may_sync, true);
}

bool EventPoller::sync(Task &&task) { return sync_l(std::move(task), false); }

bool EventPoller::sync_first(Task &&task) {
  return sync_l(std::move(task), true);
}

bool EventPoller::async_l(TaskExecutor::Task &&task, bool may_sync,
                          bool first) {
  Ticker ticker(5, WarnL, true);
  if (!task) {
    return false;
  }
  //由_loop_thread调用async_l并且may_sync==true，那么直接执行，不用放入队列中
  if (may_sync && isCurrentThread()) {
    task();
    return true;
  }

  {
    std::lock_guard<std::mutex> lg(_mutex_task);
    if (first) {
      _task_list.emplace_front(std::move(task));
    } else {
      _task_list.emplace_back(std::move(task));
    }
  }
  //唤醒runLoop线程
  _pipe.write("", 1);
  return true;
}

bool EventPoller::sync_l(TaskExecutor::Task &&task, bool first) {
  Ticker ticker(5, WarnL, true);
  if (!task) {
    return false;
  }
  Semaphore sem;
  //借用async_l
  async_l(
      [&]() {
        task();
        sem.post();
      },
      true, first);
  sem.wait();
  return true;
}

bool EventPoller::onPipeEvent() {
  Ticker ticker(5, WarnL, true);
  char buf[2048];
  int err = 0;
  do {
    if (_pipe.read(buf, sizeof buf) > 0) {
      continue;
    }

    err = errno;
  } while (
      err !=
      EAGAIN); //将管道内的内容全部读出来，直到出现EAGAIN错误（again代表已经没有数据了）（边缘触发通用做法）

  decltype(_task_list) copylist;
  {
    std::lock_guard<std::mutex> lg(_mutex_task);
    copylist.swap(_task_list);
  }

  bool catchExitException = false;
  copylist.for_each([&catchExitException](TaskExecutor::Task &task) {
    try {
      task();
    } catch (ExitException &ex) {
      catchExitException = true;
    } catch (std::exception &ex) {
      ErrorL << "EventPoller执行异步任务捕获到异常:" << ex.what();
    }
  });
  return catchExitException;
}

void EventPoller::runLoop(bool blocked) {
  if (blocked) { //执行这个分支的是一个新的线程
    std::lock_guard<std::mutex> lg(_mutex_running);
    _loop_thread_id = std::this_thread::get_id();
    _sem_run_started.post(); //进行简单的同步

    //借用ThreadPool类的static方法来设置线程优先级
    ThreadPool::setPriority(ThreadPool::PRIORITY_HIGHEST);

    u_int64_t minDelay;

    struct epoll_event events[EPOLL_SIZE];
    bool run_flag = true;
    while (run_flag) {
      minDelay = getMinDelay(); //获取第一个延时任务的到期时间
      recordSleep();            //计算epoll_wait实际休眠时间
      int ret =
          epoll_wait(_epoll_fd, events, EPOLL_SIZE, minDelay ? minDelay : -1);
      recordWakeUp(); //计算epoll_wait实际休眠时间
      if (ret > 0) {
        for (int i = 0; i < ret; i++) {
          struct epoll_event &ev = events[i];
          int fd = ev.data.fd;
          int event = toEpoll(ev.events);

          if (fd == _pipe.get_read_fd()) {
            if (event & PollEvent::EpollError) {
              std::string err_msg = "Poller异常. 退出监听: ";
              WarnL << err_msg << strerror(errno);
              run_flag = false;
              continue;
            }
            if (onPipeEvent()) {
              //收到退出事件
              run_flag = false;
              continue;
            }
          }

          //其他文件描述符
          std::shared_ptr<PollEventCB> eventCb;
          {
            std::lock_guard<std::mutex> lg(_mutex_event_map);
            auto it = _event_map.find(fd);
            if (it == _event_map.end()) {
              WarnL << "未找到Poll事件回调对象!";
              epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, fd, NULL);
              continue;
            }
            eventCb = it->second;
          }
          try {
            (*eventCb)(event);
          } catch (std::exception &ex) {
            ErrorL << "EventPoller执行事件回调捕获到异常: " << ex.what();
          }
        }
        continue;
      }

      if (ret == -1) {
        std::string err_msg = "epoll_wait interrupted:";
        WarnL << err_msg << strerror(errno);
        continue;
      }
    }
  } else {
    //创建一个线程执行自己
    _loop_thread = new std::thread(&EventPoller::runLoop, this, true);
    _sem_run_started
        .wait(); //等_mutex_running和_loop_thread_id准备就绪才能退出此方法
  }
}

void EventPoller::wait() {
  //其实就是等待runLoop方法结束，因为runLoop结束才会释放锁
  //也可以用一个semaphore来实现吧?
  std::lock_guard<std::mutex> lg(_mutex_running);
}

void EventPoller::shutdown() {
  //相当于给_loop_thread发送了一个信号，命令其退出
  async_l([]() { throw ExitException(); }, false, true);

  if (_loop_thread) {
    try {
      _loop_thread->join(); //等待_loop_thread线程退出
    } catch (std::exception &ex) {
    }
    delete _loop_thread;
    _loop_thread = nullptr;
  }
}

bool EventPoller::isCurrentThread() {
  return _loop_thread_id == std::this_thread::get_id();
}

DelayTask::Ptr EventPoller::doDelayTask(u_int64_t delayMS,
                                        std::function<u_int64_t()> &&task) {
  DelayTaskImp::Ptr ret = std::make_shared<DelayTaskImp>(std::move(task));
  auto right_time = timeSinceEpochMillisecs() + delayMS;
  // async_first调用的async_l可以激活epoll_wait()，进而尽快处理task
  async_first(
      [this, right_time, ret]() { _delay_tasks.emplace(right_time, ret); });

  return ret;
}

u_int64_t EventPoller::getMinDelay() {
  //没有剩余的定时器了
  if (_delay_tasks.empty()) {
    return 0;
  }
  auto now = timeSinceEpochMillisecs();
  auto it = _delay_tasks.begin();
  //第一个任务未到期，说明所有定时任务均未到期
  if (now < it->first) {
    //返回距离第一个定时的时间
    return it->first - now;
  }
  return flushDelayTask(now);
}

u_int64_t EventPoller::flushDelayTask(u_int64_t now) {
  decltype(_delay_tasks) task_updated;

  for (auto it = _delay_tasks.begin();
       it != _delay_tasks.end() && it->first <= now;
       it = _delay_tasks.erase(it)) {
    //处理已到期的任务
    try {
      auto next_delay =
          (*(it->second))(); //有些任务需要重复执行，任务的返回值是执行的延迟
      if (next_delay) {
        task_updated.emplace(now + next_delay,
                             std::move(it->second)); //重新入队
      }
    } catch (std::exception &ex) {
      ErrorL << "EventPoller执行延时任务捕获到异常:" << ex.what();
    }
  }

  //将task_updated内的任务全部放入_delay_tasks中
  _delay_tasks.insert(task_updated.begin(), task_updated.end());

  auto it = _delay_tasks.begin();
  //没有定时任务了
  if (it == _delay_tasks.end()) {
    return 0;
  }
  //最近一个定时的任务的时延
  return it->first - now;
}

/*********************EvenPollerPool*********************/
int EventPollerPool::_s_pool_size = 0;

EventPollerPool::EventPollerPool() {
  auto size = _s_pool_size ? _s_pool_size : std::thread::hardware_concurrency();
  createExecutor(
      []() -> TaskExecutor::Ptr {
        EventPoller::Ptr ep = std::shared_ptr<EventPoller>(new EventPoller());
        ep->runLoop(false);
        return ep;
      },
      size);
  InfoL << "创建EventPoller个数:" << size;
}

EventPollerPool &EventPollerPool::Instance() {
  static EventPollerPool instance;
  return instance;
}

void EventPollerPool::setPoolSize(int size) { _s_pool_size = size; }

EventPoller::Ptr EventPollerPool::getFirstPoller() {
  //_executors.front()返回的是一个std::shared_ptr<TaskExecutor>
  // dynamic_pointer_cast尝试将TaskExecutor类型的智能指针转成子类EventPoller的智能指针
  //如果失败则得到的指针是空指针。如果用static_pointer_cast，那么转换一定会成功，不够安全
  // dynamic_pointer_cast是dynamic_cast的智能指针版本
  return std::dynamic_pointer_cast<EventPoller>(_executors.front());
}

EventPoller::Ptr EventPollerPool::getPoller() {
  return std::dynamic_pointer_cast<EventPoller>(getExecutor());
}

} // namespace loop