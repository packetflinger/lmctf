/**
 * Voting stuff
 */
#pragma once

#include "g_local.h"

// max length of a string used in a vote proposal (like map name)
#define MAX_VOTE_STRLEN     32


#define VOTE_YES            1
#define VOTE_NO            -1


// proposal types
#define VOTE_REF            (1<<0)
#define VOTE_KICK           (1<<1)
#define VOTE_MAP            (1<<2)
#define VOTE_RESET          (1<<3)
#define VOTE_SWITCH         (1<<4)
#define VOTE_TIMELIMIT      (1<<5)
#define VOTE_MODE           (1<<6)
#define VOTE_RESTART        (1<<7)


typedef struct {
    const char  *name;
    int         bit;
    qboolean    (*func)(edict_t *);
} vote_proposal_t;


/**s
 * The main structure to hold voting data
 */
typedef struct {
    edict_t     *ent;               // the vote entity
    qboolean    active;             // are we currently voting?
    uint32_t    votetime;           // frame number this vote was started
    uint32_t    lastvote;           // frame of last vote, for throttling
    uint8_t     proposal;           // what are we voting on?
    uint32_t    intvalue;           // number values
    char        strvalue[MAX_VOTE_STRLEN];   // string values
    char        display[0xff];      // for displaying the current proposals
    int8_t      results;            // + is yes, - is no
    uint8_t     votes;              // how many players have voted
    edict_t     *initiator;         // who called the vote?
    edict_t     *victim;            // for kick/mute, who is the target?
} vote_t;


/**
 * To store the current result
 */
typedef struct {
    uint8_t     total;              // total votes cast
    uint8_t     yes;                // how many voted yes
    uint8_t     no;                 // how many voted no
    uint8_t     unvoted;            // how many didn't vote
    float       pct_raw;            // % yes of all team players
    float       pct_votes;          // % yes of just ppl who voted
} vote_result_t;


/**
 * Each client has this structure
 */
typedef struct {
    uint32_t    lastcall;           // the last frame this user called a vote
    uint32_t    callcount;          // how many votes this user has called
    int8_t      vote;               // 0 = not voted, + is yes, - is no
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
