#include "jni.h"
#include "jvmti.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hashmap.h"

#define DETAILED_RESULTS

/*
 * Agent for Java 1.6+ virtual machine. Counts #bytecode instructions executed.
 * Some code stems from the example agent of the jvmi development package.
 * @author Juergen Graf <juergen.graf@gmail.com>
 */

#define MAX_METHODS 20000

#define NEW(type) (type *)hAlloc(sizeof(type))

void *
hAlloc(int size)
{
    void *addr = calloc(1, size);
    if (addr) {
        return addr;
    } else {
        fprintf(stderr, "Could not allocate %d bytes.\n", size);
        exit(EXIT_FAILURE);
    }
}


/* Global agent data structure */
typedef struct {
    /* JVMTI Environment */
    jvmtiEnv      *jvmti;
    jboolean       vm_is_started;
    /* Data access Lock */
    jrawMonitorID  lock;
} GlobalAgentData;

typedef struct {
    long       counter;
    char      *name;
    char      *sig;
    char      *class_sig;
    jmethodID  id;
} _counter_method_data;
typedef _counter_method_data * counter_method_data;

static GlobalAgentData    *gdata;
static jvmtiEnv           *jvmti = NULL;
static jvmtiCapabilities   capa;
static unsigned long long  num_instructions_proccessed;
static map_t               map;
static jmethodID           methods[MAX_METHODS];
static long                counter[MAX_METHODS];
static int                 cur_method_index;
#ifdef DETAILED_RESULTS
static jmethodID           last_method;
static long               *cur_counter;
static counter_method_data cur_method;
#endif

/* Every JVMTI interface returns an error code, which should be checked
 * to avoid any cascading errors down the line.
 * The interface GetErrorName() returns the actual enumeration constant
 * name, making the error messages much easier to understand.
 */
static inline void
check_jvmti_error(jvmtiEnv *jvmti, jvmtiError errnum, const char *str)
{
    char *errnum_str;
    if (errnum == JVMTI_ERROR_NONE)
        return;

    errnum_str = NULL;
    (void)(*jvmti)->GetErrorName(jvmti, errnum, &errnum_str);

    printf("ERROR: JVMTI: %d(%s): %s\n", errnum, (errnum_str == NULL ? "Unknown" : errnum_str),
        (str == NULL ? "" : str));
}

/* Enter a critical section by doing a JVMTI Raw Monitor Enter */
static inline void
enter_critical_section(jvmtiEnv *jvmti)
{
    jvmtiError error;

    error = (*jvmti)->RawMonitorEnter(jvmti, gdata->lock);
    check_jvmti_error(jvmti, error, "Cannot enter with raw monitor");
}

/* Exit a critical section by doing a JVMTI Raw Monitor Exit */
static inline void
exit_critical_section(jvmtiEnv *jvmti)
{
    jvmtiError error;

    error = (*jvmti)->RawMonitorExit(jvmti, gdata->lock);
    check_jvmti_error(jvmti, error, "Cannot exit with raw monitor");
}

void
describe(jvmtiError err)
{
      jvmtiError  error;
      char       *descr;

      error = (*jvmti)->GetErrorName(jvmti, err, &descr);
      if (error == JVMTI_ERROR_NONE) {
          printf("%s", descr);
      } else {
          printf("error [%d]", err);
      }
 }


static void
jvmti_dealloc(unsigned char * ptr)
{
    jvmtiError error;

    error = (*jvmti)->Deallocate(jvmti, (unsigned char *) ptr);
    if (error != JVMTI_ERROR_NONE) {
        printf("(jvmti_dealloc) Error expected: %d, got: %d\n", JVMTI_ERROR_NONE, error);
        describe(error);
        printf("\n");
    }
}

// Exception callback
// VM Death callback
static void JNICALL
callbackVMDeath(jvmtiEnv *jvmti_env, JNIEnv* jni_env)
{
    int    i;
    (void) jvmti_env;
    (void) jni_env;

#ifdef DETAILED_RESULTS
    enter_critical_section(jvmti);

    for (i = 0; i < cur_method_index; i++) {
        jmethodID   method_id = methods[i];
        jvmtiError  error;
        char       *name;
        char       *sig;
        char       *ptr;
        error = (*jvmti)->GetMethodName(jvmti, method_id, &name, &sig, &ptr);
        check_jvmti_error(jvmti, error, "Cannot get method name");

        jclass      decl_class;
        error = (*jvmti)->GetMethodDeclaringClass(jvmti, method_id, &decl_class);
        check_jvmti_error(jvmti, error, "Cannot get declaring class");

        char       *class_sig;
        char       *class_status;
        error = (*jvmti)->GetClassSignature(jvmti, decl_class, &class_sig, &class_status);
        check_jvmti_error(jvmti, error, "Cannot get class signature");

        printf("%ld\tclass %s -> %s(%s)\n", counter[i], class_sig, name, sig);

        jvmti_dealloc((unsigned char *) name);
        jvmti_dealloc((unsigned char *) sig);
        jvmti_dealloc((unsigned char *) ptr);
        jvmti_dealloc((unsigned char *) class_sig);
        jvmti_dealloc((unsigned char *) class_status);
    }

    exit_critical_section(jvmti);
#endif
}

// VM init callback
static void JNICALL
callbackVMInit(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread)
{
    (void) jvmti_env;
    (void) jni_env;
    (void) thread;
}

static void JNICALL
callbackSingleStep(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread, jmethodID method, jlocation location)
{
    (void) jvmti_env;
    (void) jni_env;
    (void) thread;
    (void) location;
    (void) method;

#ifdef DETAILED_RESULTS
    if (last_method != method) {
        last_method = method;

        if (hashmap_get(map, (unsigned long) method, (void *) &cur_counter) == MAP_MISSING) {
            if (cur_method_index >= MAX_METHODS) {
                printf("error: more then %i methods are not supported. apply quick fix: reset counter to 0.\n",
                    MAX_METHODS);
                cur_method_index = 0;
            }
            methods[cur_method_index] = method;
            cur_counter = &counter[cur_method_index++];
            hashmap_put(map, (unsigned long) method, cur_counter);
        }
    }
    (*cur_counter)++;
#endif

    num_instructions_proccessed++;
}

JNIEXPORT jint JNICALL
Agent_OnLoad(JavaVM *jvm, char *options, void *reserved)
{
    static GlobalAgentData data;
    jvmtiError             error;
    jint                   res;
    jvmtiEventCallbacks    callbacks;
    (void)                 options;
    (void)                 reserved;

    num_instructions_proccessed = 0;
    cur_method_index            = 0;
    map                         = hashmap_new();
    (void)memset(&methods, 0, sizeof(methods));
    (void)memset(&counter, 0, sizeof(counter));

    /* Setup initial global agent data area
     * Use of static/extern data should be handled carefully here.
     * We need to make sure that we are able to cleanup after ourselves
     * so anything allocated in this library needs to be freed in
     * the Agent_OnUnload() function.
     */
    (void)memset((void*)&data, 0, sizeof(data));
    gdata = &data;

    /* We need to first get the jvmtiEnv* or JVMTI environment */
    res = (*jvm)->GetEnv(jvm, (void **) &jvmti, JVMTI_VERSION_1_0);
    if (res != JNI_OK || jvmti == NULL) {
        /* This means that the VM was unable to obtain this version of the
         * JVMTI interface, this is a fatal error.
         */
        printf("ERROR: Unable to access JVMTI Version 1 (0x%x), is your J2SE a 1.5 or newer version? JNIEnv's "
            "GetEnv() returned %d\n", JVMTI_VERSION_1, res);
    }

    /* Here we save the jvmtiEnv* for Agent_OnUnload(). */
    gdata->jvmti = jvmti;

    (void)memset(&capa, 0, sizeof(jvmtiCapabilities));
    //capa.can_signal_thread                   = 1;
    //capa.can_get_owned_monitor_info          = 1;
    //capa.can_generate_method_entry_events    = 1;
    //capa.can_generate_exception_events       = 1;
    //capa.can_generate_vm_object_alloc_events = 1;
    //capa.can_tag_objects                     = 1;
    capa.can_generate_single_step_events      = 1;

    error = (*jvmti)->AddCapabilities(jvmti, &capa);
    check_jvmti_error(jvmti, error, "Unable to get necessary JVMTI capabilities.");

    (void)memset(&callbacks, 0, sizeof(callbacks));
    callbacks.VMInit     = &callbackVMInit;     /* JVMTI_EVENT_VM_INIT */
    callbacks.VMDeath    = &callbackVMDeath;    /* JVMTI_EVENT_VM_DEATH */
    callbacks.SingleStep = &callbackSingleStep; /* JVMTI_EVENT_SINGLE_STEP */

    error = (*jvmti)->SetEventCallbacks(jvmti, &callbacks, (jint)sizeof(callbacks));
    check_jvmti_error(jvmti, error, "Cannot set jvmti callbacks");

    /* At first the only initial events we are interested in are VM
     * initialization, VM death, and Class File Loads.
     * Once the VM is initialized we will request more events.
     */
    error = (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE, JVMTI_EVENT_VM_INIT, (jthread)NULL);
    check_jvmti_error(jvmti, error, "Cannot set event notification: JVMTI_EVENT_VM_INIT");
    error = (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE, JVMTI_EVENT_VM_DEATH, (jthread)NULL);
    check_jvmti_error(jvmti, error, "Cannot set event notification: JVMTI_EVENT_VM_DEATH");
    error = (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE, JVMTI_EVENT_SINGLE_STEP, (jthread)NULL);
    check_jvmti_error(jvmti, error, "Cannot set event notification: JVMTI_EVENT_SINGLE_STEP");

    /* Here we create a raw monitor for our use in this agent to
     * protect critical sections of code.
     */
    error = (*jvmti)->CreateRawMonitor(jvmti, "agent data", &(gdata->lock));
    check_jvmti_error(jvmti, error, "Cannot create raw monitor");

    /* We return JNI_OK to signify success */
    return JNI_OK;
}

/* Agent_OnUnload: This is called immediately before the shared library is
 * unloaded. This is the last code executed.
 */
JNIEXPORT void JNICALL
Agent_OnUnload(JavaVM *vm)
{
    (void) vm;

    /* Make sure all malloc/calloc/strdup space is freed */
    hashmap_free(map);
#ifdef DETAILED_RESULTS
    printf("%llu bytecode instructions in %d methods executed.\n", num_instructions_proccessed, cur_method_index - 1);
#else
    fprintf(stderr, "%llu\n", num_instructions_proccessed);
#endif
}

