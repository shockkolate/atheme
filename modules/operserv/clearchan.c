/*
 * Copyright (c) 2006 Robin Burchell <surreal.w00t@gmail.com>
 * Rights to this code are documented in doc/LICENCE.
 *
 * This file contains functionality implementing OperServ CLEARCHAN.
 *
 * $Id: clearchan.c 6463 2006-09-25 13:03:41Z jilles $
 */

#include "atheme.h"

DECLARE_MODULE_V1
(
	"operserv/clearchan", FALSE, _modinit, _moddeinit,
	"$Id: clearchan.c 6463 2006-09-25 13:03:41Z jilles $",
	"Robin Burchell <surreal.w00t@gmail.com>"
);

#define CLEAR_KICK 1
#define CLEAR_KILL 2
#define CLEAR_GLINE 3

static void os_cmd_clearchan(sourceinfo_t *si, int parc, char *parv[]);

command_t os_clearchan = { "CLEARCHAN", "Clears a channel via KICK, KILL or GLINE", PRIV_CHAN_ADMIN, 3, os_cmd_clearchan };

list_t *os_cmdtree;
list_t *os_helptree;

void _modinit(module_t *m)
{
	MODULE_USE_SYMBOL(os_cmdtree, "operserv/main", "os_cmdtree");
	MODULE_USE_SYMBOL(os_helptree, "operserv/main", "os_helptree");

	command_add(&os_clearchan, os_cmdtree);
	help_addentry(os_helptree, "CLEARCHAN", "help/oservice/clearchan", NULL);
}

void _moddeinit(void)
{
	command_delete(&os_clearchan, os_cmdtree);
	help_delentry(os_helptree, "CLEARCHAN");
}

static void os_cmd_clearchan(sourceinfo_t *si, int parc, char *parv[])
{
	chanuser_t *cu = NULL;
	node_t *n = NULL;
	channel_t *c = NULL;
	int action;
	char *actionstr = parv[0];
	char *targchan = parv[1];
	char *treason = parv[2];
	char reason[512];
	int matches = 0;
	int ignores = 0;

	if (!actionstr || !targchan || !treason)
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "CLEARCHAN");
		command_fail(si, fault_needmoreparams, "Syntax: CLEARCHAN KICK|KILL|GLINE <#channel> <reason>");
 		return;
	}

	c = channel_find(targchan);

	if (!c)
	{
		command_fail(si, fault_nosuch_target, "The channel must exist for CLEARCHAN.");
 		return;
	}

	/* check what they are doing is valid */
	if (!strcasecmp(actionstr, "KICK"))
		action = CLEAR_KICK;
	else if (!strcasecmp(actionstr, "KILL"))
		action = CLEAR_KILL;
	else if (!strcasecmp(actionstr, "GLINE") || !strcasecmp(actionstr, "KLINE"))
		action = CLEAR_GLINE;
	else
	{
		/* not valid! */
		command_fail(si, fault_badparams, "\2%s\2 is not a valid action", actionstr);
 		return;				
	}

	if (action != CLEAR_KICK && !has_priv(si->su, PRIV_MASS_AKILL))
	{
		command_fail(si, fault_noprivs, "You do not have %s privilege.", PRIV_MASS_AKILL);
		return;
	}

	if (action == CLEAR_KICK)
		snprintf(reason, sizeof reason, "(Clearing) %s", treason);
	else
		snprintf(reason, sizeof reason, "(Clearing %s) %s", targchan, treason);

	wallops("\2%s\2 is clearing channel %s with %s",
			si->su->nick, targchan, actionstr);
	snoop("CLEARCHAN: %s on \2%s\2 by \2%s\2", actionstr, targchan, si->su->nick);
	command_success_nodata(si, "Clearing \2%s\2 with \2%s\2", targchan, actionstr);

	/* iterate over the users in channel */
	LIST_FOREACH(n, c->members.head)
	{
		cu = n->data;

		if (is_internal_client(cu->user))
			;
		else if (is_ircop(cu->user))
		{
			command_success_nodata(si, "\2CLEARCHAN\2: Ignoring IRC Operator \2%s\2!%s@%s {%s}", cu->user->nick, cu->user->user, cu->user->host, cu->user->gecos);
			ignores++;
		}
		else
		{
			command_success_nodata(si, "\2CLEARCHAN\2: \2%s\2 hit \2%s\2!%s@%s {%s}", actionstr, cu->user->nick, cu->user->user, cu->user->host, cu->user->gecos);
			matches++;

			switch (action)
			{
				case CLEAR_KICK:
					kick(opersvs.nick, targchan, cu->user->nick, reason);
					break;
				case CLEAR_KILL:
					skill(opersvs.nick, cu->user->nick, reason);
					user_delete(cu->user);
					break;
				case CLEAR_GLINE:
					kline_sts("*", "*", cu->user->host, 604800, reason);
			}
		}
	}

	command_success_nodata(si, "\2%d\2 matches, \2%d\2 ignores for \2%s\2 on \2%s\2", matches, ignores, actionstr, targchan);
	logcommand(opersvs.me, si->su, CMDLOG_ADMIN, "CLEARCHAN %s %s %s (%d matches, %d ignores)", actionstr, targchan, treason, matches, ignores);
}
