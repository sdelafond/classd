/*
 * Copyright (c) 2003-2008 Untangle, Inc.
 * All rights reserved.
 *
 * This software is the confidential and proprietary information of
 * Untangle, Inc. ("Confidential Information"). You shall
 * not disclose such Confidential Information.
 *
 * $Id: ADConnectorImpl.java 15443 2008-03-24 22:53:16Z amread $
 */

#include <pthread.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <net/if.h>

#include <mvutil/debug.h>
#include <mvutil/errlog.h>
#include <mvutil/unet.h>

#include "faild.h"
#include "faild/manager.h"

#define _MAX_SHUTDOWN_TIMEOUT     10

/* This is the file that should contain the routing table */
#define _ROUTE_FILE              "/proc/net/route"
/* For simplicity the route table is divided into 128 byte chunks */
#define _ROUTE_READ_SIZE         0x80

static struct
{
    pthread_mutex_t mutex;
    faild_config_t config;

    int init;

    int active_argon_interface_id;
    faild_uplink_status_t status[FAILD_MAX_INTERFACES];
    faild_uplink_test_instance_t* active_tests[FAILD_MAX_INTERFACES][FAILD_MAX_INTERFACE_TESTS];

    /* This is the total number of running tests.  A test is running
     * as long as the thread exists.  active_tests is set to NULL as
     * soon as is_alive is set to NULL. */
    int total_running_tests;
} _globals = {
    .init = 0,
    .mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP,
    .active_argon_interface_id = 0,
    .total_running_tests = 0
};

static int _validate_test_config( faild_test_config_t* test_config );

/* Return the index of this test in the correct interface */
static int _find_test_config( faild_test_config_t* test_config );

/* Return the index of the next open test slot. */
static int _find_open_test( int alpaca_interface_id );

int faild_manager_init( faild_config_t* config )
{
    if ( config == NULL ) return errlogargs();

    bzero( &_globals.status, sizeof( _globals.status ));
    bzero( &_globals.active_tests, sizeof( _globals.active_tests ));

    memcpy( &_globals.config, config, sizeof( _globals.config ));

    _globals.init = 1;

    return 0;
}

/**
 * Copies in the config to the global config
 */
int faild_manager_set_config( faild_config_t* config )
{
    if ( config == NULL ) return errlogargs();
    
    int _critical_section() {
        debug( 9, "Loading new config\n" );

        u_char is_active[FAILD_MAX_INTERFACES][FAILD_MAX_INTERFACE_TESTS];
        faild_test_config_t* new_tests[FAILD_MAX_INTERFACES * FAILD_MAX_INTERFACE_TESTS];
        
        int num_new_tests = 0;
        bzero( is_active, sizeof( is_active ));
        bzero( new_tests, sizeof( new_tests ));

        int c = 0;
        int d = 0;
        int test_index = 0;

        faild_test_config_t* test_config = NULL;

        /* Find all of the tests that are currently running */
        for ( c = 0 ; c < config->tests_length ; c++ ) {
            test_config = &config->tests[c];

            if ( _validate_test_config( test_config ) < 0 ) {
                errlog( ERR_WARNING, "Invalid test configuration\n" );
                continue;
            }

            test_index = _find_test_config( test_config );
            if ( test_index < 0 ) {
                new_tests[num_new_tests++] = test_config;
                continue;
            }

            is_active[test_config->alpaca_interface_id-1][test_index] = 1;
        }

        /* Stop all of the tests that are no longer needed */
        for ( c = 0 ; c < FAILD_MAX_INTERFACES; c++ ) {
            for ( d = 0 ; d < FAILD_MAX_INTERFACE_TESTS ; d++ ) {
                if ( is_active[c][d] == 1 ) continue;
                faild_uplink_test_instance_t* test_instance = _globals.active_tests[c][d];
                _globals.active_tests[c][d] = NULL;
                test_instance->is_alive = 0;
            }
        }
        
        for ( c = 0 ; c < num_new_tests ; c++ ) {
            test_config = new_tests[c];
            if ( test_config == NULL ) {
                errlog( ERR_CRITICAL, "Invalid test config\n" );
                continue;
            }

            if (( d = _find_open_test( test_config->alpaca_interface_id )) < 0 ) {
                errlog( ERR_CRITICAL, "_find_open_test\n" );
            }

            
        }
        
        memcpy( &_globals.config, config, sizeof( _globals.config ));
        
        return 0;
    }

    if ( pthread_mutex_lock( &_globals.mutex ) != 0 ) return perrlog( "pthread_mutex_lock" );
    int ret = _critical_section();
    if ( pthread_mutex_unlock( &_globals.mutex ) != 0 ) return perrlog( "pthread_mutex_unlock" );
    
    if ( ret < 0 ) return errlog( ERR_CRITICAL, "_critical_section\n" );
    
    return 0;
}

/**
 * Gets the config
 */
int faild_manager_get_config( faild_config_t* config )
{
    if ( config == NULL ) return errlogargs();
    
    int _critical_section() {
        debug( 9, "Copying out config\n" );
        memcpy( config, &_globals.config, sizeof( _globals.config ));
        return 0;
    }

    if ( pthread_mutex_lock( &_globals.mutex ) != 0 ) return perrlog( "pthread_mutex_lock" );
    int ret = _critical_section();
    if ( pthread_mutex_unlock( &_globals.mutex ) != 0 ) return perrlog( "pthread_mutex_unlock" );
    
    if ( ret < 0 ) return errlog( ERR_CRITICAL, "_critical_section\n" );
    
    return 0;
}


/* Stop all of the active tests */
int faild_manager_stop_tests( void )
{
    int c = 0;

    int _critical_section()
    {
        int d = 0;
        int count = 0;
        faild_uplink_test_instance_t* test_instance = NULL;

        /* Stop all of the active tests */
        for ( c = 0 ; c < FAILD_MAX_INTERFACES ; c++ ) {
            for ( d = 0 ; c < FAILD_MAX_INTERFACE_TESTS ; c++ ) {
                test_instance = _globals.active_tests[c][d];
                _globals.active_tests[c][d] = NULL;
                if ( test_instance == NULL ) continue;
                
                count++;
                test_instance->is_alive = 0;
            }
        }

        debug( 4, "Sent a shutdown signal to %d tests\n", count );
        
        return 0;
    }

    if ( _globals.init == 0 ) return errlog( ERR_WARNING, "manager is not initialized.\n" );
    
    if ( pthread_mutex_lock( &_globals.mutex ) != 0 ) return perrlog( "pthread_mutex_lock" );
    int ret = _critical_section();
    if ( pthread_mutex_unlock( &_globals.mutex ) != 0 ) return perrlog( "pthread_mutex_unlock" );

    if ( ret < 0 ) return errlog( ERR_CRITICAL, "_critical_section\n" );

    /* Now wait for num_tests to go to zero */
    for ( c = 0 ; c < _MAX_SHUTDOWN_TIMEOUT ; c++ ) {
        if ( _globals.total_running_tests == 0 ) {
            break;
        }

        if ( sleep( 1 ) != 0 ) {
            errlog( ERR_WARNING, "Sleep interrupted.\n" );
        }
    }
    
    debug( 4, "Waited %d seconds for all tests to exit.\n", c );

    return 0;
}


static int _validate_test_config( faild_test_config_t* test_config )
{
    int aii = test_config->alpaca_interface_id;
    if (( aii < 1 ) || ( aii  > FAILD_MAX_INTERFACES )) {
        return errlog( ERR_WARNING, "Config has an invalid interface %d\n", aii );
    }

    return 0;
}

/* Return the index of the next open test slot. */
static int _find_open_test( int aii )
{
    if (( aii < 1 ) || ( aii > FAILD_MAX_INTERFACES )) {
        return errlogargs();
    }

    int c = 0;
    aii = aii-1;
    for ( c = 0 ; c < FAILD_MAX_INTERFACE_TESTS ; c++ ) {
        if ( _globals.active_tests[aii][c] == NULL ) return c;
    }

    return errlog( ERR_CRITICAL, "No empty test slots.\n" );
}







