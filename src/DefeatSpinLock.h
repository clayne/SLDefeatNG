#pragma once
//#include <Windows.h>
#include <thread>

namespace SexLabDefeat {

    class SpinLock {

        //RE::BSSpinLock lock;

    public:
        enum {
            kFastSpinThreshold = 10000,
        };

        SpinLock() : _owningThread(0), _lockCount(0){};

        void spinLock(std::uint32_t a_pauseAttempts = 0) noexcept {
            std::uint32_t myThreadID = std::this_thread::get_id()._Get_underlying_id();

            _mm_lfence();
            if (_owningThread == myThreadID) {
                ::_InterlockedIncrement(&_lockCount);
            } else {
                std::uint32_t attempts = 0;
                std::uint32_t counter = 0;
                if (::_InterlockedCompareExchange(&_lockCount, 1, 0)) {
                    do {
                        ++attempts;
                        ++counter;
                        if (counter >= 100) {
                            SKSE::log::error("SpinLock {} attempts!", counter);
                            counter = 0;
                        }
                        _mm_pause();
                        if (attempts >= a_pauseAttempts) {
                            std::uint32_t spinCount = 0;
                            while (::_InterlockedCompareExchange(&_lockCount, 1, 0)) {
                                ++counter;
                                if (counter >= 100) {
                                    SKSE::log::error("SpinLock {} attempts!", counter);
                                    counter = 0;
                                }
                                std::this_thread::sleep_for(++spinCount < kFastSpinThreshold ? 0ms : 1ms);
                            }
                            break;
                        }

                    } while (::_InterlockedCompareExchange(&_lockCount, 1, 0));
                    _mm_lfence();
                }

                _owningThread = myThreadID;
                _mm_sfence();
            }
        }
        void spinUnlock() noexcept {
            std::uint32_t myThreadID = std::this_thread::get_id()._Get_underlying_id();

            _mm_lfence();
            if (_owningThread == myThreadID) {
                if (_lockCount == 1) {
                    _owningThread = 0;
                    _mm_mfence();
                    ::_InterlockedCompareExchange(&_lockCount, 0, 1);
                } else {
                    ::_InterlockedDecrement(&_lockCount);
                }
            }
        }

    private:
        volatile std::uint32_t _owningThread;
        volatile long _lockCount;
    };

    class UniqueSpinLock {
    public:
        UniqueSpinLock(SpinLock& a_lock) {
            _lock = &a_lock;
            // DEBUG("UniqueLock - {:016X} - {:016X}",(uintptr_t)this,(uintptr_t)_lock)
            _lock->spinLock();
        }
        ~UniqueSpinLock() {
            // DEBUG("~UniqueLock - {:016X} - {:016X}",(uintptr_t)this,(uintptr_t)_lock)
            _lock->spinUnlock();
        }
        UniqueSpinLock() = delete;
        UniqueSpinLock(UniqueSpinLock const&) = delete;
        void operator=(UniqueSpinLock const& x) = delete;

    private:
        mutable SpinLock* _lock;
    };
}