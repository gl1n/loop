#pragma once

#include "Poller/PipeWrap.h"
#include "Thread/TaskExecutor.h"
#include "Thread/semaphore.h"
#include "Utils/log.h"
#include "Utils/util.h"
#include <exception>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <sys/types.h>
#include <thread>
#include <unordered_map>
#include <utility>

namespace loop {

enum PollEvent {
  EpollRead = 1 << 0,  //读事件
  EpollWrite = 1 << 1, //写事件
  EpollError = 1 << 2, //错误事件
  EpollLT = 1 << 3,    //水平触发
};

using PollEventCB = std::function<void(int event)>;
using PollDelCB = std::function<void(bool success)>;

class DelayTask : public uncopyable {
public:
  using Ptr = std::shared_ptr<DelayTask>;
  DelayTask() = default;
  ~DelayTask() = default;

  virtual void cancel() = 0;

  virtual u_int64_t operator()() const = 0;
};

class EventPoller : public TaskExecutor,
                    public std::enable_shared_from_this<EventPoller> {
public:
  using Ptr = std::shared_ptr<EventPoller>;
  friend class EventPollerPool; //用于构造EventPoll对象

  ~EventPoller(); //由于TaskExecutor的析构函数是虚函数，~EventPoller也是虚函数

  /**
   * 添加事件监听
   * @param fd 监听的文件描述符
   * @param event 事件类型，例如 PollEvent::Read | PollEvent::Write
   * @param eventCb 事件回调functional
   * @return -1:失败, 0:成功
   */
  int addEvent(int fd, int event, const PollEventCB &eventCb);

  /**
   * 删除事件监听
   * @param fd 监听的文件描述符
   * @param delCb 删除成功回调functional
   * @return -1:失败, 0:成功
   */
  int delEvent(int fd, const PollDelCB &delCb = nullptr);

  int modifyEvent(int fd, int event);

  /**
   * 异步执行任务
   * @param task 任务
   * @param may_sync 是否允许同步执行该任务
   * @return 任务是否添加成功
   */
  bool async(Task &&task, bool may_sync = true) override;

  /**
   * 最高优先级方式异步执行任务
   * @param task 任务
   * @param may_sync 是否允许同步执行该任务
   * @return 任务是否添加成功
   */
  bool async_first(Task &&task, bool may_sync = true) override;

  /**
   * 同步执行任务
   * @param task 任务
   */
  bool sync(Task &&task) override;

  /**
   * 最高优先级方式同步执行任务
   * @param task 任务
   */
  bool sync_first(Task &&task) override;

  bool isCurrentThread();

  /**
   * 延时执行某个任务
   * @param delayMS 延时毫秒数
   * @param task
   * 任务。任务的返回值为0代表不再重复任务，否则为下次执行延迟。如果任务中抛出异常，默认不重复任务
   * @return 可以用于取消任务
   */
  DelayTask::Ptr doDelayTask(u_int64_t delayMS,
                             std::function<u_int64_t()> &&task);

private:
  /**
   * 本对象只允许在 EventPollerPool 中构造
   */
  EventPoller();

  //供内部调用的原始接口
  bool async_l(TaskExecutor::Task &&task, bool may_sync = true,
               bool first = false);

  //供内部调用的原始接口
  bool sync_l(TaskExecutor::Task &&task, bool first = false);

  /**
   * 循环处理epoll事件
   * @param blocked
   * 为true时此方法会阻塞，否则会创建一个独立的线程重新执行此方法，并且非阻塞
   */
  void runLoop(bool blocked = true);

  /**
   * 清空_pipe内的数据并执行_task_list内的所有任务
   * @return 是否捕获到ExitException
   */
  bool onPipeEvent();

  /**
   * 等待轮询结束
   */
  void wait();

  /**
   * 命令结束轮询
   */
  void shutdown();

  /**
   * 当存在需要执行的延时任务的被调用
   * 处理延时任务并返回下一个即将到达的任务的时延
   */
  u_int64_t flushDelayTask(u_int64_t now);

  /**
   * 处理时延任务逻辑
   * @return 下一个时延任务到达的时间
   */
  u_int64_t getMinDelay();

private:
  class ExitException : public std::exception {
  public:
    ExitException() = default;
    ~ExitException() = default;
  };

  class DelayTaskImp : public DelayTask {
  public:
    using Ptr = std::shared_ptr<DelayTaskImp>;
    template <typename FUNC> DelayTaskImp(FUNC &&func) {
      _strong_task = std::make_shared<std::function<u_int64_t()>>(
          std::forward<FUNC>(func));
      _weak_task = _strong_task;
    }

    ~DelayTaskImp() = default;

    void cancel() override { _strong_task.reset(); }

    u_int64_t operator()() const override {
      auto strong_task = _weak_task.lock();
      //如果任务未被取消
      if (strong_task) {
        return (*strong_task)();
      }
      return 0;
    }

  private:
    std::shared_ptr<std::function<u_int64_t()>> _strong_task;
    std::weak_ptr<std::function<u_int64_t()>> _weak_task;
  };

private:
  //正在运行事件循环时该锁处于被锁定状态
  std::mutex _mutex_running;
  //执行事件循环的线程
  std::thread *_loop_thread = nullptr;
  //事件循环的线程id
  std::thread::id _loop_thread_id;
  //通知事件循环的线程已启动
  Semaphore _sem_run_started;
  //内部事件管道
  PipeWrap _pipe;
  //普通任务队列
  List<TaskExecutor::Task> _task_list;
  std::mutex _mutex_task;

  // epoll相关
  int _epoll_fd = -1;
  std::unordered_map<int, std::shared_ptr<PollEventCB>> _event_map;
  std::mutex _mutex_event_map;

  //定时任务队列
  std::multimap<u_int64_t, DelayTaskImp::Ptr> _delay_tasks;
};

class EventPollerPool : public std::enable_shared_from_this<EventPollerPool>,
                        public TaskExecutorGetterImp {
public:
  using Ptr = std::shared_ptr<EventPollerPool>;

  ~EventPollerPool() = default;

  /**
   * 获取单例
   */
  static EventPollerPool &Instance();

  /**
   * 设置EventPoller个数，在Pool单例创建前有效
   */
  static void setPoolSize(int size);

  /**
   * 获取第一个实例
   */
  EventPoller::Ptr getFirstPoller();

  /**
   * 获取轻负载实例
   */
  EventPoller::Ptr getPoller();

private:
  EventPollerPool();
  static int _s_pool_size;
};

} // namespace loop