
/*
 * Interface definition for the list based stack class. The stack only
 * holds pointers/references to application objects. The objects are not
 * copied and the stack never attempts to dereference or access the data
 * objects. Applications should treat cstack_t references as opaque
 * handles.
 */

#ifndef _CSTACK_H_
#define	_CSTACK_H_


#ifdef __cplusplus
extern "C" {
#endif


typedef struct cstack {
	struct cstack *next;
	void *data;
	int len;
} cstack_t;


cstack_t *cstack_new(void);
void cstack_delete(cstack_t *);
int cstack_push(cstack_t *, void *, int);
int cstack_pop(cstack_t *, void **, int *);
int cstack_top(cstack_t *, void **, int *);
int is_cstack_empty(cstack_t *stk);


#ifdef __cplusplus
}
#endif


#endif /* _CSTACK_H_ */
