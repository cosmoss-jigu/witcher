## libpmemobj
- unpersisted: 5
  - memblock.c:1165
  - container_ravl.c:89
  - ulog.c:589
  - ulog.c:581
  - ulog.c:573

## WOART
  - unpersisted: 1
    - woart.c:776
  - extra flush: 2
    - woart.c:56
    - woart.c:48
  - extra fence: 3
    - woart.c:45
    - woart.c:677
    - woart.c:459

## WORT
  - unpersisted: 1
    - woart.c:454
  - extra fence: 1
    - woart.c:40

## FAST_FAIR
  - unpersisted: 5
    - ../include/btree.h:223
    - ../include/btree.h:509
    - ../include/btree.h:251
    - ../include/btree.h:444
    - ../include/btree.h:865
  - extra fence: 1
    - ../include/btree.h:67

## Level_Hashing
  - unpersisted: 11
    - level_hashing.c:707
    - level_hashing.c:538
    - level_hashing.c:552
    - level_hashing.c:409
    - level_hashing.c:419
    - level_hashing.c:603
    - level_hashing.c:659
    - level_hashing.c:726
    - level_hashing.c:727
    - level_hashing.c:588
    - level_hashing.c:706
  - extra flush: 12
    - level_hashing.c:176
    - level_hashing.c:191
    - level_hashing.c:457
    - level_hashing.c:485
    - level_hashing.c:535
    - level_hashing.c:549
    - level_hashing.c:585
    - level_hashing.c:600
    - level_hashing.c:641
    - level_hashing.c:656
    - level_hashing.c:697
    - level_hashing.c:717

## CCEH
  - unpersisted: 8
    - CCEH_MSB.cpp:375
    - CCEH_MSB.cpp:60
    - CCEH_MSB.cpp:385
    - CCEH_MSB.cpp:26
    - CCEH_MSB.cpp:47
    - CCEH_MSB.cpp:395
    - CCEH_MSB.cpp:41
    - ../src/CCEH.h:102
  - extra flush: 1
    - CCEH_MSB.cpp:330
  - extra fence: 1
    - ../util/persist.h:21

## P-ART
  - unpersisted: 9
    - ./N256.cpp:56
    - ./N256.cpp:85
    - ./N48.cpp:103
    - ./N48.cpp:63
    - ./N48.cpp:64
    - ./N16.cpp:115
    - ./N4.cpp:106
    - ./N.cpp:94
    - ./N.cpp:102
  - extra fence: 1
    - ./N.cpp:33

## P-BwTree
  - unpersisted: 1
    - ../include/bwtree.h:567
  - extra fence: 1
    - ../include/bwtree.h:237

## P-CLHT
  - unpersisted: 7
    - clht_lb_res.c:456
    - clht_lb_res.c:513
    - clht_lb_res.c:204
    - clht_lb_res.c:963
    - clht_lb_res.c:795
    - ../include/clht_lb_res.h:337
    - ../include/clht_lb_res.h:304
  - extra fence: 1
    - clht_lb_res.c:129

## P-CLHT-Aga
  - unpersisted: 10
    - clht_lb_res.c:659
    - clht_lb_res.c:594
    - clht_lb_res.c:578
    - clht_lb_res.c:245
    - ../include/clht_lb_res.h:311
    - ../include/clht_lb_res.h:344
    - clht_lb_res.c:975
    - clht_lb_res.c:976
    - clht_lb_res.c:1126
    - clht_lb_res.c:845
  - extra fence: 1
    - clht_lb_res.c:139

## P-CLHT-Aga-TX
  - unpersisted: 10
    - clht_lb_res_tx.c:594
    - clht_lb_res_tx.c:659
    - clht_lb_res_tx.c:245
    - ../include/clht_lb_res.h:311
    - ../include/clht_lb_res.h:344
    - clht_lb_res_tx.c:845
    - clht_lb_res_tx.c:1126
    - clht_lb_res_tx.c:975
    - clht_lb_res_tx.c:976
    - clht_lb_res_tx.c:559
  - extra flush: 3
    - clht_lb_res.c:558
    - clht_lb_res.c:897
    - clht_lb_res.c:898
  - extra fence: 1
    - clht_lb_res.c:139

## P-HOT
  - extra_fences:4
    - hot/rowex/include/hot/rowex/HOTRowex.hpp:244
    - hot/rowex/include/hot/rowex/HOTRowex.hpp:264
    - hot/rowex/include/hot/rowex/HOTRowex.hpp:286
    - hot/rowex/include/hot/rowex/HOTRowex.hpp:344

## P-Masstree
  - unpersisted: 5
    - ../include/masstree.h:657
    - ../include/masstree.h:660
    - ../include/masstree.h:661
    - ../include/masstree.h:664
    - ../include/masstree.h:659
  - extra fence: 1
    - ../include/masstree.h:72

## B-Tree
- extra log: 5
  - btree_map.c:334
  - btree_map.c:363
  - btree_map.c:274
  - btree_map.c:386
  - btree_map.c:360

## RB-Tree-Aga
- extra log: 13
  - rbtree_map_buggy.c:138
  - rbtree_map_buggy.c:198
  - rbtree_map_buggy.c:137
  - rbtree_map_buggy.c:197
  - rbtree_map_buggy.c:362
  - rbtree_map_buggy.c:301
  - rbtree_map_buggy.c:371
  - rbtree_map_buggy.c:225
  - rbtree_map_buggy.c:302
  - rbtree_map_buggy.c:303
  - rbtree_map_buggy.c:148
  - rbtree_map_buggy.c:367
  - rbtree_map_buggy.c:292

## memcached
- unpersisted: 29
  - slabs.c:549
  - items.c:421
  - items.c:422
  - slabs.c:550
  - items.c:470
  - assoc.c:167
  - items.c:1096
  - items.c:591
  - items.c:1098
  - items.c:423
  - slabs.c:540
  - slabs.c:547
  - items.c:1023
  - slabs.c:543
  - items.c:541
  - assoc.c:196
  - items.c:1348
  - slabs.c:413
  - items.c:469
  - slabs.c:548
  - items.c:1176
  - items.c:1300
  - items.c:1301
  - items.c:624
  - items.c:626
  - items.c:627
  - items.c:628
  - items.c:1282
  - assoc.c:197
- extra flush: 1
 - items.c:516
