
#ifndef __PROPOSED_API_H__
#define __PROPOSED_API_H__
#include <string.h>
/**
 *
 * This are wrappers around the internal buflib_* which have the core context
 * hardcoded for convinience
 */

/**
 * Initializes buflib with a predefined context for use in the core
 */
void buflib_core_init(void);

/**
 * Allocates memory from the core's memory pool
 *
 * name: A string identifier giving this allocation a name
 * size: How many bytes to allocate
 *
 * Returns: An integer handle identifying this allocation
 */
int core_alloc(const char* name, size_t size);

/**
 * Allocate memory from the core's memory pool with additional callbacks
 * and flags
 * 
 * name: A string identifier giving this allocation a name
 * size: How many bytes to allocate
 * flags: Flags giving information how this allocation needs to be handled (see below)
 * ops: a struct with pointers to callback functions (see below)
 *
 * Returns: An integer handle identifying this allocation
 */
struct buflib_callbacks;
int core_alloc_ex(const char* name, size_t size, struct buflib_callbacks *ops);


/**
 * Query the data pointer for the given handle. It's actually a cheap operation,
 * so don't hesitate using it extensivly.
 *
 * Notice that you need to re-query after every direct or indirect yield(),
 * because compaction can happen by other threads which may get your data
 * moved around (or you can get notified about changes by callbacks,
 * see further below).
 *
 * handle: The handle corresponding to the allocation
 *
 * Returns: The start pointer of the allocation
 */
void* core_get_data(int handle);

/**
 * Free memory associated with the given handle
 */
void core_free(int handle);

/**
 * Callbacks used by the buflib to inform allocation that compaction
 * is happening (before data is moved)
 *
 * Note that buflib tries to move to satisfy new allocations before shrinking.
 * So if you have something to resize try to do it outside of the callback.
 *
 * Regardless of the above, if the allocation is SHRINKABLE, but not
 * MUST_NOT_MOVE buflib will move the allocation before even attempting to
 * shrink.
 */
struct buflib_callbacks {
    /**
     * This is called before data is moved. Use this to fix up any pointers
     * pointing to within the allocation. The size is unchanged
     *
     * handle: The corresponding handle
     * current: The current start of the allocation
     * new: The new start of the allocation, after data movement
     *
     * Return: Return BUFLIB_CB_OK, or BUFLIB_CB_DEFER if
     * movement is impossible in this moment
     *
     * If NULL: this allocation must not be moved around by the buflib when
     * compation occurs
     */
    int (*move_callback)(int handle, void* current, void* new);
    /**
     * This is called when the buflib desires to shrink a SHRINKABLE buffer
     * in order to satisfy new allocation and if moving other allocations
     * failed.
     * Call core_shrink() from within the callback to do the shrink.
     *
     * handle: The corresponding handle
     * start: The old start of the allocation
     *
     * Return: Return BUFLIB_CB_OK, or BUFLIB_CB_CANNOT_SHRINK if shirinking
     * is impossible at this moment.
     *
     * if NULL: this allocation cannot be resized.
     * It is recommended that allocation that must not move are
     * at least shrinkable
     */
    int (*shrink_callback)(int handle, void* start, size_t old_size);
};

/**
 * Possible return values for the callbacks, some of them can cause
 * compaction to fail and therefore new allocations to fail
 */
/* Everything alright */
#define BUFLIB_CB_OK
/* Tell buflib that resizing failed, possibly future making allocations fail */
#define BUFLIB_CB_CANNOT_SHRINK


/**
 * Gets all available memory from buflib, for temporary use.
 * It aquires a lock so allocations from other threads will wait until the
 * lock is released (by core_shrink()).
 *
 * Buflib may call the shrin_callback() after some time if it's in need of
 * memory and core_shrink() has not been called yet.
 *
 * name: A string identifier giving this allocation a name
 * size: The actual size will be returned into size
 * ops: a struct with pointers to callback functions
 *
 * Returns: An integer handle identifying this allocation
 */

int core_alloc_maximum(const char* name, size_t *size, struct buflib_callbacks *ops);

/**
 * Shrink the memory allocation associated with the given handle
 * Mainly intended to be used with the shrink callback (call this in the
 * callback and get return BUFLIB_CB_OK, but it can also be called outside
 *
 * If a lock was aquired for this handle, the lock will be unlocked, assuming
 * the allocation has freed memory for future allocation by other threads.
 *
 * handle: The handle identifying this allocation
 * new_start: the new start of the allocation
 * new_size: the new size of the allocation
 */
void core_shrink(int handle, void* new_start, size_t new_size);

/**
 * Returns how many bytes left the buflib has to satisfy allocations (not
 * accounting possible compaction)
 *
 * There might be more after a future compaction which is not handled by
 * this function.
 */
size_t core_available(void);

/**
 * Prints an overview of all current allocations to stdout (not for Rockbox)
 */

void core_print_allocs(void);

/**
 * Returns the name, as given to core_alloc() and core_allloc_ex(), of the
 * allocation associated with the given handle
 *
 * handle: The handle indicating the allocation
 *
 * Returns: A pointer to the string identifier of the allocation
 */
const char* core_get_alloc_name(int handle);
#endif /* __PROPOSED_API_H__ */
