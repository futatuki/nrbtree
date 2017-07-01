# include <stdlib.h>
# include <assert.h>
# include <sys/stdint.h>
# include "btree.h"

static const char *revid = "$yfId: btree/trunk/btree.c 80 2017-07-01 17:08:08Z futatuki $";


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

void debug_print_leak_pointers(void)
{
  plist_t * plist, * tp, * tp2;

  plist = debug_get_allocated_pointers();
  if (plist) {
    fprintf(stderr, "leak found:\n");
    tp = plist;
    while(tp) {
      fprintf(stderr, "address 0x%llx\n", (unsigned long long)(tp->p));
      tp2 = tp->next;
      debug_free(tp->p);
      tp = tp2;
    }
    plist = debug_get_allocated_pointers();
    assert(plist == NULL);
  }
  else {
    fprintf(stderr, "leak not found:\n");
  }
  return;
}

# define malloc(x) debug_malloc(x)
# define free(x) debug_free(x)

static const char * bt_node_type_to_str(bt_node_type_t t)
{
  if (t == bt_node_only_root) {
     return "bt_node_only_root";
  }
  else if (t == bt_node_root) {
     return "bt_node_root";
  }
  else if (t == bt_node_internal) {
     return "bt_node_internal";
  }
  else if (t == bt_node_leaf) {
     return "bt_node_leaf";
  }
  else if (t == bt_node_unknown) {
     return "bt_node_unknown";
  }
  return "UNKNOWN";
}

/* check node links and return 1 if ok else return 0 */
static int _bt_check_node_consistency(bt_btree_t * bt, bt_node_t * np)
{
  int idx;
  bt_node_data_t * dp;
  bt_node_t * tnp;
  bt_node_data_t * lmdp;
  int n_leaves;
  int rv;

  rv = 1;
  if (bt == NULL) {
    printf("bt_btree_t * bt is NULL\n");
    rv = 0;
    goto _return;
  }
  if (np == NULL) {
    printf("bt_node_t * np is NULL\n");
    rv = 0;
    goto _return;
  }

  /* check parents */
  if (bt->root == np) {
    if (np->node_type != bt_node_only_root && np->node_type != bt_node_root) {
      printf("bt->root == np but bt->node_type = %s\n",
              bt_node_type_to_str(np->node_type));
    }
    /* check parent */
    if (np->parent != NULL) {
      printf("bt->root == np but np->parent != NULL\n");
      if (np->parent->e.r_child != np) {
        printf("missing link with parent: np->parent->e.r_child != np\n");
      }
    }
  }
  else { /* bt->root != np */
    if (np->node_type != bt_node_internal && np->node_type != bt_node_leaf) {
      printf("bt->root != np but bt->node_type = %s\n",
              bt_node_type_to_str(np->node_type));
    }
    /* check parent */
    if (np->parent == NULL) {
      printf("bt->root != np but np->parent == NULL\n");
    }
    else {
      if (np->parent->e.r_child != np) {
        printf("missing link with parent: np->parent->e.r_child != np\n");
      }
    }
  }
  /* check number of branch (or leaves) */
  if (np->n_indexes < 0) {
    printf("negative number of indexes : %ld\n", (long)(np->n_indexes));
    rv = 0;
    goto _return;
  }
  if (np->n_indexes >= bt_maxbranch) {
    printf("too many leaves : %ld\n", (long)(np->n_indexes));
  }
  else if (   np->n_indexes < (bt_maxbranch >> 1)
          && np->node_type != bt_node_only_root
          && np->node_type != bt_node_root) {
    printf("(warn)too few leaves : %ld\n", (long)(np->n_indexes));
  }
  /* check branch or leaves */
  if (np->node_type == bt_node_only_root || np->node_type == bt_node_leaf) {
    /* node has leaves */
    if (np->index_top.e.r_child != NULL) {
      printf("node type is leaf, but np->index_top.e.r_child != NULL\n");
      rv = 0;
    }
    dp = np->index_top.next;
    if (dp == NULL) {
      if (np->n_indexes == 0) {
        printf("empty leaf node\n");
      }
      else {
        printf("at leaf node np, np->n_indexes = %d, "
               "but actually no node cell\n", np->n_indexes);
      }
      rv = 0;
      goto _return;
    }
    if (dp->prev == NULL && dp != bt->top) {
      printf("at leaf node np, np->index_top.next->prev == NULL "
             "but bt->top != np->index_top.next\n");
      rv = 0;
    }
    else if (dp == bt->top && dp->prev != NULL) {
      printf("bt->top == np->index_top.next, "
             "but np->index_top.next points somewhere\n");
      rv = 0;
    }
    if (dp->prev != NULL && dp->prev->node == np) {
      printf("at leaf node np, np->index_top.next->prev->node points np\n");
      rv = 0;
    }
    if (   dp->status != bt_cell_status_unused
        && dp->status != bt_cell_status_value_used) {
      printf("at leaf node np, first cell type is not for value cell\n");
      rv = 0;
    }
    n_leaves = 0;
    idx = 1;
    while(idx < np->n_indexes) {
      if (dp->node != np) {
        printf("at leaf node np, %ldth cell dp->nodes not points np\n",
                (long)idx);
        rv = 0;
      }
      if (dp->status == bt_cell_status_value_used) {
        n_leaves++;
      }
      else if (dp->status != bt_cell_status_unused) {
        printf("at leaf node np, %ldth bt_node_data_cell is not "
               "valid cell status %ld\n",
                (long)idx, (long)(dp->status));
        rv = 0;
      }
      if (dp->next == NULL) {
        printf("at leaf node np, %ldth bt_node_data cell is NULL\n",
                (long)idx);
        rv = 0;
        goto _return;
      }
      if (   dp->status != bt_cell_status_unused
          && dp->status != bt_cell_status_value_used) {
        printf("at leaf node np, %ldth cell type is not for value cell\n",
                (long)idx);
        rv = 0;
      }
      if (dp->next->prev != dp) {
        printf("at leaf node np, %ldth data cell dp, dp->next->prev != dp\n",
                (long)idx);
        rv = 0;
        goto _return;
      }
      if (! KEYLT(bt, dp->key, dp->next->key)) {
        printf("at leaf node np, %ldth data cell dp, "
               "dp->key >= dp->next->key\n", (long)idx);
      }
      dp = dp->next;
      idx++;
    }
    /* check last data cell of the node */
    if (dp->next != NULL) {
      if (dp->next->prev != dp) {
        printf("at leaf node np, %ldth (last) data cell dp, "
               "dp->next->prev != dp\n", (long)idx);
        rv = 0;
      }
      if (! KEYLT(bt, dp->key, dp->next->key)) {
        printf("at leaf node np, %ldth (last) data cell dp, "
               "dp->key >= dp->next->key\n", (long)idx);
      }
      if (dp->next->node == np) {
        printf("at leaf node np, %ldth (last) data cell dp, "
               "dp->next->node points np\n", (long)idx);
        rv = 0;
      }
    }
    if (dp->status == bt_cell_status_value_used) {
      n_leaves++;
    }
    else if (dp->status != bt_cell_status_unused) {
      printf("at leaf node np, %ldth (last) bt_node_data_cell is not "
             "valid cell status %ld\n",
              (long)idx, (long)(dp->status));
      rv = 0;
    }
  }
  else if (np->node_type == bt_node_root || np->node_type == bt_node_internal) {
    /* node has branches */
    if (np->n_indexes >= bt_maxbranch) {
      printf("too many branches : %ld\n", (long)(np->n_indexes));
      /* rv = 0; */
    }
    else if(   np->n_indexes < (bt_maxbranch >> 1)
            && np->node_type != bt_node_root) {
      printf("(warn)too few leaves : %ld\n", (long)(np->n_indexes));
    }
    /* check subtree key */
    lmdp = NULL;
    tnp = np->index_top.e.r_child;
    n_leaves = tnp->n_leaves;
    while (tnp != NULL && tnp->node_type == bt_node_internal) {
      tnp = tnp->index_top.e.r_child;
    }
    if (tnp == NULL) {
      printf("in checking internal node, while descending left most branch, \n"
             "cannot reach leaf node: node pointer points NULL \n");
      rv = 0;
    }
    else if (tnp->node_type != bt_node_leaf) {
      printf("in checking internal node, while descending left most branch, \n"
             "cannot reach leaf node: node type is not bt_node_leaf \n");
      rv = 0;
    }
    else if (tnp->index_top.next == NULL) {
      printf("in checking internal node of subtree, "
             "left most leaf data cannot find: \n"
             "leaf node has no data\n");
      rv = 0;
    }
    else {
      lmdp = tnp->index_top.next;
    }
    if (lmdp != NULL) {
      tnp = np;
      while (tnp->parent != NULL) {
        dp = tnp->parent;
        tnp = dp->node;
        if (   tnp == NULL
            || (   tnp->node_type != bt_node_internal
                && tnp->node_type != bt_node_root)) {
          printf("in checking internal node of ancestors, "
                 "broken node found\n");
          rv = 0;
          break;
        }
        if (dp != &(tnp->index_top)) {
          if (! KEYEQ(bt, dp->key, lmdp->key)) {
            printf("in checking internal node of ancestors, branch key "
                   "not equal to the key of the left most\n"
                   "data of the subtree of the node np\n");
            /* while key may be updated later, so it is not critical */
            /* rv = 0; */
          }
          break;
        }
        else {
          if (tnp->node_type == bt_node_root) {
            if (bt->top != lmdp) {
               printf("in checking internal node of ancestors, the node "
                      "brong the path to left most\n"
                      "node, but the left most node of sub tree of the node "
                      "is not bt->top\n");
               rv = 0;
            }
            break;
          }
          else {
            continue;
          }
        }
      }
    }
    dp = np->index_top.next;
    if (dp->prev != &(np->index_top)) {
      printf("at internal node np, first data cell dp, "
             "dp->prev don't point back to \n"
             "&(np->index_top)\n");
      rv = 0;
    }
    if (! KEYLT(bt, lmdp->key, dp->key)) {
      printf( "at internal node np, first data cell dp, "
              "lmdp->key >= dp->key where lmdp\n"
               "is the left most data of the sub tree\n");
      rv = 0;
    }
    idx = 1;
    while(idx < np->n_indexes) {
      if (dp->node != np) {
        printf("at internal node np, %ldth cell dp->nodes not points np\n",
                (long)idx);
        rv = 0;
      }
      if (   dp->status != bt_cell_status_branch_used) {
        printf("at internal node np, %ldth cell type is not for branch cell\n",
                (long)idx);
        rv = 0;
      }
      if ( dp->e.r_child == NULL) {
        printf("at internal node np, %ldth cell of the child branch is NULL\n",
                (long)idx);
        rv = 0;
      }
      else if (dp->e.r_child->parent != dp) {
        printf("at internal node np, %ldth cell of the child branch node\n"
               "don't point back to the cell dp\n", (long)idx);
        rv = 0;
      }
      n_leaves += dp->e.r_child->n_leaves;
      if (dp->next == NULL) {
        printf("at internal node np, the next cell of %ldth (not last) \n"
               "bt_node_data cell dp is NULL\n", (long)idx);
        rv = 0;
        goto _return;
      }
      if (dp->next->prev != dp) {
        printf("at internal node np, %ldth data cell dp, "
               "dp->next->prev != dp\n", (long)idx);
        rv = 0;
        goto _return;
      }
      if (! KEYLT(bt, dp->key, dp->next->key)) {
        printf("at internal node np, %ldth data cell dp, "
               "dp->key >= dp->next->key\n", (long)idx);
      }
      dp = dp->next;
      idx++;
    }
    /* check last data cell of the node */
    if (dp->next != NULL) {
      printf("at internal node np, %ldth (last) data cell dp, "
             "dp->next is not NULL\n", (long)idx);
      rv = 0;
    }
    if (dp->node != np) {
      printf("at internal node np, "
             "%ldth (last) cell dp->nodes not points np\n", (long)idx);
      rv = 0;
    }
    if (   dp->status != bt_cell_status_branch_used) {
      printf("at internal node np, "
             "%ldth (last) cell type is not for branch cell\n", (long)idx);
      rv = 0;
    }
    if ( dp->e.r_child == NULL) {
      printf("at internal node np, "
             "%ldth (last) cell of the child branch is NULL\n", (long)idx);
      rv = 0;
    }
    else if (dp->e.r_child->parent != dp) {
      printf("at internal node np, "
             "%ldth (last) cell of the child branch node\n"
             "don't point back to the cell dp\n", (long)idx);
        rv = 0;
    }
    n_leaves += dp->e.r_child->n_leaves;
  }
  else {
    printf("invalid node type\n");
    rv = 0;
    goto _return;
  }
  /* check n_leaves */
  if (np->n_leaves != n_leaves) {
    printf("while number of effective leaves (sum) is %ld, "
           "np->n_leaves is %ld\n", (long)n_leaves, (long)(np->n_leaves));
    rv = 0;
  }

  _return:
    fflush(NULL);
    return rv;
}
# endif

bt_node_data_t * bt_new_node_data(
    BT_KEY_T key, BT_VAL_T val, bt_cell_status_t stat)
{
  bt_node_data_t * ndp;
  if (NULL != (ndp = (bt_node_data_t *)malloc(sizeof(bt_node_data_t)))) {
    ndp->key = key;
    ndp->node = NULL;
    ndp->prev = NULL;
    ndp->next = NULL;
    ndp->status = stat;
    ndp->e.val = val;
  }
  return ndp;
}

void bt_del_node_data(bt_node_data_t *nd)
{
  if (nd) free(nd);
  return;
}

/* create new B-tree */
bt_node_t * bt_new_node(bt_node_type_t node_type, bt_node_data_t * parent)
{
  bt_node_t * np;
  if (NULL != (np = (bt_node_t *)malloc(sizeof(bt_node_t)))) {
    np->node_type = node_type;
    np->n_indexes = 0;
    np->n_leaves = 0;
    np->index_top.node = np;
    np->index_top.prev = NULL; /* unused */
    np->index_top.next = NULL;
    np->index_top.status = bt_cell_status_part_of_node;
    /* np->index_top.key is unused */
    np->index_top.e.val = NULL;
    np->parent  = parent;
  }
  return np;
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
        if (tdp->status == bt_cell_status_value_used) {
            DEL_VAL(bt, tdp->e.val);
        }
        bt_del_node_data(tdp);
        (tnp->n_indexes)--;
      }
      if (pdp != NULL) pdp->next = dp;
      if (dp  != NULL)  dp->prev = pdp;
      /*  removing leaves done */
      do {
        if (tnp == np) {
          break;
        }
        /* remove this node and branch from parent */
        assert(tnp->parent != NULL);
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

bt_node_data_t * bt_search_candidate_cell(bt_btree_t * bt, BT_KEY_T key)
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


bt_error_t bt_insert(
    bt_btree_t * bt, BT_KEY_T key, BT_VAL_T val, int overwrite)
{
  bt_node_t *np, *tnp, *tnp2;
  bt_node_data_t *dp, *tdp, *tdp2;
  int idx;

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
    /* short cut (1) : the tree is empty */
    if (NULL == (dp = bt_new_node_data(key, val, bt_cell_status_value_used))) {
      return bt_not_enough_memory;
    }
    USE_KEY(bt, key);
    USE_VAL(bt, val);
    dp->node = bt->root;
    bt->root->index_top.next = bt->top = dp;
    bt->root->n_indexes = 1;
    bt->root->n_leaves = 1;
    return bt_success;
  }
  /* search place to insert */
  if (KEYEQ(bt, key, bt->top->key)) {
    /* short cut (2) : the key is equal to smallest key of the tree */
    if (bt->top->status == bt_cell_status_unused) {
      bt->top->e.val = val;
      USE_VAL(bt, val);
      bt->top->status = bt_cell_status_value_used;
      return bt_success;
    }
    else if(overwrite) {
      assert(bt->top->status == bt_cell_status_value_used);
      DEL_VAL(bt, bt->top->e.val);
      bt->top->e.val = val;
      USE_VAL(bt, val);
      return bt_success;
    }
    else {
      return bt_duplicated_key;
    }
  }
  if (KEYLT(bt, key, bt->top->key)) {
    /* short cut (3) : the key is new smallest key of the tree */
    /* insert a new cell on left most of the tree */
    tnp = bt->top->node;
    tdp = &(tnp->index_top);
    assert(tdp->next == bt->top);
    if (NULL == (dp = bt_new_node_data(key, val, bt_cell_status_value_used))) {
      return bt_not_enough_memory;
    }
    bt->top = dp;
  }
  else {
    tdp = bt_search_candidate_cell(bt, key);
    if (tdp->next && KEYEQ(bt, tdp->next->key, key)) {
      if (tdp->next->status == bt_cell_status_unused) {
        tdp->next->e.val = val;
        USE_VAL(bt, val);
        tdp->next->status = bt_cell_status_value_used;
        return bt_success;
      }
      else if (overwrite) {
        assert(tdp->next->status == bt_cell_status_value_used);
        DEL_VAL(bt, tdp->next->e.val);
        tdp->next->e.val = val;
        USE_VAL(bt, val);
        return bt_success;
      }
      else {
        return bt_duplicated_key;
      }
    }
    /* insert a new cell on the right place of the tree */
    tnp = tdp->node;
    assert(   ( (&(tnp->index_top) == tdp) || KEYLT(bt, tdp->key, key))
           && ((tdp->next == NULL) || KEYLT(bt, key, tdp->next->key)));
    if (NULL == (dp = bt_new_node_data(key, val, bt_cell_status_value_used))) {
      return bt_not_enough_memory;
    }
  }
  USE_KEY(bt, key);
  USE_VAL(bt, val);
  dp->next = tdp->next;
  tdp->next = dp;
  if (dp->next != NULL) {
    dp->prev = dp->next->prev;
    dp->next->prev = dp;
  }
  else { /* dp->next == NULL */
    assert(tdp != &(tnp->index_top));
    dp->prev = tdp;
  }
  if (dp->prev != NULL)  dp->prev->next = dp;
  dp->node = tnp;
  (tnp->n_indexes)++;
  (tnp->n_leaves)++;
  /* devine node if needed */
  if (tnp->n_indexes >= bt_maxbranch) {
    if (tnp == np) {
      assert(tnp->node_type == bt_node_only_root);
      /* jack up the root node */
      /* create new node to hold old root node branches */
      if (NULL == (tnp2 = bt_new_node(bt_node_leaf, &(tnp->index_top)))) {
        return bt_not_enough_memory;
      }
      assert(tnp2->index_top.node == tnp2);
      tnp2->n_indexes = tnp->n_indexes;
      /* as a parent node, root node np has not yet know a leaf has inserted,
         so count down */
      tnp2->n_leaves = (tnp->n_leaves)--;
      tnp2->index_top.next = tnp->index_top.next;
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
      /* endif tnp is a root node, and then new tnp is bt_node_leaf type */
    }
# if defined(DEBUG)
    printf("split a leaf node...\n");
      fflush(NULL);
# endif
    idx = 1;
    tdp = tnp->index_top.next;
    while (idx < (bt_maxbranch >> 1)) {
      tdp = tdp->next;
      idx++;
    }
    /* devine leaf node */
    if (NULL == (tdp2 = bt_new_node_data(tdp->next->key, NULL,
                                            bt_cell_status_branch_used))) {
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
    idx = tnp2->n_indexes;
    tnp2->n_leaves = 0;
    while(idx) {
      assert(tdp);
      tdp->node = tnp2;
      if (tdp->status == bt_cell_status_value_used) {
        tnp2->n_leaves++;
      }
      tdp = tdp->next;
      idx--;
    }
    tnp->n_leaves -= tnp2->n_leaves;
    /* then insert new node into parent node */
    /* tdp2 should be node_data to hold branch to new node */
    tdp = tnp->parent;
    tnp = tdp->node;
    tdp2->next = tdp->next;
    tdp->next = tdp2;
    if (tdp2->next != NULL) tdp2->next->prev = tdp2;
    tdp2->prev = tdp; /* tdp may be &(tnp->index_top), but it is ok. */
    tdp2->node = tnp;
    (tnp->n_indexes)++;
    /* as one of child node has been increased a leaf counter of this node
       must be counted up */
    (tnp->n_leaves)++;
  }
  /* leaf mode has devided */
  while (tnp->n_indexes >= bt_maxbranch) {
    /* need to devine node */
    if (np == tnp) {
      /* tnp is a root node, then jack up the root node  */
      assert(tnp->node_type == bt_node_root);
# if defined(DEBUG)
      printf("root node need to devine, so, create a child node to hold"
             "branches holding root\n");
      fflush(NULL);
# endif
      /* create new node to hold old root node branches */
      if (NULL == (tnp2 = bt_new_node(bt_node_internal, &(tnp->index_top)))) {
        return bt_not_enough_memory;
      }
      assert(tnp2->index_top.node == tnp2);
      tnp2->n_indexes = tnp->n_indexes;
      /* as a parent node, root node np has not yet know a leaf has inserted,
         so count down */
      tnp2->n_leaves = (tnp->n_leaves--);
      tnp2->index_top.next = tnp->index_top.next;
      tnp2->index_top.next->prev = &(tnp2->index_top);
      tnp2->index_top.e.val = tnp->index_top.e.val;
      tnp2->index_top.e.r_child = tnp->index_top.e.r_child;
      tnp2->index_top.e.r_child->parent = &(tnp2->index_top);
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
      /* endif tnp is a root node, and then new tnp is not a root node */
    }
    idx = 1;
    tdp = tnp->index_top.next;
    while (idx < (bt_maxbranch >> 1)) {
      tdp = tdp->next;
      idx++;
    }
    assert(tdp->next);
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
    tdp->prev = &(tnp2->index_top);
    tnp2->index_top.e.r_child = tdp2->e.r_child;
    tnp2->index_top.e.r_child->parent = &(tnp2->index_top);
    tnp2->n_leaves = tnp2->index_top.e.r_child->n_leaves;
    tdp2->e.r_child = tnp2;
    /* tdp2->node, tdp2->prev and tdp2->next are dirty (not updated) */
    tnp2->n_indexes = tnp->n_indexes - idx - 1;
    tnp->n_indexes = idx;
    assert(tnp2->n_indexes > 0);
    idx = tnp2->n_indexes;
    while(idx) {
      assert(tdp);
      tdp->node = tnp2;
      tnp2->n_leaves += tdp->e.r_child->n_leaves;
      tdp = tdp->next;
      idx--;
    }
    tnp->n_leaves -= tnp2->n_leaves;
    /* then insert new node into parent node */
    /* tdp2 should be node_data to hold branch to new node */
    tdp = tnp->parent;
    tnp = tdp->node;
    tdp2->next = tdp->next;
    tdp->next = tdp2;
    if (tdp2->next != NULL) tdp2->next->prev = tdp2;
    tdp2->prev = tdp; /* tdp may be &(tnp->index_top), but it is ok. */
    tdp2->node = tnp;
    (tnp->n_indexes)++;
    /* as one of child node has been increased a leaf counter of this node
       must be counted up */
    (tnp->n_leaves)++;
  }
  /* counting up n_leaves of ancestors */
  while (tnp != np) {
    tnp = tnp->parent->node;
    (tnp->n_leaves)++;
  }
  return bt_success;
}

bt_error_t bt_search_data(bt_btree_t * bt, BT_KEY_T key, BT_VAL_T *valp)
{
  bt_node_data_t *dp;
  bt_error_t err;

  if (NULL == bt) return bt_tree_is_null;
  dp = bt_search_candidate_cell(bt, key);
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
  dp = bt_search_candidate_cell(bt, key);
  if (dp->next && KEYEQ(bt, dp->next->key, key)
               && (dp->next->status == bt_cell_status_value_used)) {
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

static
void _bt_ancestor_node_decrement_n_leaves(bt_btree_t *bt, bt_node_t * np)
{
  bt_node_t *tnp;
  tnp = np;
  (tnp->n_leaves)--;
  while(tnp != bt->root) {
    tnp = tnp->parent->node;
    (tnp->n_leaves)--;
  }
  return;
}

/* move branches from right sibling node */
static void _bt_move_branch_from_right_sibling(
    bt_btree_t * bt, bt_node_t * np, bt_node_t * rnp, int n)
{
  bt_node_data_t *np_last_dp;
  bt_node_data_t *rnp_old_top;
  BT_KEY_T tmp_key;
  bt_node_t *tnp;
  int idx;

  assert(np->parent != NULL && rnp->parent != NULL);
  assert(np->parent->next == rnp->parent);
  assert(n > 0 && rnp->n_indexes > n);
  if (np->node_type == bt_node_leaf) {
    assert(rnp->node_type == bt_node_leaf);
    for (idx = n; idx > 0; idx--) {
      assert(rnp->index_top.next->next != NULL);
      rnp->index_top.next->node = np;
      rnp->index_top.next = rnp->index_top.next->next;
      if (rnp->index_top.next->status == bt_cell_status_value_used) {
        rnp->n_leaves--;
        np->n_leaves++;
      }
    }
    rnp->n_indexes -= n;
    rnp->parent->key = rnp->index_top.next->key;
  }
  else {
# if defined(DEBUG)
  printf("_bt_move_branch_from_right_sibling: internal_node, n = %ld\n",
        (long)n);
  fflush(NULL);
# endif
    np_last_dp = &(np->index_top);
    for (idx = 0; idx < np->n_indexes; idx++) {
      assert(np_last_dp->next != NULL);
      np_last_dp = np_last_dp->next;
    }
    assert(np_last_dp->next == NULL);
    rnp_old_top = rnp->index_top.next;
    for (idx = n; idx > 0; idx--) {
      assert(rnp->index_top.next->next != NULL);
      rnp->index_top.next = rnp->index_top.next->next;
    }
    rnp->n_indexes -= n;
    /* re-connect cells (reuse border cell which have held branch that
       shall be the left most branch of right node) */
    np_last_dp->next = rnp->index_top.next->prev;
    rnp->index_top.next->prev = &(rnp->index_top);
    np_last_dp->next->node = np;
    if (n == 1) {
      assert(np_last_dp->next->prev == &(rnp->index_top));
      np_last_dp->next->next = NULL;
      np_last_dp->next->prev = np_last_dp;
    }
    else {
      assert(n > 1);
      assert(np_last_dp->next->prev != &(rnp->index_top));
      np_last_dp->next->prev->next = NULL;
      np_last_dp->next->prev = np_last_dp;
      np_last_dp->next->next = rnp_old_top;
      rnp_old_top->prev = np_last_dp->next;
      idx = n;
      while (--idx > 0) {
        rnp_old_top->node = np;
        rnp->n_leaves -= rnp_old_top->e.r_child->n_leaves;
        np->n_leaves += rnp_old_top->e.r_child->n_leaves;
        rnp_old_top = rnp_old_top->next;
      }
      assert(rnp_old_top == NULL);
    }
    /* swap key */
    tmp_key = rnp->parent->key;
    rnp->parent->key = np_last_dp->next->key;
    np_last_dp->next->key = tmp_key;
    /* swap r_child */
    tnp = rnp->index_top.e.r_child;
    rnp->index_top.e.r_child = np_last_dp->next->e.r_child;
    np_last_dp->next->e.r_child = tnp;
    rnp->index_top.e.r_child->parent = &(rnp->index_top);
    np_last_dp->next->e.r_child->parent = np_last_dp->next;
    rnp->n_leaves -= np_last_dp->next->e.r_child->n_leaves;
    np->n_leaves += np_last_dp->next->e.r_child->n_leaves;
  }
  np->n_indexes += n;
  return;
}

/* move branches from left sibling node */
static void _bt_move_branch_from_left_sibling(
    bt_btree_t * bt, bt_node_t * np, bt_node_t * lnp, int n)
{
  bt_node_data_t *tdp;
  bt_node_data_t *tdp2;
  BT_KEY_T tmp_key;
  bt_node_t *tnp;
  int idx;

  assert(np->parent != NULL && lnp->parent != NULL);
  assert(lnp->parent->next == np->parent);
  assert(n > 0);
  idx = lnp->n_indexes;
  assert(idx > n);
  tdp = lnp->index_top.next;
  while (idx > n) {
    tdp = tdp->next;
    idx--;
  }
  lnp->n_indexes -= n;
  if (np->node_type == bt_node_leaf) {
    np->index_top.next = tdp;
    np->parent->key = tdp->key;
    while(idx > 0) {
      tdp->node = np;
      if (tdp->status == bt_cell_status_value_used) {
        (lnp->n_leaves)--;
        (np->n_leaves)++;
      }
      tdp = tdp->next;
      idx--;
    }
  }
  else {
# if defined(DEBUG)
    printf("_bt_move_branch_from_left_sibling: internal_node, n = %ld\n",
          (long)n);
  fflush(NULL);
# endif
    tdp->node = np;
    tdp->prev->next = NULL;
    /* swap branch */
    tnp = tdp->e.r_child;
    tdp->e.r_child = np->index_top.e.r_child;
    np->index_top.e.r_child = tnp;
    lnp->n_leaves -= tnp->n_leaves;
    np->n_leaves += tnp->n_leaves;
    /* re connect branch and parent */
    tdp->e.r_child->parent = tdp;
    np->index_top.e.r_child->parent = &(np->index_top);
    /* swap keys */
    tmp_key = tdp->key;
    tdp->key = np->parent->key;
    np->parent->key = tmp_key;
    if (n == 1) {
      assert(tdp->next == NULL);
      tdp->next = np->index_top.next;
      tdp->next->prev = tdp;
      np->index_top.next = tdp;
      tdp->prev = &(np->index_top);
    }
    else {
      assert(n > 1);
      tdp2 = tdp;
      assert(idx == n);
      while(--idx > 0) {
        tdp2 = tdp2->next;
        tdp2->node = np;
        lnp->n_leaves -= tdp2->e.r_child->n_leaves;
        np->n_leaves += tdp2->e.r_child->n_leaves;
      }
      assert(tdp2->next == NULL);
      tdp2->next = tdp;
      tdp->prev = tdp2;
      tdp = tdp->next;   /* new index_top.next of np */
      tdp2 = tdp2->next; /* tail of cells brought from left node */
      assert(tdp2->next == tdp);
      tdp2->next = np->index_top.next;
      tdp2->next->prev = tdp2;
      np->index_top.next = tdp;
      tdp->prev = &(np->index_top);
    }
  }
  np->n_indexes += n;
  return;
}

static void _bt_merge_nodes(bt_btree_t * bt, bt_node_t * np, bt_node_t * rnp)
{
  bt_node_data_t *np_last_dp;
  bt_node_data_t *tdp;
  bt_node_t * pnp;
  int idx;

  assert(np->parent != NULL && rnp->parent != NULL);
  assert(np->parent->next == rnp->parent);
  pnp = np->parent->node;
  if (--(pnp->n_indexes) || bt->root != pnp) {
    if (rnp->parent->next != NULL) {
      rnp->parent->next->prev = np->parent;
    }
    np->parent->next = rnp->parent->next;
    /* rnp->parent cell is freed from pnp node data chain */
    if (np->node_type == bt_node_leaf) {
      assert(rnp->node_type == bt_node_leaf);
      tdp = rnp->index_top.next;
      idx = rnp->n_indexes;
      while (idx > 0) {
        assert(tdp != NULL);
        tdp->node = np;
        tdp = tdp->next;
        idx--;
      }
      np->n_indexes += rnp->n_indexes;
      bt_del_node_data(rnp->parent);
    }
    else {
# if defined(DEBUG)
  printf("_bt_merge_nodes: internal_node\n");
  fflush(NULL);
# endif
      np_last_dp = &(np->index_top);
      for (idx = 0; idx < np->n_indexes; idx++) {
        assert(np_last_dp->next != NULL);
        np_last_dp = np_last_dp->next;
      }
      assert(np_last_dp->next == NULL);
      /* reuse rnp->parent cell */
      np_last_dp->next = rnp->parent;
      np_last_dp->next->prev = np_last_dp;
      np_last_dp = np_last_dp->next;
      np_last_dp->node = np;
      np_last_dp->e.r_child = rnp->index_top.e.r_child;
      np_last_dp->e.r_child->parent = np_last_dp;
      np_last_dp->next = rnp->index_top.next;
      np_last_dp->next->prev = np_last_dp;
      for (idx = rnp->n_indexes; idx > 0; idx--) {
        assert(np_last_dp->next != NULL);
        np_last_dp = np_last_dp->next;
        np_last_dp->node = np;
      }
      assert(np_last_dp->next == NULL);
      np->n_indexes += rnp->n_indexes + 1;
    }
    np->n_leaves += rnp->n_leaves;
  }
  else { /* pnp->n_indexes == 0 and pnp is root node */
    /* do jack down ... merge 2 children node into parent node */
    assert(pnp->index_top.e.r_child == np);
    assert(pnp->index_top.next->e.r_child == rnp);
    assert(pnp->index_top.next->next == NULL);
    if (np->node_type == bt_node_leaf) {
# if defined(DEBUG)
      printf("_bt_merge_nodes: do jack down from leaf to only root\n");
      fflush(NULL);
# endif
      assert(rnp->node_type == bt_node_leaf);
      pnp->node_type = bt_node_only_root;
      bt_del_node_data(rnp->parent);
      idx = pnp->n_indexes = np->n_indexes + rnp->n_indexes;
      tdp = pnp->index_top.next = np->index_top.next;
      pnp->index_top.e.r_child = NULL;
      assert(bt->top == tdp);
      while (idx-- > 0) {
        tdp->node = pnp;
        tdp = tdp->next;
      }
      assert(tdp == NULL);
    }
    else {
      assert(np->node_type == bt_node_internal);
      assert(rnp->node_type == bt_node_internal);
# if defined(DEBUG)
      printf("_bt_merge_nodes: do jack down internal node\n");
      fflush(NULL);
# endif
      pnp->n_indexes = np->n_indexes + rnp->n_indexes + 1;
      idx = np->n_indexes;
      pnp->index_top.e.r_child = np->index_top.e.r_child;
      pnp->index_top.e.r_child->parent = &(pnp->index_top);
      pnp->index_top.next = np->index_top.next;
      tdp = &(pnp->index_top);
      tdp->next->prev = tdp;
      while (idx-- > 0) {
        tdp->next->node = pnp;
        tdp = tdp->next;
      }
      assert(tdp->next == NULL);
      tdp->next = rnp->parent;
      tdp->next->prev = tdp;
      assert(tdp->next->node == pnp);
      tdp = tdp->next;
      tdp->e.r_child = rnp->index_top.e.r_child;
      tdp->e.r_child->parent = tdp;
      tdp->next = rnp->index_top.next;
      tdp->next->prev = tdp;
      idx = rnp->n_indexes;
      while(idx-- > 0) {
        tdp->next->node = pnp;
        tdp = tdp->next;
      }
      assert(tdp->next == NULL);
    }
    assert(pnp->n_leaves == np->n_leaves + rnp->n_leaves);
    free(np);
  }
  free(rnp);
  return;
}

/* update left most data key of subtree which node np belonged to */
static void _bt_update_lmd_key(bt_btree_t * bt, bt_node_t * np, BT_KEY_T key)
{
  bt_node_t * tnp;
  bt_node_data_t * tdp;

  assert(np != bt->root);
  tnp = np;
  while(tnp != bt->root) {
    tdp = tnp->parent;
    tnp  = tdp->node;
    if (tdp != &(tnp->index_top)) {
      tdp->key = key;
      # if !defined(NDEBUG)
      tdp = NULL; /* mark to use dkey  */
      # endif
      break;
    }
  }
  assert(tdp == NULL);
  return;
}

/* remove leaf node data bt_node_data_t dp from btree bt */
static
void _bt_remove_cell(bt_btree_t  * bt, bt_node_data_t * dp, int dec_leaves)
{
  bt_node_t * np;
  bt_node_t * pnp;
  bt_node_t * rnp;
  bt_node_t * lnp;
  bt_node_data_t * lmdp;
  bt_node_data_t * dp2;
  int idx;
# if defined(DEBUG)
  int check_parent_only;
# endif

  assert(bt && dp);
  np = dp->node;
  assert(np->n_indexes > 0);
  lmdp = NULL;
  assert(np->node_type == bt_node_leaf || np->node_type == bt_node_only_root);
  DEL_KEY(bt, dp->key);
  if (dp->prev != NULL) dp->prev->next = dp->next;
  if (dp->next != NULL) dp->next->prev = dp->prev;
  if (np->index_top.next == dp) {
    np->index_top.next = dp->next;
    lmdp = dp->next;
  }
  if (dp == bt->top) {
    bt->top = dp->next;
    lmdp = NULL;
  }
  if (dec_leaves) {
    (np->n_leaves)--;
  }
  (np->n_indexes)--;
  bt_del_node_data(dp);
  while(np != bt->root) {
    /* on the top of loop, np->n_leaves has already been adjusted */
    /* if np is the root node of the tree then nothing to do any thing else */
    /* so this is for level 2 or upper trees */
    if (np->n_indexes >= (bt_maxbranch >> 1)) {
      if (lmdp != NULL) {
        /* lmdp is the left most cell of some subtree. if root of the subtree
           is some cell of branch, the key of the cell shoud be same as
           lmdp's key. */
        _bt_update_lmd_key(bt, np, lmdp->key);
# if defined(DEBUG)
        assert(_bt_check_node_consistency(bt, np));
# endif
      }
      break;
    }
    dp2 = np->parent;
    pnp = dp2->node;
    /* even if np and siblings has been adjusted, total leaves under pnp is
       not changed, so pnp->n_leaves can be adjusted here */
    if (dec_leaves) {
      (pnp->n_leaves)--;
    }
    if (&(pnp->index_top) == dp2) {
      if (dp2->next == NULL) {
        /* As this cannot be occured for normal btree operation, it seems
           that the code of this block has not been tested since r72
           (on the code before r72, assertion below has wrong condition
           'pnp->n_indexes = 0' which is alwayse false)                 */
        assert (pnp->n_indexes == 0);
        assert (pnp->n_leaves == np->n_leaves);
        if (bt->root == pnp) {
          /* jack down the root node */
          pnp->n_indexes = np->n_indexes;
          pnp->index_top.next = np->index_top.next;
          if (np->node_type == bt_node_leaf) {
            pnp->node_type = bt_node_only_root;
            pnp->index_top.e.r_child = NULL;
          }
          else {
            pnp->index_top.next->prev = &(pnp->index_top);
            pnp->index_top.e.r_child = np->index_top.e.r_child;
            pnp->index_top.e.r_child->parent = &(pnp->index_top);
          }
          idx = 0;
          dp2 = np->index_top.next;
          while (idx < np->n_indexes) {
            idx++;
            assert(dp2);
            dp2->node = pnp;
            dp2 = dp2->next;
          }
          free(np);
# if defined(DEBUG)
          assert(_bt_check_node_consistency(bt, pnp));
# endif
          np = pnp;
          break;
        }
        else {
          /* irregular tree: only one branch in intermediate node */
          /* try to rebalance this node level with  parent branch ... */
          /* and give up balance children */
          np = pnp;
          continue;
        }
      }
      else {
        /* dp2 is the left most branch in this node and this node has
           at leaset one sibling branch. */
        rnp = dp2->next->e.r_child;
        if (   rnp->n_indexes > (bt_maxbranch >> 1)
            && rnp->n_indexes > np->n_indexes + 1) {
          /* move (rnp->n_indexes - np->n_indexes)/2 branches from rnp to np */
          _bt_move_branch_from_right_sibling(
              bt, np, rnp, (rnp->n_indexes - np->n_indexes) >> 1);
# if defined(DEBUG)
          assert(_bt_check_node_consistency(bt, np));
          assert(_bt_check_node_consistency(bt, rnp));
# endif
        }
        else if (np->n_indexes + rnp->n_indexes < bt_maxbranch - 1) {
          /* merge nodes */
# if defined(DEBUG)
          check_parent_only = (pnp == bt->root && pnp->n_indexes <= 1);
# endif
          _bt_merge_nodes(bt, np, rnp);
# if defined(DEBUG)
          assert(_bt_check_node_consistency(bt, pnp));
          if (!check_parent_only)
            assert(_bt_check_node_consistency(bt, np));
# endif
          np = pnp;
          continue;
        }
      }
      if (lmdp != NULL) {
        /* lmdp is the left most cell of some subtree. if root of the subtree
           is some cell of branch, the key of the cell shoud be same as
           lmdp's key. */
        _bt_update_lmd_key(bt, np, lmdp->key);
      }
      np = pnp;
      break;
    }
    else { /* dp2 is not the left most branch */
      if(lmdp != NULL) {
        /* dp2 is the top of the branch of subtree which of left most node
           is lmdp */
        dp2->key = lmdp->key;
        lmdp = NULL;
      }
      if (dp2->next == NULL) {
        /* dp2 is the right most branch in this node and this node has
           at leaset one sibling branch. */
        lnp = dp2->prev->e.r_child;
        if (   lnp->n_indexes > (bt_maxbranch >> 1)
            && lnp->n_indexes > np->n_indexes + 1) {
          /* move (lnp->n_indexes - np->n_indexes)/2 branches from lnp to np */
          _bt_move_branch_from_left_sibling(
              bt, np, lnp, (lnp->n_indexes - np->n_indexes) >> 1);
# if defined(DEBUG)
          assert(_bt_check_node_consistency(bt, np));
          assert(_bt_check_node_consistency(bt, lnp));
# endif
        }
        else if (np->n_indexes + lnp->n_indexes < bt_maxbranch - 1) {
          /* merge nodes */
# if defined(DEBUG)
          check_parent_only = (pnp == bt->root && pnp->n_indexes <= 1);
# endif
          _bt_merge_nodes(bt, lnp, np);
# if defined(DEBUG)
          assert(_bt_check_node_consistency(bt, pnp));
          if (! check_parent_only)
            assert(_bt_check_node_consistency(bt, lnp));
# endif
          np = pnp;
          continue;
        }
        np = pnp;
        break;
      }
      else {
        /* dp2 has left and right sibling branches */
        lnp = dp2->prev->e.r_child;
        rnp = dp2->next->e.r_child;
        if (lnp->n_indexes <= rnp->n_indexes) {
          if (   rnp->n_indexes > (bt_maxbranch >> 1)
              && rnp->n_indexes > np->n_indexes + 1) {
            /* move (rnp->n_indexes - np->n_indexes)/2 branches
               from rnp to np */
            _bt_move_branch_from_right_sibling(
                bt, np, rnp, (rnp->n_indexes - np->n_indexes) >> 1);
# if defined(DEBUG)
            assert(_bt_check_node_consistency(bt, np));
            assert(_bt_check_node_consistency(bt, rnp));
# endif
          }
          else if (np->n_indexes + lnp->n_indexes < bt_maxbranch - 1) {
            /* merge with left node */
# if defined(DEBUG)
            check_parent_only = (pnp == bt->root && pnp->n_indexes <= 1);
# endif
            _bt_merge_nodes(bt, lnp, np);
# if defined(DEBUG)
          assert(_bt_check_node_consistency(bt, pnp));
          if (!check_parent_only)
            assert(_bt_check_node_consistency(bt, lnp));
# endif
            np = pnp;
            continue;
          }
        }
        else { /* case:  lnp->n_indexes > rnp->n_indexes) */
          if (   lnp->n_indexes > (bt_maxbranch >> 1)
              && lnp->n_indexes > np->n_indexes + 1) {
            /* move (lnp->n_indexes - np->n_indexes)/2 branches
               from lnp to np */
            _bt_move_branch_from_left_sibling(
                bt, np, lnp, (lnp->n_indexes - np->n_indexes) >> 1);
# if defined(DEBUG)
            assert(_bt_check_node_consistency(bt, lnp));
            assert(_bt_check_node_consistency(bt, np));
# endif
          }
          else if (np->n_indexes + rnp->n_indexes < bt_maxbranch - 1) {
            /* merge with right node */
# if defined(DEBUG)
            check_parent_only = (pnp == bt->root && pnp->n_indexes <= 1);
# endif
            _bt_merge_nodes(bt, np, rnp);
# if defined(DEBUG)
          assert(_bt_check_node_consistency(bt, pnp));
          if (! check_parent_only)
          assert(_bt_check_node_consistency(bt, np));
# endif
            np = pnp;
            continue;
          }
        }
        np = pnp;
        break;
      }
    }
  }
  while (np != bt->root) {
    np = np->parent->node;
    (np->n_leaves)--;
  }
  return;
}

bt_error_t bt_remove_data(bt_btree_t * bt, BT_KEY_T key, int remove_cell)
{
  bt_node_data_t *dp;
  bt_error_t err;

  if (NULL == bt) return bt_tree_is_null;
  dp = bt_search_candidate_cell(bt, key);
  if (dp->next && KEYEQ(bt, dp->next->key, key)) {
    if (dp->next->status == bt_cell_status_value_used) {
      DEL_VAL(bt, dp->next->e.val);
      err = bt_success;
    }
    else {
      assert(dp->next->status == bt_cell_status_unused);
      err = bt_key_cell_is_unused;
    }
    if (remove_cell) {
      _bt_remove_cell(bt, dp->next, (err == bt_success)?1:0);
    }
    else if(err == bt_success) {
      dp->next->status = bt_cell_status_unused;
      _bt_ancestor_node_decrement_n_leaves(bt, dp->node);
    }
  }
  else {
    err = bt_key_not_found;
  }
  return err;
}

bt_node_data_t const * bt_get_nth_from_smallest(bt_btree_t * bt, int n)
{
  bt_node_t * np;
  bt_node_data_t * dp;
  int l_leaves;
  if (n < 0 || n >= bt->root->n_leaves) {
    return NULL;
  }
  if (n < bt_maxbranch) {
     l_leaves = 0;
     dp = bt->top;
     while(l_leaves < n) {
       dp = dp->next;
       l_leaves++;
     }
  }
  else {
    np = bt->root;
    l_leaves = 0;
    while (np->node_type != bt_node_leaf) {
      dp = &(np->index_top);
      while (l_leaves + dp->e.r_child->n_leaves < n) {
        l_leaves += dp->e.r_child->n_leaves;
        assert(dp->next != NULL);
        dp = dp->next;
      }
      np = dp->e.r_child;
    }
    /* now on a leaf node */
    assert(n  < l_leaves + np->n_leaves);
    dp = np->index_top.next;
    while (l_leaves < n) {
      dp = dp->next;
      l_leaves++;
    }
  }
  return (bt_node_data_t const *)dp;
}

bt_error_t bt_clean_unused(bt_btree_t *bt)
{
  bt_node_data_t *dp;

  if (bt == NULL) return bt_tree_is_null;
  if (bt->top == NULL) return bt_success;
  dp = bt->top;
  while (dp->next != NULL) {
    assert(dp->next->prev == dp);
    dp = dp->next;
    if (dp->prev->status == bt_cell_status_unused) {
      _bt_remove_cell(bt, dp->prev, 0);
    }
  }
  if (dp->status == bt_cell_status_unused) {
    _bt_remove_cell(bt, dp, 0);
  }
  return bt_success;
}
