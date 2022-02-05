#include "g_local.h"
#include "g_menu.h"
#include "g_ctffunc.h"
#include "g_tourney.h"
#include "g_vote.h"

int Clear_All_Ballots (edict_t *ent);
void Vote_Skip_Level (edict_t *ent);
void Vote_Jump_Level (edict_t *ent);
void Vote_Ref_Player (edict_t *ent);

int VoteStarted = false;
float VoteTime = 0.0;
int VoteType;

vote_t vote;

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

    ent->client->vote.vote = VOTE_YES;
    ent->client->vote.callcount++;
    ent->client->vote.lastcall = level.framenum;
}


void VoteBuildProposalString(char *output)
{
    switch (vote.proposal) {
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

    gi.cprintf(ent, PRINT_HIGH, buf);
}


/**
 * Runs every second to check on an active vote
 */
void VoteThink(edict_t *ent)
{
    static char timeleft[15];
    vote_result_t res;
    float percent;

    // count the votes and see if they're above the winning threshold
    VoteCounts(&res);
    percent = (res.yes_count / (res.yes_count + res.no_count)) * 100;
    if (res.yes_count + res.no_count > 0 && percent >= vote_threshold->value) {
        VoteSuccess();
    }

    // we ran out of time
    if (ent->count == 0) {
        VoteFail();
        VoteReset();
        return;
    }

    SecondsToTime(timeleft, ent->count);

    gi.configstring(CS_VOTETIME, va("%s     %d yes, %d no", timeleft, res.yes_count, res.no_count));

    ent->nextthink = level.time + 1;
    ent->count--;
}


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
                results->yes_count++;
            }

            if (ent->client->vote.vote < 0) {
                results->no_count++;
            }
        }
    }
}


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
    }

    VoteFinished();
}


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




//////// Remove all below ///////////////
int Clear_All_Ballots (edict_t *ent)
{
	edict_t * player = NULL;
	unsigned int num_players = 0;

	//Reset the flags to neither yes or no (ie abstain) to prepare for next vote
	player = ctf_findplayer(NULL, NULL, CTF_TEAM_IGNORETEAM);
	while (player)
	{
		num_players++;
		//gi.bprintf(PRINT_HIGH, "processing: %s --", player->client->pers.netname); //for debugging
		player->client->ctf.extra_flags &= ~CTF_EXTRAFLAGS_VOTE_NO; //reset all players so they are all in
		player->client->ctf.extra_flags &= ~CTF_EXTRAFLAGS_VOTE_YES;  //'abstain' mode.
		//gi.bprintf(PRINT_HIGH," reset\n"); //for debugging
		player = ctf_findplayer(player, NULL, CTF_TEAM_IGNORETEAM);
	}
	return (num_players);
}


void Vote_Skip_Level (edict_t *ent)
{
	unsigned int num_players = 0;

	num_players = Clear_All_Ballots (ent);
	if (num_players < 4 )
		gi.cprintf(ent, PRINT_HIGH,"You need at least four players on the server to initiate a vote\n");
	else if (VoteStarted)
		gi.cprintf(ent, PRINT_HIGH,"Vote has already been started\n");
	else
	{
		VoteStarted = true;
		VoteTime = level.time;  //start the 30 second timer
		VoteType |= CTF_VOTETYPE_SKIP;

		gi.bprintf(PRINT_HIGH,"%s started vote to skip level.\n", ent->client->pers.netname);
		gi.sound(ent, CHAN_ITEM, gi.soundindex("misc/secret.wav"), 1, ATTN_NONE, 0);
		ent->client->ctf.extra_flags |= CTF_EXTRAFLAGS_VOTE_YES; //client who initiates vote must vote yes by default
		ent->client->ctf.extra_flags &= ~CTF_EXTRAFLAGS_VOTE_NO;
	}
	Vote_Menu(ent);
}

void Vote_Jump_Level (edict_t *ent)
{
	unsigned int num_players = 0;

	num_players = Clear_All_Ballots (ent);
	if (num_players < 4 )
		gi.cprintf(ent, PRINT_HIGH,"You need at least four players on the server to initiate a vote\n");
	else if (VoteStarted)
		gi.cprintf(ent, PRINT_HIGH,"Vote has already been started\n");
	else
	{
		VoteStarted = true;
		VoteTime = level.time;  //start the 30 second timer
		VoteType |= CTF_VOTETYPE_GOTOMAP;

		gi.bprintf(PRINT_HIGH,"%s started vote to jump to specific level.\n", ent->client->pers.netname);
		gi.sound(ent, CHAN_ITEM, gi.soundindex("misc/secret.wav"), 1, ATTN_NONE, 0);
		ent->client->ctf.extra_flags |= CTF_EXTRAFLAGS_VOTE_YES; //client who initiates vote must vote yes by default
		ent->client->ctf.extra_flags &= ~CTF_EXTRAFLAGS_VOTE_NO;
	}
	Vote_Menu(ent);
}

void Vote_Ref_Player (edict_t *ent)
{
	unsigned int num_players = 0;

	num_players = Clear_All_Ballots (ent);
	if (num_players < 4 )
		gi.cprintf(ent, PRINT_HIGH,"You need at least four players on the server to initiate a vote\n");
	else if (VoteStarted)
		gi.cprintf(ent, PRINT_HIGH,"Vote has already been started\n");
	else
	{
		VoteStarted = true;
		VoteTime = level.time;  //start the 30 second timer
		VoteType |= CTF_VOTETYPE_REFPLAYER;

		gi.bprintf(PRINT_HIGH,"%s started vote to referee a player.\n", ent->client->pers.netname);
		gi.sound(ent, CHAN_ITEM, gi.soundindex("misc/secret.wav"), 1, ATTN_NONE, 0);
		ent->client->ctf.extra_flags |= CTF_EXTRAFLAGS_VOTE_YES; //client who initiates vote must vote yes by default
		ent->client->ctf.extra_flags &= ~CTF_EXTRAFLAGS_VOTE_NO;
	}	
	Vote_Menu(ent);
}

void Check_Vote(void)  //called from p_client.c
{
	edict_t * player = NULL;
	unsigned int total_votes = 0;
	unsigned int num_yes = 0;
	unsigned int num_no  = 0;
	unsigned int num_abstained = 0;
	unsigned int remainder = 0;
	unsigned int vote_result = 0;
	unsigned int midpoint = 0;

	//gi.bprintf(PRINT_HIGH,"%f : %f\n", level.time, VoteTime+30);  //for debugging purposes
	if (level.time > (VoteTime + 30)) // if the 30 seconds are up then process the votes.
	{
		gi.bprintf(PRINT_HIGH,"Vote session has ended\n");
		VoteStarted = false;

		//go in and check the ent->client->ctf.extra_flags of each player and tabulate the votes
		player = ctf_findplayer(NULL, NULL, CTF_TEAM_IGNORETEAM);
		while (player)
		{
			//gi.bprintf(PRINT_HIGH, "processing: %s --", player->client->pers.netname); //for debugging
			//grab vote, if any
			if (player->client->ctf.extra_flags & CTF_EXTRAFLAGS_VOTE_YES)
			{
				//gi.bprintf(PRINT_HIGH," voted YES\n"); //for debugging
				num_yes++;
			}
			else if (player->client->ctf.extra_flags & CTF_EXTRAFLAGS_VOTE_NO)
			{
				//gi.bprintf(PRINT_HIGH," voted NO\n");  //for debugging
				num_no++;
			}
			else
			{
				//gi.bprintf(PRINT_HIGH," abstained\n");  //for debugging
				num_abstained++;
			}
			player = ctf_findplayer(player, NULL, CTF_TEAM_IGNORETEAM);
		}
		
		gi.bprintf(PRINT_HIGH,"VOTE RESULT: YES:%d  NO:%d  Abstained:%d\n", num_yes, num_no, num_abstained);
		total_votes = num_yes + num_no;  //only those who voted will be counted
		if (total_votes < 2)
			gi.bprintf(PRINT_HIGH,"Vote Fails: you need at least 2 ballots cast!\n");
		else
		{
			vote_result = (num_yes * 100) / total_votes;
			remainder = (num_yes * 100) % total_votes;  //find the remainder of the integer division if there is one.
			midpoint = total_votes>>1;
			//gi.bprintf(PRINT_HIGH,"total_votes:%d  vote_result:%d  remainder:%d  midpoint:%d\n", total_votes, vote_result, remainder, midpoint); //for debugging
			if (remainder > midpoint) vote_result++;  //if the remainder is greater than 50% then round up by one.
			if (vote_result >= PERCENT_MAJORITY_REQUIRED)
			{
				if (VoteType & CTF_VOTETYPE_SKIP)
				{
					gi.bprintf(PRINT_HIGH,"Vote to skip level Passes with %d percent majority\n", vote_result);
					EndDMLevel();
				}
				else if (VoteType & CTF_VOTETYPE_GOTOMAP)
				{
					gi.bprintf(PRINT_HIGH,"Vote to goto level Passes with %d percent majority\n", vote_result);
					gi.bprintf(PRINT_HIGH,"Too bad this feature is not done yet :P\n");
				}
				else if (VoteType & CTF_VOTETYPE_REFPLAYER)
				{
					gi.bprintf(PRINT_HIGH,"Vote to ref player Passes with %d percent majority\n", vote_result);
					gi.bprintf(PRINT_HIGH,"Too bad this feature is not done yet :P\n");
				}
			}
			else
				gi.bprintf(PRINT_HIGH,"Vote Fails with %d percent majority\n", (100-vote_result));
		}
		VoteType &= ~CTF_VOTETYPE_SKIP;  //clear the vote modes
		VoteType &= ~CTF_VOTETYPE_GOTOMAP;
		VoteType &= ~CTF_VOTETYPE_REFPLAYER;
	}
}


void Vote_YES (edict_t *ent)
{
	gi.cprintf(ent, PRINT_HIGH,"You have voted YES\n");
	ent->client->ctf.extra_flags |= CTF_EXTRAFLAGS_VOTE_YES;
	ent->client->ctf.extra_flags &= ~CTF_EXTRAFLAGS_VOTE_NO;
	Vote_Menu(ent);
}

void Vote_NO (edict_t *ent)
{
	gi.cprintf(ent, PRINT_HIGH,"You have voted NO\n");
	ent->client->ctf.extra_flags &= ~CTF_EXTRAFLAGS_VOTE_YES;
	ent->client->ctf.extra_flags |= CTF_EXTRAFLAGS_VOTE_NO;
	Vote_Menu(ent);
}

void Vote_Menu (edict_t *ent)             //Vampire -- voting menu
{
	gclient_t	*cl;
	char text[MAX_INFO_STRING];

	cl = ent->client;

	Menu_Free(ent);
	ent->client->menu = MENU_LOCAL;
	ent->client->menuselect = 1;

	Menu_Set(ent, 0, "LMCTF Vote Menu", Main_Menu);
	Menu_Set(ent, 1, "------------------", NULL);
	if (!VoteStarted)
	{
		Menu_Set(ent, 2, "Skip to next map", Vote_Skip_Level);
		Menu_Set(ent, 3, "Jump to specific map", Vote_Jump_Level);
		Menu_Set(ent, 4, "Referee a player", Vote_Ref_Player);
	}
	else
	{
		Menu_Set(ent, 3, "Vote YES", Vote_YES);
		Menu_Set(ent, 4, "Vote NO", Vote_NO);
	}
	if (VoteStarted)
		sprintf(text, "Vote:     Started");
	else
		sprintf(text, "Vote:     Idle");
	Menu_Set(ent, 6, text, NULL);
	if (VoteStarted)
	{
		if (ent->client->ctf.extra_flags & CTF_EXTRAFLAGS_VOTE_YES)
			sprintf(text, "You have voted YES");
		else if (ent->client->ctf.extra_flags & CTF_EXTRAFLAGS_VOTE_NO)
			sprintf(text, "You have voted NO");
		else
			sprintf(text, "You have not voted");
		Menu_Set(ent, 8, text, NULL);
	}

	cl->menuselect = 0;
		
	Menu_Draw (ent);
}
/* END -- Vampire */
