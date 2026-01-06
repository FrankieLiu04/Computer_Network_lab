#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//#define BIDIRECTIONAL

/********* SECTION 0: GLOBAL DATA STRUCTURES*********/
/* a "msg__" is the data unit passed from layer 5 (teachers code) to layer  */
/* 4 (students' code).  It contains the data (characters) to be delivered */
/* to layer 5 via the students transport level protocol entities.         */

struct msg_ {
    char data[20];
};

/* a packet is the data unit passed from layer 4 (students code) to layer */
/* 3 (teachers code).  Note the pre-defined packet structure, which all   */
/* students must follow. Only a small number of packets will be sent, so
the sequence number is a monotically increasing integer, which never causes overflow.*/

struct pkt {
    int seqnum;
    int acknum;
    int checksum;
    char payload[20];
};

/* Each packet has a timer. Note that it will be used by the function starttime_r()*/
struct timer {
    int pkt_seq;
    float start_time;
};

void ComputeChecksum(struct pkt *);

int CheckCorrupted(struct pkt);

void A_output(struct msg_);

void tolayer3(int, struct pkt);

void starttime_r(struct timer*, int, float);

void A_input( struct pkt);

void B_input(struct pkt);

void B_init(void);

void A_init(void);

void tolayer5(int, char *);

void stoptime_r(int, int);

void printevlist(void);

void generate_next_arrival(void);

void init(void);

void A_packet_time_rinterrupt(struct timer*);

/********* SECTION I: GLOBAL VARIABLES*********/
/* the following global variables will be used by the routines to be implemented.
   you may define new global variables if necessary. However, you should reduce
   the number of new global variables to the minimum. Excessive new global variables
   will result in point deduction.
*/

#define MAXBUFSIZE 5000

#define RTT  15.0

#define NOTUSED 0

#define TRUE   1
#define FALSE  0

#define   A    0
#define   B    1

int WINDOWSIZE = 8;
float currenttime_();         /* get the current time_ */

int nextseqnum;              /* next sequence number to use in sender side */
int base;                    /* the head of sender window */

struct pkt *winbuf;             /* window packets buffer, which has been allocated enough space to by the simulator */
int winfront, winrear;        /* front and rear points of window buffer */
int pktnum;                     /* the # of packets in window buffer, i.e., the # of packets to be resent when time_out */

struct msg_ buffer[MAXBUFSIZE];
int buffront, bufrear;          /* front and rear pointers of buffer */
int msg_num;            /* # of messages in buffer */
int ack_adv_num = 0;    /*the # of acked packets in window buffer*/

int totalmsg_ = -1;
int packet_lost = 0;
int packet_corrupt = 0;
int packet_sent = 0;
int packet_correct = 0;
int packet_resent = 0;
int packet_time_out = 0;
int packet_receieve = 0;
float packet_throughput = 0;
int total_sim_msg = 0;

/********* SECTION II: FUNCTIONS TO BE COMPLETED BY STUDENTS*********/

/* compute the checksum of the packet to be sent from the sender */
void ComputeChecksum(struct pkt *packet)
{
    unsigned int sum = 0;
    sum += packet->seqnum;
    sum += packet->acknum;

    for(int i=0; i<20; i++) {
        sum += (unsigned char)packet->payload[i];
        if(sum & 0xFFFF0000) {
            sum = (sum & 0xFFFF) + (sum >> 16);
        }
    }
    packet->checksum = ~(sum & 0xFFFF);
}

int CheckCorrupted(struct pkt packet)
{
    unsigned int sum = 0;
    sum += packet.seqnum;
    sum += packet.acknum;

    for(int i=0; i<20; i++) {
        sum += (unsigned char)packet.payload[i];
        if(sum & 0xFFFF0000) {
            sum = (sum & 0xFFFF) + (sum >> 16);
        }
    }
    sum += packet.checksum;
    return (sum & 0xFFFF) != 0xFFFF;
}


/* called from layer 5, passed the data to be sent to other side */
void A_output(struct msg_ message)
{
    struct timer *pkt_timer;

    /* Create pkt before window is full */
    if (nextseqnum < base + WINDOWSIZE) {
        /* Create new data packet */
        struct pkt packet;
        packet.seqnum = nextseqnum;
        packet.acknum = NOTUSED;
        memcpy(packet.payload, message.data, 20);
        ComputeChecksum(&packet);

        /* sent to layer3 */
        int current_base = base; /* preserve value to sendout */
        printf("[%.1f] A: send packet [%d] base [%d]\n", currenttime_(), nextseqnum, current_base);
        tolayer3(A, packet);
        packet_sent++; // update packet sent counter

        /* save to buffer */
        winbuf[winrear] = packet;
        winbuf[winrear].acknum = 0; /* initializing */
        winrear = (winrear + 1) % WINDOWSIZE;
        pktnum++;

        /* set timer */
        pkt_timer = (struct timer*)malloc(sizeof(struct timer));
        pkt_timer->pkt_seq = nextseqnum;
        starttime_r(pkt_timer, A, RTT);

        nextseqnum++;
    } else if (msg_num < MAXBUFSIZE - 1) {
        /* preserve mes when windows full but buffer not */
        buffer[bufrear] = message;
        bufrear = (bufrear + 1) % MAXBUFSIZE;
        msg_num++;
        printf("[%.1f] A: buffer packet [%d] base [%d]\n", currenttime_(), nextseqnum + msg_num - 1, base);
    } else {
        printf("Buffer full! [%.1f] A: buffer packet [%d] base [%d]\n", currenttime_(), nextseqnum, base);
        exit(1);
    }
}


/* called from layer 3, when a packet arrives for layer 4 */
void A_input(struct pkt packet)
{
    /* check ack */
    if (CheckCorrupted(packet)) {
        printf("[%.1f] A: ACK corrupted\n", currenttime_());
        // remove this line: packet_corrupt++;
        return;
    }

    /* 2. check if ack valid */
    int acknum = packet.acknum;
    if (acknum < base || acknum >= nextseqnum) {
        /* deal with ack outside window */
        if (acknum < base && acknum >= base - WINDOWSIZE) {
            printf("[%.1f] A: received ACK [%d]\n", currenttime_(), acknum);
        }
        return;
    }

    /* 3. stop timer */
    int foundIndex = -1;
    for (int i = 0; i < pktnum; i++) {
        int idx = (winfront + i) % WINDOWSIZE;
        if (winbuf[idx].seqnum == acknum && winbuf[idx].acknum == 0) {
            foundIndex = idx;
            break;
        }
    }
    if (foundIndex >= 0) {
        printf("[%.1f] A: received ACK [%d]\n", currenttime_(), acknum);
        stoptime_r(A, acknum);  // stop timer for this packet
        winbuf[foundIndex].acknum = 1;  // mark this packet as acknowledged
    } else {
        return; // if no matching ACK found, just return
    }


    while (pktnum > 0 && winbuf[winfront].acknum == 1 && winbuf[winfront].seqnum == base) {
        base++;
        winfront = (winfront + 1) % WINDOWSIZE;
        pktnum--;
        ack_adv_num++;
    }


    while (msg_num > 0 && nextseqnum < base + WINDOWSIZE) {
        struct msg_ message = buffer[buffront];
        buffront = (buffront + 1) % MAXBUFSIZE;
        msg_num--;

        struct pkt new_pkt;
        new_pkt.seqnum = nextseqnum;
        new_pkt.acknum = NOTUSED; /* use NOTUSED label pkt */
        memcpy(new_pkt.payload, message.data, 20);
        ComputeChecksum(&new_pkt);


        printf("[%.1f] A: send packet [%d] base [%d]\n", currenttime_(), nextseqnum, base);
        tolayer3(A, new_pkt);
        packet_sent++; // update packet sent counter

        winbuf[winrear] = new_pkt;
        winbuf[winrear].acknum = 0; /* initializing */
        winrear = (winrear + 1) % WINDOWSIZE;
        pktnum++;

        struct timer *t = (struct timer*)malloc(sizeof(struct timer));
        t->pkt_seq = nextseqnum;
        starttime_r(t, A, RTT);
        nextseqnum++;
    }
}

/* called when A's any time_r of packet goes off */

/* called when A's any time_r of packet goes off */
void A_packet_time_rinterrupt(struct timer* pkt_timer)
{
    packet_time_out++;

    /* find pkt from buffer */
    int found = 0;
    for(int i=0; i<pktnum; i++) {
        if(winbuf[(winfront+i)%WINDOWSIZE].seqnum == pkt_timer->pkt_seq && winbuf[(winfront+i)%WINDOWSIZE].acknum == 0) {

            printf("[%.1f] A: packet [%d] time_ out, resend packet\n", currenttime_(), pkt_timer->pkt_seq);


            tolayer3(A, winbuf[(winfront+i)%WINDOWSIZE]);
            packet_sent++; // add this line
            packet_resent++;
            found = 1;
            break;
        }
    }

    if(found) {
        starttime_r(pkt_timer, A, RTT);
    }
}

/* called from layer 3, when a packet arrives for layer 4 at B*/
void B_input(struct pkt packet)
{
    static int expected_seq = 0;
    static struct pkt recv_window[8];
    static int recv_marked[8] = {0}; /* 0: not marked, 1: marked */

    if (CheckCorrupted(packet)) {
        if (packet.seqnum == 999999) {
            printf("[%.1f] B: packet [999999] corrupted\n", currenttime_());
        } else {
            printf("[%.1f] B: received corrupted packet\n", currenttime_());
        }
        /* remoce packet_corrupt++，since checked in tolayer3 */
        return;
    }

    /* 2. check */
    if ((packet.seqnum >= expected_seq && packet.seqnum < expected_seq + WINDOWSIZE) ||
        (packet.seqnum < expected_seq && packet.seqnum >= expected_seq - WINDOWSIZE)) {
        int slot = packet.seqnum % WINDOWSIZE;

        /* 3. buffer and sendACK */
        if (recv_marked[slot] == 0 || recv_window[slot].seqnum != packet.seqnum) {
            /* buffer */
            memcpy(&recv_window[slot], &packet, sizeof(struct pkt));
            recv_marked[slot] = 1;

            /* send ack */
            struct pkt ack_pkt;
            ack_pkt.acknum = packet.seqnum;
            ack_pkt.seqnum = NOTUSED;
            memset(ack_pkt.payload, 0, 20); /* Clear ACK packet payload */
            ComputeChecksum(&ack_pkt);
            tolayer3(B, ack_pkt);

            printf("[%.1f] B: packet [%d] received, send ACK [%d]\n", currenttime_(), packet.seqnum, packet.seqnum);

        }


        while (recv_marked[expected_seq % WINDOWSIZE] == 1 &&
               recv_window[expected_seq % WINDOWSIZE].seqnum == expected_seq) {
            tolayer5(B, recv_window[expected_seq % WINDOWSIZE].payload);
            recv_marked[expected_seq % WINDOWSIZE] = 0;
            packet_receieve++;
            expected_seq++;
        }
    } else {

        struct pkt ack_pkt;
        ack_pkt.acknum = packet.seqnum;
        ack_pkt.seqnum = NOTUSED;
        memset(ack_pkt.payload, 0, 20);
        ComputeChecksum(&ack_pkt);
        tolayer3(B, ack_pkt);


        printf("[%.1f] B: packet [%d] received, send ACK [%d]\n", currenttime_(), packet.seqnum, packet.seqnum);
    }
}

/* the following routine will be called once (only) before any other */
/* entity A routines are called. You can use it to do any initialization */
void A_init() {
    base = 0;
    nextseqnum = 0;
    buffront = 0;
    bufrear = 0;
    msg_num = 0;
    winfront = 0;
    winrear = 0;
    pktnum = 0;
    packet_sent = 0;
    packet_correct = 0;
    packet_resent = 0;
    packet_lost = 0;
    packet_corrupt = 0;
    packet_time_out = 0;
    packet_receieve = 0;
}


/* the following routine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
void B_init() {
    // init receiver variables
}

void print_info() {
    printf("***********************************************************************************************************\n");
    printf("                                       Packet Transmission Summary                                         \n");
    printf("***********************************************************************************************************\n");

    /* Statistics calculation - each packet is 32KB, convert to Kb/s */
    float throughput = (packet_receieve * 32.0 * 8) / currenttime_();

    /* 正确传输的包数量应该等于总消息数量，即接收方接收到的包数量 */
    packet_correct = packet_receieve;

    printf("From Sender to Receiver:  \n");
    printf(" total sent pkts:         %d \n", packet_sent);
    printf(" total correct pkts:      %d \n", packet_correct);
    printf(" total resent pkts:       %d \n", packet_resent);
    printf(" total lost pkts:         %d \n", packet_lost);
    printf(" total corrupted pkts:    %d \n", packet_corrupt);
    printf(" the overall throughput is: %.3f Kb/s \n\n", throughput);
}

/***************** SECTION III: NETWORK EMULATION CODE ***********
The code below emulates the layer 3 and below network environment:
  - emulates the tranmission and delivery (possibly with bit-level corruption
    and packet loss) of packets across the layer 3/4 interface
  - handles the starting/stopping of a time_r, and generates time_r
    interrupts (resulting in calling students time_r handler).
  - generates message to be sent (passed from later 5 to 4)

THERE IS NOT REASON THAT ANY STUDENT SHOULD HAVE TO READ OR UNDERSTAND
THE CODE BELOW.  YOU SHOLD NOT TOUCH, OR REFERENCE (in your code) ANY
OF THE DATA STRUCTURES BELOW.  If you're interested in how I designed
the emulator, you're welcome to look at the code - but again,
you defeinitely should not have to modify
******************************************************************/

struct event {
    float evtime_;           /* event time_ */
    int evtype;             /* event type code */
    int eventity;           /* entity where event occurs */
    struct pkt *pktptr;     /* ptr to packet (if any) assoc w/ this event */
    struct event *prev;
    struct event *next;
    struct timer *pkt_timer;  /* record packet seq number and start time */
};
struct event *evlist = NULL;   /* the event list */
void insertevent(struct event *);
/* possible events: */
#define  time_R_INTERRUPT 0
#define  FROM_LAYER5     1
#define  FROM_LAYER3     2

#define   A    0
#define   B    1


int TRACE = -1;             /* for my debugging */
int nsim = 0;              /* number of messages from 5 to 4 so far */
int nsimmax = 0;           /* number of msg_s to generate, then stop */
float time_ = 0.0;
float lossprob;            /* probability that a packet is dropped  */
float corruptprob;         /* probability that one bit is packet is flipped */
float lambda;              /* arrival rate of messages from layer 5 */
int ntolayer3;           /* number sent into layer 3 */
int nlost;               /* number lost in media */
int ncorrupt;              /* number corrupted by media*/

char pattern[40]; /*channel pattern string*/
int npttns = 0;
int cp = -1; /*current pattern*/
char pttnchars[3] = {'o', '-', 'x'};
enum pttns {
    OK = 0, CORRUPT, LOST
};


int main(void) {
    struct event *eventptr;
    struct msg_ msg_2give;
    struct pkt pkt2give;

    int i, j;


    init();
    A_init();
    B_init();

    while (1) {
        eventptr = evlist;            /* get next event to simulate */
        if (eventptr == NULL)
            goto terminate;

        evlist = evlist->next;        /* remove this event from event list */
        if (evlist != NULL)
            evlist->prev = NULL;

        if (TRACE >= 2) {
            printf("\nEVENT time_: %f,", eventptr->evtime_);
            printf("  type: %d", eventptr->evtype);
            if (eventptr->evtype == 0)
                printf(", time_rinterrupt  ");
            else if (eventptr->evtype == 1)
                printf(", fromlayer5 ");
            else
                printf(", fromlayer3 ");
            printf(" entity: %d\n", eventptr->eventity);
        }
        time_ = eventptr->evtime_;        /* update time_ to next event time_ */
        //if (nsim==nsimmax)
        //    break;                        /* all done with simulation */

        if (eventptr->evtype == FROM_LAYER5) {
            generate_next_arrival();   /* set up future arrival */

            /* fill in msg_ to give with string of same letter */
            j = nsim % 26;
            for (i = 0; i < 20; i++)
                msg_2give.data[i] = 97 + j;

            if (TRACE > 2) {
                printf("          MAINLOOP: data given to student: ");
                for (i = 0; i < 20; i++)
                    printf("%c", msg_2give.data[i]);
                printf("\n");
            }
            //nsim++;
            if (eventptr->eventity == A)
                A_output(msg_2give);
            else {}
            //B_output(msg_2give);

        } else if (eventptr->evtype == FROM_LAYER3) {
            pkt2give.seqnum = eventptr->pktptr->seqnum;
            pkt2give.acknum = eventptr->pktptr->acknum;
            pkt2give.checksum = eventptr->pktptr->checksum;
            for (i = 0; i < 20; i++)
                pkt2give.payload[i] = eventptr->pktptr->payload[i];

            if (eventptr->eventity == A)      /* deliver packet by calling */
                A_input(pkt2give);            /* appropriate entity */
            else
                B_input(pkt2give);

            free(eventptr->pktptr);          /* free the memory for packet */
        } else if (eventptr->evtype == time_R_INTERRUPT) {
            if (eventptr->eventity == A){
                A_packet_time_rinterrupt(eventptr->pkt_timer);
            }

            else {}
            //B_time_rinterrupt();
        } else {
            printf("INTERNAL PANIC: unknown event type \n");
        }
        free(eventptr);
    }

    terminate:
    printf("Simulator terminated.\n");
    print_info();
    return 0;
}


void init()                         /* initialize the simulator */
{
    printf("***********************************************************************************************************\n");
    printf("         Lab1: Reliable Data Transfer Protocol-SR          \n");
    printf("***********************************************************************************************************\n");
//    float jimsrand();

    printf("Enter the number of messages to simulate: \n");

    scanf("%d", &nsimmax);
    total_sim_msg = nsimmax * 32;
    printf("Enter time_ between messages from sender's layer5 [ > 0.0]:\n");

    scanf("%f", &lambda);
    printf("Enter channel pattern string\n");

    scanf("%s", pattern);
    npttns = strlen(pattern);

    printf("Enter sender's window size\n");

    scanf("%d", &WINDOWSIZE);
    winbuf = (struct pkt *) malloc(sizeof(struct pkt) * WINDOWSIZE);

    ntolayer3 = 0;
    nlost = 0;
    ncorrupt = 0;

    time_ = 0.0;                    /* initialize time_ to 0.0 */
    generate_next_arrival();     /* initialize event list */

    printf("***********************************************************************************************************\n");
    printf("                                        Packet Transmission Log                                            \n");
    printf("***********************************************************************************************************\n");

}


/********************* EVENT HANDLINE ROUTINES *******/
/*  The next set of routines handle the event list   */
/*****************************************************/

void generate_next_arrival() {
    double x;
    struct event *evptr;
    //double log(), ceil();
    //char *malloc();

    if (nsim >= nsimmax) return;

    if (TRACE > 2)
        printf("          GENERATE NEXT ARRIVAL: creating new arrival\n");

    x = lambda;

    evptr = (struct event *) malloc(sizeof(struct event));
    evptr->evtime_ = time_ + x;
    evptr->evtype = FROM_LAYER5;
    evptr->pkt_timer = NULL;

#ifdef BIDIRECTIONAL
    evptr->eventity = B;
#else
    evptr->eventity = A;
#endif
    insertevent(evptr);
    nsim++;
}


void insertevent(struct event *p)
{
    struct event *q, *qold;

    if (TRACE > 2) {
        printf("            INSERTEVENT: time_ is %lf\n", time_);
        printf("            INSERTEVENT: future time_ will be %lf\n", p->evtime_);
    }
    q = evlist;     /* q points to front of list in which p struct inserted */
    if (q == NULL) {   /* list is empty */
        evlist = p;
        p->next = NULL;
        p->prev = NULL;
    } else {
        for (qold = q; q != NULL && p->evtime_ >= q->evtime_; q = q->next)
            qold = q;
        if (q == NULL) {   /* end of list */
            qold->next = p;
            p->prev = qold;
            p->next = NULL;
        } else if (q == evlist) { /* front of list */
            p->next = evlist;
            p->prev = NULL;
            p->next->prev = p;
            evlist = p;
        } else {     /* middle of list */
            p->next = q;
            p->prev = q->prev;
            q->prev->next = p;
            q->prev = p;
        }
    }
}

void printevlist() {
    struct event *q;

    printf("--------------\nEvent List Follows:\n");
    for (q = evlist; q != NULL; q = q->next) {
        printf("Event time_: %f, type: %d entity: %d, seq %d\n", q->evtime_, q->evtype, q->eventity, q->pkt_timer->pkt_seq);
    }
    printf("--------------\n");
}



/********************** SECTION IV: Student-callable ROUTINES ***********************/

/* get the current time_ of the system*/
float currenttime_() {
    return time_;
}


/* called by students routine to cancel a previously-started time_r */
void stoptime_r(int AorB, int pkt_del)
{
    struct event *q;
    if (TRACE > 2)
        printf("STOP time_R of packet [%d]: stopping time_r at %f\n",pkt_del, time_);
    for (q = evlist; q != NULL; q = q->next)
        if (q->evtype == time_R_INTERRUPT && q->pkt_timer->pkt_seq == pkt_del && q->eventity == AorB) {
            /* remove this event */
            if (q->next == NULL && q->prev == NULL)
                evlist = NULL;         /* remove first and only event on list */
            else if (q->next == NULL) /* end of list - there is one in front */
                q->prev->next = NULL;
            else if (q == evlist) { /* front of list - there must be event after */
                q->next->prev = NULL;
                evlist = q->next;
            } else {     /* middle of list */
                q->next->prev = q->prev;
                q->prev->next = q->next;
            }
            free(q);
            return;
        }
    printf("Warning: unable to cancel your time_r. It wasn't running.\n");
}

void starttime_r(struct timer* packet_timer, int AorB, float increment)
{
    if (TRACE > 2)
        printf("          START time_R: starting time_r at %f\n", time_);
    /* be nice: check to see if time_r is already started, if so, then  warn */
/* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next)  */
//    for (q = evlist; q != NULL; q = q->next)
//        if ((q->evtype == time_R_INTERRUPT && q->eventity == AorB)) {
//            printf("Warning: attempt to start a time_r that is already started\n");
//            return 0;
//        }

/* create future event for when time_r goes off */
    struct event *evptr = (struct event *) malloc(sizeof(struct event));
    evptr->evtime_ = time_ + increment;
    evptr->evtype = time_R_INTERRUPT;
    evptr->pkt_timer = (struct timer *) malloc(sizeof(struct timer));
    evptr->pkt_timer->start_time = time_;
    evptr->pkt_timer->pkt_seq = packet_timer->pkt_seq;
//    printf("        START time_R: starting packet [%d] time_r at %f\n", evptr->pkt_timer->pkt_pos, time_);

    evptr->eventity = AorB;
    insertevent(evptr);
}


/************************** TOLAYER3 ***************/
void tolayer3(int AorB, struct pkt packet)
{
    struct pkt *mypktptr;
    struct event *evptr;
    //char *malloc();
    //float jimsrand();
    int i;

    cp++;

    ntolayer3++;

    /* simulate losses: */
    if (pattern[cp % npttns] == pttnchars[LOST]) {
        nlost++;
        packet_lost++;
        if (TRACE > 0)
            printf("          TOLAYER3: packet being lost\n");
        return;
    }

/* make a copy of the packet student just gave me since he/she may decide */
/* to do something with the packet after we return back to him/her */
    mypktptr = (struct pkt *) malloc(sizeof(struct pkt));
    mypktptr->seqnum = packet.seqnum;
    mypktptr->acknum = packet.acknum;
    mypktptr->checksum = packet.checksum;
    for (i = 0; i < 20; i++)
        mypktptr->payload[i] = packet.payload[i];
    if (TRACE > 2) {
        printf("          TOLAYER3: seq: %d, ack %d, check: %d ", mypktptr->seqnum,
               mypktptr->acknum, mypktptr->checksum);
        for (i = 0; i < 20; i++)
            printf("%c", mypktptr->payload[i]);
        printf("\n");
    }

    /* create future event for arrival of packet at the other side */
    evptr = (struct event *) malloc(sizeof(struct event));
    evptr->evtype = FROM_LAYER3;   /* packet will pop out from layer3 */
    evptr->eventity = (AorB + 1) % 2; /* event occurs at other entity */
    evptr->pktptr = mypktptr;       /* save ptr to my copy of packet */
    evptr->evtime_ = time_ + RTT / 2 - 1; /* hard code the delay on channel */
    evptr->pkt_timer = (struct timer *) malloc(sizeof(struct timer));
    evptr->pkt_timer->pkt_seq = packet.acknum;


    /* simulate corruption: */
    if (pattern[cp % npttns] == pttnchars[CORRUPT]) {
        ncorrupt++;
        packet_corrupt++;
        mypktptr->payload[0] = 'Z';   /* corrupt payload */
        mypktptr->seqnum = 999999;
        mypktptr->acknum = 999999;
        if (TRACE > 0)
            printf("          TOLAYER3: packet being corrupted\n");
    }

    if (TRACE > 2)
        printf("          TOLAYER3: scheduling arrival on other side\n");
    insertevent(evptr);
}

void tolayer5(int AorB, char *datasent)
{
    int i;
    if (TRACE > 2) {
        printf("        [%d] TOLAYER5: data received: ", AorB);
        for (i = 0; i < 20; i++)
            printf("%c", datasent[i]);
        printf("\n");
    }
}
