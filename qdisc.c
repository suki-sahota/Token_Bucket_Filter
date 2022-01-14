/*
 * Author: Suki Sahota
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <limits.h>

#include "my_math.h"

#include "my_list.h"

/* Constants */
#define MIC_TO_MIL  1000 
#define MIC_TO_SEC  1000000
#define MIL_TO_MIC  1000UL
#define MIL_TO_SEC  1000
#define SEC_TO_MIC  1000000UL 
#define SEC_TO_MIL  1000
#define MAX_TIME  10000UL /* 10,000 milliseconds */
#define ASCII_TAB  9
#define ASCII_SPACE  32
#define ASCII_ZERO  48
#define ASCII_NINE  57

/* Packet Data Structure */
typedef struct tagPacket { 
    int num;
    int inter_arrival_time; /* milliseconds */
    int tokens_required;
    int service_time_requested; /* milliseconds */
    unsigned long arrival_time; /* microseconds */
    unsigned long enter_time; /* microseconds */
    unsigned long leave_time; /* microseconds */
} Packet;

/* ----------------------- Global Variables ----------------------- */
pthread_mutex_t mut;
pthread_cond_t cv;
pthread_t packet_thread, token_thread, server_one_thread, server_two_thread;
pthread_t signal_thread;
sigset_t set;

/* Shared Variables */
MyList Q1, Q2;
int token_bucket;

/* Commandline options */
long n;
double lambda;
double mu;
double rate;
long B;
long P;
char buf[1026];

/* Rates converted to times in milliseconds */
unsigned long l; /* inter-arrival time */
unsigned long r; /* inter-token-arrival time */
unsigned long m; /* service time */

unsigned long current_time;
unsigned long emulation_begin, emulation_end;
int all_packets_arrived; /* TRUE = packet thread termination */
int time_to_quit; /* TRUE = signal <Ctrl-c> pressed */
int completed_packets, dropped_packets, removed_packets;
int accepted_tokens, dropped_tokens;

/* microseconds for precision */
int avg_inter_arrival_time;
int avg_service_time;
unsigned long total_Q1_time, total_Q2_time, total_S1_time, total_S2_time;

double avg_x, avg_x_sqr; /* milliseconds */

/* ----------------------- Utility Functions ----------------------- */

void MalformedCommandline(int flag) {
    switch(flag) {
        case 1: /* lambda error */
            fprintf(stderr, "malformed commandline - argument missing for lambda\n");
            break;
        case 2: /* mu error */
            fprintf(stderr, "malformed commandline - argument missing for mu\n");
            break;
        case 3: /* r error */
            fprintf(stderr, "malformed commandline - argument missing for r\n");
            break;
        case 4: /* B error */
            fprintf(stderr, "malformed commandline - argument missing for B\n");
            break;
        case 5: /* P error */
            fprintf(stderr, "malformed commandline - argument missing for P\n");
            break;
        case 6: /* n error */
            fprintf(stderr, "malformed commandline - argument missing for n\n");
            break;
        case 7: /* t error */
            fprintf(stderr, "malformed commandline - argument missing for t\n");
            break;
        default: /* unknown flag used */
            fprintf(stderr, "malformed commandline - unknown flag used\n");
            break;
    }
    fprintf(stderr, 
            "usage: qdisc [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\n");
    exit(1);
}

void Init() {
    /* Default values */
    n = 20;
    lambda = 1.0;
    mu = 0.35;
    rate = 1.5;
    B = 10;
    P = 3;

    mut = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
    cv = (pthread_cond_t) PTHREAD_COND_INITIALIZER;

    MyListInit(&Q1);
    MyListInit(&Q2);
    token_bucket = 0;

    current_time = 0UL;
    emulation_begin = emulation_end = 0UL;
    all_packets_arrived = FALSE;
    time_to_quit = FALSE;
    completed_packets = dropped_packets = removed_packets = 0;
    accepted_tokens = dropped_tokens = 0;

    avg_inter_arrival_time = 0;
    avg_service_time = 0;
    total_Q1_time = total_Q2_time = total_S1_time = total_S2_time = 0UL;
    avg_x = 0UL;
    avg_x_sqr = 0UL;
}

static
void ProcessOptions(int argc, char *argv[]) {
    for (--argc, ++argv; argc > 0; --argc, ++argv) {
        if (*argv[0] == '-') {
            if (strcmp(*argv, "-lambda") == 0) {
                if (argc == 1 || *(++argv)[0] == '-') {
                    MalformedCommandline(1);
                }
                lambda = strtod(*argv, NULL);
                if (lambda <= 0) {
                    fprintf(stderr,
                            "error in the input - lambda is not positive\n");
                }
            } else if (strcmp(*argv, "-mu") == 0) {
                if (argc == 1 || *(++argv)[0] == '-') {
                    MalformedCommandline(2);
                }
                mu = strtod(*argv, NULL);
                if (mu <= 0) {
                    fprintf(stderr,
                            "error in the input - mu is not positive\n");
                }
            } else if (strcmp(*argv, "-r") == 0) {
                if (argc == 1 || *(++argv)[0] == '-') {
                    MalformedCommandline(3);
                }
                rate = strtod(*argv, NULL);
                if (rate <= 0) {
                    fprintf(stderr,
                            "error in the input - rate is not positive\n");
                }
            } else if (strcmp(*argv, "-B") == 0) {
                if (argc == 1 || *(++argv)[0] == '-') {
                    MalformedCommandline(4);
                }
                B = strtol(*argv, 0, 10);
                if (B > INT_MAX) {
                    fprintf(stderr, "error in the input - B is too large\n");
                } else if (B <= 0) {
                    fprintf(stderr, "error in the input - B is not positive\n");
                }
            } else if (strcmp(*argv, "-P") == 0) {
                if (argc == 1 || *(++argv)[0] == '-') {
                    MalformedCommandline(5);
                }
                P = strtol(*argv, 0, 10);
                if (P > INT_MAX) {
                    fprintf(stderr, "error in the input - P is too large\n");
                } else if (P <= 0) {
                    fprintf(stderr, "error in the input - P is not positive\n");
                }
            } else if (strcmp(*argv, "-n") == 0) {
                if (argc == 1 || *(++argv)[0] == '-') {
                    MalformedCommandline(6);
                }
                n = strtol(*argv, 0, 10);
                if (n > INT_MAX) {
                    fprintf(stderr, "error in the input - n is too large\n");
                } else if (n <= 0) {
                    fprintf(stderr, "error in the input - n is not positive\n");
                }
            } else if (strcmp(*argv, "-t") == 0) {
                if (argc == 1 || *(++argv)[0] == '-') {
                    MalformedCommandline(7);
                }
                strcpy(buf, *argv);
            } else {
                MalformedCommandline(0); /* Unknown flag used */
            }
        --argc;
        } else {
            MalformedCommandline(0); /* Unknown flag used */
        }
    }
}

void LineTooLong(int line_num) {
    if (strlen(buf) > 1024) { 
        fprintf(stderr, "error in the input - line %i is too long\n", line_num);
        exit(1);
    }
}

void NotValidFile(int line_num) {
    fprintf(stderr, 
            "error in the input - line %i not in tsfile format\n", line_num);
    exit(1);
}

int IsDigit(char *chr_ptr) {
    if (*chr_ptr < ASCII_ZERO || *chr_ptr > ASCII_NINE) { return FALSE; }
    else { return TRUE; }
}

int IsSpaceOrTab(char *chr_ptr) {
    if (*chr_ptr != ASCII_SPACE && *chr_ptr != ASCII_TAB) { return FALSE; }
    else { return TRUE; }
}

void PrintParams() {
    fprintf(stdout, "Emulation Parameters:\n");
    fprintf(stdout, "\tnumber to arrive = %ld\n", n);
    if (!*buf) { fprintf(stdout, "\tlambda = %.6g\n", lambda); }
    if (!*buf) { fprintf(stdout, "\tmu = %.6g\n", mu); }
    fprintf(stdout, "\tr = %.6g\n", rate);
    fprintf(stdout, "\tB = %ld\n", B);
    if (!*buf) { fprintf(stdout, "\tP = %ld\n", P); }
    if (*buf) { fprintf(stdout, "\ttsfile = %s\n", buf); }
    fprintf(stdout, "\n");
}

void ConvertParams() {
    lambda = SEC_TO_MIL / lambda;
    lambda = round(lambda);
    l = (unsigned long) lambda;
    if (l > MAX_TIME) { l = MAX_TIME; }

    mu = SEC_TO_MIL / mu;
    mu = round(mu);
    m = (unsigned long) mu;
    if (m > MAX_TIME) { m = MAX_TIME; }

    rate = SEC_TO_MIL / rate;
    rate = round(rate);
    r = (unsigned long) rate;
    if (r > MAX_TIME) { r = MAX_TIME; }
}

unsigned long GetTime(struct timeval *tv) {
    gettimeofday(tv, NULL);
    current_time = (tv->tv_sec * SEC_TO_MIC) + tv->tv_usec;
    return current_time;
}

void PrintTime(unsigned long time) {
    time -= emulation_begin;
    int milliseconds = (int) (time / MIC_TO_MIL);
    int milliseconds_decimal = (int) (time % MIC_TO_MIL);
    fprintf(stdout, "%08d", milliseconds);
    fprintf(stdout, ".%03dms: ", milliseconds_decimal);
}

void PrintEmulationBegins() {
    struct timeval tv;
    emulation_begin = GetTime(&tv);

    PrintTime(current_time);
    fprintf(stdout, "emulation begins\n");
}

void ReadLine(int *arr_t, int *tok, int *ser_t, int line_num) {
    if (sscanf(buf, "%d %d %d", arr_t, tok, ser_t) != 3) {
        NotValidFile(line_num);
    }
}

void SigQuit() {
    while (!MyListEmpty(&Q1)) {
        MyListElem *elem = MyListFirst(&Q1);
        Packet *p = elem->obj;
        MyListUnlink(&Q1, elem);
        struct timeval tv;
        PrintTime(GetTime(&tv));
        fprintf(stdout, "p%i removed from Q1\n", p->num);
        free(p);
        ++removed_packets;
    }
    while (!MyListEmpty(&Q2)) {
        MyListElem *elem = MyListFirst(&Q2);
        Packet *p = elem->obj;
        MyListUnlink(&Q2, elem);
        struct timeval tv;
        PrintTime(GetTime(&tv));
        fprintf(stdout, "p%i removed from Q2\n", p->num);
        free(p);
        ++removed_packets;
    }
}

void PacketArrives(Packet *packet, unsigned long *last_arr_time) {
    struct timeval tv;
    packet->arrival_time = GetTime(&tv);

    int diff = (int) (current_time - *last_arr_time);
    int milliseconds = diff / MIC_TO_MIL;
    int milliseconds_decimal = diff % MIC_TO_MIL;

    packet->inter_arrival_time = diff; /* Measured inter-arrival time */
    *last_arr_time = current_time;

    PrintTime(current_time);
    fprintf(stdout, 
            "p%i arrives, needs %i tokens, inter-arrival time = ",
            packet->num, packet->tokens_required);
    fprintf(stdout, "%d", milliseconds);
    fprintf(stdout, ".%03dms", milliseconds_decimal);
}

void PacketEntersQ1(Packet *p) {
    struct timeval tv;
    p->enter_time = GetTime(&tv);
    PrintTime(current_time);
    fprintf(stdout, "p%i enters Q1\n", p->num);
}

void PacketLeavesQ1(Packet *p) {
    struct timeval tv;
    p->leave_time = GetTime(&tv);
    
    int diff = (int) (current_time - p->enter_time); /* Time in Q1 */
    int milliseconds = diff / MIC_TO_MIL;
    int milliseconds_decimal = diff % MIC_TO_MIL;

    total_Q1_time += diff; /* For running averages */

    PrintTime(current_time);
    fprintf(stdout, "p%i leaves Q1, time in Q1 = ", p->num);
    fprintf(stdout, "%d", milliseconds);
    fprintf(stdout, ".%03dms", milliseconds_decimal);
    fprintf(stdout, ", token bucket now has %i token", token_bucket);
    if (token_bucket > 1) { fprintf(stdout, "s"); }
    fprintf(stdout, "\n");
}

void PacketEntersQ2(Packet *p) {
    struct timeval tv;
    p->enter_time = GetTime(&tv);
    PrintTime(current_time);
    fprintf(stdout, "p%i enters Q2\n", p->num);
}

void CheckQ1() {
    Packet *packet = MyListFirst(&Q1)->obj;
    if (token_bucket >= packet->tokens_required) {
        token_bucket -= packet->tokens_required;
        MyListUnlink(&Q1, MyListFirst(&Q1));
        PacketLeavesQ1(packet);
        MyListAppend(&Q2, packet);
        PacketEntersQ2(packet);
        pthread_cond_broadcast(&cv);
    }
}

void TokenArrives(int t_num, unsigned long *last_tok_time) {
    struct timeval tv;
    *last_tok_time = GetTime(&tv);

    PrintTime(current_time);
    fprintf(stdout, "token t%i arrives, ", t_num);
    if (token_bucket < B) {
        ++token_bucket;
        ++accepted_tokens;
        if (token_bucket == 1) {
            fprintf(stdout, "token bucket now has 1 token\n");
        } else {
            fprintf(stdout, "token bucket now has %i tokens\n", token_bucket);
        }
    } else {
        ++dropped_tokens;
        fprintf(stdout, "dropped\n");
    }
}

void PacketLeavesQ2(Packet *p) {
    struct timeval tv;
    p->leave_time = GetTime(&tv);
    
    int diff = (int) (current_time - p->enter_time); /* Time in Q2 */
    int milliseconds = diff / MIC_TO_MIL;
    int milliseconds_decimal = diff % MIC_TO_MIL;

    total_Q2_time += diff; /* For running averages */

    PrintTime(current_time);
    fprintf(stdout, "p%i leaves Q2, time in Q2 = ", p->num);
    fprintf(stdout, "%d", milliseconds);
    fprintf(stdout, ".%03dms\n", milliseconds_decimal);
}

Packet *CheckQ2() {
    Packet *packet = MyListFirst(&Q2)->obj;
    MyListUnlink(&Q2, MyListFirst(&Q2));
    PacketLeavesQ2(packet);
    return packet;
}

void BeginService(Packet *packet, int s_num) {
    struct timeval tv;
    packet->enter_time = GetTime(&tv);

    PrintTime(current_time);
    fprintf(stdout,
            "p%i begins service at S%i, requesting %ims of service\n",
            packet->num, s_num, packet->service_time_requested);
}

void DepartService(Packet *p, int s_num) {
    struct timeval tv;
    p->leave_time = GetTime(&tv);
    
    /* Measured service time (microseconds) */
    int diff = (int) (current_time - p->enter_time); 
    int milliseconds = diff / MIC_TO_MIL;
    int milliseconds_decimal = diff % MIC_TO_MIL;

    /* Service time running averages */
    if (s_num == 1) { total_S1_time += diff; }
    else { total_S2_time += diff; }
    avg_service_time = (avg_service_time * (completed_packets) +
                        diff) / (completed_packets + 1);

    unsigned long time_in_system = current_time - p->arrival_time;
    int ms = time_in_system / MIC_TO_MIL;
    int ms_decimal = time_in_system % MIC_TO_MIL;

    /* Time in system running averages */
    avg_x = (avg_x * (completed_packets) +
             ((double) time_in_system / MIC_TO_MIL))
             / (completed_packets + 1);
    avg_x_sqr = (avg_x_sqr * (completed_packets) +
                 pow(time_in_system / (double) MIC_TO_MIL, 2))
                 / (completed_packets + 1);
    ++completed_packets;

    PrintTime(current_time);
    fprintf(stdout, "p%i departs from S%i, service time = ", p->num, s_num);
    fprintf(stdout, "%d", milliseconds);
    fprintf(stdout, ".%03dms", milliseconds_decimal);
    fprintf(stdout, ", time in system = ");
    fprintf(stdout, "%d", ms);
    fprintf(stdout, ".%03dms\n", ms_decimal);
}

void PrintEmulationEnds() {
    struct timeval tv;
    emulation_end = GetTime(&tv);

    PrintTime(current_time);
    fprintf(stdout, "emulation ends\n");
    fprintf(stdout, "\n");
}

void PrintStatistics() {
    fprintf(stdout, "Statistics:\n");
    fprintf(stdout, "\n");

    if (avg_inter_arrival_time == 0) {
        fprintf(stdout,
                "\taverage packet inter-arrival time = \"N/A\" no packet arrived\n");
    } else {
        fprintf(stdout, "\taverage packet inter-arrival time = %.6g\n", 
                (double) avg_inter_arrival_time / MIC_TO_SEC);
    }
    if (completed_packets == 0) {
        fprintf(stdout,
                "\taverage packet service time = \"N/A\" no packet served\n");
    } else {
        fprintf(stdout, "\taverage packet service time = %.6g\n", 
                (double) avg_service_time / MIC_TO_SEC);
    }
    fprintf(stdout, "\n");

    fprintf(stdout, "\taverage number of packets in Q1 = %.6g\n", 
            (double) total_Q1_time 
            / (emulation_end - emulation_begin));
    fprintf(stdout, "\taverage number of packets in Q2 = %.6g\n", 
            (double) total_Q2_time 
            / (emulation_end - emulation_begin));
    fprintf(stdout, "\taverage number of packets in S1 = %.6g\n", 
            (double) total_S1_time 
            / (emulation_end - emulation_begin));
    fprintf(stdout, "\taverage number of packets in S2 = %.6g\n", 
            (double) total_S2_time 
            / (emulation_end - emulation_begin));
    fprintf(stdout, "\n");

    if (completed_packets == 0) {
        fprintf(stdout,
                "\taverage time a packet spent in system = \"N/A\" no packet served\n");
        fprintf(stdout,
                "\tstandard deviation for time spent in system = \"N/A\" no packet served\n");
    } else {
        fprintf(stdout, "\taverage time a packet spent in system = %.6g\n", 
                avg_x / MIL_TO_SEC);
        fprintf(stdout, "\tstandard deviation for time spent in system = %.6g\n", 
                sqrt((avg_x_sqr) - pow(avg_x, 2)) / MIL_TO_SEC);
    }
    fprintf(stdout, "\n");

    if (dropped_tokens + accepted_tokens == 0) {
        fprintf(stdout, "\tpacket drop probability = \"N/A\" no token arrived\n");
    } else {
        fprintf(stdout, "\ttoken drop probability = %.6g\n", 
                (double) dropped_tokens / (dropped_tokens + accepted_tokens));
    }
    if (dropped_packets + completed_packets + removed_packets == 0) {
        fprintf(stdout, "\tpacket drop probability = \"N/A\" no packet arrived\n");
    } else {
        fprintf(stdout, "\tpacket drop probability = %.6g\n", 
                (double) dropped_packets
                / (dropped_packets + completed_packets + removed_packets));
    }
}

/* ----------------------- First Procedures ----------------------- */

void *monitor(void *arg) {
    int sig = 0;

    for(;;) {
        sigwait(&set, &sig);
        pthread_mutex_lock(&mut);
        time_to_quit = TRUE;
        pthread_cancel(packet_thread);
        pthread_cancel(token_thread);
        struct timeval tv;
        PrintTime(GetTime(&tv));
        fprintf(stdout,
                "SIGINT caught, no new packets or tokens will be allowed\n");
        pthread_cond_broadcast(&cv);
        pthread_mutex_unlock(&mut);
        break;
    }
    return (void *) 0;
}

void *packet_thread_func(void *arg) {
    int oldstate;
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);

    FILE *fp = (FILE *) arg;
    int p_num = 0; /* Variable to count number of packets */
    unsigned long last_arrival_time = emulation_begin;
    
    for (; n > 0; --n) {
        ++p_num;
        Packet *packet = (Packet *) malloc(sizeof(Packet));
        packet->num = p_num;

        if (!*buf) { /* deterministic mode */
            packet->inter_arrival_time = l;
            packet->tokens_required = P;
            packet->service_time_requested = m;
        } else {          /* trace-driven mode */
            if (fgets(buf, sizeof(buf), fp) != NULL) {
                LineTooLong(packet->num + 1); /* Checks if line is too long */
                ReadLine(&(packet->inter_arrival_time),
                         &(packet->tokens_required),
                         &(packet->service_time_requested),
                         packet->num + 1);
            } else { 
                fprintf(stderr, "error in the input - reached EOF earlier than expected\n");
                exit(1);
            }
        }

        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &oldstate);
        struct timeval tv;
        unsigned long curr_time = GetTime(&tv);
        /* Simulate passage of time with usleep() */
        if (last_arrival_time + (packet->inter_arrival_time * MIL_TO_MIC) 
            > curr_time)
        {
            usleep(last_arrival_time + (packet->inter_arrival_time * MIL_TO_MIC)
                   - curr_time);
        }
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);

        pthread_mutex_lock(&mut);
        if (time_to_quit) {
            free(packet);
            pthread_mutex_unlock(&mut);
            return (void *) 1;
        }
        PacketArrives(packet, &last_arrival_time);
        

        avg_inter_arrival_time = (avg_inter_arrival_time * (p_num - 1) + 
                                  packet->inter_arrival_time) / (p_num);

        if (packet->tokens_required > B) {
            ++dropped_packets;
            fprintf(stdout, ", dropped\n");
            free(packet);
        } else {
            fprintf(stdout, "\n");
            MyListAppend(&Q1, packet);
            PacketEntersQ1(packet);
            if (MyListLength(&Q1) == 1) {
                CheckQ1();
            }
        }
        pthread_mutex_unlock(&mut);
    }
    all_packets_arrived = TRUE;
    pthread_cond_broadcast(&cv);
    return (void *) 2;
}

void *token_thread_func(void *arg) {
    int oldstate;
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);

    int t_num = 0; /* Variable to count number of tokens */
    unsigned long last_token_time = emulation_begin;

    while (!all_packets_arrived || !MyListEmpty(&Q1)) {
        ++t_num;

        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &oldstate);
        struct timeval tv;
        unsigned long curr_time = GetTime(&tv);
        /* Simulate passage of time with usleep() */
        if (last_token_time + (r * MIL_TO_MIC) > curr_time) {
            usleep(last_token_time + (r * MIL_TO_MIC) - curr_time);
        }
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);

        pthread_mutex_lock(&mut);
        if (time_to_quit) {
            pthread_mutex_unlock(&mut);
            return (void *) 1;
        }
        
        if (all_packets_arrived && MyListEmpty(&Q1)) {
            pthread_mutex_unlock(&mut);
            break;
        }

        TokenArrives(t_num, &last_token_time);

        if (!MyListEmpty(&Q1)) {
            CheckQ1();
        }
        pthread_mutex_unlock(&mut);
    }
    pthread_cond_broadcast(&cv);
    return (void *) 2;
}

void *server_thread_func(void *arg) {
    int *s_num = (int *) arg;

    for (;;) {
        pthread_mutex_lock(&mut);

        while (!time_to_quit && MyListEmpty(&Q2) &&
              (!MyListEmpty(&Q1) || !all_packets_arrived))
        {
            pthread_cond_wait(&cv, &mut);
        }

        if (time_to_quit) { /* Signal caught; time to terminate program */
            pthread_cond_broadcast(&cv);
            SigQuit();
            pthread_mutex_unlock(&mut);
            return (void *) 1;
        } else if (all_packets_arrived && MyListEmpty(&Q1) && 
                   MyListEmpty(&Q2))
        { /* No more packets to service; time to terminate program */
            pthread_cond_broadcast(&cv);
            pthread_mutex_unlock(&mut);
            return (void *) 2;
        } else {
            if (!MyListEmpty(&Q2)) {
                Packet *p = CheckQ2();
                BeginService(p, *s_num);

                struct timeval tv;
                unsigned long curr_time = GetTime(&tv);
                /* Simulate passage of time with usleep() */
                if (p->enter_time + (p->service_time_requested * MIL_TO_MIC) 
                    > curr_time)
                {
                    pthread_mutex_unlock(&mut);
                    usleep(p->enter_time + (p->service_time_requested * MIL_TO_MIC)
                           - curr_time);
                    pthread_mutex_lock(&mut);
                }

                DepartService(p, *s_num);
                pthread_mutex_unlock(&mut);
                free(p);
            }
        }
    }
    return (void *) 2;
}

/* ----------------------- Process() ----------------------- */

void Process() {
    FILE *fp = NULL;
    if (*buf) { /* User specified a tsfile in commandline */
        fp = fopen(buf, "r");
        if (fp == NULL) {
            perror(buf); /* OS tells us whether permission denied, etc. */
            exit(1);
        }

        char temp_buf[1026];
        if (fgets(temp_buf, sizeof(temp_buf), fp) != NULL) {
            LineTooLong(1); /* Checks if line is too long */
            char tmp_buf[1026]; /* used to grab unnecessary info on line 1 */
            if (sscanf(temp_buf, "%ld %s", &n, tmp_buf) != 1) {
                fprintf(stderr, "error in the input - line 1 not just a number\n");
                exit(1);
            }
        } else {
            fprintf(stderr, "error in the input - empty file\n");
            exit(1);
        }
    }

    PrintParams();
    ConvertParams();
    PrintEmulationBegins();
    void *result = (void *) 0; /* To capture child thread return code */
    int server_one = 1;
    int server_two = 2;

    /* Create four threads */
    pthread_create(&packet_thread, NULL, packet_thread_func, fp);
    pthread_create(&token_thread, NULL, token_thread_func, 0);
    pthread_create(&server_one_thread, NULL, server_thread_func, &server_one);
    pthread_create(&server_two_thread, NULL, server_thread_func, &server_two);
    
    /* Join four threads */
    pthread_join(packet_thread, (void **) &result);
    pthread_join(token_thread, (void **) &result);
    pthread_join(server_one_thread, (void **) &result);
    pthread_join(server_two_thread, (void **) &result);
    
    if (fp != NULL) { fclose(fp); }

    PrintEmulationEnds();
    PrintStatistics();
}

/* ----------------------- main() ----------------------- */

int main(int argc, char *argv[]) {
    Init();
    ProcessOptions(argc, argv);

    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigprocmask(SIG_BLOCK, &set, 0); /* Main thread blocks SIGINT */
    pthread_create(&signal_thread, NULL, monitor, 0);

    Process();
    return(0);
}
