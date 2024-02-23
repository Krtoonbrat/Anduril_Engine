//
// Created by Hughe on 2/19/2024.
//

#ifndef ANDURIL_ENGINE_THREAD_H
#define ANDURIL_ENGINE_THREAD_H

#include <condition_variable>
#include <mutex>
#include <thread>

#include "Anduril.h"

class Thread {
public:
    Thread(libchess::Position &b, int n);
    virtual ~Thread();

    void idle();
    void startSearch();
    void waitForSearchFinish();
    int id() const { return ID; }

    std::unique_ptr<Anduril> engine;

private:
    int ID;
    std::mutex mutex;
    std::condition_variable cv;
    bool exit = false, searching = true;
    std::thread thread;
    libchess::Position &board;
};

class ThreadPool {
public:
    ~ThreadPool();

    void set(libchess::Position &b, int n);
    void clear();
    void startSearch();
    void wakeThreads();
    void waitForSearchFinish();

    Thread* mainThread() const { return threads.front(); }

    std::atomic_bool stop;
    int numThreads;

    // iterator functions
    auto cbegin() const { return threads.cbegin(); }
    auto cend() const { return threads.cend(); }
    auto begin() { return threads.begin(); }
    auto end() { return threads.end(); }

private:
    std::vector<Thread*> threads;

};


#endif //ANDURIL_ENGINE_THREAD_H
