/*
 * Generic map implementation.
 */
#include "hashmap.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define INITIAL_SIZE (256)
#define MAX_CHAIN_LENGTH (8)

#define M_NEW(type) (type *) malloc(sizeof(type))
#define C_NEW(count, type) (type *) calloc(count, sizeof(type))

/* We need to keep keys and values */
typedef struct _hashmap_element {
    map_key_t key;
    int          in_use;
    any_t        data;
} hashmap_element;

/* A hashmap has some maximum size and current size,
 * as well as the data to hold. */
typedef struct _hashmap_map{
    int              table_size;
    int              size;
    hashmap_element *data;
} hashmap_map;

/*
 * Return an empty hashmap, or NULL on failure.
 */
map_t
hashmap_new(void)
{
    hashmap_map* m = M_NEW(hashmap_map);
    if (m) {
        m->data = C_NEW(INITIAL_SIZE, hashmap_element);
        if (m->data) {
            m->table_size = INITIAL_SIZE;
            m->size = 0;
        } else {
            hashmap_free(m);
            return NULL;
        }
    } else {
        return NULL;
    }

    return m;
}

/*
 * Hashing function for a string
 */
static inline unsigned int 
hashmap_hash_int(hashmap_map * m, map_key_t key)
{
    return key % m->table_size;
}

/*
 * Return the integer of the location in data
 * to store the point to the item, or MAP_FULL.
 */
static int
hashmap_hash(map_t in, map_key_t key)
{
    int curr;
    int i;

    /* Cast the hashmap */
    hashmap_map* m = (hashmap_map *) in;

    /* If full, return immediately */
    if (m->size >= (m->table_size/2))
        return MAP_FULL;

    /* Find the best index */
    curr = hashmap_hash_int(m, key);

    /* Linear probing */
    for (i = 0; i < MAX_CHAIN_LENGTH; i++){
        if (m->data[curr].in_use == 0)
            return curr;

        if (m->data[curr].in_use == 1 && (m->data[curr].key == key))
            return curr;

        curr = (curr + 1) % m->table_size;
    }

    return MAP_FULL;
}

/*
 * Doubles the size of the hashmap, and rehashes all the elements
 */
static int
hashmap_rehash(map_t in)
{
    int              i;
    int              old_size;
    hashmap_element *curr;

    /* Setup the new elements */
    hashmap_map *m = (hashmap_map *) in;
    hashmap_element* temp = C_NEW(2 * m->table_size, hashmap_element);
    if (!temp)
        return MAP_OMEM;

    /* Update the array */
    curr = m->data;
    m->data = temp;

    /* Update the size */
    old_size = m->table_size;
    m->table_size = 2 * m->table_size;
    m->size = 0;

    /* Rehash the elements */
    for (i = 0; i < old_size; i++) {
        int status;

        if (curr[i].in_use == 0)
            continue;
            
        status = hashmap_put(m, curr[i].key, curr[i].data);
        if (status != MAP_OK)
            return status;
    }

    free(curr);

    return MAP_OK;
}

/*
 * Add a pointer to the hashmap with some key
 */
int
hashmap_put(map_t in, map_key_t key, any_t value)
{
    int index;
    hashmap_map* m;

    /* Cast the hashmap */
    m = (hashmap_map *) in;

    /* Find a place to put our value */
    index = hashmap_hash(in, key);
    while (index == MAP_FULL) {
        if (hashmap_rehash(in) == MAP_OMEM) {
            return MAP_OMEM;
        }
        index = hashmap_hash(in, key);
    }

    /* Set the data */
    m->data[index].data = value;
    m->data[index].key = key;
    m->data[index].in_use = 1;
    m->size++; 

    return MAP_OK;
}

/*
 * Get your pointer out of the hashmap with a key
 */
int
hashmap_get(map_t in, map_key_t key, any_t *arg)
{
    int curr;
    int i;
    hashmap_map* m;

    /* Cast the hashmap */
    m = (hashmap_map *) in;

    /* Find data location */
    curr = hashmap_hash_int(m, key);

    /* Linear probing, if necessary */
    for (i = 0; i < MAX_CHAIN_LENGTH; i++) {
        int in_use = m->data[curr].in_use;
        if (in_use == 1) {
            if (m->data[curr].key == key) {
                *arg = (m->data[curr].data);
                return MAP_OK;
            }
        }

        curr = (curr + 1) % m->table_size;
    }

    *arg = NULL;

    /* Not found */
    return MAP_MISSING;
}

/*
 * Iterate the function parameter over each element in the hashmap.  The
 * additional any_t argument is passed to the function as its first
 * argument and the hashmap element is the second.
 */
int
hashmap_iterate(map_t in, PFany f, any_t item)
{
    int i;

    /* Cast the hashmap */
    hashmap_map* m = (hashmap_map*) in;

    /* On empty hashmap, return immediately */
    if (hashmap_length(m) <= 0)
        return MAP_MISSING;    

    /* Linear probing */
    for (i = 0; i < m->table_size; i++) {
        if (m->data[i].in_use != 0) {
            any_t data = (any_t) (m->data[i].data);
            int status = f(item, data);
            if (status != MAP_OK) {
                return status;
            }
        }
    }

    return MAP_OK;
}

/*
 * Remove an element with that key from the map
 */
int
hashmap_remove(map_t in, map_key_t key)
{
    int          i;
    int          curr;
    hashmap_map *m;

    /* Cast the hashmap */
    m = (hashmap_map *) in;

    /* Find key */
    curr = hashmap_hash_int(m, key);

    /* Linear probing, if necessary */
    for (i = 0; i < MAX_CHAIN_LENGTH; i++) {
        int in_use = m->data[curr].in_use;
        if (in_use == 1) {
            if (m->data[curr].key == key) {
                /* Blank out the fields */
                m->data[curr].in_use = 0;
                m->data[curr].data = NULL;
                m->data[curr].key = 0;

                /* Reduce the size */
                m->size--;
                return MAP_OK;
            }
        }
        curr = (curr + 1) % m->table_size;
    }

    /* Data not found */
    return MAP_MISSING;
}

/* Deallocate the hashmap */
void
hashmap_free(map_t in)
{
    hashmap_map* m = (hashmap_map*) in;
    free(m->data);
    free(m);
}

/* Return the length of the hashmap */
int
hashmap_length(map_t in)
{
    hashmap_map* m = (hashmap_map *) in;
    if (m != NULL)
        return m->size;
    else
        return 0;
}
