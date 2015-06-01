
#include "../cc3200_common.h"

#include <string.h>
#include <stdlib.h>

// HTTP Client lib
#include <http/client/httpcli.h>
#include <http/client/common.h>

#include "http_if.h"

#define PORT			80

#define MAX_BUFF_SIZE       (1500)

//*****************************************************************************
//                 GLOBAL VARIABLES -- Start
//*****************************************************************************
unsigned long  g_ulDestinationIP; // IP address of destination server
HTTPCli_Struct httpClient;

char g_buff[MAX_BUFF_SIZE+1];
char *g_pool = g_buff;

unsigned char *h_name;

#if defined(ccs) || defined(gcc)
extern void (* const g_pfnVectors[])(void);
#endif
#if defined(ewarm)
extern uVectorEntry __vector_table;
#endif

//*****************************************************************************
//
//! Function to connect to HTTP server
//!
//! \param  httpClient - Pointer to HTTP Client instance
//!
//! \return Error-code or SUCCESS
//!
//*****************************************************************************
static int ConnectToHTTPServer(HTTPCli_Handle httpClient)
{
    long lRetVal = -1;
    struct sockaddr_in addr;

    /* Resolve HOST NAME/IP */
    lRetVal = sl_NetAppDnsGetHostByName((signed char *)h_name,
                                          strlen((const char *)h_name),
                                          &g_ulDestinationIP,SL_AF_INET);

    if(lRetVal < 0)
    {
        //ASSERT_ON_ERROR(GET_HOST_IP_FAILED);
    }

    /* Set up the input parameters for HTTP Connection */
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = sl_Htonl(g_ulDestinationIP);

    /* Testing HTTPCli open call: handle, address params only */
    HTTPCli_construct(httpClient);
    lRetVal = HTTPCli_connect(httpClient, (struct sockaddr *)&addr, 0, NULL);
    if (lRetVal < 0)
    {
        ERR_PRINT(lRetVal);
        //ASSERT_ON_ERROR(SERVER_CONNECTION_FAILED);
    }    
    return 0;
}

int HTTPGet(HTTPCli_Handle httpClient, char *uri)
{
    int id = 0;
    unsigned long len = 0;
    long lRetVal = -1;
    bool moreFlags = 1;

    HTTPCli_Field fields[] = {
                                {HTTPCli_FIELD_NAME_HOST, h_name},
                                {HTTPCli_FIELD_NAME_ACCEPT, "*/*"},
                               // {HTTPCli_FIELD_NAME_CONTENT_LENGTH, "0"},
                                {HTTPCli_FIELD_NAME_USER_AGENT, "Mozilla/5.0 (Windows NT 6.1)"},
                                {NULL, NULL}
                            };

    const char *ids[4] = {
                            HTTPCli_FIELD_NAME_CONTENT_LENGTH,
                            HTTPCli_FIELD_NAME_CONNECTION,
                            HTTPCli_FIELD_NAME_CONTENT_TYPE,
                            NULL
                         };

    /* Set request header fields to be send for HTTP request. */
    HTTPCli_setRequestFields(httpClient, fields);

    moreFlags = 0;
    lRetVal = HTTPCli_sendRequest(httpClient, HTTPCli_METHOD_GET, uri, moreFlags);
    if(lRetVal < 0)
    {
        ERR_PRINT(lRetVal);
        return lRetVal;
    }

    /* Read HTTP POST request status code */
    lRetVal = HTTPCli_getResponseStatus(httpClient);
    if(lRetVal > 0)
    {
        switch(lRetVal)
        {
        case 200:
            HTTPCli_setResponseFields(httpClient, (const char **)ids);

            while((id = HTTPCli_getResponseField(httpClient, (char *)g_buff, sizeof(g_buff), &moreFlags))
                    != HTTPCli_FIELD_ID_END)
            {

                switch(id)
                {
                case 0: /* HTTPCli_FIELD_NAME_CONTENT_LENGTH */
                {
                    len = strtoul((char *)g_buff, NULL, 0);
                }
                break;
                case 1: /* HTTPCli_FIELD_NAME_CONNECTION */
                case 2: /* HTTPCli_FIELD_NAME_CONTENT_TYPE */
                break;
                default:
                {
                    
                    ERR_PRINT(lRetVal);
                    HTTPCli_disconnect(&httpClient);
                    return -1;
                }
                }
            }
            break;
        default:
            break;
        }
    }
    else
    {
        HTTPCli_disconnect(&httpClient);
        return -1;
    }


    return len;
}

int http_read(char **rbuf)
{
    long lRetVal = 0;
    unsigned long cnt = 0;

    cnt = HTTPCli_readRawResponseBody(&httpClient, g_pool, MAX_BUFF_SIZE);

    if (cnt <= 0)
    {   
        HTTPCli_disconnect(&httpClient);
        return cnt;
    }

    *rbuf = g_pool;
    return cnt;
}

int http_open(char *req)
{
	long lRetVal = -1;

    int  transfer_len = 0;

    char *pBuff = 0;
    char *end = 0;

    char host[64] = {0};

    int i;

    pBuff = strstr(req, "http://");
    if (pBuff)
    {
        pBuff = req + 7;
    }
    else
    {
        pBuff = req;
    }

    end = strstr(pBuff, "/");

    if (!end)
    {
        ERR_PRINT(0);
        return -1;
    }

    for(i = 0; i + pBuff < end; i++ )
    {
        host[i] = *(pBuff + i);
    }

    host[i+1] = '\0';
    pBuff = end;

    h_name = host;

    lRetVal = ConnectToHTTPServer(&httpClient);
    if(lRetVal < 0)
    {
        ERR_PRINT(lRetVal);
        return -1;
    }

    lRetVal = HTTPGet(&httpClient, pBuff);

    if(lRetVal < 0)
    {
        ERR_PRINT(lRetVal);
        return -1;
    }
    return lRetVal;
}
