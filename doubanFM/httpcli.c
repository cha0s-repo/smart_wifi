
#include "../cc3200_common.h"

#include <string.h>
#include <stdlib.h>

// HTTP Client lib
#include <http/client/httpcli.h>
#include <http/client/common.h>

#include "http.h"
#include "../misc/audio_spi/vs1053b.h"

// JSON Parser
#include "jsmn.h"

#define HOST_NAME		"www.douban.com"
#define PORT			80
#define GET_URI		"/j/app/radio/people?app_name=radio_desktop_win&version=100&channel=1&type=n"

#define MAX_BUFF_SIZE       3072 //1460

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
//                 GLOBAL VARIABLES -- End
//*****************************************************************************

static int get_mp3(char *buff, char songs[][128], int max)
{
    char *ptr;
    char *end;
    int i,j,k,index;
    char b[256] = {0};

    ptr = buff;

	index = 0;

	while(strlen(songs[index]) && index < max)
		index++;
    while(index < max)
    {
		ptr = strstr(ptr, "url\":");

		end = strstr(ptr, "mp3");

		if(ptr == 0 || end == 0)
			break;
		j = end - ptr;
		ptr += 6;
		k = 0;
		memset(b, '\0', sizeof(b));
		for(i = 0; i < j - 3; i++)
		{
			if (*(ptr + i) == '\\')
			{
				k++;

			}
			else
				b[i-k] = *(ptr + i);
		}
		memcpy(songs[index++], b, strlen(b));
		//UART_PRINT("[paras: %s]\r\n", b);
    }
    //UART_PRINT("paras over\r\n");
    return index;
}

static int readResponse(HTTPCli_Handle httpClient, char list[][128], int num)
{
	long lRetVal = 0;
	int id = 0;
	unsigned long len = 0;
	bool moreFlags = 1;
	const char *ids[4] = {
	                        HTTPCli_FIELD_NAME_CONTENT_LENGTH,
			                HTTPCli_FIELD_NAME_CONNECTION,
			                HTTPCli_FIELD_NAME_CONTENT_TYPE,
			                NULL
	                     };

	/* Read HTTP POST request status code */
	lRetVal = HTTPCli_getResponseStatus(httpClient);
	if(lRetVal > 0)
	{
		switch(lRetVal)
		{
		case 200:
		{
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
					lRetVal = -1;
					goto end;
				}
				}
			}

			int cnt = 0;
			while (len > cnt) {
				cnt += HTTPCli_readRawResponseBody(httpClient, g_buff + cnt, (len - cnt));
			}
			g_buff[len] = '\0';

			lRetVal = get_mp3(g_buff, list, num);
			if(lRetVal < 0)
			{
				goto end;
			}
		}
		break;

		case 404:
		default:
			ERR_PRINT(lRetVal);
			break;
		}
	}
	else
	{
		ERR_PRINT(lRetVal);
		goto end;
	}

	lRetVal = 0;

end:
    return lRetVal;
}

//*****************************************************************************
//
//! \brief HTTP GET Demonstration
//!
//! \param[in]  httpClient - Pointer to http client
//!
//! \return 0 on success else error code on failure
//!
//*****************************************************************************
static int HTTPGetSong(HTTPCli_Handle httpClient, char songs[][128], int max)
{
  
    long lRetVal = 0;
    HTTPCli_Field fields[] = {
                                {HTTPCli_FIELD_NAME_HOST, HOST_NAME},
                                {HTTPCli_FIELD_NAME_ACCEPT, "*/*"},
                               // {HTTPCli_FIELD_NAME_CONTENT_LENGTH, "0"},
                                {HTTPCli_FIELD_NAME_USER_AGENT, "Mozilla/5.0 (Windows NT 6.1)"},
                                {NULL, NULL}
                            };
    bool        moreFlags;

    /* Set request header fields to be send for HTTP request. */
    HTTPCli_setRequestFields(httpClient, fields);

    /* Send GET method request. */
    /* Here we are setting moreFlags = 0 as there are no more header fields need to send
       at later stage. Please refer HTTP Library API documentaion @ HTTPCli_sendRequest
       for more information.
    */
    moreFlags = 0;
    lRetVal = HTTPCli_sendRequest(httpClient, HTTPCli_METHOD_GET, GET_URI, moreFlags);
    if(lRetVal < 0)
    {
        ERR_PRINT(lRetVal);
        return lRetVal;
    }

    lRetVal = readResponse(httpClient, songs, max);

    return lRetVal;
}

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


    return lRetVal;
}


int request_song(char songs[][128], int max)
{
    long lRetVal = -1;
    HTTPCli_Struct httpClient;

    h_name = HOST_NAME;

    lRetVal = ConnectToHTTPServer(&httpClient);
    if(lRetVal < 0)
    {
        ERR_PRINT(lRetVal);
        return -1;
    }

    lRetVal = HTTPGetSong(&httpClient, songs, max);

    if(lRetVal < 0)
    {
    	ERR_PRINT(lRetVal);
    }


    HTTPCli_disconnect(&httpClient);
    return 0;
}
int play(char *req)
{
	long lRetVal = -1;
    HTTPCli_Struct httpClient;

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
	lRetVal = HTTPPlay(&httpClient, pBuff);

    HTTPCli_disconnect(&httpClient);

    return 0;
}

static int HTTPPlay(HTTPCli_Handle httpClient, unsigned char *uri)
{
  
    long lRetVal = 0;
    HTTPCli_Field fields[] = {
                                {HTTPCli_FIELD_NAME_HOST, h_name},
                                {HTTPCli_FIELD_NAME_ACCEPT, "*/*"},
                               // {HTTPCli_FIELD_NAME_CONTENT_LENGTH, "0"},
                                {HTTPCli_FIELD_NAME_USER_AGENT, "Mozilla/5.0 (Windows NT 6.1)"},
                                {NULL, NULL}
                            };
    bool        moreFlags;

    /* Set request header fields to be send for HTTP request. */
    HTTPCli_setRequestFields(httpClient, fields);

    /* Send GET method request. */
    /* Here we are setting moreFlags = 0 as there are no more header fields need to send
       at later stage. Please refer HTTP Library API documentaion @ HTTPCli_sendRequest
       for more information.
    */
    moreFlags = 0;
    lRetVal = HTTPCli_sendRequest(httpClient, HTTPCli_METHOD_GET, uri, moreFlags);
    if(lRetVal < 0)
    {
        ERR_PRINT(lRetVal);
        return lRetVal;
    }

    lRetVal = playSong(httpClient);

    return lRetVal;
}

static int playSong(HTTPCli_Handle httpClient)
{
    long lRetVal = 0;
    int id = 0;
    unsigned long len = 0;

    bool moreFlags = 1;
    long bytesReceived = 0; 

    const char *ids[4] = {
                            HTTPCli_FIELD_NAME_CONTENT_LENGTH,
                            HTTPCli_FIELD_NAME_CONNECTION,
                            HTTPCli_FIELD_NAME_CONTENT_TYPE,
                            NULL
                         };

    /* Read HTTP POST request status code */
    lRetVal = HTTPCli_getResponseStatus(httpClient);
    if(lRetVal > 0)
    {
        switch(lRetVal)
        {
        case 200:
        {
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
                    lRetVal = -1;
                    goto end;
                    
                }
                }
            }

            audio_play_start();

            if (len <= 0)
                len = 0xffffffff;

            int cnt = 0;
            bytesReceived = 0;
            while (len > bytesReceived) {
                cnt = HTTPCli_readRawResponseBody(httpClient, g_pool, (len - bytesReceived));
                bytesReceived += cnt;

                if (cnt > 0)
                {
                    audio_player(g_pool, cnt);
                }
                else
                {
                    ERR_PRINT(bytesReceived);
                    break;
                }
            }
            audio_play_end();
        }
        break;

        case 404:
        default:

            ERR_PRINT(lRetVal);
            break;
        }
    }
    else
    {
        ERR_PRINT(lRetVal);
        goto end;
    }

    lRetVal = 0;

end:
    return lRetVal;
}