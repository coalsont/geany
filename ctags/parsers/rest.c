/*
*
*   Copyright (c) 2007-2011, Nick Treleaven
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*   This module contains functions for generating tags for reStructuredText (reST) files.
*/

/*
*   INCLUDE FILES
*/
#include "general.h"	/* must always come first */

#include <ctype.h>
#include <string.h>

#include "parse.h"
#include "read.h"
#include "vstring.h"
#include "nestlevel.h"
#include "routines.h"
#include "entry.h"

/*
*   DATA DEFINITIONS
*/
typedef enum {
	K_CHAPTER = 0,
	K_SECTION,
	K_SUBSECTION,
	K_SUBSUBSECTION,
	SECTION_COUNT
} restKind;

static kindOption RestKinds[] = {
	{ true, 'n', "namespace",     "chapters"},
	{ true, 'm', "member",        "sections" },
	{ true, 'd', "macro",         "subsections" },
	{ true, 'v', "variable",      "subsubsections" }
};

static char kindchars[SECTION_COUNT];

static NestingLevels *nestingLevels = NULL;

/*
*   FUNCTION DEFINITIONS
*/

static NestingLevel *getNestingLevel(const int kind)
{
	NestingLevel *nl;
	tagEntryInfo *e;

	while (1)
	{
		nl = nestingLevelsGetCurrent(nestingLevels);
		e = getEntryOfNestingLevel (nl);
		if ((nl && (e == NULL)) || (e && (e->kind - RestKinds) >= kind))
			nestingLevelsPop(nestingLevels);
		else
			break;
	}
	return nl;
}

static void makeRestTag (const vString* const name, const int kind)
{
	const NestingLevel *const nl = getNestingLevel(kind);
	int r = CORK_NIL;

	if (vStringLength (name) > 0)
	{
		tagEntryInfo *parent = getEntryOfNestingLevel (nl);
		tagEntryInfo e;

		initTagEntry (&e, vStringValue (name), &(RestKinds [kind]));

		e.lineNumber--;	/* we want the line before the '---' underline chars */

		r = makeTagEntry (&e);
	}
	nestingLevelsPush(nestingLevels, r);
}


/* checks if str is all the same character */
static bool issame(const char *str)
{
	char first = *str;

	while (*str)
	{
		char c;

		str++;
		c = *str;
		if (c && c != first)
			return false;
	}
	return true;
}


static int get_kind(char c)
{
	int i;

	for (i = 0; i < SECTION_COUNT; i++)
	{
		if (kindchars[i] == c)
			return i;

		if (kindchars[i] == 0)
		{
			kindchars[i] = c;
			return i;
		}
	}
	return -1;
}


/* computes the length of an UTF-8 string
 * if the string doesn't look like UTF-8, return -1 */
static int utf8_strlen(const char *buf, int buf_len)
{
	int len = 0;
	const char *end = buf + buf_len;

	for (len = 0; buf < end; len ++)
	{
		/* perform quick and naive validation (no sub-byte checking) */
		if (! (*buf & 0x80))
			buf ++;
		else if ((*buf & 0xe0) == 0xc0)
			buf += 2;
		else if ((*buf & 0xf0) == 0xe0)
			buf += 3;
		else if ((*buf & 0xf8) == 0xf0)
			buf += 4;
		else /* not a valid leading UTF-8 byte, abort */
			return -1;

		if (buf > end) /* incomplete last byte */
			return -1;
	}

	return len;
}


/* TODO: parse overlining & underlining as distinct sections. */
static void findRestTags (void)
{
	vString *name = vStringNew ();
	const unsigned char *line;

	memset(kindchars, 0, sizeof kindchars);
	nestingLevels = nestingLevelsNew(0);

	while ((line = readLineFromInputFile ()) != NULL)
	{
		int line_len = strlen((const char*) line);
		int name_len_bytes = vStringLength(name);
		int name_len = utf8_strlen(vStringValue(name), name_len_bytes);

		/* if the name doesn't look like UTF-8, assume one-byte charset */
		if (name_len < 0)
			name_len = name_len_bytes;

		/* underlines must be the same length or more */
		if (line_len >= name_len && name_len > 0 &&
			ispunct(line[0]) && issame((const char*) line))
		{
			char c = line[0];
			int kind = get_kind(c);

			if (kind >= 0)
			{
				makeRestTag(name, kind);
				continue;
			}
		}
		vStringClear (name);
		if (! isspace(*line))
			vStringCatS(name, (const char*) line);
	}
	vStringDelete (name);
	nestingLevelsFree(nestingLevels);
}

extern parserDefinition* RestParser (void)
{
	static const char *const patterns [] = { "*.rest", "*.reST", NULL };
	static const char *const extensions [] = { "rest", NULL };
	parserDefinition* const def = parserNew ("reStructuredText");

	def->kinds = RestKinds;
	def->kindCount = ARRAY_SIZE (RestKinds);
	def->patterns = patterns;
	def->extensions = extensions;
	def->parser = findRestTags;
	def->useCork = true;
	return def;
}
