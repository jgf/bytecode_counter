/*
 * Generic hashmap manipulation functions
 *
 * Originally by Elliot C Back - http://elliottback.com/wp/hashmap-implementation-in-c/
 *
 * Modified by Pete Warden to fix a serious performance problem, support strings as keys
 * and removed thread synchronization - http://petewarden.typepad.com
 *
 * Modified by Juergen Graf to support unsigend long values as keys.
 */
#ifndef __HASHMAP_H__
#define __HASHMAP_H__

#define MAP_MISSING -3  /* No such element */
#define MAP_FULL -2     /* Hashmap is full */
#define MAP_OMEM -1     /* Out of Memory */
#define MAP_OK 0        /* OK */

typedef unsigned long map_key_t;

/*
 * map_visit_func is a pointer to a function that can take two (void *) arguments
 * and return an integer. Returns status code..
 */
typedef int (*map_visit_func_p)(void *, void *);

/*
 * map_s is an internally maintained data structure.
 * Clients of this package do not need to know how hashmaps are
 * represented. They see and manipulate only map_t's.
 */
typedef struct map_s map_t;

/*
 * Return an empty hashmap. Returns NULL if empty.
 */
extern map_t * hashmap_new(void);

/*
 * Iteratively call f with argument (value, f_arg) for
 * each element 'value' in the hashmap. The function must
 * return a map status code. If it returns anything other
 * than MAP_OK the traversal is terminated. f must
 * not reenter any hashmap functions, or deadlock may arise.
 */
extern int hashmap_iterate(const map_t *in, map_visit_func_p f, void *f_arg);

/*
 * Add an element to the hashmap. Return MAP_OK or MAP_OMEM.
 */
extern int hashmap_put(map_t *in, const map_key_t key, void *value);

/*
 * Get an element from the hashmap. Return MAP_OK or MAP_MISSING.
 */
extern int hashmap_get(const map_t *in, const map_key_t key, void **value);

/*
 * Remove an element from the hashmap. Return MAP_OK or MAP_MISSING.
 */
extern int hashmap_remove(map_t *in, const map_key_t key);

/*
 * Free the hashmap
 */
extern void hashmap_free(map_t *in);

/*
 * Get the current size of a hashmap
 */
extern int hashmap_length(const map_t *in);

#endif /* __HASHMAP_H__ */
