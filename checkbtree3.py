#!/usr/local/bin/python2.7

import random
import Btree

def fill_btree3(max = 1000):
    test_pat  = []
    test_tree = Btree.Btree()
    c = 0
    while c < max:
        k = random.randint(0,32767) 
        v = random.randint(0,32767) 
        try:
            test_tree.insert(k,v)
            test_pat.append((k,v))
            c = c + 1
        except Btree.BtDuplicatedKey:
            pass
    return test_tree, test_pat

def random_check_btree3(max = 1000):

    test_tree, test_fill_pat = fill_btree3(max)

    # print '# import Btree'
    # print '# bt = Btree.Btree()'
    # print '# addseq = %s' % repr(test_fill_pat)
    # print '# for k, v in addseq:'
    # print '#   bt.insert(k, v)'
    # remove randomly from test_file_pat 
    rest_pat = test_fill_pat[0:]  
    test_remove_pat = []

    while(len(rest_pat) > 0):  
        x = random.randint(0,len(rest_pat)-1) 
        # verify
        missed = 0
        wrong = 0
        for tk,tv in rest_pat:
            ttv = test_tree.search(tk)
            if ttv is None:
                missed = missed + 1
            elif ttv != tv:
                wrong  = wrong + 1
        if missed or wrong:
            break
        # print '# bt.remove(%d, 1)' % rest_pat[x][0]
        test_tree.remove(rest_pat[x][0],1)
        test_remove_pat.append(rest_pat[x])
        del rest_pat[x]

    result = {'missed'  : missed, 'wrong' : wrong,
              'add_seq' : test_fill_pat,
              'del_seq' : test_remove_pat}
    return result

def main():
    if 1:
        rl = []
        n = 0
        while n < 1000:
            n = n + 1
            r = random_check_btree3(1000)
            if r['missed'] or r['wrong']:
                print ('at %d th remove, incorrect result returned:'
                        % len(r['del_seq']))
                print 'add sequence :'
                print r['add_seq']
                print 'remove sequence :'
                print r['del_seq']
                print 'missed : %d' % r['missed']
                print 'wrong : %d' % r['wrong']
                r['n-th'] = n
                rl.append(r)  
    else:
        r = random_check_btree3(1000)
        if r['missed'] or r['wrong']:
          print ('at %d th remove, incorrect result returned:'
                  % len(r['del_seq']))
          print 'add sequence :'
          print r['add_seq']
          print 'remove sequence :'
          print r['del_seq']
          print 'missed : %d' % r['missed']
          print 'wrong : %d' % r['wrong']
    return

if __name__ == "__main__":
    main()
