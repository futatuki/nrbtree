#!/usr/local/bin/python2.7

import random
import pybtree

def random_check_btree2(max = 1000):
    test_pat  = []
    test_tree = pybtree.Btree()

    c = 0
    missed = 0
    wrong = 0
    while c < max:
        k = random.randint(0,32767) 
        v = random.randint(0,32767) 
        try:
            test_tree.insert(k,v)
            test_pat.append((k,v))
            c = c + 1
        except pybtree.BtDuplicatedKey:
            pass
    # verify
    for tk,tv in test_pat:
        ttv = test_tree.search(tk)
        if ttv is None:
            missed = missed + 1
        elif ttv != tv:
            wrong  = wrong + 1
    result = {'missed' : missed, 'wrong' : wrong }
    del test_pat
    del test_tree
    return result

def main():
    rl = []
    n = 0
    while n < 10000:
        n = n + 1
        r = random_check_btree2(10000)
        if (r['missed'] or r['wrong']): 
           r['n-th'] = n
           rl.append(r)  
    print rl
    return

if __name__ == "__main__":
    main()
