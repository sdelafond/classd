// HASHOBJECT.CPP
// Traffic Classification Engine
// Copyright (c) 2011 Untangle, Inc.
// All Rights Reserved
// Written by Michael A. Hotz

#include "common.h"
#include "classd.h"
/*--------------------------------------------------------------------------*/
HashObject::HashObject(unsigned short aProto,const char *aHashname)
{
hashname = (char *)malloc(strlen(aHashname)+1);
strcpy(hashname,aHashname);
netproto = aProto;
next = NULL;

timeout = time(NULL);
if (netproto == IPPROTO_TCP) timeout+=cfg_tcp_timeout;
if (netproto == IPPROTO_UDP) timeout+=cfg_udp_timeout;
}
/*--------------------------------------------------------------------------*/
HashObject::~HashObject(void)
{
free(hashname);
}
/*--------------------------------------------------------------------------*/
int HashObject::GetObjectSize(void)
{
int			mysize;

mysize = sizeof(*this);
if (hashname != NULL) mysize+=(strlen(hashname) + 1);
return(mysize);
}
/*--------------------------------------------------------------------------*/
void HashObject::GetObjectString(char *target,int maxlen)
{
snprintf(target,maxlen,"N:%s",hashname);
}
/*--------------------------------------------------------------------------*/
void HashObject::UpdateObject(void)
{
timeout = time(NULL);
if (netproto == IPPROTO_TCP) timeout+=cfg_tcp_timeout;
if (netproto == IPPROTO_UDP) timeout+=cfg_udp_timeout;
}
/*--------------------------------------------------------------------------*/
