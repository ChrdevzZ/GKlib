#include <GKlib.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>


#ifdef GKLIB_THREAD_LOCAL_STORAGE
static void host_handler(int) {}


static int check_linked_error()
{
  volatile int caught=0;
  size_t value=0;
  int depth=gk_cur_jbufs;

  if (!gk_sigtrap())
    return 1;
  switch (gk_sigcatch()) {
    case 0:
      gk_GetVMInfo(NULL, &value);
      break;
    case SIGERR:
      caught = SIGERR;
      break;
    default:
      caught = -1;
      break;
  }
  return gk_siguntrap() && caught == SIGERR && errno == EINVAL &&
      gk_cur_jbufs == depth ? 0 : 2;
}


static int check_overlap()
{
  std::atomic<int> failures(0);
  std::mutex mutex;
  std::condition_variable condition;
  int step=0;
  auto old_error = signal(SIGERR, host_handler);
  auto old_memory = signal(SIGMEM, host_handler);
  if (old_error == SIG_ERR || old_memory == SIG_ERR)
    return 3;

  auto wait_for = [&](int expected) {
    std::unique_lock<std::mutex> lock(mutex);
    condition.wait(lock, [&] { return step == expected; });
  };
  auto advance = [&](int next) {
    std::lock_guard<std::mutex> lock(mutex);
    step = next;
    condition.notify_all();
  };
  auto check_owner = [&] {
    auto error = signal(SIGERR, host_handler);
    auto memory = signal(SIGMEM, host_handler);
    if (error != host_handler || memory != host_handler)
      ++failures;
  };
  std::thread first([&] {
    int active=gk_sigtrap();
    if (!active) {
      ++failures;
      advance(1);
      wait_for(2);
    }
    else {
      switch (gk_sigcatch()) {
        case 0:
          advance(1);
          wait_for(2);
          if (check_linked_error() != 0)
            ++failures;
          break;
        default:
          ++failures;
          break;
      }
      if (!gk_siguntrap())
        ++failures;
    }
    check_owner();
    advance(3);
  });
  std::thread second([&] {
    wait_for(1);
    int active=gk_sigtrap();
    if (!active) {
      ++failures;
      advance(2);
      wait_for(3);
    }
    else {
      switch (gk_sigcatch()) {
        case 0:
          advance(2);
          wait_for(3);
          if (check_linked_error() != 0)
            ++failures;
          break;
        default:
          ++failures;
          break;
      }
      if (!gk_siguntrap())
        ++failures;
    }
  });
  first.join();
  second.join();
  check_owner();
  signal(SIGERR, old_error);
  signal(SIGMEM, old_memory);
  return failures == 0 ? 0 : 4;
}
#endif


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
    int error = expected == 17 ? EINVAL : ENOMEM;
    std::string reference(strerror(error));
    const char *message = gk_strerror(error);

    gk_cur_jbufs = expected;
    {
      std::unique_lock<std::mutex> lock(ready_mutex);
      ++ready;
      ready_condition.notify_all();
      ready_condition.wait(lock, [&] { return ready == 2; });
    }
    if (gk_cur_jbufs != expected)
      ++failures;
    if (message == NULL || reference != message)
      ++failures;
  };

  std::thread first(check_thread_local, 17);
  std::thread second(check_thread_local, 29);
  first.join();
  second.join();

  return failures == 0 ? check_overlap() : 2;
#else
  gk_cur_jbufs = 17;
  return gk_cur_jbufs == 17 ? 0 : 2;
#endif
}
