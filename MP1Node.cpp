/**********************************
 * FILE NAME: MP1Node.cpp
 *
 * DESCRIPTION: Membership protocol run by this Node.
 * 				Definition of MP1Node class functions.
 **********************************/

#include "MP1Node.h"

/*
 * Note: You can change/add any functions in MP1Node.{h,cpp}
 */

/**
 * Overloaded Constructor of the MP1Node class
 * You can add new members to the class if you think it
 * is necessary for your logic to work
 */
MP1Node::MP1Node(Member *member, Params *params, EmulNet *emul, Log *log, Address *address) {
    for (int i = 0; i < 6; i++) {
        NULLADDR[i] = 0;
    }
    this->memberNode = member;
    this->emulNet = emul;
    this->log = log;
    this->par = params;
    this->memberNode->addr = *address;
    this->toBeRemoved = new map<int, int>;
    this->broadcast = 0;
}

/**
 * Destructor of the MP1Node class
 */
MP1Node::~MP1Node() {}

/**
 * FUNCTION NAME: recvLoop
 *
 * DESCRIPTION: This function receives message from the network and pushes into the queue
 * 				This function is called by a node to receive messages currently waiting for it
 */
int MP1Node::recvLoop() {
    if (memberNode->bFailed) {
        return false;
    } else {
        return emulNet->ENrecv(&(memberNode->addr), enqueueWrapper, NULL, 1, &(memberNode->mp1q));
    }
}

/**
 * FUNCTION NAME: enqueueWrapper
 *
 * DESCRIPTION: Enqueue the message from Emulnet into the queue
 */
int MP1Node::enqueueWrapper(void *env, char *buff, int size) {
    Queue q;
    return q.enqueue((queue<q_elt> *) env, (void *) buff, size);
}

/**
 * FUNCTION NAME: nodeStart
 *
 * DESCRIPTION: This function bootstraps the node
 * 				All initializations routines for a member.
 * 				Called by the application layer.
 */
void MP1Node::nodeStart(char *servaddrstr, short servport) {
    Address joinaddr;
    joinaddr = getJoinAddress();

    // Self booting routines
    if (initThisNode(&joinaddr) == -1) {
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "init_thisnode failed. Exit.");
#endif
        exit(1);
    }

    if (!introduceSelfToGroup(&joinaddr)) {
        finishUpThisNode();
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Unable to join self to group. Exiting.");
#endif
        exit(1);
    }

}

/**
 * FUNCTION NAME: initThisNode
 *
 * DESCRIPTION: Find out who I am and start up
 */
int MP1Node::initThisNode(Address *joinaddr) {
    /*
     * This function is partially implemented and may require changes
     */
    /*int id = *(int *) (&memberNode->addr.addr);
    int port = *(short *) (&memberNode->addr.addr[4]);*/

    //if node has already failed, it cannot be turned on again: CRASH-STOP failure mode
    log->LOG(&memberNode->addr, "Node initalised!", memberNode->addr.addr[0]);


    memberNode->bFailed = false;
    memberNode->inited = true;
    memberNode->inGroup = false;
    // node is up!
    memberNode->nnb = 0;
    memberNode->heartbeat = 0;
    memberNode->pingCounter = TFAIL;
    memberNode->timeOutCounter = -1;
    initMemberListTable(memberNode);

    log->LOG(&memberNode->addr, "Node initalised!", memberNode->addr.addr[0]);


    return SUCCESS;
}

/**
 * FUNCTION NAME: introduceSelfToGroup
 *
 * DESCRIPTION: Join the distributed system
 */
int MP1Node::introduceSelfToGroup(Address *joinaddr) {
    MessageHdr *msg;
#ifdef DEBUGLOG
    static char s[1024];
#endif

    if (0 == memcmp((char *) &(memberNode->addr.addr), (char *) &(joinaddr->addr), sizeof(memberNode->addr.addr))) {
        // I am the group booter (first process to join the group). Boot up the group
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Starting up group...");
#endif
        memberNode->inGroup = true;
    } else {
        size_t msgsize = sizeof(MessageHdr) + sizeof(joinaddr->addr) + sizeof(long) + 1;
        msg = (MessageHdr *) malloc(msgsize * sizeof(char));

        // create JOINREQ message: format of data is {struct Address myaddr}
        msg->msgType = JOINREQ;
        memcpy((char *) (msg + 1), &memberNode->addr.addr, sizeof(memberNode->addr.addr));
        memcpy((char *) (msg + 1) + 1 + sizeof(memberNode->addr.addr), &memberNode->heartbeat, sizeof(long));

#ifdef DEBUGLOG
        sprintf(s, "Trying to join...");
        log->LOG(&memberNode->addr, s);
#endif

        // send JOINREQ message to introducer member
        emulNet->ENsend(&memberNode->addr, joinaddr, (char *) msg, static_cast<int>(msgsize));

        free(msg);
    }

    int id = 0;
    short port;
    memcpy(&id, &(memberNode->addr.addr[0]), sizeof(int));
    memcpy(&port, &(memberNode->addr.addr[4]), sizeof(short));
    MemberListEntry *newEntry = new MemberListEntry(id, port, 0, par->getcurrtime());
    memberNode->memberList.push_back(*newEntry);

    return 1;

}

/**
 * FUNCTION NAME: finishUpThisNode
 *
 * DESCRIPTION: Wind up this node and clean up state
 */
int MP1Node::finishUpThisNode() {
    /*
     * Your code goes here
     */
    emulNet->ENcleanup();
    return 0;
}

/**
 * FUNCTION NAME: nodeLoop
 *
 * DESCRIPTION: Executed periodically at each member
 * 				Check your messages in queue and perform membership protocol duties
 */
void MP1Node::nodeLoop() {
    if (memberNode->bFailed) {
        return;
    }

    // Check my messages
    checkMessages();

    // Wait until you're in the group...
    if (!memberNode->inGroup) {
        return;
    }

    // ...then jump in and share your responsibilites!
    nodeLoopOps();
}

/**
 * FUNCTION NAME: checkMessages
 *
 * DESCRIPTION: Check messages in the queue and call the respective message handler
 */
void MP1Node::checkMessages() {
    void *ptr;
    int size;
    // Pop waiting messages from memberNode's mp1q
    while (!memberNode->mp1q.empty()) {
        ptr = memberNode->mp1q.front().elt;
        size = memberNode->mp1q.front().size;
        memberNode->mp1q.pop();
        recvCallBack((void *) memberNode, (char *) ptr, size);
    }
}

bool nodeHasReceivedJoinReplyMessage(MsgTypes message) {
    return message == JOINREP;
}

bool nodeHasReceivedJoinRequestMessage(MsgTypes message) {
    return message == JOINREQ;
}

bool nodeHasReceivedUpdateMembershipMessage(MsgTypes message) {
    return message == UPDATEMEMBERSHIPTABLE;
}

MemberListEntry MP1Node::createMemberListEntry(Member *member, long timestamp) {

    MemberListEntry *memberListEntry = new MemberListEntry;

    memberListEntry->setid(member->addr.addr[0]);
    memberListEntry->setport(member->addr.addr[4]);
    memberListEntry->setheartbeat(member->heartbeat);
    memberListEntry->settimestamp(timestamp);
    return *memberListEntry;
}

void MP1Node::addNewMemberToMemberListEntries(Member *member, MemberListEntry entry) {
    member->memberList.push_back(entry);
}

MessageHdr *MP1Node::createJoinReplyMessage(char *address, int memberSize) {

    //allocate memory
    size_t joinReplyMessageSize = sizeof(MessageHdr) + (sizeof(address) + sizeof(long)) * memberSize;
    MessageHdr *joinReplyMessage;
    joinReplyMessage = static_cast<MessageHdr *>(malloc(joinReplyMessageSize * sizeof(char)));
    joinReplyMessage->msgType = JOINREP;

    return joinReplyMessage;

}

MessageHdr *MP1Node::createUpdateMembershipMessage(int memberSize) {

    //allocate memory
    size_t updateMembershipMessageSize = sizeof(MessageHdr) + (6 + sizeof(long)) * memberSize;
    MessageHdr *updateMembershipMessage;
    updateMembershipMessage = static_cast<MessageHdr *>(malloc(updateMembershipMessageSize * sizeof(char)));
    updateMembershipMessage->msgType = UPDATEMEMBERSHIPTABLE;

    return updateMembershipMessage;

}

void MP1Node::updateToBeRemovedMap() {

    map<int, int>::iterator it = toBeRemoved->begin();

    while (it != toBeRemoved->end()) {
        it->second = it->second + 1;

        if (it->second >= TREMOVE) {
            int id = it->first;
            short port = 0;
            Address address = Address(*getNodeAddress(id, port));

            log->logNodeRemove(&memberNode->addr, &address);

            //remove member from list
            bool exists = false;

            memberNode->myPos = memberNode->memberList.begin();
            while (memberNode->myPos != memberNode->memberList.end()) {
                if ((*memberNode->myPos).getid() == it->first) {
                    exists = true;
                    memberNode->myPos = memberNode->memberList.erase(memberNode->myPos);
                    break;
                }
                ++memberNode->myPos;
            }
            it = this->toBeRemoved->erase(it);

        } else {
            ++it;
        }
    }

}

#define DEBUG_JOINREQ


void MP1Node::handleJoinRequestMessages(Member *member, MessageHdr *message, int size, long timestamp) {

    cout << "received a join request message " << endl;

    char addr[6];

    memcpy(&addr, (char *) (message) + 4, 6);

    int id = 0;
    short port;
    int currentOffset = 0;

    memcpy(&id, &addr[0], sizeof(int));
    memcpy(&port, &addr[4], sizeof(short));

    cout << "id = " << id << "; port = " << port << endl;
    cout << "before adding member list entry, there are " << member->memberList.size() << " entries" << endl;
    MemberListEntry newEntry = createMemberListEntry(member, timestamp);
    addNewMemberToMemberListEntries(member, newEntry);
    cout << "after adding member list entry, there are " << member->memberList.size() << " entries" << endl;

    MessageHdr *joinReplyMessage = createJoinReplyMessage(addr, member->memberList.size());

    currentOffset += sizeof(int);

    for (auto memberListEntry: member->memberList) {

        // if it is to be deleted, don't add it to member list messages
        if (toBeRemoved->count(memberListEntry.getid()) != 0) {
            continue;
        }

        Address address = Address(*getNodeAddress(memberListEntry.getid(), memberListEntry.getport()));

        //add to join reply message
        memcpy((char *) joinReplyMessage + currentOffset, &address.addr, sizeof(address.addr));
        currentOffset += sizeof(address.addr);
        memcpy((char *) (joinReplyMessage) + currentOffset, &memberListEntry.heartbeat, sizeof(long));
        currentOffset += sizeof(long);
        cout << "currentOffset = " << currentOffset << ";" << endl;
    }

    Address address = Address(*getNodeAddress(id, port));

    cout << "Sending join address to " << address.getAddress() << endl;

    emulNet->ENsend(&member->addr, &address, (char *) joinReplyMessage, member->memberList.size());
}


void MP1Node::logNodeAddWrapper(Member *memberNode, int id, short port) {
    Address address;
    string _address = to_string(id) + ":" + to_string(port);
    address = Address(_address);
    log->logNodeAdd(&memberNode->addr, &address);
}

void MP1Node::handleJoinReplyMessages(Member *member, MessageHdr *message, int size, long timestamp) {

    cout << "received a join reply message " << endl;

    handleUpdateMembershipListMessages(member, message, size, timestamp);

    member->inGroup = true;
}

void MP1Node::updateEntry(Member *member, int id, short port, long heartbeat) {

    //if the node with the id is already in the to be remove map
    auto existsInToBeRemovedMap = toBeRemoved->count(id);

    if (existsInToBeRemovedMap != 0) {
        return;
    }

    //if it is not to be deleted, then update/insert normally
    bool exists = false;

    for (auto memberListEntry: member->memberList) {
        if (memberListEntry.getid() == id && memberListEntry.getport() == port) {
            exists = true;
            // the incoming hearbeat is more latest
            if (memberListEntry.getheartbeat() < heartbeat) {
                cout << "updating the heartbeat for " << id << endl;
                memberListEntry.setheartbeat(heartbeat);
                memberListEntry.settimestamp(par->getcurrtime());
            }
            break;
        }
    }

    if (!exists) {
        MemberListEntry *newEntry = new MemberListEntry(id, port, heartbeat, par->getcurrtime());
        addNewMemberToMemberListEntries(member, *newEntry);
        logNodeAddWrapper(memberNode, id, port);
    }
}

void MP1Node::handleUpdateMembershipListMessages(Member *member, MessageHdr *message, int size, long timestamp) {

    int entry_num = (size - 4) / (6 + 8);
    int currentOffset = 4;
    char addr[6];
    int id = 0;
    short port;
    long heartbeat = 0;

    memcpy(&addr, (char *) (message) + currentOffset, 6);
    memcpy(&id, &addr[0], sizeof(int));

    if (toBeRemoved->count(id) != 0) {

        map<int, int>::iterator it = this->toBeRemoved->begin();

        while (it != toBeRemoved->end()) {

            if (it->first == id) {
                this->toBeRemoved->erase(it);
                break;
            }
            ++it;
        }

    }

    for (int i = 0; i < entry_num; ++i) {

        memcpy(&addr, (char *) (message) + currentOffset, 6);
        currentOffset += 6;
        memcpy(&id, &addr[0], sizeof(int));
        memcpy(&port, &addr[4], sizeof(short));

        // heartbeat
        memcpy(&heartbeat, (char *) (message) + currentOffset, 8);
        currentOffset += 8;


        updateEntry(member, id, port, heartbeat);

    }

}


/**
 * FUNCTION NAME: recvCallBack
 *
 * DESCRIPTION: Message handler for different message types
 */
bool MP1Node::recvCallBack(void *env, char *data, int size) {
    /*
     * Your code goes here
     */
    memberNode = (Member *) env;
    MsgTypes messageType = reinterpret_cast<MessageHdr *>(data)->msgType;
    MessageHdr *message = reinterpret_cast<MessageHdr *>(data);
    int localtimestamp = par->getcurrtime();


    if (nodeHasReceivedJoinRequestMessage(messageType)) {
        handleJoinRequestMessages(memberNode, message, size, localtimestamp);
        return 1;
    }


    if (nodeHasReceivedJoinReplyMessage(messageType)) {
        handleJoinReplyMessages(memberNode, message, size, localtimestamp);
    }

    // node has received message to update its membership list
    if (nodeHasReceivedUpdateMembershipMessage(messageType)) {
        cout << "node has received update membership list" << endl;
        handleUpdateMembershipListMessages(memberNode, message, size, localtimestamp);

    }
    return 1;
}

Address *MP1Node::getNodeAddress(int id, short port) {

    Address *address = new Address;
    address->init();

    *(int *) (&(address->addr)) = id;
    *(short *) (&(address->addr[4])) = id;
    return address;

}

bool nodeIsAliveAndInGroup(bool memberStatus, bool isInGroup) {
    return !memberStatus && isInGroup;
}

void MP1Node::updateHeartBeatAndTimeStamp(Member *memberNode, long timestamp) {

    memberNode->heartbeat++;

    // update the entry of self in self memberlist
    memberNode->memberList[0].setheartbeat(memberNode->heartbeat);
    memberNode->memberList[0].settimestamp(timestamp);
}

/**
 * FUNCTION NAME: nodeLoopOps
 *
 * DESCRIPTION: Check if any node hasn't responded within a timeout period and then delete
 * 				the nodes
 * 				Propagate your membership list
 */
void MP1Node::nodeLoopOps() {

    /*
     * Your code goes here
     */

    // update self heartbeat and timestamp

    updateHeartBeatAndTimeStamp(memberNode, par->getcurrtime());


    for (auto memberList: memberNode->memberList) {

        // if member is invalid
        if (isEntryInvalid(memberNode, memberList)) {
            int id = memberList.getid();
            // short port = memberList.getport();

            // Address address = Address(*getNodeAddress(id, port));

            cout << "found invalid entry with id " << id << endl;

            if (toBeRemoved->count(id) == id) {
                cout << "inserting " << id << " into the to be remove map";
                toBeRemoved->insert(make_pair(id, 0));
            }
        }
    }

    updateToBeRemovedMap();


    int entry_num = memberNode->memberList.size();
    int currentOffset = 0;
    MessageHdr *updateMembershipsMessage = createUpdateMembershipMessage(entry_num);
    currentOffset += sizeof(int);


    for (auto memberList: memberNode->memberList) {
        int id = memberList.getid();
        short port = memberList.getport();
        if (toBeRemoved->count(id) == 0) {
            continue;
        }

        Address address = Address(*getNodeAddress(id, port));

        // add to MEMBERLIST
        memcpy((char *) (updateMembershipsMessage) + currentOffset, &address.addr, sizeof(address.addr));
        currentOffset += sizeof(address.addr);
        memcpy((char *) (updateMembershipsMessage) + currentOffset, &memberList.heartbeat, sizeof(long));
        currentOffset += sizeof(long);
    }

    broadcast++;


    if (broadcast >= BROADCAST_INTERVAL) {
        // broadcast
        for (auto memberList: memberNode->memberList) {
            int id = memberList.getid();
            short port = memberList.getport();
            Address address = Address(*getNodeAddress(id, port));
            emulNet->ENsend(&memberNode->addr, &address, (char *) updateMembershipsMessage, currentOffset);
        }
        broadcast = 0;
    } else {
        int member_num = memberNode->memberList.size();
        int index = rand() % member_num;

        int id = memberNode->memberList.at(index).getid();
        short port = memberNode->memberList.at(index).getport();

        Address address = Address(*getNodeAddress(id, port));

        emulNet->ENsend(&memberNode->addr, &address, (char *) updateMembershipsMessage, currentOffset);
    }


    free(updateMembershipsMessage);

}


/**
 * FUNCTION NAME: isNullAddress
 *
 * DESCRIPTION: Function checks if the address is NULL
 */
int MP1Node::isNullAddress(Address *addr) {
    return (memcmp(addr->addr, NULLADDR, 6) == 0 ? 1 : 0);
}

/**
 * FUNCTION NAME: getJoinAddress
 *
 * DESCRIPTION: Returns the Address of the coordinator
 */
Address MP1Node::getJoinAddress() {
    Address joinaddr;

    memset(&joinaddr, 0, sizeof(Address));
    *(int *) (&joinaddr.addr) = 1;
    *(short *) (&joinaddr.addr[4]) = 0;

    return joinaddr;
}

/**
 * FUNCTION NAME: initMemberListTable
 *
 * DESCRIPTION: Initialize the membership list
 */
void MP1Node::initMemberListTable(Member *memberNode) {
    memberNode->memberList.clear();
}

bool MP1Node::isEntryInvalid(Member *memberNode, MemberListEntry &memberListEntry) {
    long entryTimestamp = memberListEntry.gettimestamp();
    long nodeTimestamp = memberNode->memberList[0].gettimestamp();

    if (nodeTimestamp - entryTimestamp > TFAIL) {
        return true;
    }
    return false;
}

/**
 * FUNCTION NAME: printAddress
 *
 * DESCRIPTION: Print the Address
 */
void MP1Node::printAddress(Address *addr) {
    printf("%d.%d.%d.%d:%d \n", addr->addr[0], addr->addr[1], addr->addr[2], addr->addr[3], *(short *) &addr->addr[4]);
}
