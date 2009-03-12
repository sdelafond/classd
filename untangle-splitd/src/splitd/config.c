/*
 * Copyright (c) 2003-2008 Untangle, Inc.
 * All rights reserved.
 *
 * This software is the confidential and proprietary information of
 * Untangle, Inc. ("Confidential Information"). You shall
 * not disclose such Confidential Information.
 *
 * $Id: config.c 18527 2008-08-27 17:50:43Z amread $
 */

#include <stdlib.h>
#include <stddef.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <mvutil/debug.h>
#include <mvutil/errlog.h>

#include "splitd.h"
#include "json/object_utils.h"
#include "json/serializer.h"

static int _verify_config( splitd_config_t* config );

static int _interface_array_get_size( void *c_array );

static int _test_config_array_get_size( void *c_array );

static json_serializer_string_t _os_string = {
    .offset = offsetof( splitd_interface_t, os_name ),
    .len = sizeof((( splitd_interface_t *)0)->os_name )
};

static json_serializer_t _interface_serializer = {
    .name = "interface",
    .fields = {{
            .name = "os_name",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_ERROR,
            .to_c = json_serializer_to_c_string,
            .to_json = json_serializer_to_json_string,
            .arg = &_os_string
        },{
            .name = "alpaca_interface_index",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_ERROR,
            .to_c = json_serializer_to_c_int,
            .to_json = json_serializer_to_json_int,
            .arg = (void*)offsetof( splitd_interface_t, alpaca_interface_index )
        }, JSON_SERIALIZER_FIELD_TERM }
};

static json_serializer_array_t _interface_array_arg =
{
    .max_length = FAILD_MAX_INTERFACES,
    .data_offset = offsetof( splitd_config_t, interfaces ),
    .length_offset = offsetof( splitd_config_t, interfaces_length ),
    .get_size = _interface_array_get_size,
    .default_value = NULL,
    .serializer = &_interface_serializer,
    .item_size = sizeof( splitd_interface_t )
};

static json_serializer_string_t _test_class_name = {
    .offset = offsetof( splitd_test_config_t, test_class_name ),
    .len = sizeof((( splitd_test_config_t *)0)->test_class_name )
};

static json_serializer_t _test_config_serializer = {
    .name = "test",
    .fields = {{
            .name = "alpaca_interface_index",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_ERROR,
            .to_c = json_serializer_to_c_int,
            .to_json = json_serializer_to_json_int,
            .arg = (void*)offsetof( splitd_test_config_t, alpaca_interface_index )
        },{
            .name = "test_class_name",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_ERROR,
            .to_c = json_serializer_to_c_string,
            .to_json = json_serializer_to_json_string,
            .arg = &_test_class_name
        },{
            .name = "timeout",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_ERROR,
            .to_c = json_serializer_to_c_int,
            .to_json = json_serializer_to_json_int,
            .arg = (void*)offsetof( splitd_test_config_t, timeout_ms )
        },{
            .name = "delay",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_ERROR,
            .to_c = json_serializer_to_c_int,
            .to_json = json_serializer_to_json_int,
            .arg = (void*)offsetof( splitd_test_config_t, delay_ms )
        },{
            .name = "bucket_size",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_ERROR,
            .to_c = json_serializer_to_c_int,
            .to_json = json_serializer_to_json_int,
            .arg = (void*)offsetof( splitd_test_config_t, bucket_size )
        },{
            .name = "threshold",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_ERROR,
            .to_c = json_serializer_to_c_int,
            .to_json = json_serializer_to_json_int,
            .arg = (void*)offsetof( splitd_test_config_t, threshold )
        }, JSON_SERIALIZER_FIELD_TERM}
};

static json_serializer_array_t _test_config_array_arg =
{
    .max_length = FAILD_MAX_INTERFACES * FAILD_MAX_INTERFACE_TESTS,
    .data_offset = offsetof( splitd_config_t, tests ),
    .length_offset = offsetof( splitd_config_t, tests_length ),
    .get_size = _test_config_array_get_size,
    .default_value = NULL,
    .serializer = &_test_config_serializer,
    .item_size = sizeof( splitd_test_config_t )
};

static json_serializer_t _config_serializer = {
    .name = "config",
    .fields = {{
            .name = "enabled",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_ERROR,
            .to_c = json_serializer_to_c_boolean,
            .to_json = json_serializer_to_json_boolean,
            .arg = (void*)offsetof( splitd_config_t, is_enabled )
        },{
            .name = "interface_min",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_ERROR,
            .to_c = json_serializer_to_c_int,
            .to_json = json_serializer_to_json_int,
            .arg = (void*)offsetof( splitd_config_t, interface_min_ms )
        },{
            .name = "interfaces",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_ERROR,
            .to_c = json_serializer_to_c_array,
            .to_json = json_serializer_to_json_array,
            .arg = &_interface_array_arg
        },{
            .name = "tests",
            .fetch_arg = 1,
            .if_empty = JSON_SERIALIZER_FIELD_EMPTY_ERROR,
            .to_c = json_serializer_to_c_array,
            .to_json = json_serializer_to_json_array,
            .arg = &_test_config_array_arg
        }, JSON_SERIALIZER_FIELD_TERM}
};

splitd_config_t* splitd_config_malloc( void )
{
    splitd_config_t* config = NULL;

    if (( config = calloc( 1, sizeof( splitd_config_t ))) == NULL ) return errlogmalloc_null();

    return config;
}

int splitd_config_init( splitd_config_t* config )
{
    if ( config == NULL ) return errlogargs();

    bzero( config, sizeof( splitd_config_t ));
    
    return 0;
}

splitd_config_t* splitd_config_create()
{
    splitd_config_t* config = NULL;
    
    if (( config = splitd_config_malloc()) == NULL ) {
        return errlog_null( ERR_CRITICAL, "splitd_config_malloc\n" );
    }

    if ( splitd_config_init( config ) < 0 ) {
        return errlog_null( ERR_CRITICAL, "splitd_config_init\n" );
    }

    return config;
}

int splitd_config_load_json( splitd_config_t* config, struct json_object* json_config )
{
    if ( config == NULL ) return errlogargs();
    if ( json_config == NULL ) return errlogargs();

    if ( json_serializer_to_c( &_config_serializer, json_config, config ) < 0 ) {
        return errlog( ERR_CRITICAL, "json_serializer_to_c\n" );
    }

    return 0;
}

struct json_object* splitd_config_to_json( splitd_config_t* config )
{
    if ( config == NULL ) return errlogargs_null();
    
    struct json_object* json_object = NULL;
    if (( json_object = json_serializer_to_json( &_config_serializer, config )) == NULL ) {
        return errlog_null( ERR_CRITICAL, "json_serializer_to_json\n" );
    }

    return json_object;
}

static int _interface_array_get_size( void *c_array )
{
    return sizeof((( splitd_config_t *)0)->interfaces );
}

static int _test_config_array_get_size( void *c_array )
{
    return sizeof((( splitd_config_t *)0)->tests );    
}
