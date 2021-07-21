/*
 * AWS IoT Device Embedded C SDK for ZephyrRTOS
 * Copyright (C) 2021 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* Standard includes. */
#include <assert.h>
#include <string.h>
#include <errno.h>

/* Zephyr socket includes. */
#include <net/socket.h>

#include "plaintext_zephyr.h"

/* Milliseconds per second */
#define ONE_SEC_TO_MS    ( 1000 )
/* Nanoseconds per millisecond */
#define ONE_MS_TO_US     ( 1000 )

/*-----------------------------------------------------------*/

/* Each compilation unit must define the NetworkContext struct. */
struct NetworkContext
{
    PlaintextParams_t * pParams;
};

/*-----------------------------------------------------------*/

/**
 * @brief Log possible error from send/recv.
 *
 * @param[in] errorNumber Error number to be logged.
 */
static void logTransportError( int32_t errorNumber );

/*-----------------------------------------------------------*/

static void logTransportError( int32_t errorNumber )
{
    /* Remove unused parameter warning. */
    ( void ) errorNumber;

    LogError( ( "A transport error occurred: %d.", errorNumber ) );
}
/*-----------------------------------------------------------*/

SocketStatus_t Plaintext_Connect( NetworkContext_t * pNetworkContext,
                                  const ServerInfo_t * pServerInfo,
                                  uint32_t sendTimeoutMs,
                                  uint32_t recvTimeoutMs )
{
    SocketStatus_t returnStatus = SOCKETS_SUCCESS;
    PlaintextParams_t * pPlaintextParams = NULL;

    /* Validate parameters. */
    if( ( pNetworkContext == NULL ) || ( pNetworkContext->pParams == NULL ) )
    {
        LogError( ( "Parameter check failed: pNetworkContext is NULL." ) );
        returnStatus = SOCKETS_INVALID_PARAMETER;
    }
    else
    {
        pPlaintextParams = pNetworkContext->pParams;
        returnStatus = Sockets_Connect( &pPlaintextParams->socketDescriptor,
                                        pServerInfo,
                                        sendTimeoutMs,
                                        recvTimeoutMs );
    }

    return returnStatus;
}
/*-----------------------------------------------------------*/

SocketStatus_t Plaintext_Disconnect( const NetworkContext_t * pNetworkContext )
{
    SocketStatus_t returnStatus = SOCKETS_SUCCESS;
    PlaintextParams_t * pPlaintextParams = NULL;

    /* Validate parameters. */
    if( ( pNetworkContext == NULL ) || ( pNetworkContext->pParams == NULL ) )
    {
        LogError( ( "Parameter check failed: pNetworkContext is NULL." ) );
        returnStatus = SOCKETS_INVALID_PARAMETER;
    }
    else
    {
        pPlaintextParams = pNetworkContext->pParams;
        returnStatus = Sockets_Disconnect( pPlaintextParams->socketDescriptor );
    }

    return returnStatus;
}
/*-----------------------------------------------------------*/

int32_t Plaintext_Recv( NetworkContext_t * pNetworkContext,
                        void * pBuffer,
                        size_t bytesToRecv )
{
    PlaintextParams_t * pPlaintextParams = NULL;
    int32_t bytesReceived = -1, selectStatus = -1, getTimeoutStatus = -1;
    struct timeval recvTimeout;
    fd_set readfds;

    assert( pNetworkContext != NULL && pNetworkContext->pParams != NULL );
    assert( pBuffer != NULL );
    assert( bytesToRecv > 0 );

    pPlaintextParams = pNetworkContext->pParams;

    /* Set a receive timemout to use as the timeout for #select. */
    recvTimeout.tv_sec = ( ( ( int64_t ) 500 ) / ONE_SEC_TO_MS );
    recvTimeout.tv_usec = ( ONE_MS_TO_US * ( ( ( int64_t ) 500 ) % ONE_SEC_TO_MS ) );

    ZSOCK_FD_ZERO( &readfds );

    ZSOCK_FD_SET( pPlaintextParams->socketDescriptor, &readfds );

    /* Check if there is data to read from the socket. */
    selectStatus = zsock_select( pPlaintextParams->socketDescriptor + 1,
                                 &readfds,
                                 NULL,
                                 NULL,
                                 &recvTimeout );

    if( selectStatus > 0 )
    {
        /* The socket is available for receiving data. */
        bytesReceived = ( int32_t ) zsock_recv( pPlaintextParams->socketDescriptor,
                                                pBuffer,
                                                bytesToRecv,
                                                0 );
    }
    else if( selectStatus < 0 )
    {
        /* An error occurred while polling. */
        bytesReceived = -1;
    }
    else
    {
        /* Timed out waiting for data to be received. */
        bytesReceived = 0;
    }

    if( ( selectStatus > 0 ) && ( bytesReceived == 0 ) )
    {
        /* Peer has closed the connection. Treat as an error. */
        bytesReceived = -1;
    }
    else if( bytesReceived < 0 )
    {
        logTransportError( errno );
    }

    return bytesReceived;
}
/*-----------------------------------------------------------*/

int32_t Plaintext_Send( NetworkContext_t * pNetworkContext,
                        const void * pBuffer,
                        size_t bytesToSend )
{
    PlaintextParams_t * pPlaintextParams = NULL;
    int32_t bytesSent = -1, selectStatus = -1, getTimeoutStatus = -1;
    struct timeval sendTimeout;
    fd_set writefds;

    assert( pNetworkContext != NULL && pNetworkContext->pParams != NULL );
    assert( pBuffer != NULL );
    assert( bytesToSend > 0 );

    pPlaintextParams = pNetworkContext->pParams;

    /* Set a receive timemout to use as the timeout for #select. */
    sendTimeout.tv_sec = ( ( ( int64_t ) 500 ) / ONE_SEC_TO_MS );
    sendTimeout.tv_usec = ( ONE_MS_TO_US * ( ( ( int64_t ) 500 ) % ONE_SEC_TO_MS ) );

    ZSOCK_FD_ZERO( &writefds );

    ZSOCK_FD_SET( pPlaintextParams->socketDescriptor, &writefds );

    /* Check if data can be written to the socket. */
    selectStatus = zsock_select( pPlaintextParams->socketDescriptor + 1,
                                 NULL,
                                 &writefds,
                                 NULL,
                                 &sendTimeout );

    if( selectStatus > 0 )
    {
        /* The socket is available for sending data. */
        bytesSent = ( int32_t ) zsock_send( pPlaintextParams->socketDescriptor,
                                            pBuffer,
                                            bytesToSend,
                                            0 );
    }
    else if( selectStatus < 0 )
    {
        /* An error occurred while polling. */
        bytesSent = -1;
    }
    else
    {
        /* Timed out waiting for data to be sent. */
        bytesSent = 0;
    }

    if( ( selectStatus > 0 ) && ( bytesSent == 0 ) )
    {
        /* Peer has closed the connection. Treat as an error. */
        bytesSent = -1;
    }
    else if( bytesSent < 0 )
    {
        logTransportError( errno );
    }

    return bytesSent;
}
/*-----------------------------------------------------------*/
