import os

tasks = {
    #"woart"          : os.getenv('WITCHER_HOME') + '/benchmark/WORT/woart/random/2000',
    #"wort"           : os.getenv('WITCHER_HOME') + '/benchmark/WORT/wort/random/2000',
    "FAST_FAIR"      : os.getenv('WITCHER_HOME') + '/benchmark/FAST_FAIR/random/2000',
    "Level_Hashing"  : os.getenv('WITCHER_HOME') + '/benchmark/Level_Hashing/random/2000',
    "CCEH"           : os.getenv('WITCHER_HOME') + '/benchmark/CCEH/random/2000',
    ##
    #"P-ART"          : os.getenv('WITCHER_HOME') + '/benchmark/RECIPE/P-ART/random/2000',
    #"P-BwTree"       : os.getenv('WITCHER_HOME') + '/benchmark/RECIPE/P-BwTree/random/2000',
    #"P-CLHT"         : os.getenv('WITCHER_HOME') + '/benchmark/RECIPE/P-CLHT/random/2000',
    "P-CLHT-Aga"     : os.getenv('WITCHER_HOME') + '/benchmark/RECIPE/P-CLHT-Aga/random/2000',
    #"P-CLHT-Aga-TX"  : os.getenv('WITCHER_HOME') + '/benchmark/RECIPE/P-CLHT-Aga-TX/random/2000',
    #"P-HOT"          : os.getenv('WITCHER_HOME') + '/benchmark/RECIPE/P-HOT/random/2000',
    #"P-Masstree"     : os.getenv('WITCHER_HOME') + '/benchmark/RECIPE/P-Masstree/random/2000',
    ##
    #'btree'          : os.getenv('WITCHER_HOME') + '/benchmark/pmdk_examples/btree/random/2000',
    #"ctree"          : os.getenv('WITCHER_HOME') + '/benchmark/pmdk_examples/ctree/random/2000',
    #"rbtree"         : os.getenv('WITCHER_HOME') + '/benchmark/pmdk_examples/rbtree/random/2000',
    "rbtree-aga"     : os.getenv('WITCHER_HOME') + '/benchmark/pmdk_examples/rbtree-aga/random/2000',
    #"hashmap_tx"     : os.getenv('WITCHER_HOME') + '/benchmark/pmdk_examples/hashmap_tx/random/2000',
    #"hashmap_atomic" : os.getenv('WITCHER_HOME') + '/benchmark/pmdk_examples/hashmap_atomic/random/2000',
    ##
    "mmecached-pmem" : os.getenv('WITCHER_HOME') + '/benchmark/memcached-pmem/random/2000',
    #"redis-3.2-nvml" : os.getenv('WITCHER_HOME') + '/benchmark/redis-3.2-nvml/random/2000',
}
