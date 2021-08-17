#ifndef __HOT__ROWEX__SPIN_LOCK__
#define __HOT__ROWEX__SPIN_LOCK__

#include <atomic>

#include "pmdk.h"

namespace hot { namespace rowex {

class SpinLock {
	std::atomic_flag mFlag;

public:
	SpinLock() : mFlag(ATOMIC_FLAG_INIT) {
	}

	void lock() {
		bool wasSetBefore;
		while((wasSetBefore = mFlag.test_and_set())) {
			_mm_pause();
		}
	}

	void unlock() {
		mFlag.clear(std::memory_order_release);
	}

	void *operator new(size_t size) {
		void *ret;
		ret = nvm_alloc(size);
		return ret;
	}
	void operator delete(void *p) {
		nvm_free(p);
	}
};

}}

#endif
