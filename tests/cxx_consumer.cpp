#include <GKlib.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

int main()
{
  int values[] = {3, 1, 2};

  gk_isorti(3, values);
  if (values[0] != 1 || values[1] != 2 || values[2] != 3)
    return 1;

#ifdef GKLIB_THREAD_LOCAL_STORAGE
  std::atomic<int> failures(0);
  std::condition_variable ready_condition;
  std::mutex ready_mutex;
  int ready = 0;

  auto check_thread_local = [&](int expected) {
    gk_cur_jbufs = expected;
    {
      std::unique_lock<std::mutex> lock(ready_mutex);
      ++ready;
      ready_condition.notify_all();
      ready_condition.wait(lock, [&] { return ready == 2; });
    }
    if (gk_cur_jbufs != expected)
      ++failures;
  };

  std::thread first(check_thread_local, 17);
  std::thread second(check_thread_local, 29);
  first.join();
  second.join();

  return failures == 0 ? 0 : 2;
#else
  gk_cur_jbufs = 17;
  return gk_cur_jbufs == 17 ? 0 : 2;
#endif
}
