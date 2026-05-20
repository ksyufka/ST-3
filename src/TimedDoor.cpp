// Copyright 2021 GHA Test Team
#include "TimedDoor.h"

#include <chrono>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace {
std::mutex g_door_state_mutex;
}  // namespace

DoorTimerAdapter::DoorTimerAdapter(TimedDoor& timedDoor) : door(timedDoor) {}

void DoorTimerAdapter::Timeout() {
  door.throwState();
}

TimedDoor::TimedDoor(int timeout)
    : adapter(new DoorTimerAdapter(*this)), iTimeout(timeout), isOpened(false) {}

bool TimedDoor::isDoorOpened() {
  std::lock_guard<std::mutex> lock(g_door_state_mutex);
  return isOpened;
}

void TimedDoor::unlock() {
  {
    std::lock_guard<std::mutex> lock(g_door_state_mutex);
    isOpened = true;
  }

  Timer timer;
  timer.tregister(iTimeout, adapter);
}

void TimedDoor::lock() {
  std::lock_guard<std::mutex> lock(g_door_state_mutex);
  isOpened = false;
}

int TimedDoor::getTimeOut() const {
  return iTimeout;
}

void TimedDoor::throwState() {
  std::lock_guard<std::mutex> lock(g_door_state_mutex);
  if (isOpened) {
    throw std::runtime_error("The door is still open");
  }
}

void Timer::sleep(int timeout) {
  const int normalized_timeout = timeout < 0 ? 0 : timeout;
  std::this_thread::sleep_for(std::chrono::milliseconds(normalized_timeout));
}

void Timer::tregister(int timeout, TimerClient* timerClient) {
  client = timerClient;
  sleep(timeout);
  if (client != nullptr) {
    client->Timeout();
  }
}
