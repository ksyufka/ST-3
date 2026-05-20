// Copyright 2021 GHA Test Team

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <exception>
#include <memory>
#include <thread>

#include "TimedDoor.h"

class MockTimerClient : public TimerClient {
 public:
  MOCK_METHOD(void, Timeout, (), (override));
};

class MockDoor : public Door {
 public:
  MOCK_METHOD(void, lock, (), (override));
  MOCK_METHOD(void, unlock, (), (override));
  MOCK_METHOD(bool, isDoorOpened, (), (override));
};

class DoorController {
 public:
  void Close(Door* door) {
    door->lock();
  }

  void Open(Door* door) {
    door->unlock();
  }

  bool IsOpened(Door* door) {
    return door->isDoorOpened();
  }
};

class TimedDoorTest : public ::testing::Test {
 protected:
  std::unique_ptr<TimedDoor> door;

  void SetUp() override {
    door = std::make_unique<TimedDoor>(60);
  }

  void TearDown() override {
    door.reset();
  }

  std::thread RunUnlock(TimedDoor& targetDoor, std::exception_ptr* error) {
    return std::thread([&targetDoor, error]() {
      try {
        targetDoor.unlock();
      } catch (...) {
        *error = std::current_exception();
      }
    });
  }

  bool WaitUntilOpened(TimedDoor& targetDoor, int timeoutMs = 30) {
    for (int i = 0; i < timeoutMs; ++i) {
      if (targetDoor.isDoorOpened()) {
        return true;
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    return false;
  }
};

TEST_F(TimedDoorTest, ConstructorStoresTimeout) {
  EXPECT_EQ(60, door->getTimeOut());
}

TEST_F(TimedDoorTest, DoorIsClosedAfterConstruction) {
  EXPECT_FALSE(door->isDoorOpened());
}

TEST(TimerTest, RegisteredClientReceivesTimeoutCallback) {
  Timer timer;
  MockTimerClient client;

  EXPECT_CALL(client, Timeout()).Times(1);

  timer.tregister(0, &client);
}

TEST(DoorInterfaceTest, ControllerCallsLockOnDoor) {
  DoorController controller;
  MockDoor mockDoor;

  EXPECT_CALL(mockDoor, lock()).Times(1);

  controller.Close(&mockDoor);
}

TEST(DoorInterfaceTest, ControllerCallsUnlockOnDoor) {
  DoorController controller;
  MockDoor mockDoor;

  EXPECT_CALL(mockDoor, unlock()).Times(1);

  controller.Open(&mockDoor);
}

TEST(DoorInterfaceTest, ControllerReturnsDoorStateFromInterface) {
  DoorController controller;
  MockDoor mockDoor;

  EXPECT_CALL(mockDoor, isDoorOpened())
      .Times(1)
      .WillOnce(::testing::Return(true));

  EXPECT_TRUE(controller.IsOpened(&mockDoor));
}

TEST_F(TimedDoorTest, ThrowStateDoesNotThrowForClosedDoor) {
  EXPECT_NO_THROW(door->throwState());
}

TEST_F(TimedDoorTest, ThrowStateThrowsForOpenedDoor) {
  std::exception_ptr error;
  std::thread worker = RunUnlock(*door, &error);

  ASSERT_TRUE(WaitUntilOpened(*door));
  EXPECT_THROW(door->throwState(), std::runtime_error);

  door->lock();
  worker.join();
  EXPECT_EQ(nullptr, error);
}

TEST_F(TimedDoorTest, AdapterTimeoutThrowsForOpenedDoor) {
  DoorTimerAdapter adapter(*door);
  std::exception_ptr error;
  std::thread worker = RunUnlock(*door, &error);

  ASSERT_TRUE(WaitUntilOpened(*door));
  EXPECT_THROW(adapter.Timeout(), std::runtime_error);

  door->lock();
  worker.join();
  EXPECT_EQ(nullptr, error);
}

TEST(TimedDoorBehaviorTest, UnlockThrowsIfDoorRemainsOpen) {
  TimedDoor shortTimeoutDoor(0);

  EXPECT_THROW(shortTimeoutDoor.unlock(), std::runtime_error);
}

TEST(TimedDoorBehaviorTest, UnlockDoesNotThrowIfDoorIsClosedBeforeTimeout) {
  TimedDoor timedDoor(80);
  std::exception_ptr error;

  std::thread worker([&timedDoor, &error]() {
    try {
      timedDoor.unlock();
    } catch (...) {
      error = std::current_exception();
    }
  });

  for (int i = 0; i < 20 && !timedDoor.isDoorOpened(); ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  timedDoor.lock();
  worker.join();

  EXPECT_EQ(nullptr, error);
  EXPECT_FALSE(timedDoor.isDoorOpened());
}

TEST(TimedDoorBehaviorTest, UnlockOpensDoorBeforeTimeoutExpires) {
  TimedDoor timedDoor(80);
  std::exception_ptr error;

  std::thread worker([&timedDoor, &error]() {
    try {
      timedDoor.unlock();
    } catch (...) {
      error = std::current_exception();
    }
  });

  bool opened = false;
  for (int i = 0; i < 30; ++i) {
    if (timedDoor.isDoorOpened()) {
      opened = true;
      break;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  timedDoor.lock();
  worker.join();

  EXPECT_TRUE(opened);
  EXPECT_EQ(nullptr, error);
}
