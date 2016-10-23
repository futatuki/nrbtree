#!/usr/local/bin/python2.7

import random
import pybtree

def random_check_btree(stop=False):
    test_pat  = []
    test_tree = pybtree.Btree()
    results   = []

    c = 0
    missed = 0
    wrong = 0
    while c < 100:
        k = random.randint(0,32767) 
        v = random.randint(0,32767) 
        test_pat.append((k,v))
        try:
            test_tree.insert(k,v)
        except pybtree.BtDuplicatedKey:
            pass
        # verify
        for tk,tv in test_pat:
            ttv = test_tree.search(tk)
            if ttv is None:
                missed = missed + 1
            elif ttv != tv:
                wrong  = wrong + 1
        results.append(
                {'key' : k, 'val' : v, 'missed' : missed, 'wrong' : wrong })
        if stop and (missed or wrong):
            break
        c = c + 1
    del test_tree
    return results

def main():
    r = random_check_btree(True)
    s = [ d['key'] for d in r] 
    print r
    print s
    return

if __name__ == "__main__":
    main()
