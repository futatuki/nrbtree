/* $yfId: readindex/branches/on_the_way/btree/check_leak.c 56 2016-10-18 15:09:55Z futatuki $ */
# include <stdio.h>
# include <stdlib.h>
# include <stdint.h>
# include "btree.h"

int
main(void)
{
    int c;
    plist_t * plist, * tp, * tp2;
    uint32_t k;
    uint32_t v;
    bt_btree_t *t;
    bt_error_t err;
 
    if (NULL ==  (t = bt_new_btree(NULL,NULL,NULL,NULL, NULL))) {
        fprintf(stderr, "Cannot allocate for btree structure\n");
        return 1;
    }
    /* srandomdev(); */

    c = 0;
    while(c < 10000) {
        /* k = random(), v = random(); */
        k = arc4random(), v = arc4random();
        err = bt_insert(t, (BT_KEY_T)k, (BT_VAL_T)v);
        if (err == bt_duplicated_key) continue;
        if (err) {
            fprintf(stderr, "an error occur while running test\n");
            return 2;
        }
        c++;
    }
    bt_del_btree(t);
    plist = debug_get_allocated_pointers();
    if (plist) {
        fprintf(stderr, "leak found:\n");
        tp = plist;
        while(tp) {
            fprintf(stderr, "address 0x%llx\n",
                    (unsigned long long)(tp->p));
            tp2 = tp->next;
            debug_free(tp->p);
            tp = tp2;
        }
        plist = debug_get_allocated_pointers();
        if (! plist) {
            return 3;
        }
        else {
            fprintf(stderr, "debug_free is buggy...\n");
            return 4;
        }
    }
    return 0;
}
