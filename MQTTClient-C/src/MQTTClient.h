/*******************************************************************************
 * Copyright (c) 2014, 2017 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Allan Stockdill-Mander/Ian Craggs - initial API and implementation and/or initial documentation
 *    Ian Craggs - documentation and platform specific header
 *    Ian Craggs - add setMessageHandler function
 *******************************************************************************/

#if !defined(__MQTT_CLIENT_C_)
#define __MQTT_CLIENT_C_

#if defined(__cplusplus)
 extern "C" {
#endif

#if defined(WIN32_DLL) || defined(WIN64_DLL)
  #define DLLImport __declspec(dllimport)
  #define DLLExport __declspec(dllexport)
#elif defined(LINUX_SO)
  #define DLLImport extern
  #define DLLExport  __attribute__ ((visibility ("default")))
#else
  #define DLLImport
  #define DLLExport
#endif

#include "MQTTPacket.h"

#if defined(MQTTCLIENT_PLATFORM_HEADER)
/* The following sequence of macros converts the MQTTCLIENT_PLATFORM_HEADER value
 * into a string constant suitable for use with include.
 */
#define xstr(s) str(s)
#define str(s) #s
#include xstr(MQTTCLIENT_PLATFORM_HEADER)
#endif

#define MAX_PACKET_ID 65535 /* according to the MQTT specification - do not change! */

#if !defined(MAX_MESSAGE_HANDLERS)
#define MAX_MESSAGE_HANDLERS 5 /* redefinable - how many subscriptions do you want? */
#endif

#if !defined(MAX_PENDING_TRANSACTIONS)
#define MAX_PENDING_TRANSACTIONS 5 /* redefinable - how many asynchronous operations are allowed simultaneously? */
#endif

#if !defined(MAILBOX_CAPACITY)
/* How many concurrent messages are allowed?
This can be overridden in MQTTCLIENT_PLATFORM_HEADER. */
#define MAILBOX_CAPACITY     10
#endif

enum QoS { QOS0, QOS1, QOS2, SUBFAIL=0x80 };

/* all failure return codes must be negative */
enum returnCode { BUFFER_OVERFLOW = -2, FAILURE = -1, SUCCESS = 0 };

//TODO: The documentation for these interfaces is lacking - the interface should be well-defined, so why not doxygen comment it?

/* The Platform specific header must define the Network and Timer structures and functions
 * which operate on them.
 *
typedef struct Network
{
	int (*mqttread)(Network*, unsigned char* read_buffer, int, int);
	int (*mqttwrite)(Network*, unsigned char* send_buffer, int, int);
} Network;*/

#if defined(MQTT_ASYNC)
/* Mailboxes are used as an interface to MQTTClient, which reduces
direct calls into the Paho library. The problem with this is the callback
interface, which will not work across processes under a real operating
system. The callback model only works in an embedded RTOS application
because address space is shared. TODO: If we use mailboxes, they should
be used for both input and output (need to think about this). */
/* The Mailbox structure must be defined in the platform specific header,
 * and have the following functions to operate on it. */
extern void MailboxInit(Mailbox*, unsigned int, size_t msgSize);
extern int MailboxPost(Mailbox*, void* data, unsigned int timeout_ms);
extern int MailboxRetrieve(Mailbox*, void* data, unsigned int timeout_ms);
extern int MailboxPeek(Mailbox*, void* data, unsigned int timeout_ms);
#endif

/* The Timer structure must be defined in the platform specific header,
 * and have the following functions to operate on it.  */
extern void TimerInit(Timer*);
extern char TimerIsExpired(Timer*);
extern void TimerCountdownMS(Timer*, unsigned int);
extern void TimerCountdown(Timer*, unsigned int);
extern int TimerLeftMS(Timer*);

typedef struct MQTTMessage
{
    enum QoS qos;
    unsigned char retained;
    unsigned char dup;
    unsigned short id;
    void *payload;
    size_t payloadlen;
} MQTTMessage;

typedef struct MessageData
{
    MQTTMessage* message;
    MQTTString* topicName;
} MessageData;

typedef struct MQTTConnackData
{
    unsigned char rc;
    unsigned char sessionPresent;
} MQTTConnackData;

typedef struct MQTTSubackData
{
    enum QoS grantedQoS;
} MQTTSubackData;

typedef void (*messageHandler)(MessageData*);

typedef struct MQTTPayload
{
    union
    {
        char *cstr;
        struct
        {
            char *raw;
            size_t len;
        };
    };
} MQTTPayload;

typedef enum PubSubAction
{
    Action_Subscribe,
    Action_Unsubscribe,
    Action_Publish
} PubSubAction;

/*typedef int SubHandle;*/

typedef struct PubSubRequest
{
    PubSubAction action;
    char *topic;
    enum QoS qos;
    union
    {
        struct
        {
            MQTTPayload payload;
        } pub;
        struct
        {
            /* SubHandle handle; */
            messageHandler callback;
        } sub;
    };
    
} PubSubRequest;

typedef struct MQTTClient
{
    unsigned int next_packetid,
      command_timeout_ms;
    size_t buf_size,
      readbuf_size;
    unsigned char *buf,
      *readbuf;
    unsigned int keepAliveInterval;
    char ping_outstanding;
    int isconnected;
    int cleansession;

    struct MessageHandlers
    {
        const char* topicFilter;
        void (*fp) (MessageData*);
    } messageHandlers[MAX_MESSAGE_HANDLERS];      /* Message handlers are indexed by subscription topic */

#if defined(MQTT_ASYNC)
    struct
    {
        int packetid;
        /*const char* topic;
        enum QoS qos;
        void (*fp) (MessageData*);*/
        //COME BACK HERE
        PubSubRequest req;
    } pendingTransactions[MAX_PENDING_TRANSACTIONS];
#endif

    void (*defaultMessageHandler) (MessageData*);

    Network* ipstack;
    Timer last_sent, last_received;
#if defined(MQTT_TASK)
    Mutex mutex;
    Thread thread;
#endif
#if defined(MQTT_ASYNC)
    Mailbox mailbox;
#endif
} MQTTClient;

#define DefaultClient {0, 0, 0, 0, NULL, NULL, 0, 0, 0}


/**
 * Create an MQTT client object
 * @param client
 * @param network
 * @param command_timeout_ms
 * @param
 */
DLLExport void MQTTClientInit(MQTTClient* client, Network* network, unsigned int command_timeout_ms,
		unsigned char* sendbuf, size_t sendbuf_size, unsigned char* readbuf, size_t readbuf_size);

/** MQTT Connect - send an MQTT connect packet down the network and wait for a Connack
 *  The nework object must be connected to the network endpoint before calling this
 *  @param options - connect options
 *  @return success code
 */
DLLExport int MQTTConnectWithResults(MQTTClient* client, MQTTPacket_connectData* options,
    MQTTConnackData* data);

/** MQTT Connect - send an MQTT connect packet down the network and wait for a Connack
 *  The nework object must be connected to the network endpoint before calling this
 *  @param options - connect options
 *  @return success code
 */
DLLExport int MQTTConnect(MQTTClient* client, MQTTPacket_connectData* options);

/** MQTT Publish - send an MQTT publish packet and wait for all acks to complete for all QoSs
 *  @param client - the client object to use
 *  @param topic - the topic to publish to
 *  @param message - the message to send
 *  @return success code
 */
DLLExport int MQTTPublish(MQTTClient* client, const char*, MQTTMessage*);

/** MQTT SetMessageHandler - set or remove a per topic message handler
 *  @param client - the client object to use
 *  @param topicFilter - the topic filter set the message handler for
 *  @param messageHandler - pointer to the message handler function or NULL to remove
 *  @return success code
 */
DLLExport int MQTTSetMessageHandler(MQTTClient* c, const char* topicFilter, messageHandler messageHandler);


#if defined(MQTT_ASYNC)
/** MQTT Asynchronous Subscribe - send an MQTT subscribe packet and return immediately.
 *  @param client - the client object to use
 *  @param topicFilter - the topic filter to subscribe to
 *  @param qos - the quality of service level to use for the subscription
 *  @param messageHandler - the callback function to handle subscription updates on this topic
 *  @return success code
 */
DLLExport int MQTTAsyncSubscribe(MQTTClient* client, const char* topicFilter, enum QoS, messageHandler);
#endif



/** MQTT Subscribe - send an MQTT subscribe packet and wait for suback before returning.
 *  @param client - the client object to use
 *  @param topicFilter - the topic filter to subscribe to
 *  @param qos - the quality of service level to use for the subscription
 *  @param messageHandler - the callback function to handle subscription updates on this topic
 *  @return success code
 */
DLLExport int MQTTSubscribe(MQTTClient* client, const char* topicFilter, enum QoS qos, messageHandler messageHandler);

/** MQTT Subscribe - send an MQTT subscribe packet and wait for suback before returning.
 *  @param client - the client object to use
 *  @param topicFilter - the topic filter to subscribe to
 *  @param message - the message to send
 *  @param data - suback granted QoS returned
 *  @return success code
 */
DLLExport int MQTTSubscribeWithResults(MQTTClient* client, const char* topicFilter, enum QoS, messageHandler, MQTTSubackData* data);

/** MQTT Subscribe - send an MQTT unsubscribe packet and wait for unsuback before returning.
 *  @param client - the client object to use
 *  @param topicFilter - the topic filter to unsubscribe from
 *  @return success code
 */
DLLExport int MQTTUnsubscribe(MQTTClient* client, const char* topicFilter);

/** MQTT Disconnect - send an MQTT disconnect packet and close the connection
 *  @param client - the client object to use
 *  @return success code
 */
DLLExport int MQTTDisconnect(MQTTClient* client);

/** MQTT Yield - MQTT background
 *  @param client - the client object to use
 *  @param time - the time, in milliseconds, to yield for
 *  @return success code
 */
DLLExport int MQTTYield(MQTTClient* client, int time);

/** MQTT isConnected
 *  @param client - the client object to use
 *  @return truth value indicating whether the client is connected to the server
 */
DLLExport int MQTTIsConnected(MQTTClient* client)
{
  return client->isconnected;
}

/**
 * Tests a payload's contents to discriminate between raw bytes and zero-terminated strings.
 * @param pl Address containing the payload data
 * @param len the length of the payload data, in bytes
 * @return 0 if the payload data contains raw bytes.
 */
DLLExport int MQTTIsRawPayload(const void *pl, size_t len);

#if defined(MQTT_TASK)
/** MQTT start background thread for a client.  After this, MQTTYield should not be called.
*  @param client - the client object to use
*  @return success code
*/
DLLExport int MQTTStartTask(MQTTClient* client);
#endif

#if defined(__cplusplus)
     }
#endif

#endif
