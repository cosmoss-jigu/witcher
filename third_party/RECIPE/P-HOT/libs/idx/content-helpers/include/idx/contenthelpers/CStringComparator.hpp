#ifndef __IDX__CONTENTHELPERS__C_STRING_COMPARATOR__HPP__
#define __IDX__CONTENTHELPERS__C_STRING_COMPARATOR__HPP__

/** @author robert.binna@uibk.ac.at */

#include "pmdk.h"

namespace idx { namespace contenthelpers {

/**
 * Comparator which lexicographically compares two c-strings
 */
class CStringComparator {
public:
	inline bool operator()(const char* first, const char* second) const {
		return strcmp(first, second) < 0;
	};

	void *operator new(size_t size) {
		void *ret;
		ret = nvm_alloc(size);
		return ret;
	}
	void operator delete(void *p) {
		nvm_free(p);
	}
};

} }

#endif

