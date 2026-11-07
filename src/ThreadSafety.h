#pragma once
/**
 * Copyright Martin Raiber. All Rights Reserved.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

// Clang thread safety analysis annotations (-Wthread-safety).
// See https://clang.llvm.org/docs/ThreadSafetyAnalysis.html
// The macros expand to nothing on compilers without support.
// std::mutex and std::scoped_lock are only annotated in libc++ with
// _LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS, see cmake/thread-safety-check.sh.
// std::unique_lock is not annotated, use CondVarLock below for condition variables or manual unlock/lock

#include <condition_variable>
#include <mutex>

#if defined(__clang__)
#define THREAD_ANNOTATION_ATTRIBUTE__(x) __attribute__((x))
#else
#define THREAD_ANNOTATION_ATTRIBUTE__(x)
#endif

#define CAPABILITY(x) THREAD_ANNOTATION_ATTRIBUTE__(capability(x))
#define SCOPED_CAPABILITY THREAD_ANNOTATION_ATTRIBUTE__(scoped_lockable)
#define GUARDED_BY(x) THREAD_ANNOTATION_ATTRIBUTE__(guarded_by(x))
#define PT_GUARDED_BY(x) THREAD_ANNOTATION_ATTRIBUTE__(pt_guarded_by(x))
#define ACQUIRED_BEFORE(...) THREAD_ANNOTATION_ATTRIBUTE__(acquired_before(__VA_ARGS__))
#define ACQUIRED_AFTER(...) THREAD_ANNOTATION_ATTRIBUTE__(acquired_after(__VA_ARGS__))
#define REQUIRES(...) THREAD_ANNOTATION_ATTRIBUTE__(requires_capability(__VA_ARGS__))
#define REQUIRES_SHARED(...) THREAD_ANNOTATION_ATTRIBUTE__(requires_shared_capability(__VA_ARGS__))
#define ACQUIRE(...) THREAD_ANNOTATION_ATTRIBUTE__(acquire_capability(__VA_ARGS__))
#define ACQUIRE_SHARED(...) THREAD_ANNOTATION_ATTRIBUTE__(acquire_shared_capability(__VA_ARGS__))
#define RELEASE(...) THREAD_ANNOTATION_ATTRIBUTE__(release_capability(__VA_ARGS__))
#define RELEASE_SHARED(...) THREAD_ANNOTATION_ATTRIBUTE__(release_shared_capability(__VA_ARGS__))
#define TRY_ACQUIRE(...) THREAD_ANNOTATION_ATTRIBUTE__(try_acquire_capability(__VA_ARGS__))
#define EXCLUDES(...) THREAD_ANNOTATION_ATTRIBUTE__(locks_excluded(__VA_ARGS__))
#define ASSERT_CAPABILITY(x) THREAD_ANNOTATION_ATTRIBUTE__(assert_capability(x))
#define RETURN_CAPABILITY(x) THREAD_ANNOTATION_ATTRIBUTE__(lock_returned(x))
#define NO_THREAD_SAFETY_ANALYSIS THREAD_ANNOTATION_ATTRIBUTE__(no_thread_safety_analysis)

// Capability representing "running on a specific thread". Never locked; held for the
// analysis after calling a function annotated with ASSERT_CAPABILITY that checks the thread
class CAPABILITY("role") ThreadRole
{
};

// Annotated std::unique_lock replacement for waiting on a std::condition_variable or
// temporarily unlocking (std::unique_lock is not annotated in libc++). Use std::scoped_lock otherwise
class SCOPED_CAPABILITY CondVarLock
{
    std::unique_lock<std::mutex> l;

public:
    explicit CondVarLock(std::mutex& m) ACQUIRE(m)
        : l(m)
    {
    }

    ~CondVarLock() RELEASE()
    {
    }

    void lock() ACQUIRE()
    {
        l.lock();
    }

    void unlock() RELEASE()
    {
        l.unlock();
    }

    // Mutex is held again on return
    void wait(std::condition_variable& cond)
    {
        cond.wait(l);
    }

    CondVarLock(const CondVarLock&) = delete;
    CondVarLock& operator=(const CondVarLock&) = delete;
};
