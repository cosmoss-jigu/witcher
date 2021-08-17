#ifndef _PMDK_H
#define _PMDK_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#include <libpmemobj.h>

// 1 MiB
#define MIB_SIZE ((size_t)(1024 * 1024))

typedef struct p_hot_root_obj
{
  void *p_hot_ptr;
} root_obj;

root_obj *init_nvmm_pool(char *path,
                         size_t size,
                         char *layout_name,
                         int *is_created);

static PMEMobjpool *g_pop;

root_obj *init_nvmm_pool(char *path,
                         size_t size,
                         char *layout_name,
                         int *is_created) {
  /* You should not init twice. */
  assert(g_pop == NULL);

  /* check if the path exsists already, else create one
  * or open the exsisting file*/
  if (access(path, F_OK) != 0) {
    if ((g_pop = pmemobj_create(path, layout_name, size*MIB_SIZE, 0666)) == NULL) {
      perror("failed to create pool\n");
      return NULL;
    }
    *is_created = 1;
  } else {
    if ((g_pop = pmemobj_open(path, layout_name)) == NULL) {
      perror("failed to open th exsisting pool\n");
      return NULL;
    }
    *is_created = 0;
  }
  /* allocate a root in the nvmem pool, here on root_obj*/
  PMEMoid g_root = pmemobj_root(g_pop, sizeof(root_obj));
  return (root_obj*)pmemobj_direct(g_root);
}

int destroy_nvmm_pool() {
  pmemobj_close(g_pop);
  g_pop = NULL;
  return 0;
}

void *nvm_alloc(size_t size) {
  int ret;
  PMEMoid _addr;

  ret = pmemobj_alloc(g_pop, &_addr, size, 0, NULL, NULL);
  if (ret) {
    perror("nvmm memory allocation failed\n");
  }
  return pmemobj_direct(_addr);
}

void nvm_free(void *addr) {
  PMEMoid _addr;

  _addr = pmemobj_oid(addr);
  pmemobj_free(&_addr);
  return;
}
int nvm_aligned_alloc(void **res, size_t align, size_t len){
	int ret;
	PMEMoid _addr;
	unsigned char *mem, *_new, *end;
	size_t header, footer;
	PMEMobjpool *pop;

	pop = g_pop;

	if ((align & -align) != align) return EINVAL;
	if (len > SIZE_MAX - align) return ENOMEM;

	if (align <= 4*sizeof(size_t)) {

		ret = pmemobj_alloc(pop, &_addr, len, 0, NULL, NULL);
		if (ret) {
			perror("[0] nvmm memory allocation failed\n");
			return -1;
		}

		*res = pmemobj_direct(_addr);
		if (!*res){
			perror("[1] nvmm memory allocation failed\n");
			return -1;
		}

		return 0;
	}

	ret = pmemobj_alloc(pop, &_addr, (len + align-1), 0, NULL, NULL);
	if (ret) {
		perror("[00] nvmm memory allocation failed\n");
		return -1;
	}

	mem = reinterpret_cast<unsigned char*>(pmemobj_direct(_addr));
	if (!mem){
		perror("[11] nvmm memory allocation failed\n");
		return -1;
	}


	header = ((size_t *)mem)[-1];
	end = mem + (header & -8);
	footer = ((size_t *)end)[-2];
	_new = reinterpret_cast<unsigned char*>((uintptr_t)mem + align-1 & -align);

	if (!(header & 7)) {
		((size_t *)_new)[-2] = ((size_t *)mem)[-2] + (_new-mem);
		((size_t *)_new)[-1] = ((size_t *)mem)[-1] - (_new-mem);
		*res = _new;
		return 0;
	}

	((size_t *)mem)[-1] = header&7 | _new-mem;
	((size_t *)_new)[-2] = footer&7 | _new-mem;
	((size_t *)_new)[-1] = header&7 | end-_new;
	((size_t *)end)[-2] = footer&7 | end-_new;

	if (_new != mem) nvm_free(mem);
	*res = _new;
	return 0;
}



#endif /* pmdk.h*/
