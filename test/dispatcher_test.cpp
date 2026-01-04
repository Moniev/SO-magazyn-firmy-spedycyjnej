#include "../include/Dispatcher.h"
#include <gtest/gtest.h>
#include <cstring>
#include <atomic>

class DispatcherTest : public ::testing::Test {
protected:
    SharedState mock_shared_memory;

    int lock_calls = 0;
    int unlock_calls = 0;
    SignalType last_signal = SIGNAL_NONE;
    int signal_calls = 0;

    std::function<void()> no_op = [](){};

    std::function<void()> mock_lock = [this](){ lock_calls++; };
    std::function<void()> mock_unlock = [this](){ unlock_calls++; };
    std::function<void(SignalType)> mock_signal = [this](SignalType s){
        last_signal = s;
        signal_calls++;
    };

    void SetUp() override {
        std::memset(&mock_shared_memory, 0, sizeof(SharedState));

        mock_shared_memory.dock_truck.is_present = true;
        mock_shared_memory.dock_truck.id = 10;
        mock_shared_memory.dock_truck.max_load = 10;
        mock_shared_memory.dock_truck.max_weight = 100.0;
        mock_shared_memory.dock_truck.current_load = 0;
        mock_shared_memory.dock_truck.current_weight = 0.0;
    }

    std::unique_ptr<Belt> createDummyBelt() {
        return std::make_unique<Belt>(
            &mock_shared_memory, no_op, no_op, no_op, no_op, no_op, no_op
        );
    }
};

TEST_F(DispatcherTest, SuccessfullyLoadsPackage) {
    auto belt = createDummyBelt();
    Dispatcher dispatcher(belt.get(), &mock_shared_memory, mock_lock, mock_unlock, mock_signal);

    Package p;
    p.id = 1;
    p.weight = 10.0;

    mock_shared_memory.belt[0] = p;
    mock_shared_memory.head = 0;
    mock_shared_memory.tail = 1;

    dispatcher.processNextPackage();

    EXPECT_EQ(mock_shared_memory.belt[0].id, 0);

    EXPECT_EQ(mock_shared_memory.dock_truck.current_load, 1);
    EXPECT_DOUBLE_EQ(mock_shared_memory.dock_truck.current_weight, 10.0);

    EXPECT_EQ(lock_calls, 1);
    EXPECT_EQ(unlock_calls, 1);

    EXPECT_EQ(signal_calls, 0);
}

TEST_F(DispatcherTest, HandlesNoTruckSafely) {
    auto belt = createDummyBelt();
    Dispatcher dispatcher(belt.get(), &mock_shared_memory, mock_lock, mock_unlock, mock_signal);

    mock_shared_memory.dock_truck.is_present = false;

    Package p; p.id = 2; p.weight = 5.0;
    mock_shared_memory.belt[0] = p;

    dispatcher.processNextPackage();

    EXPECT_EQ(mock_shared_memory.dock_truck.current_load, 0);

    EXPECT_GE(lock_calls, 1);
    EXPECT_GE(unlock_calls, 1);
}

TEST_F(DispatcherTest, RejectsOverweightPackageAndSignals) {
    auto belt = createDummyBelt();
    Dispatcher dispatcher(belt.get(), &mock_shared_memory, mock_lock, mock_unlock, mock_signal);

    mock_shared_memory.dock_truck.current_weight = 90.0;
    mock_shared_memory.dock_truck.current_load = 5;

    Package p; p.id = 3; p.weight = 20.0;
    mock_shared_memory.belt[0] = p;

    dispatcher.processNextPackage();

    EXPECT_DOUBLE_EQ(mock_shared_memory.dock_truck.current_weight, 90.0);
    EXPECT_EQ(mock_shared_memory.dock_truck.current_load, 5);

    EXPECT_EQ(signal_calls, 1);
    EXPECT_EQ(last_signal, SIGNAL_DEPARTURE);
}

TEST_F(DispatcherTest, RejectsCountOverflowAndSignals) {
    auto belt = createDummyBelt();
    Dispatcher dispatcher(belt.get(), &mock_shared_memory, mock_lock, mock_unlock, mock_signal);

    mock_shared_memory.dock_truck.current_load = 10;
    mock_shared_memory.dock_truck.max_load = 10;

    Package p; p.id = 4; p.weight = 1.0;
    mock_shared_memory.belt[0] = p;

    dispatcher.processNextPackage();

    EXPECT_EQ(mock_shared_memory.dock_truck.current_load, 10);
    EXPECT_EQ(last_signal, SIGNAL_DEPARTURE);
}

TEST_F(DispatcherTest, LoadsLastPackageAndSignalsDeparture) {
    auto belt = createDummyBelt();
    Dispatcher dispatcher(belt.get(), &mock_shared_memory, mock_lock, mock_unlock, mock_signal);

    mock_shared_memory.dock_truck.current_load = 9;
    mock_shared_memory.dock_truck.max_load = 10;

    Package p; p.id = 5; p.weight = 1.0;
    mock_shared_memory.belt[0] = p;

    dispatcher.processNextPackage();

    EXPECT_EQ(mock_shared_memory.dock_truck.current_load, 10);
    EXPECT_EQ(signal_calls, 1);
    EXPECT_EQ(last_signal, SIGNAL_DEPARTURE);
}
