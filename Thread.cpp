//
// Created by Hughe on 2/19/2024.
//

#include "Thread.h"

Thread::Thread(libchess::Position &b, int n) : ID(n),
                        board(b),
                        engine(std::make_unique<Anduril>(n)),
                        thread(&Thread::idle, this){
    waitForSearchFinish();
}

// destructor sets exit to true, then calls startSearch to wake up the thread
Thread::~Thread() {
    exit = true;
    startSearch();
    thread.join();
}

// Wakes up the thread that is going to start searching
void Thread::startSearch() {
    mutex.lock();
    searching = true;
    mutex.unlock();
    cv.notify_one();
}

// Blocks on condition variable until the thread is done searching
void Thread::waitForSearchFinish() {
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait(lock, [&] { return !searching; });
}

// Thread will be parked here and blocked on the condition variable until there is work
void Thread::idle() {

    while (true) {
        std::unique_lock<std::mutex> lock(mutex);
        searching = false;
        cv.notify_one();
        cv.wait(lock, [&] { return searching; });

        if (exit) {
            break;
        }

        lock.unlock();

        engine->go(board);

    }

}

ThreadPool::~ThreadPool() {
    if (threads.size() > 0) {
        mainThread()->waitForSearchFinish();

        while (threads.size() > 0) {
            delete threads.back();
            threads.pop_back();
        }

    }
}

// creates/destroys threads to match the requested number
void ThreadPool::set(libchess::Position &b, int n) {
    // destroy existing threads
    if (threads.size() > 0) {
        mainThread()->waitForSearchFinish();

        while (threads.size() > 0) {
            delete threads.back();
            threads.pop_back();
        }
    }

    if (n > 0) {
        while (threads.size() < n) {
            threads.push_back(new Thread(b, threads.size()));
        }

        mainThread()->waitForSearchFinish();
    }
}

// clears the search information from each thread
void ThreadPool::clear() {
    for (Thread* t : threads) {
        //t->engine->pTable = HashTable<PawnEntry, 8>();
        //t->engine->evalTable = HashTable<SimpleNode, 8>();
        t->engine->resetHistories();
    }
}

// starts the search on the main thread
// main thread starts the rest of the threads
void ThreadPool::startSearch() {
    mainThread()->waitForSearchFinish();
    stop = false;
    mainThread()->startSearch();
}

// wakes the non main threads
void ThreadPool::wakeThreads() {
    for (Thread* t : threads) {
        if (t != threads.front()) {
            t->startSearch();
        }
    }
}

// waits for non-main threads to finish searching
void ThreadPool::waitForSearchFinish() {
    for (Thread* t : threads) {
        if (t != threads.front()) {
            t->waitForSearchFinish();
        }
    }
}