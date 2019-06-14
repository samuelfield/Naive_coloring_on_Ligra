#include <pthread.h>

#ifndef __RWLOCK_HPP__
#define __RWLOCK_HPP__

class RWLock {
    pthread_rwlock_t rwlock;

public:
    void init() {
        pthread_rwlock_init(&rwlock, NULL);
    }

    void readLock() {
        pthread_rwlock_rdlock(&rwlock);
    }

    int tryReadLock() {
        return pthread_rwlock_tryrdlock(&rwlock);
    }

    void writeLock() {
        pthread_rwlock_wrlock(&rwlock);
    }

    void unlock() {
        pthread_rwlock_unlock(&rwlock);
    }

    void destroy() {
        pthread_rwlock_destroy(&rwlock);
    }
};

#endif //__RWLOCK_HPP__
