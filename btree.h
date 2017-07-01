/* $yfId: btree/trunk/btree.h 80 2017-07-01 17:08:08Z futatuki $ */

# include <sys/stdint.h>
# include "bt_conf.h"

# if !defined(bt_maxbranch)
#   if defined(DEBUG)
#     define bt_maxbranch 5
#   else
#     define bt_maxbranch 7
#   endif
# endif

# if !defined(BT_KEY_T)
#   define BT_KEY_T int
# endif

# if !defined(BT_VAL_T)
#   define BT_VAL_T void *
# endif

# if defined(DEBUG)
typedef struct PLIST_T {
  void *p;
  struct PLIST_T * next;
  char mem[0];
} plist_t;

void * debug_malloc(size_t s);
void debug_free(void * p);
plist_t * debug_get_allocated_pointers(void);
void debug_print_leak_pointers(void);
# endif

typedef enum {
  bt_node_only_root,
  bt_node_root,
  bt_node_internal,
  bt_node_leaf,
  bt_node_unknown = -1
} bt_node_type_t;

typedef enum {
  bt_success  = 0,
  bt_tree_is_null,
  bt_duplicated_key,
  bt_not_enough_memory,
  bt_key_not_found,
  bt_key_cell_is_unused
} bt_error_t;

typedef enum {
  bt_cell_status_unused = 0,
  bt_cell_status_value_used = 1,
  bt_cell_status_part_of_node = 2,
  bt_cell_status_branch_used = 3
} bt_cell_status_t;

struct BT_NODE;

typedef struct BT_NODE_DATA {
  BT_KEY_T key;
  struct BT_NODE * node;
  struct BT_NODE_DATA * prev;
  struct BT_NODE_DATA * next;
  bt_cell_status_t status;
  union {
    struct BT_NODE * r_child;
    BT_VAL_T val;
  } e;
} bt_node_data_t;

typedef struct BT_NODE {
  bt_node_type_t node_type;
  int n_indexes;
  int n_leaves;
  struct BT_NODE_DATA index_top;
  struct BT_NODE_DATA * parent;
} bt_node_t;

# if defined(GENERIC_LIB) && GENERIC_LIB
typedef int  (*key_comp_func_t)(BT_KEY_T t1, BT_KEY_T t2);
typedef void (*key_func_t)(BT_KEY_T k);
typedef void (*val_func_t)(BT_VAL_T v);
# endif

typedef struct BT_TREE {
  bt_node_t * root;
  bt_node_data_t *top;
# if defined(GENERIC_LIB) && GENERIC_LIB
  key_comp_func_t key_comp_func;
  key_func_t key_use_func;
  key_func_t key_del_func;
  val_func_t val_use_func;
  val_func_t val_del_func;
# endif
} bt_btree_t;

bt_node_data_t * bt_new_node_data(
    BT_KEY_T key, BT_VAL_T val, bt_cell_status_t stat);
void bt_del_node_data(bt_node_data_t *nd);
bt_node_t * bt_new_node(bt_node_type_t node_type, bt_node_data_t * parent);

# if defined(GENERIC_LIB) && GENERIC_LIB
# define BT_NEW_BTREE_ARGS \
                key_comp_func_t key_comp_func,\
                key_func_t key_use_func, key_func_t key_del_func,\
                val_func_t val_use_func, val_func_t val_del_func
# else
# define BT_NEW_BTREE_ARGS void
# endif
bt_btree_t * bt_new_btree(BT_NEW_BTREE_ARGS);
void bt_del_btree(bt_btree_t *bt);
bt_error_t bt_insert(
    bt_btree_t * bt, BT_KEY_T key, BT_VAL_T val, int overwrite);
bt_error_t bt_search_data(bt_btree_t * bt, BT_KEY_T key, BT_VAL_T *valp);
bt_node_data_t * bt_search_candidate_cell(bt_btree_t * bt, BT_KEY_T key);
bt_error_t bt_update_data(bt_btree_t * bt, BT_KEY_T key, BT_VAL_T val);
bt_error_t bt_remove_data(bt_btree_t * bt, BT_KEY_T key, int remove_cell);
bt_node_data_t const * bt_get_nth_from_smallest(bt_btree_t * bt, int n);
bt_error_t bt_clean_unused(bt_btree_t *bt);
