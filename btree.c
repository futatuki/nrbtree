# include <stdlib.h>
# include <assert.h>
# include <sys/stdint.h>
# include "bt_conf.h"
# include "btree.h"

static const char *revid = "$yfId: readindex/branches/on_the_way/btree/btree.c 63 2016-10-23 03:16:17Z futatuki $";


# if defined(GENERIC_LIB) && GENERIC_LIB
# define KEYCOMP(t,x,y) (((t)->key_comp_func)?\
        (*((t)->key_comp_func))((x),(y)):((x)-(y)))
# define USE_KEY(t,k) if ((t)->key_use_func) (*((t)->key_use_func))(k)
# define DEL_KEY(t,k) if ((t)->key_del_func) (*((t)->key_del_func))(k)
# define USE_VAL(t,v) if ((t)->val_use_func) (*((t)->val_use_func))(v)
# define DEL_VAL(t,v) if ((t)->val_del_func) (*((t)->val_del_func))(v)
# else
# define USE_KEY(t,k)
# define DEL_KEY(t,k)
# define USE_VAL(t,v)
# define DEL_VAL(t,v)
# endif

# if !defined(KEYCOMP)
# define KEYLT(t,x,y)     ((x)<(y))
# define KEYGT(t,x,y)     ((x)>(y))
# define KEYLE(t,x,y)     ((x)<=(y))
# define KEYGE(t,x,y)     ((x)>=(y))
# define KEYEQ(t,x,y)     ((x)==(y))
# define KEYNE(t,x,y)     ((x)!=(y))
# else
# define KEYLT(t,x,y)     (KEYCOMP((t),(x),(y))<0)
# define KEYGT(t,x,y)     (KEYCOMP((t),(x),(y))>0)
# define KEYLE(t,x,y)     (KEYCOMP((t),(x),(y))<=0)
# define KEYGE(t,x,y)     (KEYCOMP((t),(x),(y))>=0)
# define KEYEQ(t,x,y)     (KEYCOMP((t),(x),(y))==0)
# define KEYNE(t,x,y)     (KEYCOMP((t),(x),(y))!=0)
# endif

# if defined(DEBUG)
# include <stdio.h>

static plist_t alloc_p = { NULL, NULL, {}};

void * debug_malloc(size_t s)
{
  plist_t *plp;

  plp = (plist_t*)malloc(sizeof(plist_t)+s);
  if (!plp) {
    return NULL;
  }
  plp->p = (void *)(&(plp->mem[0]));
  plp->next = alloc_p.next;
  alloc_p.next = plp;
  return plp->p;
}

int debug_is_allocated_pointer(void *p)
{
  plist_t *plp, *fplp;

  plp = &alloc_p;
  while(plp->next) {
    if (plp->next->p == p) {
      fplp = plp->next;
      plp->next = fplp->next;
      return 1;
    }
    plp = plp->next;
  }
  return 0;
}

void debug_free(void *p)
{
  plist_t *plp, *fplp;

  plp = &alloc_p;

  while(plp->next) {
    if (plp->next->p == p) {
      fplp = plp->next;
      plp->next = fplp->next;
      free(fplp);
      return;
    }
    plp = plp->next;
  }
  free(p);
}

plist_t * debug_get_allocated_pointers(void)
{
  return alloc_p.next;
}

# define malloc(x) debug_malloc(x)
# define free(x) debug_free(x)
# endif

bt_node_data_t * bt_new_node_data(BT_KEY_T key, BT_VAL_T val)
{
  bt_node_data_t * ndp;
  if (NULL != (ndp = (bt_node_data_t *)malloc(sizeof(bt_node_data_t)))) {
    ndp->key = key;
    ndp->node = NULL;
    ndp->prev = NULL;
    ndp->next = NULL;
    ndp->e.val = val;
  }
  return ndp;
}

void bt_del_node_data(bt_node_data_t *nd)
{
  if (nd) free(nd);
  return;
}

/* create new B-tree for hash offset to URL/REDR record pointer */
bt_node_t * bt_new_node(bt_node_type_t node_type, bt_node_data_t * parent)
{
  bt_node_t * np;
  if (NULL != (np = (bt_node_t *)malloc(sizeof(bt_node_t)))) {
    np->node_type = node_type;
    np->n_indexes = 0;
    np->index_top.node = np;
    np->index_top.prev = NULL;
    np->index_top.next = NULL;
    np->index_top.e.val = NULL;
    np->parent  = parent;
  }
  return np;
}

void bt_del_btree(bt_btree_t *bt)
{
  bt_node_t *np, *tnp;
  bt_node_data_t *dp, *tdp, *pdp;

  assert(bt);
  np = bt->root;
  assert(np);
  tnp = np;
  dp = &(tnp->index_top);
  while (np->n_indexes) {
    if (   tnp->node_type == bt_node_internal
        || tnp->node_type == bt_node_root) {
      /* node has sub trees */
      tnp = dp->e.r_child;
      dp = &(tnp->index_top);
    }
    else {
      /* node is leaf */
      assert(dp == &(tnp->index_top));
      dp  = dp->next;
      pdp = dp->prev;
      while(tnp->n_indexes) {
        assert(dp);
        tdp = dp;
        dp = tdp->next;
        DEL_KEY(bt, tdp->key);
        DEL_VAL(bt, tdp->e.val);
        bt_del_node_data(tdp);
        (tnp->n_indexes)--;
      }
      if (pdp) pdp->next = dp;
      if (dp)  dp->prev = pdp;
      /*  removing leaves done */
      do {
        if (tnp == np) {
          break;
        }
        /* remove this node and branch from parent */
        assert(tnp->parent);
        tdp = tnp->parent;
        free(tnp);
        tnp = tdp->node;
        dp = tdp->next;
        if (tdp != &(tnp->index_top)) {
          bt_del_node_data(tdp);
          (tnp->n_indexes)--;
        }
      }
      while(tnp->n_indexes == 0);
    }
  }
  free(np);
  free(bt);
  return;
}

static
bt_node_data_t * _bt_search_candidate_cell(bt_btree_t * bt, BT_KEY_T key)
{
  bt_node_t *np, *tnp;
  bt_node_data_t *tdp;
  int idx;

  assert(bt);
  np = bt->root;
  assert(np);
  tnp = np;
# if defined(DEBUG)
  printf("enter _bt_search_data: np = 0x%0llx, key = %lld\n",
        (unsigned long long)np, (unsigned long long)key);
  fflush(NULL);
# endif
  while(tnp->node_type == bt_node_root || tnp->node_type == bt_node_internal) {
    tdp = &(tnp->index_top);
    idx = 0;
    while(idx < tnp->n_indexes) {
      assert(tdp->next);
      if (KEYEQ(bt, key, tdp->next->key)) {
        tdp = tdp->next;
        break;
      }
      else if (KEYLT(bt, key, tdp->next->key)) {
        break;
      }
      tdp = tdp->next;
      idx++;
    }
    tnp = tdp->e.r_child;
# if defined(DEBUG)
    if (tdp == &(tdp->node->index_top)) {
      assert(tdp->next);
      printf("at node top, tdp->next->key = %llu: descend to tnp = 0x%0llx\n",
              (unsigned long long)(tdp->next->key), (unsigned long long)tnp);
    }
    else {
      assert(tdp);
      printf("at idx = %ld, tdp->key = %llu: descend to tnp = 0x%0llx\n",
              (long)idx, (unsigned long long)(tdp->key),
              (unsigned long long)tnp);
    }
    fflush(NULL);
# endif
  }
  /* now on a leaf node */
  idx = 0;
  tdp = &(tnp->index_top);
  while(idx < tnp->n_indexes && KEYLT(bt, tdp->next->key, key)) {
    tdp = tdp->next;
    idx++;
    assert(idx > tnp->n_indexes || tdp);
  }
# if defined(DEBUG)
  printf( "candidate node data found at "
          "tnp = 0x%llx, idx = %d, tdp->key = %llu\n",
          (unsigned long long)tnp, idx, (unsigned long long)tdp->key);
  fflush(NULL);
# endif
  return tdp;
}

bt_error_t bt_insert(bt_btree_t * bt, BT_KEY_T key, BT_VAL_T val)
{
  bt_node_t *np, *tnp, *tnp2;
  bt_node_data_t *dp, *tdp, *tdp2;
  int idx;
  bt_node_type_t new_node_type;

  if (bt == NULL || bt->root == NULL) return bt_tree_is_null;
  np = bt->root;
# if defined(DEBUG)
  printf("enter _bt_insert: bt = 0x%0llx, key = %llu\n",
         (unsigned long long)bt, (unsigned long long)key);
  fflush(NULL);
# endif

  if (bt->top == NULL) {
    assert(    bt->root->n_indexes == 0
            && bt->root->node_type == bt_node_only_root);
    /* short cut : the tree is empty */
    if (NULL == (dp = bt_new_node_data(key, val))) {
      return bt_not_enough_memory;
    }
    USE_KEY(bt, key);
    USE_VAL(bt, val);
    dp->node = bt->root;
    bt->root->index_top.next = bt->top = dp;
    bt->root->n_indexes = 1;
    return bt_success;
  }
  /* search place to insert */
  if (KEYLT(bt, key, bt->top->key)) {
    /* short cut */
    tnp = bt->top->node;
    tdp = &(tnp->index_top);
    assert(tdp->next == bt->top);
    if (NULL == (dp = bt_new_node_data(key, val))) {
      return bt_not_enough_memory;
    }
    bt->top = dp;
  }
  else {
    tdp = _bt_search_candidate_cell(bt, key);
    if (tdp->next && KEYEQ(bt, tdp->next->key, key)) {
      return bt_duplicated_key;
    }
    tnp = tdp->node;
    assert(   ( (&(tnp->index_top) == tdp) || KEYLT(bt, tdp->key, key))
           && ((tdp->next == NULL) || KEYLT(bt, key, tdp->next->key)));
    if (NULL == (dp = bt_new_node_data(key, val))) {
      return bt_not_enough_memory;
    }
  }
  USE_KEY(bt, key);
  USE_VAL(bt, val);
  dp->next = tdp->next;
  tdp->next = dp;
  if (dp->next) {
    dp->prev = dp->next->prev;
    dp->next->prev = dp;
  }
  else { /* dp->next == NULL */
    assert(tdp != &(tnp->index_top));
    dp->prev = tdp;
  }
  if (dp->prev)  dp->prev->next = dp;
  dp->node = tnp;
  (tnp->n_indexes)++;
  /* devine node if needed */
  while (tnp->n_indexes >= bt_maxbranch) {
    /* need to devine node */
    if (np == tnp) {
      assert(   tnp->node_type == bt_node_only_root
             || tnp->node_type == bt_node_root);
# if defined(DEBUG)
      printf("root node need to devine, so, create a child node to hold"
             "leaves holding root\n");
      fflush(NULL);
# endif
      new_node_type = (tnp->node_type == bt_node_only_root)?
                      bt_node_leaf : bt_node_internal;
      /* create new node to hold old new root node branches */
      if (NULL == (tnp2 = bt_new_node(new_node_type, &(tnp->index_top)))) {
        return bt_not_enough_memory;
      }
      assert(tnp2->index_top.node == tnp2);
      tnp2->n_indexes = tnp->n_indexes;
      tnp2->index_top.next = tnp->index_top.next;
      tnp2->index_top.e.val = tnp->index_top.e.val;
      if (new_node_type == bt_node_internal) {
          tnp2->index_top.e.r_child = tnp->index_top.e.r_child;
          assert(tnp2->index_top.e.r_child->parent == &(tnp->index_top));
          tnp2->index_top.e.r_child->parent = &(tnp2->index_top);
      }
      else {
          tnp2->index_top.e.val = tnp->index_top.e.val;
      }
      idx = 0;
      tdp = tnp2->index_top.next;
      while (idx < tnp2->n_indexes) {
        idx++;
        assert(tdp);
        tdp->node = tnp2;
        tdp = tdp->next;
      }
      tnp->index_top.next = NULL;
      tnp->index_top.e.r_child = tnp2;
      tnp->n_indexes = 0;
      tnp->node_type = bt_node_root;
      tnp = tnp2;
    }
    idx = 1;
    tdp = tnp->index_top.next;
    while (idx < (bt_maxbranch >> 1)) {
      tdp = tdp->next;
      idx++;
    }
    assert(tdp->next);
    if (tnp->node_type == bt_node_leaf) {
# if defined(DEBUG)
      printf("split a leaf node...\n");
      fflush(NULL);
# endif
      if (NULL == (tdp2 = bt_new_node_data(tdp->next->key, NULL))) {
        return bt_not_enough_memory;
      }
      if (NULL == (tnp2 = bt_new_node(bt_node_leaf, tdp2))) {
        bt_del_node_data(tdp2);
        return bt_not_enough_memory;
      }
      tdp2->e.r_child= tnp2;
      tdp = tdp->next;
      tnp2->index_top.next = tdp;
      tnp2->n_indexes = tnp->n_indexes - idx;
      tnp->n_indexes = idx;
    }
    else {
      assert(tnp->node_type == bt_node_internal);
# if defined(DEBUG)
      printf("split a internal node...\n");
      fflush(NULL);
# endif
      tdp2 = tdp->next;
      assert(tdp == tdp2->prev);
      tdp->next = NULL;
      if (NULL == (tnp2 = bt_new_node(bt_node_internal, tdp2))) {
        return bt_not_enough_memory;
      }
      tdp = tdp2->next;
      tnp2->index_top.next = tdp;
      tdp->prev = NULL;
      tnp2->index_top.e.r_child = tdp2->e.r_child;
      tnp2->index_top.e.r_child->parent = &(tnp2->index_top);
      tdp2->e.r_child = tnp2;
      /* tdp2->node, tdp2->prev and tdp2->next are dirty (not updated) */
      tnp2->n_indexes = tnp->n_indexes - idx - 1;
      tnp->n_indexes = idx;
      assert(tnp2->n_indexes > 0);
    }
    idx = tnp2->n_indexes;
    while(idx) {
      assert(tdp);
      tdp->node = tnp2;
      tdp = tdp->next;
      idx--;
    }
    /* then insert new node into parent node */
    /* tdp2 should be node_data to hold branch to new node */
    tdp = tnp->parent;
    tnp = tdp->node;
    tdp2->next = tdp->next;
    tdp->next = tdp2;
    if (tdp2->next) {
      /* tdp2 is not the last child */
      tdp2->prev = tdp2->next->prev;
      tdp2->next->prev = tdp2;
    }
    else {
      /* tdp2 is the last child */
      if (tdp != &(tnp->index_top)) {
        tdp2->prev = tdp;
      }
      else {
        assert(tnp == np);
        tdp2->prev = NULL;
      }
    }
    tdp2->node = tnp;
    (tnp->n_indexes)++;
  }
  return bt_success;
}

bt_error_t bt_search_data(bt_btree_t * bt, BT_KEY_T key, BT_VAL_T *valp)
{
  bt_node_data_t *dp;
  bt_error_t err;

  if (NULL == bt) return bt_tree_is_null;
  dp = _bt_search_candidate_cell(bt, key);
  if (dp->next && KEYEQ(bt, dp->next->key, key)) {
    *valp = dp->next->e.val;
    err = bt_success;
  }
  else {
    err = bt_key_not_found;
  }
  return err;
}

bt_error_t bt_update_data(bt_btree_t * bt, BT_KEY_T key, BT_VAL_T val)
{
  bt_node_data_t *dp;
  bt_error_t err;

  if (NULL == bt) return bt_tree_is_null;
  dp = _bt_search_candidate_cell(bt, key);
  if (dp->next && KEYEQ(bt, dp->next->key, key)) {
    DEL_VAL(bt, dp->next->e.val);
    dp->next->e.val = val;
    USE_VAL(bt, val);
    err = bt_success;
  }
  else {
    err = bt_key_not_found;
  }
  return err;
}

bt_btree_t * bt_new_btree(BT_NEW_BTREE_ARGS)
{
  bt_btree_t * btp;

  if (NULL != (btp = (bt_btree_t*)malloc(sizeof(bt_btree_t)))) {
    if (NULL == (btp->root = bt_new_node(bt_node_only_root, NULL))) {
      free(btp);
      btp = NULL;
    }
    else {
      btp->top = NULL;
# if defined(GENERIC_LIB) && GENERIC_LIB
      btp->key_comp_func = key_comp_func;
      btp->key_use_func = key_use_func;
      btp->val_use_func = val_use_func;
      btp->key_del_func = key_del_func;
      btp->val_del_func = val_del_func;
# endif
    }
  }
  return btp;
}
