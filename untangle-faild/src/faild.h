/*
 * Copyright (c) 2003-2009 Untangle, Inc.
 * All rights reserved.
 *
 * This software is the confidential and proprietary information of
 * Untangle, Inc. ("Confidential Information"). You shall
 * not disclose such Confidential Information.
 *
 * $Id$
 */

#ifndef __FAILD_H_
#define __FAILD_H_


#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <net/if.h>
#include <json/json.h>

#define FAILD_MAX_INTERFACES 8
#define FAILD_MAX_INTERFACE_TESTS 8

#define FAILD_TEST_CLASS_NAME_SIZE  32
#define FAILD_TEST_LIB_NAME_SIZE  32


typedef struct
{
    /* This is the os ifindex index of the interface (/sys/class/net/eth0/ifindex). */
    int ifindex;
    
    /* This is the index from the alpaca. */
    int alpaca_interface_id;
    
    char os_name[IF_NAMESIZE];
} faild_interface_t;

typedef struct
{
    int alpaca_interface_id;
    
    char test_class_name[FAILD_TEST_CLASS_NAME_SIZE];

    /* Timeout to run a test in milliseconds. */
    int timeout_ms;

    /* Delay between consecutive executions of the test, this should
     * be at least twice as long as the timeout. */
    int delay_ms;
    
    /* Number of previous test results to average */
    int bucket_size;

    /* Number of previous test results that must pass in order for the
     * interface to be considered online. */
    int threshold;

    /* The configuration parameters for this test.  Stored as a JSON
     * object for maximum flexibility.  This is a copy and must be freed
     * when destroying this object. */
    struct json_object* params;
} faild_test_config_t;

typedef struct
{
    int is_enabled;

    int interfaces_length;

    /* Array of all of the interfaces. */
    faild_interface_t interfaces[FAILD_MAX_INTERFACES];

    /* Array, maps ( alpaca index - 1) -> an interface. */
    faild_interface_t interface_map[FAILD_MAX_INTERFACES];

    /* Minimum amount of time to stay on an interface before switching */
    int interface_min_ms;

    /* Path to the script that should be used to change the interface. */
    char* switch_interface_script;

    /* The number of tests */
    int tests_length;

    /* Array of all of the current test configurations. */
    faild_test_config_t tests[FAILD_MAX_INTERFACES * FAILD_MAX_INTERFACE_TESTS];
} faild_config_t;

typedef struct
{
    /* The number of succesful tests in last size results */
    int success;
    
    /* The time of the last update. */
    struct timeval last_update;

    /* The number of results in results.  */
    int size;
    
    /* Array of size .size.  Must be freed and allocated at creation. */
    u_char* results;

    /* Position inside of results of the current test, results is a circular
     * buffer. */
    int position;
} faild_uplink_results_t;

typedef struct
{
    int alpaca_interface_id;
    
    int online;

    faild_uplink_results_t results[FAILD_MAX_INTERFACE_TESTS];
} faild_uplink_status_t;

typedef struct
{
    /* Flag that indicates whether this test is alive */
    int is_alive;
    
    /* The interface this test is running on. */
    faild_interface_t interface;
    
    /* The test results for the previous test runs. */
    faild_uplink_results_t results;
    
    /* The configuration for this test. */
    faild_test_config_t config;
} faild_uplink_test_instance_t;

typedef struct
{
    char name[FAILD_TEST_CLASS_NAME_SIZE];

    /* Initialize this instance of the test.  Allocate any variables, etc. */
    int (*init)( faild_uplink_test_instance_t *instance );
    
    /* Run one iteration of the test.  Timeouts are automatically
     * handled outside of the test. */
    int (*run)( faild_uplink_test_instance_t *instance, struct in_addr* primary_address, 
                struct in_addr* default_gateway );

    /* Cleanup a single iteration of a test.  Run executes in a thread
     * and can be killed prematurely.  This gives the class a chance
     * to free any resources. */
    int (*cleanup)( faild_uplink_test_instance_t *instance );

    /* Cleanup resources related to an instance of a test.  This is
     * executed when a test instance stops */
    int (*destroy)( faild_uplink_test_instance_t *instance );

    /* An array defining the parameters associated with this test
     * class. */
    struct json_array* params;
} faild_uplink_test_class_t;

typedef struct
{
    /* The name of the test library. */
    char name[FAILD_TEST_LIB_NAME_SIZE];

    /* A function to initialize the test library.  This is always
     * called once when the library is loaded. */ 
    int (*init)( void );

    /* A function to destroy the library, and free any used resources.
     * This is called at shutdown. */
    int (*destroy)( void );

    /* A function to retrieve the available test classes.  It is the
     * callers responsibility to free the memory returned be this
     * function */
    int (*get_test_classes)( faild_uplink_test_class_t **test_classes );
} faild_uplink_test_lib_t;

/* This is the typedef of the function that gets the definition from the shared lib */
typedef int (*faild_uplink_test_prototype_t)( faild_uplink_test_lib_t* lib );

/* Initialize a configuration object */
faild_config_t* faild_config_malloc( void );
int faild_config_init( faild_config_t* config );
faild_config_t* faild_config_create( void );

/* Load a configuration */
int faild_config_load_json( faild_config_t* config, struct json_object* json_config );

/* Serialize back to JSON */
struct json_object* faild_config_to_json( faild_config_t* config );

/* test lib handling functions */
int faild_libs_init( void );
int faild_libs_load_test_classes( char* lib_dir_name );

/* test class utility functions */
faild_uplink_test_class_t* faild_uplink_test_class_malloc( void );
int faild_uplink_test_class_init( faild_uplink_test_class_t* test_class, char* name,
                                  int (*init)( faild_uplink_test_instance_t *instance ),
                                  int (*run)( faild_uplink_test_instance_t *instance,
                                              struct in_addr* primary_address, 
                                              struct in_addr* default_gateway ),
                                  int (*cleanup)( faild_uplink_test_instance_t *instance ),
                                  int (*destroy)( faild_uplink_test_instance_t *instance ),
                                  struct json_array* params );


faild_uplink_test_class_t* 
faild_uplink_test_class_create( char* name,
                                int (*init)( faild_uplink_test_instance_t *instance ),
                                int (*run)( faild_uplink_test_instance_t *instance,
                                            struct in_addr* primary_address, 
                                            struct in_addr* default_gateway ),
                                int (*cleanup)( faild_uplink_test_instance_t *instance ),
                                int (*destroy)( faild_uplink_test_instance_t *instance ),
                                struct json_array* params );

#endif // __FAILD_H_
