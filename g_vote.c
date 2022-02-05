#include "g_local.h"
#include "g_menu.h"
#include "g_ctffunc.h"
#include "g_tourney.h"
#include "g_vote.h"

vote_t vote;

static qboolean vote_ref(edict_t *ent)
{
    if (gi.argc() < 3) {
        return false;
    }
    static edict_t *newref;
    char *id = gi.argv(2);
    unsigned val = strtoul(id, NULL, 10);
    if (val > game.maxclients) {
        return false;
    }

    newref = g_edicts + 1 + val;
    if (ISREF(newref)) {
        gi.cprintf(ent, PRINT_HIGH, "%s is already a referee\n", NAME(newref));
        return false;
    }

    vote.victim = newref;
    return true;
}


static qboolean vote_victim(edict_t *ent) {
    if (gi.argc() < 3) {
        return false;
    }
    static edict_t *vic;
    char *id = gi.argv(2);
    unsigned val = strtoul(id, NULL, 10);
    if (val > game.maxclients) {
        return false;
    }

    vic = g_edicts + 1 + val;
    if (!vic->inuse) {
        return false;
    }

    if (ISREF(vic)){
        gi.cprintf(ent, PRINT_HIGH, "%s is a referee, you can't vote kick them\n", NAME(vic));
        return false;
    }

    if (vic == ent) {
        gi.cprintf(ent, PRINT_HIGH, "You can't vote kick yourself\n");
        return false;
    }

    vote.victim = vic;
    return true;
}


static qboolean vote_map(edict_t *ent) {
    if (gi.argc() < 3) {
        return false;
    }

    char *map = gi.argv(2);
    strncpy(vote.strvalue, map , sizeof(vote.strvalue)-1);
    return true;
}


static qboolean vote_reset(edict_t *ent) {
    return false;
}


static qboolean vote_fastswitch(edict_t *ent) {
    char *fs = gi.argv(2);
    unsigned val = strtoul(fs, NULL, 10);

    if ((int)fastswitch->value == val) {
        gi.cprintf(ent, PRINT_HIGH, "Fastswitch is already set to %d\n",val);
        return false;
    }

    vote.intvalue = val;
    return true;
}


static qboolean vote_timelimit(edict_t *ent) {
    char *arg = gi.argv(2);
    unsigned count = strtoul(arg, NULL, 10);
    vote.intvalue = CLAMP(count, 1, 60);
    return true;
}


static qboolean vote_mode(edict_t *ent) {
    return false;
}


static qboolean vote_restart(edict_t *ent) {
    return false;
}


static const vote_proposal_t vote_proposals[] = {
    {"ref",         VOTE_REF,       vote_ref},
    {"kick",        VOTE_KICK,      vote_victim},
    {"map",         VOTE_MAP,       vote_map},
    {"reset",       VOTE_RESET,     vote_reset},
    {"fastswitch",  VOTE_SWITCH,    vote_fastswitch},
    {"timelimit",   VOTE_TIMELIMIT, vote_timelimit},
    {"mode",        VOTE_MODE,      vote_mode},
    {"restart",     VOTE_RESTART,   vote_restart},
    {NULL}
};


/**
 * Locate the proposal value based on string input
 */
static vote_proposal_t *find_proposal(char *str)
{
    const vote_proposal_t  *v;
    for (v = vote_proposals; v->name; v++) {
        if (!strcmp(str, v->name)) {
            break;
        }
    }

    if (!v) {
        return NULL;
    }

    return (vote_proposal_t *)v;
}


/**
 * A player cast their vote
 */
void VoteCast(edict_t *ent, int8_t v)
{
    if (!ent->client) {
        return;
    }

    if (!vote.active) {
        gi.cprintf(ent, PRINT_HIGH, "No vote in progress\n");
        return;
    }

    char *strvote = (v > 0) ? "YES" : "NO";
    if (ent->client->vote.vote != 0) {
        gi.bprintf(PRINT_HIGH, "%s changed their vote to %s\n", NAME(ent), strvote);
    } else {
        gi.bprintf(PRINT_HIGH, "%s voted %s\n", NAME(ent), strvote);
    }

    ent->client->vote.vote = v;
}

/**
 * Called when a player uses the vote command
 */
void Cmd_Vote_f(edict_t *ent)
{
    if (!(int)vote_enabled->value) {
        gi.cprintf(ent, PRINT_HIGH, "Voting is disabled on this server\n");
        return;
    }

    if (ent->client->pers.spectator) {
        gi.cprintf(ent, PRINT_HIGH, "Spectators cannot vote\n");
        return;
    }

    if (gi.argc() < 2) {
        VoteUsage(ent);
        return;
    }

    char *arg1 = gi.argv(1);

    if (Q_stricmp(arg1, "yes") == 0) {
        VoteCast(ent, VOTE_YES);
        return;
    }

    if (Q_stricmp(arg1, "no") == 0) {
        VoteCast(ent, VOTE_NO);
        return;
    }

    if (vote.active) {
        gi.cprintf(ent, PRINT_HIGH, "Vote already in progress\n");
        return;
    }

    vote_proposal_t *p = find_proposal(arg1);

    if (!p) {
        VoteUsage(ent);
        return;
    }

    if (!p->bit) {
        VoteUsage(ent);
        return;
    }

    // make sure we're allowed to use this proposal
    if (!(p->bit & (int)vote_mask->value)) {
        VoteUsage(ent);
        return;
    }

    if (!p->func(ent)) {
        return;
    }

    vote.initiator = ent;
    vote.proposal = p->bit;

    VoteBuildProposalString(vote.display);

    gi.bprintf(PRINT_HIGH, "%s started a vote: %s\n", NAME(ent), vote.display);
    gi.configstring(CS_VOTEPROPOSAL, va("vote: %s", vote.display));

    VoteStart();

    gi.sound(ent, CHAN_ITEM, gi.soundindex("misc/secret.wav"), 1, ATTN_NONE, 0);

    // initiator automatically votes yes
    ent->client->vote.vote = VOTE_YES;
    ent->client->vote.callcount++;
    ent->client->vote.lastcall = level.framenum;
}


void VoteBuildProposalString(char *output)
{
    switch (vote.proposal) {
    case VOTE_REF:
        sprintf(output, "promote %s to referee", NAME(vote.victim));
        break;

    case VOTE_TIMELIMIT:
        sprintf(output, "timelimit %d", vote.intvalue);
        break;

    case VOTE_MAP:
        sprintf(output, "change map to %s", vote.strvalue);
        break;

    case VOTE_RESTART:
        sprintf(output, "restart current match");
        break;

    case VOTE_SWITCH:
        sprintf(output, "%sable fast weapon switching", ((int)fastswitch->value) ? "dis" : "en");
        break;

    case VOTE_KICK:
        sprintf(output, "kick %s", NAME(vote.victim));
        break;
    }
}


/**
 *
 */
void VoteUsage(edict_t *ent)
{
    char buf[0xffff];
    uint32_t mask = (int)vote_mask->value;

    sprintf(buf, "Usage: vote <proposal> <value>\n");
    strcat(buf, "Available proposals:\n");

    if (mask & VOTE_TIMELIMIT) {
        strcat(buf, "  timelimit <minutes>                  Set the timelimit in minutes of the match\n");
    }

    if (mask & VOTE_SWITCH) {
        strcat(buf, "  fastswitch <0/1>                     Enable/disable fast weapon switching\n");
    }

    if (mask & VOTE_MAP) {
        strcat(buf, "  map <mapname>                        Change the map\n");
    }

    if (mask & VOTE_RESTART) {
        strcat(buf, "  restart                              Restart the current match\n");
    }

    if (mask & VOTE_KICK) {
        strcat(buf, "  kick <playerID>                      Remove a player from the server (use \"players\" command to find ID\n");
    }

    if (mask & VOTE_REF) {
        strcat(buf, "  ref <playerID>                       Give a player referee status (use \"players\" command to find ID\n");
    }

    gi.cprintf(ent, PRINT_HIGH, buf);
}


/**
 * Runs every second to check on an active vote
 */
void VoteThink(edict_t *ent)
{
    static char timeleft[15];
    vote_result_t res;

    VoteCounts(&res);

    if (res.total > 0 && res.pct_raw >= vote_threshold->value) {
        VoteSuccess();
        return;
    }

    // time is up
    if (ent->count == 0) {
        if (res.total > 0 && res.pct_votes >= vote_threshold->value) {
            VoteSuccess();
            return;
        }

        VoteFail();
        VoteReset();
        return;
    }

    SecondsToTime(timeleft, ent->count);

    gi.configstring(CS_VOTETIME, va("%s     %d yes, %d no", timeleft, res.yes, res.no));

    ent->nextthink = level.time + 1;
    ent->count--;
}


/**
 * Counts everyone's votes, calculates percentages/totals
 */
void VoteCounts(vote_result_t *results)
{
    edict_t *ent;

    memset(results, 0, sizeof(vote_result_t));

    for (uint8_t i = 0; i < game.maxclients; i++) {
        ent = g_edicts + 1 + i;
        if (!ent->inuse) {
            continue;
        }

        if (!ent->client) {
            continue;
        }

        // only team members can vote
        if (ent->client->ctf.teamnum) {
            if (ent->client->vote.vote > 0) {
                results->yes++;
            }

            if (ent->client->vote.vote < 0) {
                results->no++;
            }

            if (ent->client->vote.vote == 0) {
                results->unvoted++;
            }
        }
    }

    results->total = results->yes + results->no;
    results->pct_raw = (results->yes / (results->total + results->unvoted)) * 100;
    results->pct_votes = (results->yes / results->total) * 100;
}


/**
 * Either we hit the timelimit and had the votes
 * or hit the threshold before the timer ran out.
 *
 * Actually perform the necessary stuff for the
 * proposal here
 */
void VoteSuccess(void)
{
    gi.bprintf(PRINT_HIGH, "Vote succeeded\n");

    switch (vote.proposal) {
    case VOTE_TIMELIMIT:
        gi.AddCommandString(va("set timelimit %d\n", vote.intvalue));
        break;

    case VOTE_SWITCH:
        gi.cvar_set("fastswitch", va("%d", vote.intvalue));
        break;

    case VOTE_MAP:
        gi.AddCommandString(va("gamemap %s\n", vote.strvalue));
        break;

    case VOTE_KICK:
        gi.AddCommandString(va("kick %d", (int)(vote.victim->client - game.clients)));
        break;

    case VOTE_REF:
        vote.victim->client->ctf.extra_flags |= CTF_EXTRAFLAGS_REFEREE;
        gi.bprintf(PRINT_HIGH, "%s is now a referee\n", NAME(vote.victim));
        break;

    case VOTE_RESTART:
        break;
    }

    VoteFinished();
}


/**
 * We hit the timeout and didn't have the votes
 */
void VoteFail(void)
{
    gi.bprintf(PRINT_HIGH, "Vote failed\n");
    VoteReset();
}


/**
 * Creates a vote entity which runs and controls the vote
 */
void VoteStart(void)
{
    if (!(int)vote_enabled->value) {
        return;
    }

    vote.ent = G_Spawn();
    vote.votetime = level.framenum;
    vote.ent->think = VoteThink;
    vote.ent->nextthink = level.time;   // fire immediately
    vote.ent->count = (int)vote_time->value;
    vote.active = true;
}


/**
 * Stops the current vote and resets the whole struct
 */
void VoteReset(void)
{
    vote.ent->think = G_FreeEdict;
    vote.ent->nextthink = level.time;

    vote.active = false;
    vote.initiator = NULL;
    vote.intvalue = 0;
    vote.proposal = 0;
    vote.results = 0;
    vote.strvalue[0] = 0;
}

void VoteFinished(void)
{
    VoteReset();
}
