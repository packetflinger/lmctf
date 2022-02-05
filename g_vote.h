/**
 * Voting stuff
 */

#pragma once

#include "g_local.h"

void Vote_Menu (edict_t *ent);
void Vote_YES (edict_t *ent);
void Vote_NO (edict_t *ent);
void Check_Vote(void);

// vote->value flags
#define CTF_VOTE_STARTED        1
//assorted client flags, in the true meaning
#define CTF_EXTRAFLAGS_VOTE_YES 64
#define CTF_EXTRAFLAGS_VOTE_NO  128

//votetypes
#define CTF_VOTETYPE_SKIP       1
#define CTF_VOTETYPE_GOTOMAP    2
#define CTF_VOTETYPE_REFPLAYER  4

#define PERCENT_MAJORITY_REQUIRED   75 //requires a 75% majority for the vote to pass

extern int VoteStarted;
extern float VoteTime;
extern int VoteType;

// max length of a string used in a vote proposal (like map name)
#define MAX_VOTE_STRLEN     32
#define VOTE_YES            1
#define VOTE_NO            -1


#define VOTE_KICK           (1<<0)
#define VOTE_MAP            (1<<1)
#define VOTE_RESET          (1<<2)
#define VOTE_SWITCH         (1<<3)
#define VOTE_TIMELIMIT      (1<<4)
#define VOTE_MODE           (1<<5)
#define VOTE_RESTART        (1<<6)


typedef struct {
    const char  *name;
    int         bit;
    qboolean    (*func)(edict_t *);
} vote_proposal_t;


/**s
 * The main structure to hold voting data
 */
typedef struct {
    edict_t     *ent;                   // the vote entity
    qboolean    active;                 // are we currently voting?
    uint32_t    votetime;               // frame number this vote was started
    uint32_t    lastvote;               // frame of last vote, for throttling
    uint8_t     proposal;               // what are we voting on?
    uint32_t    intvalue;               // number values
    char        strvalue[MAX_VOTE_STRLEN];   // string values
    char        display[0xff];          // for displaying the current proposals
    int8_t      results;                // + is yes, - is no
    uint8_t     votes;                  // how many players have voted
    edict_t     *initiator;             // who called the vote?
    edict_t     *victim;                // for kick/mute, who is the target?
} vote_t;


/**
 * To store the current result
 */
typedef struct {
    uint8_t total;              // total votes cast
    uint8_t yes;                // how many voted yes
    uint8_t no;                 // how many voted no
    uint8_t unvoted;            // how many didn't vote
    float   pct_raw;            // % yes of all team players
    float   pct_votes;          // % yes of just ppl who voted
} vote_result_t;


/**
 * Each client has this structure
 */
typedef struct {
    uint32_t    lastcall;       // the last frame this user called a vote
    uint32_t    callcount;      // how many votes this user has called
    int8_t      vote;           // 0 = not voted, + is yes, - is no
} client_vote_t;


void VoteThink(edict_t *ent);
void VoteStart(void);
void VoteReset(void);
void Cmd_Vote_f(edict_t *ent);
void VoteCast(edict_t *ent, int8_t v);
void VoteBuildProposalString(char *output);
void VoteFinished(void);
void VoteCounts(vote_result_t *results);
void VoteSuccess(void);
void VoteFail(void);
void VoteUsage(edict_t *ent);
