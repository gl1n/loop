#pragma once

#include <Utils/list.h>
#include <Utils/semaphore.h>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <list>
#include <memory>
#include <mutex>
#include <ostream>
#include <queue>
#include <sstream>
#include <string>
#include <sys/time.h>
#include <thread>
#include <utility>

namespace loop {

class LogEvent;
class LogChannel;
class AsyncWriter;

enum class LogLevel { Trace, Debug, Info, Warn, Error };
class Logger {
public:
  //单例
  static Logger &Instance();
  Logger(const Logger &) = delete;            //禁用复制构造函数
  Logger &operator=(const Logger &) = delete; //禁用赋值函数

  //写event
  void write_event(std::shared_ptr<LogEvent> event);
  //增加channel
  void add_channel(std::shared_ptr<LogChannel> ch);
  //写到_channels中
  void write_to_channels(std::shared_ptr<LogEvent> event);
  //设置为异步
  void set_async();

private:
  //单例
  Logger() {} //默认同步

private:
  std::shared_ptr<AsyncWriter> _writer;
  std::list<std::shared_ptr<LogChannel>> _channels; //输出目的地
};

class LogEvent : public std::ostringstream {
public:
  using Ptr = std::shared_ptr<LogEvent>;
  LogEvent(LogLevel level, const char *file, const char *function, int line);
  ~LogEvent() = default;

public:
  LogLevel _level;
  std::string _file;
  std::string _function;
  int _line;
  struct timeval _tv;
  std::thread::id _thread_id;
};

class LogEventCapture {
public:
  LogEventCapture(Logger &logger, LogLevel level, const char *file,
                  const char *function, int line);

  ~LogEventCapture();

  //模板不能放在cpp里
  template <class T> LogEventCapture &operator<<(T &&data) {
    *_event << std::forward<T>(data);
    return *this;
  }
  LogEventCapture &operator<<(std::ostream &(*f)(std::ostream &));

private:
  std::shared_ptr<LogEvent> _event; //生成的event
  Logger &_logger;                  //目标logger
};

class AsyncWriter {
public:
  AsyncWriter();
  ~AsyncWriter();
  void push_event(LogEvent::Ptr event, Logger *logger);

private:
  void run();
  void flush_all();

private:
  std::mutex _mtx;
  Semaphore _sem;
  List<std::pair<LogEvent::Ptr, Logger *>> _pending;
  bool _exit;
  std::shared_ptr<std::thread> _thread;
};

class LogChannel {
public:
  virtual ~LogChannel() {}
  virtual void write(LogEvent::Ptr event) = 0;

protected:
  void format(LogEvent::Ptr event, std::ostream &stream, bool if_color);
};

class ConsoleChannel : public LogChannel {
public:
  void write(LogEvent::Ptr event) override;
};
class FileChannel : public LogChannel {
public:
  FileChannel();
  ~FileChannel();
  void write(LogEvent::Ptr event) override;
  void reopen();

private:
  std::string _path;
  std::fstream _stream;
};
} // namespace loop

/*********************宏定义***********************/
#define LOG(level)                                                             \
  loop::LogEventCapture(loop::Logger::Instance(), level, __FILE__,             \
                        __FUNCTION__, __LINE__)

#define TraceL LOG(loop::LogLevel::Trace)
#define DebugL LOG(loop::LogLevel::Debug)
#define InfoL LOG(loop::LogLevel::Info)
#define WarnL LOG(loop::LogLevel::Warn)
#define ErrorL LOG(loop::LogLevel::Error)

// 默认初始化
#define LOG_DEFAULT                                                            \
  loop::Logger::Instance().add_channel(                                        \
      std::make_shared<loop::ConsoleChannel>());