# $yfId: readindex/branches/on_the_way/btree/btree_debug.pxd 40 2016-10-15 06:46:26Z futatuki $
cimport btree

cdef extern from "btree.h" nogil:
    cdef struct plist_t:
        void * p
        plist_t * next

    cdef int debug_is_allocated_pointer(void *p) nogil
    cdef plist_t * debug_get_allocated_pointers() nogil
