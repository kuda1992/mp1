 /**********************************
 * FILE NAME: MP1Node.cpp
 *
 * DESCRIPTION: Membership protocol run by this Node.
 * 				Header file of MP1Node class.
 **********************************/

#ifndef _MP1NODE_H_
#define _MP1NODE_H_

#include "stdincludes.h"
#include "Log.h"
#include "Params.h"
#include "Member.h"
#include "EmulNet.h"
#include "Queue.h"

/**
 * Macros
 */
#define TFAIL 5
#define TREMOVE 10
#define TFAIL 5
#define BROADCAST_INTERVAL 4
/*
 * Note: You can change/add any functions in MP1Node.{h,cpp}
 */

/**
 * Message Types
 */
enum MsgTypes{
    JOINREQ,
    JOINREP,
    UPDATEMEMBERSHIPTABLE,
    DUMMYLASTMSGTYPE
};

/**
 * STRUCT NAME: MessageHdr
 *
 * DESCRIPTION: Header and content of a message
 */
typedef struct MessageHdr {
	enum MsgTypes msgType;
}MessageHdr;


/**
 * CLASS NAME: MP1Node
 *
 * DESCRIPTION: Class implementing Membership protocol functionalities for failure detection
 */
class MP1Node {
private:
	EmulNet *emulNet;
	Log *log;
	Params *par;
	Member *memberNode;
	char NULLADDR[6];
	map<int, int> *toBeRemoved;
	int broadcast;

public:
	MP1Node(Member *, Params *, EmulNet *, Log *, Address *);
	Member * getMemberNode() {
		return memberNode;
	}
	int recvLoop();
	static int enqueueWrapper(void *env, char *buff, int size);
	void nodeStart(char *servaddrstr, short serverport);
	int initThisNode(Address *joinaddr);
	int introduceSelfToGroup(Address *joinAddress);
	int finishUpThisNode();
	void nodeLoop();
	void checkMessages();
	bool recvCallBack(void *env, char *data, int size);
	void nodeLoopOps();
    void updateToBeRemovedMap();
    int isNullAddress(Address *addr);
    void logNodeAddWrapper(Member *memberNode, int id, short port);
    void updateHeartBeatAndTimeStamp(Member* memberNode, long timestamp);
    void handleJoinRequestMessages(Member *member, MessageHdr *message, int size, long timestamp);
    void handleJoinReplyMessages(Member *member, MessageHdr *message, int size, long timestamp);
    void handleUpdateMembershipListMessages(Member *member, MessageHdr *message, int size, long timestamp);
    void updateEntry(Member *member, int id, short port, long heartbeat);
    MemberListEntry createMemberListEntry(Member *member, long timestamp);
    void addNewMemberToMemberListEntries(Member *member, MemberListEntry entry);
    MessageHdr* createJoinReplyMessage(char *address, int memberSize);
    MessageHdr* createUpdateMembershipMessage(int memberSize);
	Address getJoinAddress();
	Address * getNodeAddress(int id, short port);
	void initMemberListTable(Member *memberNode);
    bool isEntryInvalid(Member *memberNode, MemberListEntry& memberListEntry);
    void printAddress(Address *addr);
	virtual ~MP1Node();
};

#endif /* _MP1NODE_H_ */
