## libpmemobj

### Bug 1:
- Atomicity Bug
- PM allocation
- [issue](https://github.com/pmem/pmdk/issues/4945)


## WOART
- [github](https://github.com/SeKwonLee/WORT/tree/a11041369474d9d4672dd61c9e2aa24bbc0f2f7d/src/woart)
- commit: a11041369474d9d4672dd61c9e2aa24bbc0f2f7d

### Bug 2:
- Atomicity Bug
- [loc](https://github.com/SeKwonLee/WORT/blob/a11041369474d9d4672dd61c9e2aa24bbc0f2f7d/src/woart/woart.c#L727)
  ```c++
  *ref = (art_node*)new_node;
  *((uint64_t *)&n->path) = *((uint64_t *)&temp_path);

  mfence();
  flush_buffer(&n->path, sizeof(path_comp), false);
  flush_buffer(ref, sizeof(uintptr_t), false);
  mfence();
  ```
- description:
  The ref and n->path are not persisted atomically.
  Inconsistency may happen if there is a crash before the second fence, and only
  one of them is persisted.
- potential fix:
  This is not a simple programming mistake but a design issue.
  Logging two variables would be a simple solution. However, it is not
  clear if the data structure specific solution is possible using its
  own semantics.


## FAST_FAIR
- [github](https://github.com/DICL/FAST_FAIR/tree/c86f5fb6344eb3d2b3ca6dae4fc80e8ae6d28a5b)
- commit: c86f5fb6344eb3d2b3ca6dae4fc80e8ae6d28a5b

### Bug 3:
- Ordering Bug
- [loc](https://github.com/DICL/FAST_FAIR/blob/c86f5fb6344eb3d2b3ca6dae4fc80e8ae6d28a5b/single/src/btree.h#L220)
  ```c++
  if(shift) {
    records[i].key = records[i + 1].key;
    records[i].ptr = records[i + 1].ptr;

    // flush
    uint64_t records_ptr = (uint64_t)(&records[i]);
    int remainder = records_ptr % CACHE_LINE_SIZE;
    bool do_flush = (remainder == 0) ||
      ((((int)(remainder + sizeof(entry)) / CACHE_LINE_SIZE) == 1) &&
      ((remainder + sizeof(entry)) % CACHE_LINE_SIZE) != 0);
    if(do_flush) {
      clflush((char *)records_ptr, CACHE_LINE_SIZE);
    }
  }
  ```
- description:
  The clflush in remove_key is not correct.
  The current logic is: if the destination pointer address is at the beginning
  of one cache line, a clflush of this cache line will be called.
- potential fix:
  Since deletion is moving from right to left, which is opposite to insertion,
  the correct logic should be:
  if the destination pointer address (plus length) is at the end of one cache
  line, a clflush of this cache line should be called.

### Bug 4:
- Atomicity Bug
- description:
  There is no recovery code, and deletion doesn't check inconsistency like
  search.
- Example:
  ```c++
  Current status:
  | key_1 | key_3 |       |...
  |       |       |       |
  NULL    val_1   val_3   NULL

  Try to insert (key_2, val_2) and crash in the middle:
  | key_1 | key_3 | key_3 |       |...
  |       |       |       |       |
  NULL    val_1   val_1   val_3   NULL


  At this point, search operation will still work because it checks
  inconsistency.
  But if we call delete(key_3), since it doesn't check inconsistency, it will
  reuturn success and the status will be like below:
  | key_1 | key_3 |       |...
  |       |       |       |
  NULL    val_1   val_3   NULL
  The (key_3, val_3) is still there.
  ```
- potential fix:
  Design mistake.

### Bug 5:
- Atomicity Bug
- [loc](https://github.com/DICL/FAST_FAIR/blob/c86f5fb6344eb3d2b3ca6dae4fc80e8ae6d28a5b/single/src/btree.h#L576)
  ```c++
  if(bt->root == (char *)this) { // only one node can update the root ptr
    page* new_root = new page((page*)this, split_key, sibling,
    hdr.level + 1);
    bt->setNewRoot((char *)new_root);
  }
  ```
- description:
  The split process is not atomic.
  Suppose there is only one root node and it is in split process.
  Then a sibling node is created and the sibling_ptr of the root node has been
  updated to the sibling node.
  If a crash happens before setting the new root, an inconsistent case will
  happen.
  Then the data structure will be like a linked list of two nodes.
  Furthermore, since the new sibling node has no parent node, a future spit on
  the sibling node will fail.
- potential solution:
  Atomic update of new_root, this->root, and height should be
  guaranteed. Using logging?

### Bug 6:
- Atomicity Bug
- [loc](https://github.com/DICL/FAST_FAIR/blob/c86f5fb6344eb3d2b3ca6dae4fc80e8ae6d28a5b/single/src/btree.h#L299)
- description:
  The merge process is not atomic.



## Level_Hashing
- [github](https://github.com/Pfzuo/Level-Hashing/tree/28eca31b2c991a0a3bb9f5515352fd44def3c27c)
- commit: 28eca31b2c991a0a3bb9f5515352fd44def3c27c

### Bug 7:
- Ordering Bug
- [loc](https://github.com/Pfzuo/Level-Hashing/blob/28eca31b2c991a0a3bb9f5515352fd44def3c27c/persistent_level_hashing/level_hashing.c#L492)
  ```c++
  if (level->buckets[i][f_idx].token[j] == 0)
  {
    memcpy(level->buckets[i][f_idx].slot[j].key, key, KEY_LEN);
    memcpy(level->buckets[i][f_idx].slot[j].value, value, VALUE_LEN);
    level->buckets[i][f_idx].token[j] = 1;

    // When the key-value item and token are in the same cache line, only one flush is acctually executed.
    pflush((uint64_t *)&level->buckets[i][f_idx].slot[j].key);
    pflush((uint64_t *)&level->buckets[i][f_idx].slot[j].value);
    asm_mfence();
    pflush((uint64_t *)&level->buckets[i][f_idx].token[j]);
    level->level_item_num[i] ++;
    asm_mfence();
    return 0;
  }
  ```
- description:
  Key and value should be persisted before updating token.
  Support key-value and token are not in the same cache line, if there is a
  crash right after writing the token, an inconsistent case may happen.
  It is possible that token is evicted from the cache before the crash but the
  key-value is not.
  In this case the token it set to 1, but key-value is not persisted and could
  be arbitrary value.
- potential fix:
  Put persistent barrier before updating token.

### Bug 8:
- Ordering Bug
- [loc](https://github.com/Pfzuo/Level-Hashing/blob/28eca31b2c991a0a3bb9f5515352fd44def3c27c/persistent_level_hashing/level_hashing.c#L507)
  ```c++
  if (level->buckets[i][s_idx].token[j] == 0)
  {
    memcpy(level->buckets[i][s_idx].slot[j].key, key, KEY_LEN);
    memcpy(level->buckets[i][s_idx].slot[j].value, value, VALUE_LEN);
    level->buckets[i][s_idx].token[j] = 1;

    pflush((uint64_t *)&level->buckets[i][s_idx].slot[j].key);
    pflush((uint64_t *)&level->buckets[i][s_idx].slot[j].value);
    asm_mfence();
    pflush((uint64_t *)&level->buckets[i][s_idx].token[j]);
    level->level_item_num[i] ++;
    asm_mfence();
    return 0;
  }
  ```
- description:
  Key and value should be persisted before updating token.
- potential fix:
  Put persistent barrier before updating token.

### Bug 9:
- Ordering Bug
- [loc](https://github.com/Pfzuo/Level-Hashing/blob/28eca31b2c991a0a3bb9f5515352fd44def3c27c/persistent_level_hashing/level_hashing.c#L417)
  ```c++
  if (level->buckets[i][f_idx].token[k] == 0){        // Log-free update
      memcpy(level->buckets[i][f_idx].slot[k].key, key, KEY_LEN);
      memcpy(level->buckets[i][f_idx].slot[k].value, new_value, VALUE_LEN);

      level->buckets[i][f_idx].token[j] = 0;
      level->buckets[i][f_idx].token[k] = 1;

      pflush((uint64_t *)&level->buckets[i][f_idx].slot[k].key);
      pflush((uint64_t *)&level->buckets[i][f_idx].slot[k].value);
      asm_mfence();
      pflush((uint64_t *)&level->buckets[i][f_idx].token[j]);
      asm_mfence();
      return 0;
  }
  ```
- description:
  Key and value should be persisted before updating token.
- potential fix:
  Put persistent barrier before updating token.

### Bug 10:
- Ordering Bug
- [loc](https://github.com/Pfzuo/Level-Hashing/blob/28eca31b2c991a0a3bb9f5515352fd44def3c27c/persistent_level_hashing/level_hashing.c#L610)
  ```c++
  memcpy(level->buckets[level_num][jdx].slot[j].key, m_key, KEY_LEN);
  memcpy(level->buckets[level_num][jdx].slot[j].value, m_value, VALUE_LEN);
  level->buckets[1][s_idx].token[empty_location] = 1;

  pflush((uint64_t *)&level->buckets[level_num][jdx].slot[j].key);
  pflush((uint64_t *)&level->buckets[level_num][jdx].slot[j].value);
  asm_mfence();
  pflush((uint64_t *)&level->buckets[level_num][jdx].token[j]);
  asm_mfence();
  ```
- description:
  Key and value should be persisted before updating token.
- potential fix:
  Put persistent barrier before updating token.

### Bug 11:
- Ordering Bug
- [loc](https://github.com/Pfzuo/Level-Hashing/blob/28eca31b2c991a0a3bb9f5515352fd44def3c27c/persistent_level_hashing/level_hashing.c#L616)
  ```c++
  memcpy(level->buckets[level_num][idx].slot[i].key, key, KEY_LEN);
  memcpy(level->buckets[level_num][idx].slot[i].value, value, VALUE_LEN);
  level->buckets[level_num][idx].token[i] = 1;

  pflush((uint64_t *)&level->buckets[level_num][idx].slot[i].key);
  pflush((uint64_t *)&level->buckets[level_num][idx].slot[i].value);
  asm_mfence();
  pflush((uint64_t *)&level->buckets[level_num][idx].token[i]);
  level->level_item_num[level_num] ++;
  asm_mfence();
  ```
- description:
  Key and value should be persisted before updating token.
- potential fix:
  Put persistent barrier before updating token.

### Bug 12:
- Ordering Bug
- [loc](https://github.com/Pfzuo/Level-Hashing/blob/28eca31b2c991a0a3bb9f5515352fd44def3c27c/persistent_level_hashing/level_hashing.c#L657)
  ```c++
  memcpy(level->buckets[0][f_idx].slot[j].key, key, KEY_LEN);
  memcpy(level->buckets[0][f_idx].slot[j].value, value, VALUE_LEN);
  level->buckets[0][f_idx].token[j] = 1;

  pflush((uint64_t *)&level->buckets[0][f_idx].slot[j].key);
  pflush((uint64_t *)&level->buckets[0][f_idx].slot[j].value);
  asm_mfence();
  pflush((uint64_t *)&level->buckets[0][f_idx].token[j]);
  asm_mfence();
  ```
- description:
  Key and value should be persisted before updating token.
- potential fix:
  Put persistent barrier before updating token.

### Bug 13:
- Ordering Bug
- [loc](https://github.com/Pfzuo/Level-Hashing/blob/28eca31b2c991a0a3bb9f5515352fd44def3c27c/persistent_level_hashing/level_hashing.c#L677)
  ```c++
  memcpy(level->buckets[0][s_idx].slot[j].key, key, KEY_LEN);
  memcpy(level->buckets[0][s_idx].slot[j].value, value, VALUE_LEN);
  level->buckets[0][s_idx].token[j] = 1;

  pflush((uint64_t *)&level->buckets[0][s_idx].slot[j].key);
  pflush((uint64_t *)&level->buckets[0][s_idx].slot[j].value);
  asm_mfence();
  pflush((uint64_t *)&level->buckets[0][s_idx].token[j]);
  asm_mfence();
  ```
- description:
  Key and value should be persisted before updating token.
- potential fix:
  Put persistent barrier before updating token.

### Bug 14:
- Ordering Bug
- [loc](https://github.com/Pfzuo/Level-Hashing/blob/28eca31b2c991a0a3bb9f5515352fd44def3c27c/persistent_level_hashing/level_hashing.c#L545)
  ```c++
  empty_location = b2t_movement(level, f_idx);
  if(empty_location != -1){
      memcpy(level->buckets[1][f_idx].slot[empty_location].key, key, KEY_LEN);
      memcpy(level->buckets[1][f_idx].slot[empty_location].value, value, VALUE_LEN);
      level->buckets[1][f_idx].token[empty_location] = 1;

      pflush((uint64_t *)&level->buckets[1][f_idx].slot[empty_location].key);
      pflush((uint64_t *)&level->buckets[1][f_idx].slot[empty_location].value);
      asm_mfence();
      pflush((uint64_t *)&level->buckets[1][f_idx].token[empty_location]);
      level->level_item_num[1] ++;
      asm_mfence();
      return 0;
  }
  ```
- description:
  Key and value should be persisted before updating token.
- potential fix:
  Put persistent barrier before updating token.

### Bug 15:
- Ordering Bug
- [loc](https://github.com/Pfzuo/Level-Hashing/blob/28eca31b2c991a0a3bb9f5515352fd44def3c27c/persistent_level_hashing/level_hashing.c#L560)
  ```c++
  empty_location = b2t_movement(level, s_idx);
  if(empty_location != -1){
      memcpy(level->buckets[1][s_idx].slot[empty_location].key, key, KEY_LEN);
      memcpy(level->buckets[1][s_idx].slot[empty_location].value, value, VALUE_LEN);
      level->buckets[1][s_idx].token[empty_location] = 1;

      pflush((uint64_t *)&level->buckets[1][s_idx].slot[empty_location].key);
      pflush((uint64_t *)&level->buckets[1][s_idx].slot[empty_location].value);
      asm_mfence();
      pflush((uint64_t *)&level->buckets[1][s_idx].token[empty_location]);
      level->level_item_num[1] ++;
      asm_mfence();
      return 0;
  }
  ```
- description:
  Key and value should be persisted before updating token.
- potential fix:
  Put persistent barrier before updating token.

### Bug 16:
- Ordering Bug
- [loc](https://github.com/Pfzuo/Level-Hashing/blob/28eca31b2c991a0a3bb9f5515352fd44def3c27c/persistent_level_hashing/level_hashing.c#L445)
  ```c++
  if (level->buckets[i][s_idx].token[k] == 0){        // Log-free update
      memcpy(level->buckets[i][s_idx].slot[k].key, key, KEY_LEN);
      memcpy(level->buckets[i][s_idx].slot[k].value, new_value, VALUE_LEN);

      level->buckets[i][s_idx].token[j] = 0;
      level->buckets[i][s_idx].token[k] = 1;

      pflush((uint64_t *)&level->buckets[i][s_idx].slot[k].key);
      pflush((uint64_t *)&level->buckets[i][s_idx].slot[k].value);
      asm_mfence();
      pflush((uint64_t *)&level->buckets[i][s_idx].token[j]);
      asm_mfence();
      return 0;
  }
  ```
- description:
  Key and value should be persisted before updating token.
- potential fix:
  Put persistent barrier before updating token.

### Bug 17:
- Atomicity Bug
- [loc](https://github.com/Pfzuo/Level-Hashing/blob/28eca31b2c991a0a3bb9f5515352fd44def3c27c/persistent_level_hashing/level_hashing.c#L112)
  ```c++
  level->addr_capacity = pow(2, level->level_size + 1);
  level->interim_level_buckets = pmalloc(level->addr_capacity*sizeof(level_bucket));
  ```
- description:
  The expand process is not atomic.
  For example, before creating new buckets, the addr_capacity is updated.
  If there is a crash right after updating the addr_capacity and it is evicted
  from the cache and persisted before crash, an inconsistent case may happen.
  Becasue we are still using the old buckets but with the new addr_capacity,
  which determines the hash index for each key.
- potential fix:
  Not a simple programming mistake. Need to use logging or another
  flag variable.

### Bug 18:
- Atomicity Bug
- [loc](https://github.com/Pfzuo/Level-Hashing/blob/28eca31b2c991a0a3bb9f5515352fd44def3c27c/persistent_level_hashing/level_hashing.c#L228)
  ```c++
  level->level_size --;
  level_bucket *newBuckets = pmalloc(pow(2, level->level_size - 1)*sizeof(level_bucket));
  level->interim_level_buckets = level->buckets[0];
  level->buckets[0] = level->buckets[1];
  level->buckets[1] = newBuckets;
  newBuckets = NULL;
  ```
- description:
  The shrink process is not atomic.
  For example, a crash after setting the level->buckets[1] may incur
  inconsistency.
- potential fix:
  Not a simple programming mistake. Need to use logging or another
  flag variable.

### Bug 19:
- Atomicity Bug
- [loc](https://github.com/Pfzuo/Level-Hashing/blob/28eca31b2c991a0a3bb9f5515352fd44def3c27c/persistent_level_hashing/level_hashing.c#L609)
  ```c++
  memcpy(level->buckets[level_num][jdx].slot[j].key, m_key, KEY_LEN);
  memcpy(level->buckets[level_num][jdx].slot[j].value, m_value, VALUE_LEN);
  level->buckets[1][s_idx].token[empty_location] = 1;

  pflush((uint64_t *)&level->buckets[level_num][jdx].slot[j].key);
  pflush((uint64_t *)&level->buckets[level_num][jdx].slot[j].value);
  asm_mfence();
  pflush((uint64_t *)&level->buckets[level_num][jdx].token[j]);
  asm_mfence();

  level->buckets[level_num][idx].token[i] = 0;
  pflush((uint64_t *)&level->buckets[level_num][idx].token[j]);
  asm_mfence();
  // The movement is finished and then the new item is inserted
  ```
- description:
  Updating two tokens is not atomic (moving a key from `i` to
  `j`). If a crash makes the first token and its key-val persisted but
  not the second token, the we will have one duplicated
  key-value.
- potential fix:
  Not a simple programming mistake. Need to use logging or another
  flag variable.

### Bug 20:
- Atomicity Bug
- [loc](https://github.com/Pfzuo/Level-Hashing/blob/28eca31b2c991a0a3bb9f5515352fd44def3c27c/persistent_level_hashing/level_hashing.c#L665)
  ```c++
  memcpy(level->buckets[0][f_idx].slot[j].key, key, KEY_LEN);
  memcpy(level->buckets[0][f_idx].slot[j].value, value, VALUE_LEN);
  level->buckets[0][f_idx].token[j] = 1;

  pflush((uint64_t *)&level->buckets[0][f_idx].slot[j].key);
  pflush((uint64_t *)&level->buckets[0][f_idx].slot[j].value);
  asm_mfence();
  pflush((uint64_t *)&level->buckets[0][f_idx].token[j]);
  asm_mfence();

  level->buckets[1][idx].token[i] = 0;
  pflush((uint64_t *)&level->buckets[1][idx].token[i]);
  asm_mfence();
  ```
- description:
  Updating two tokens is not atomic (moving a key from `i` to `j`).
  If a crash makes the first token and its key-val persisted but not the second
  token, the we will have one duplicated key-value.
- potential fix:
  Not a simple programming mistake. Need to use logging or another
  flag variable.

### Bug 21:
- Atomicity Bug
- [loc](https://github.com/Pfzuo/Level-Hashing/blob/28eca31b2c991a0a3bb9f5515352fd44def3c27c/persistent_level_hashing/level_hashing.c#L685)
  ```c++
  memcpy(level->buckets[0][s_idx].slot[j].key, key, KEY_LEN);
  memcpy(level->buckets[0][s_idx].slot[j].value, value, VALUE_LEN);
  level->buckets[0][s_idx].token[j] = 1;

  pflush((uint64_t *)&level->buckets[0][s_idx].slot[j].key);
  pflush((uint64_t *)&level->buckets[0][s_idx].slot[j].value);
  asm_mfence();
  pflush((uint64_t *)&level->buckets[0][s_idx].token[j]);
  asm_mfence();

  level->buckets[1][idx].token[i] = 0;
  pflush((uint64_t *)&level->buckets[1][idx].token[i]);
  asm_mfence();
  ```
- description:
  Updating two tokens is not atomic (moving a key from `i` to `j`).
  If a crash makes the first token and its key-val persisted but not the second
  token, the we will have one duplicated key-value.
- potential fix:
  Not a simple programming mistake. Need to use logging or another
  flag variable.

### Bug 22:
- Atomicity Bug
- [loc](https://github.com/Pfzuo/Level-Hashing/blob/28eca31b2c991a0a3bb9f5515352fd44def3c27c/persistent_level_hashing/level_hashing.c#L416)
  ```c++
  if (level->buckets[i][f_idx].token[k] == 0){        // Log-free update
      memcpy(level->buckets[i][f_idx].slot[k].key, key, KEY_LEN);
      memcpy(level->buckets[i][f_idx].slot[k].value, new_value, VALUE_LEN);

      level->buckets[i][f_idx].token[j] = 0;
      level->buckets[i][f_idx].token[k] = 1;

      pflush((uint64_t *)&level->buckets[i][f_idx].slot[k].key);
      pflush((uint64_t *)&level->buckets[i][f_idx].slot[k].value);
      asm_mfence();
      pflush((uint64_t *)&level->buckets[i][f_idx].token[j]);
      asm_mfence();
      return 0;
  }
  ```
- description:
  Updating two tokens is not atomic (moving a key from `j` to `k`). If
  there is a crash after updating `token[j]` and before updating
  `token[k]`, an inconsistent case may happen. In this case, if `token[j]`
  is evicted and persisted, we will lose one key-value.
- potential fix:
  Not a simple programming mistake. Need to use logging or another
  flag variable.

### Bug 23:
- Atomicity Bug
- [loc](https://github.com/Pfzuo/Level-Hashing/blob/28eca31b2c991a0a3bb9f5515352fd44def3c27c/persistent_level_hashing/level_hashing.c#L444)
  ```c++
  if (level->buckets[i][s_idx].token[k] == 0){        // Log-free update
      memcpy(level->buckets[i][s_idx].slot[k].key, key, KEY_LEN);
      memcpy(level->buckets[i][s_idx].slot[k].value, new_value, VALUE_LEN);

      level->buckets[i][s_idx].token[j] = 0;
      level->buckets[i][s_idx].token[k] = 1;

      pflush((uint64_t *)&level->buckets[i][s_idx].slot[k].key);
      pflush((uint64_t *)&level->buckets[i][s_idx].slot[k].value);
      asm_mfence();
      pflush((uint64_t *)&level->buckets[i][s_idx].token[j]);
      asm_mfence();
      return 0;
  }
  ```
- description:
  Updating two tokens is not atomic (moving a key from `j` to `k`). If
  there is a crash after updating token[j] and before updating
  token[k], an inconsistent case may happen. In this case, if token[j]
  is evicted and persisted, we will lose one key-value.
- potential fix:
  Not a simple programming mistake. Need to use logging or another
  flag variable.


## CCEH
- [github](https://github.com/DICL/CCEH/tree/d53b33632e8ea2add4e27992718b8f4023312743)
- commit: d53b33632e8ea2add4e27992718b8f4023312743

### Bug 24:
- Atomicity Bug
- description:
  The split process is not atomic.
- Example 0:
  - [loc](https://github.com/DICL/CCEH/blob/d53b33632e8ea2add4e27992718b8f4023312743/src/CCEH_MSB.cpp#L103)
  ```c++
  clflush((char*)split[1], sizeof(Segment));
  local_depth = local_depth + 1;
  clflush((char*)&local_depth, sizeof(size_t));
  ```
  - description:
  In the split process, a new Segment will be created. And right after that,
  the local_depth of the old Segment is updated.
  Suppose this is a Directory Doubling Split, if there is a crash right after
  updating the local_depth of the old Segment, an inconsistent case will happen.
  The recover code will not work because the local_depth of the old Segment is
  larger than the global depth.
- Example 1:
  - [loc](https://github.com/DICL/CCEH/blob/d53b33632e8ea2add4e27992718b8f4023312743/src/CCEH_MSB.cpp#L198)
  ```c++
  s[0]->pattern = (key_hash >> (8*sizeof(key_hash)-s[0]->local_depth+1)) << 1;
  s[1]->pattern = ((key_hash >> (8*sizeof(key_hash)-s[1]->local_depth+1)) << 1) + 1;
  ```
  - description:
  After creating a new Segment and updating the local_depth of the old Segment,
  and before updating/doubling the Directory, the pattern of the both Segments
  is updated.
  If there is a crash right after updating the pattern, an inconsistent case
  will happen. At this moment, the Directory is not updated, so the new Segment
  is not accessible. But the local_depth and the pattern of the old Segment has
  been updated, then some existing key-value pairs may not be accessible
  anymore.
- potential fix:
  This is not a simple programming mistake but a design issue.
  Logging fours variables would be a simple solution. However, it is not
  clear if the data structure specific solution is possible using its
  own semantics.

### Bug 25:
- Atomicity Bug
- [loc](https://github.com/DICL/CCEH/blob/d53b33632e8ea2add4e27992718b8f4023312743/src/CCEH_MSB.cpp#L29)
  ```c++
  if (CAS(&_[slot].key, &LOCK, SENTINEL)) {
    _[slot].value = value;
    mfence();
    _[slot].key = key;
    ret = 0;
    break;
  } else {
    LOCK = INVALID;
  }
  ```
- description:
  If there is a crash after the CAS operation, and the cacheline of that slot
  is evicted from the cache (thus persistent) before the crash, there will be
  a dead slot which cannot be inserted any more, because only INVALID slot can
  be inserted. This would not affect insert operation but InsertOnly operation
  because InsertOnly doesn't allow re-sizing.
  Potential fix: recovery code for reverting all SENTINEL slots to INVALID
  slots.


## P-ART
- [github](https://github.com/utsaslab/RECIPE/5b4cf3efedecdbd22d816376309349b2c8050711/P-ART)
- commit: 5b4cf3efedecdbd22d816376309349b2c8050711

### Bug 26:
- Atomicity Bug
- [loc](https://github.com/utsaslab/RECIPE/blob/5b4cf3efedecdbd22d816376309349b2c8050711/P-ART/N16.cpp#L15)
  ```c++
  keys[compactCount].store(flipSign(key), flush ? std::memory_order_release : std::memory_order_relaxed);
  children[compactCount].store(n, flush ? std::memory_order_release : std::memory_order_relaxed);
  if (flush) clflush((char *)&children[compactCount], sizeof(N *), false, true);
  compactCount++;
  count++;
  // this clflush will atomically flush the cache line including counters and entire key entries
  if (flush) clflush((char *)this, sizeof(uintptr_t), true, true);
  ```
- description:
  The children and compactCount are not persisted atomically.
  An inconsistency may happen if a crash happens before updating compactCount
  and keys and children are already persisted.
  Since range operation relies on N16::getChildren function which uses
  compactCount, so the range operation will miss this key value pair but the
  get operation will still get it.
- potential fix:
  This is not a simple programming mistake but a design issue.
  Logging two variables would be a simple solution. However, it is not
  clear if the data structure specific solution is possible using its
  own semantics.

### Bug 27:
- Atomicity Bug
- [loc](https://github.com/utsaslab/RECIPE/blob/5b4cf3efedecdbd22d816376309349b2c8050711/P-ART/N4.cpp#L17)
  ```c++
  inline bool N4::insert(uint8_t key, N *n, bool flush) {
      if (compactCount == 4) {
          return false;
      }
      keys[compactCount].store(key, flush ? std::memory_order_release : std::memory_order_relaxed);
      children[compactCount].store(n, flush ? std::memory_order_release : std::memory_order_relaxed);
      compactCount++;
      count++;
      // As the size of node4 is lower than cache line size (64bytes),
      // only one clflush is required to atomically synchronize its updates
      if (flush) clflush((char *)this, sizeof(N4), true, true);
      return true;
  }
  ```
- description:
  keys, children, compactCount and count should be updated in atomic.
  Event N4 size is less than cache line size, crash in the middle may lead to
  inconsistency.
- example 0:
  If there is a crash right after children is updated, and keys, children are
  persisted (evicted from cache) while compactCount, count are not.
  A later get operation can still get this key value, because get operation
  doesn't rely on compactCount or count. See
  [code](https://github.com/utsaslab/RECIPE/blob/5b4cf3efedecdbd22d816376309349b2c8050711/P-ART/N4.cpp#L54)
  ```c++
  N *N4::getChild(const uint8_t k) const {
    for (uint32_t i = 0; i < 4; ++i) {
        N *child = children[i].load();
        if (child != nullptr && keys[i].load() == k) {
            return child;
        }
    }
    return nullptr;
  }
  ```
  But the insert operation relies on compactCount, so a later insertion into
  this node may overwrite this key value, and user will not notice it.
- example 1:
  If there is a crash right after compactCount is updated, and keys, children,
  compactCount are persisted (evicted from cache) while count is not.
  And suppose before this insertion, compactCount==3 and count==3, so after
  crash compactCount==4 and count==3.
  compactCount and count are used for determining whether it should grow, see
  [code](https://github.com/utsaslab/RECIPE/blob/5b4cf3efedecdbd22d816376309349b2c8050711/P-ART/N.cpp#L209)
  ```c++
  void N::insertAndUnlock(N *node, N *parentNode, uint8_t keyParent, uint8_t key, N *val, ThreadInfo &threadInfo, bool &needRestart) {
    switch (node->getType()) {
        case NTypes::N4: {
            auto n = static_cast<N4 *>(node);
            if (n->compactCount == 4 && n->count <= 3) {
                insertCompact<N4>(n, parentNode, keyParent, key, val, threadInfo, needRestart);
                break;
            }
            insertGrow<N4, N16>(n, parentNode, keyParent, key, val, threadInfo, needRestart);
            break;
        }
  ```
  In this example, the next insertion to this node will choose insertCompact but
  not insertGrow. But the correct one should be insertGrow. So this next
  insertion will fail but return normally.
- potential fix:
  Not a simple programming mistake. Need to use logging or another
  flag variable.


## P-BwTree
- [github](https://github.com/utsaslab/RECIPE/tree/5b4cf3efedecdbd22d816376309349b2c8050711/P-BwTree)
- commit: 5b4cf3efedecdbd22d816376309349b2c8050711

### Bug 28:
- Ordering Bug
- [loc](https://github.com/utsaslab/RECIPE/blob/5b4cf3efedecdbd22d816376309349b2c8050711/P-BwTree/src/bwtree.h#L2012)
  ```c++
  char *new_tail = tail.fetch_sub(size) - size;
  ```
- description:
  - Call graph:
    Insert -> LeafInlineAllocateOfType -> InlineAllocate -> Allocate -> TryAllocate
  - The tail is not explicitly flushed after the fetch_sub.
- potential fix:
  Adding a clflush/sfence after `tail.fetch_sub(size)` will be a
  solution. But it will still cause persistent memory leak (i.e.,
  memory is allocated but not assigned to its caller).

### Bug 29:
- Ordering Bug
- [loc](https://github.com/utsaslab/RECIPE/blob/5b4cf3efedecdbd22d816376309349b2c8050711/P-BwTree/src/bwtree.h#L2369)
  ```c++
  new (reinterpret_cast<AllocationMeta *>(alloc_base)) \
    AllocationMeta{alloc_base + AllocationMeta::CHUNK_SIZE,
                   alloc_base + sizeof(AllocationMeta)};
  ```
- description:
  - Call graph:
    Delete->Traverse->LoadNodeID->TryConsolidateNode->ConsolidateNode->ConsolidateLeafNode->CollectAllValuesOnLeaf->ElasticNode<KeyValuePair>::Get->new AllocationMeta
  - The `alloc_base` is not explicitly flushed after the allocation.

## P-CLHT
- [github](https://github.com/utsaslab/RECIPE/tree/5b4cf3efedecdbd22d816376309349b2c8050711/P-CLHT)
- commit: 5b4cf3efedecdbd22d816376309349b2c8050711

### Bug 30:
- Ordering Bug
- [loc](https://github.com/utsaslab/RECIPE/blob/5b4cf3efedecdbd22d816376309349b2c8050711/P-CLHT/src/clht_lb_res.c#L166)
  ```c++
  static inline void clflush_next_check(char *data, int len, bool fence)
  {
    volatile char *ptr = (char *)((unsigned long)data &~(CACHE_LINE_SIZE-1));
    if (fence)
      mfence();
    for(; ptr<data+len; ptr+=CACHE_LINE_SIZE){
      unsigned long etsc = read_tsc() + (unsigned long)(write_latency*CPU_FREQ_MHZ/1000);
  #ifdef CLFLUSH
      asm volatile("clflush %0" : "+m" (*(volatile char *)ptr));
  #elif CLFLUSH_OPT
      asm volatile(".byte 0x66; clflush %0" : "+m" (*(volatile char *)(ptr)));
  #elif CLWB
      asm volatile(".byte 0x66; xsaveopt %0" : "+m" (*(volatile char *)(ptr)));
  #endif
      if (((bucket_t *)data)->next)
        clflush_next_check((char *)(((bucket_t *)data)->next), sizeof(bucket_t), false);
      while(read_tsc() < etsc) cpu_pause();
    }
    if (fence)
      mfence();
  }
  ```
- description:
  The recursive clflush_next_check is incorrect.
- potential fix:
  Below should be the correct one:
  ```diff
  -   if (((bucket_t *)data)->next)
  -     clflush_next_check((char *)(((bucket_t *)data)->next), sizeof(bucket_t), false);
  +   if (((bucket_t *)ptr)->next)
  +     clflush_next_check((char *)(((bucket_t *)ptr)->next), sizeof(bucket_t), false);
  ```

## P-CLHT-Aga
- [github](https://github.com/utsaslab/RECIPE/blob/53923cf001be2f4ff19a320ec2b0a7bf5825fe51/P-CLHT)
- commit: 53923cf001be2f4ff19a320ec2b0a7bf5825fe51

### Bug 31:
- Ordering Bug
- [loc](https://github.com/utsaslab/RECIPE/blob/53923cf001be2f4ff19a320ec2b0a7bf5825fe51/P-CLHT/src/clht_lb_res.c#L177)

### Bug 32:
- Ordering Bug
- [loc](https://github.com/utsaslab/RECIPE/blob/53923cf001be2f4ff19a320ec2b0a7bf5825fe51/P-CLHT/src/clht_lb_res.c#L578)

### Bug 33:
- Ordering Bug
- [loc](https://github.com/utsaslab/RECIPE/blob/53923cf001be2f4ff19a320ec2b0a7bf5825fe51/P-CLHT/src/clht_lb_res.c#L583)


## P-CLHT-Aga-TX
- [github](https://github.com/utsaslab/RECIPE/blob/53923cf001be2f4ff19a320ec2b0a7bf5825fe51/P-CLHT)
- commit: 53923cf001be2f4ff19a320ec2b0a7bf5825fe51

### Bug 34:
- Ordering Bug
- [loc](https://github.com/utsaslab/RECIPE/blob/53923cf001be2f4ff19a320ec2b0a7bf5825fe51/P-CLHT/src/clht_lb_res.c#L559)

### Bug 35:
- Ordering Bug
- [loc](https://github.com/utsaslab/RECIPE/blob/53923cf001be2f4ff19a320ec2b0a7bf5825fe51/P-CLHT/src/clht_lb_res.c#L583)


## P-HOT
- [github](https://github.com/utsaslab/RECIPE/tree/5b4cf3efedecdbd22d816376309349b2c8050711/P-HOT)
- commit: 5b4cf3efedecdbd22d816376309349b2c8050711

### Bug 36:
- Ordering Bug
- [loc](https://github.com/utsaslab/RECIPE/blob/5b4cf3efedecdbd22d816376309349b2c8050711/P-HOT/libs/hot/rowex/include/hot/rowex/HOTRowex.hpp#L116)
  ```c++
  if (mismatchingBit.mIsValid) {
    HOTRowexChildPointer const & newRoot = hot::commons::createTwoEntriesNode<HOTRowexChildPointer, HOTRowexNode>(hot::commons::BiNode<HOTRowexChildPointer>::createFromExistingAndNewEntry(mismatchingBit.mValue, mRoot, valueToInsert))->toChildPointer();
    insertionResult = { mRoot.compareAndSwap(currentRoot, newRoot) , true};
    hot::commons::clflush(reinterpret_cast <char *> (&mRoot), sizeof(HOTRowexChildPointer));
    hot::commons::mfence();
  }
  ```
- [loc](https://github.com/utsaslab/RECIPE/blob/5b4cf3efedecdbd22d816376309349b2c8050711/P-HOT/libs/hot/commons/include/hot/commons/TwoEntriesNode.hpp#L30)
  ```c++
  clflush(reinterpret_cast <char *> (node), allocationInformation.mTotalSizeInBytes);
  return node;
  ```
- description:
  A newRoot is created before mRoot.compareAndSwap.
  However the newRoot is not guaranteed to be persisted before setting it to mRoot.
- potential fix:
  Inside TwoEntriesNode, a fence should be added after the clflush and before the return.

### Bug 37:
- Ordering Bug
- [loc](https://github.com/utsaslab/RECIPE/blob/5b4cf3efedecdbd22d816376309349b2c8050711/P-HOT/libs/hot/rowex/include/hot/rowex/HOTRowex.hpp#L264)
  ```c++
  currentNodeStackEntry.updateChildPointer(
    currentNodeStackEntry.getChildPointer().executeForSpecificNodeType(false, [&](auto & currentNode) -> HOTRowexChildPointer {
      return currentNode.addEntry(insertInformation, valueToInsert);
    })
  );
  hot::commons::mfence();
  hot::commons::clflush(reinterpret_cast <char *> (&currentNodeStackEntry.getChildPointerLocation()), sizeof(intptr_t));
  hot::commons::mfence();
  ```
- [loc](https://github.com/utsaslab/RECIPE/blob/5b4cf3efedecdbd22d816376309349b2c8050711/P-HOT/libs/hot/rowex/include/hot/rowex/HOTRowexNode.hpp#L315)
  ```c++
    hot::commons::clflush(reinterpret_cast<char *> (newChild.getNode()), allocationInformation.mTotalSizeInBytes);
  } else {
    newChild = (new (newNumberEntries) HOTRowexNode<typename ToDiscriminativeBitsRepresentation<NewDiscriminativeBitsRepresentationType,
      typename NextPartialKeyType<PartialKeyType>::Type>::Type, typename ToPartialKeyType<NewDiscriminativeBitsRepresentationType,typename NextPartialKeyType<PartialKeyType>::Type>::Type> (
        self, newNumberEntries, newDiscriminativeBitsRepresentation, insertInformation, newValue))->toChildPointer();
    hot::commons::mfence();
    hot::commons::NodeAllocationInformation const & allocationInformation =
    hot::commons::NodeAllocationInformations<HOTRowexNode<typename ToDiscriminativeBitsRepresentation<NewDiscriminativeBitsRepresentationType, typename NextPartialKeyType<PartialKeyType>::Type>::Type, typename ToPartialKeyType<NewDiscriminativeBitsRepresentationType,typename NextPartialKeyType<PartialKeyType>::Type>::Type>>::getAllocationInformation(newNumberEntries);
    hot::commons::clflush(reinterpret_cast<char *> (newChild.getNode()), allocationInformation.mTotalSizeInBytes);
  }
  return newChild;
  ```
- description:
  The child pointer is not guaranteed to be persisted before updateChildPointer.
- potential fix:
  Inside the currentNode.addEntry, a fence should be added after the clflush and before the return.

### Bug 38:
- Ordering Bug
- [loc](https://github.com/utsaslab/RECIPE/blob/5b4cf3efedecdbd22d816376309349b2c8050711/P-HOT/libs/hot/rowex/include/hot/rowex/HOTRowex.hpp#L264)
  ```c++
  currentNodeStackEntry.updateChildPointer(
    currentNodeStackEntry.getChildPointer().executeForSpecificNodeType(false, [&](auto & currentNode) -> HOTRowexChildPointer {
      return currentNode.addEntry(insertInformation, valueToInsert);
    })
  );
  hot::commons::mfence();
  hot::commons::clflush(reinterpret_cast <char *> (&currentNodeStackEntry.getChildPointerLocation()), sizeof(intptr_t));
  hot::commons::mfence();
  ```
- description:
  The parameter passing to clflush is incorrect. The & symbol is not needed.
- potential fix:
  Below should be the correct one:
  ```c++
  hot::commons::clflush(reinterpret_cast <char *> (currentNodeStackEntry.getChildPointerLocation()), sizeof(intptr_t));
  ```

## P-Masstree
- [github](https://github.com/utsaslab/RECIPE/tree/5b4cf3efedecdbd22d816376309349b2c8050711/P-Masstree)
- commit: 5b4cf3efedecdbd22d816376309349b2c8050711

### Bug 39:
- Atomicity Bug
- [loc](https://github.com/utsaslab/RECIPE/blob/5b4cf3efedecdbd22d816376309349b2c8050711/P-Masstree/masstree.h#L1378)
  ```c++
    if (t->root() == this) {
      leafnode *new_root = new leafnode(this, split_key, new_sibling, level_ + 1);
      clflush((char *) new_root, sizeof(leafnode), true);
      t->setNewRoot(new_root);
  ```
- description:
  The split process is not atomic.
  For example, the inconsistency will happen if there is a crash before the
  setNewRoot, since the old root has already connected to a sibling.
- potential fix:
  I think it needs a TX/Logging, or a flag indicating recovery

## B-Tree
### Bug 40:
- Atomicity Bug
- $WITCHER_HOME/third_party/pmtest/nvml/src/examples/libpmemobj/tree_map/btree_map.c:201

## RB-Tree
### Bug 41:
- Atomicity Bug
- $WITCHER_HOME/third_party/pmtest/nvml/src/examples/libpmemobj/tree_map/rbtree_map.c:418

## RB-Tree-Aga
### Bug 42:
- Atomicity Bug
- $WITCHER_HOME/third_party/agamotto/rbtree_map_buggy.c:174
### Bug 43:
- Atomicity Bug
- $WITCHER_HOME/third_party/agamotto/rbtree_map_buggy.c:355

## Hashmap-TX
### Bug 44:
- Ordering Bug
- $WITCHER_HOME/third_party/pmtest/nvml/src/examples/libpmemobj/hashmap/hashmap_tx.c:278
- $WITCHER_HOME/third_party/pmtest/nvml/src/examples/libpmemobj/hashmap/hashmap_tx.c:288

## Hashmap-TX
### Bug 45:
- Atomicity Bug
- $WITCHER_HOME/third_party/pmtest/nvml/src/examples/libpmemobj/hashmap/hashmap_atomic.c:133
### Bug 46:
- Atomicity Bug
- $WITCHER_HOME/third_party/pmtest/nvml/src/examples/libpmemobj/hashmap/hashmap_atomic.c:207

## Memcached
### Bug 47:
- Ordering Bug
- $$WITCHER_HOME/third_party/memcached-pmem/items.c:538
