# $yfId: readindex/branches/on_the_way/btree/btree.pxd 56 2016-10-18 15:09:55Z futatuki $
from libc.stddef cimport size_t
from libc.stdint cimport uint32_t

cdef extern from "bt_conf.h":
    ctypedef void * BT_KEY_T
    ctypedef void * BT_VAL_T

cdef extern from "btree.h":
    ctypedef enum bt_node_type_t:
        bt_node_only_root
        bt_node_root
        bt_node_internal
        bt_node_leaf
        bt_node_unknown
    ctypedef enum bt_error_t:
        bt_success = 0
        bt_tree_is_null
        bt_duplicated_key
        bt_not_enough_memory
        bt_key_not_found
    #ctypedef struct bt_node_data_t:
    #    pass
    ctypedef union e_t:
        bt_node_t * r_child
        BT_VAL_T val
    ctypedef struct bt_node_data_t:
        BT_KEY_T key
        bt_node_t * node
        bt_node_data_t * prev
        bt_node_data_t * next
        e_t e
    #ctypedef struct bt_node_t:
    #    pass 
    ctypedef struct bt_node_t:
        bt_node_type_t node_type
        int n_indexes
        bt_node_data_t index_top
        bt_node_data_t * parent 
    ctypedef int (*key_comp_func_t)(BT_KEY_T t1, BT_KEY_T t2)
    ctypedef void (*key_func_t)(BT_KEY_T)
    ctypedef void (*val_func_t)(BT_VAL_T)

    ctypedef struct bt_btree_t:
        bt_node_t * root
        bt_node_data_t *top
        key_comp_func_t key_comp_func
        key_func_t key_use_func
        key_func_t key_del_func
        val_func_t val_use_func
        val_func_t val_del_func

    cdef bt_btree_t * bt_new_btree(
            key_comp_func_t key_comp_func,
            key_func_t key_use_func, key_func_t key_del_func,
            val_func_t val_use_func, val_func_t val_del_func) nogil
    cdef void bt_del_btree(bt_btree_t *bt)
    cdef bt_error_t bt_insert(bt_btree_t * bt, BT_KEY_T key, BT_VAL_T val)
    cdef bt_error_t bt_search_data(
                         bt_btree_t * bt, BT_KEY_T key, BT_VAL_T * valp)
    cdef bt_error_t bt_update_data(
                         bt_btree_t * bt, BT_KEY_T key, BT_VAL_T val)
