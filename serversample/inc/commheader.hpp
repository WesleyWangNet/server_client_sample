#ifndef __COMMHEADER_HPP__
#define __COMMHEADER_HPP__

#include "csocket.hpp"

typedef struct COMM_PKG_HEADER
{
    unsigned short pkgLen;
    unsigned short msgCode;
    int crc32;
}__attribute__((packed)) COMM_PKG_HEADER, *LPCOMM_PKG_HEADER;

typedef struct MSG_HEADER_STRUCT
{
    Connections_p pConn;
    uint64_t iCurrsequence;
} MSG_HADER_STRUCT, *LPMSG_HADER_STRUCT;

#endif