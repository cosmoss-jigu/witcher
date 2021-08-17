#ifndef __HOT__ROWEX__MEMORY_GUARD__
#define __HOT__ROWEX__MEMORY_GUARD__

#include "hot/rowex/EpochBasedMemoryReclamationStrategy.hpp"

#include "pmdk.h"

namespace hot { namespace rowex {

class MemoryGuard {
	EpochBasedMemoryReclamationStrategy* mMemoryReclamation;

public:
	MemoryGuard(EpochBasedMemoryReclamationStrategy* memoryReclamation) : mMemoryReclamation(memoryReclamation) {
		mMemoryReclamation->enterCriticalSection();
	}

	~MemoryGuard() {
		mMemoryReclamation->leaveCriticialSection();
	}

	MemoryGuard(MemoryGuard const & other) = delete;
	MemoryGuard &operator=(MemoryGuard const & other) = delete;

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

#endif // __HOT_MEMORY_GUARD__
