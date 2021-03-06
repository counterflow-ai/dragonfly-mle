/* Copyright (C) 2017-2018 CounterFlow AI, Inc.
 *
 * You can copy, redistribute or modify this Program under the terms of
 * the GNU General Public License version 2 as published by the Free
 * Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * version 2 along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

/*
 *
 * author Don J. Rude <dr@counterflowai.com>
 *
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <syslog.h>
#include <pthread.h>
#include <assert.h>

#include "benchmark.h"
#include "dragonfly-io.h"

#define MAX_BENCH0_MESSAGES 100000
#define QUANTUM (MAX_BENCH0_MESSAGES / 10)

static const char *CONFIG_LUA =
	"inputs = {\n"
	"   { tag=\"LUAinput\", uri=\"ipc://input.ipc\", script=\"filter.lua\", default_analyzer=\"bench0\"}\n"
	"}\n"
	"\n"
	"analyzers = {\n"
	"    { tag=\"bench0-analyzer\", script=\"analyzer.lua\", default_analyzer=\"\", default_output=\"bench0-log\" },\n"
	"}\n"
	"\n"
	"outputs = {\n"
    "    { tag=\"bench0-log\", uri=\"file:///dev/null\"},\n"
	"}\n"
	"\n";

static const char *INPUT_LUA =
	"function setup()\n"
	"end\n"
	"\n"
	"function loop(msg)\n"
  "   -- do nothing\n"
	"end\n";

static const char *ANALYZER_LUA =
	"function setup()\n"
	"end\n"
	"function loop (msg)\n"
    "  -- do nothing\n"
	"end\n\n";
/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
static void write_file(const char *file_path, const char *content)
{
//	fprintf(stderr, "generated %s\n", file_path);
	FILE *fp = fopen(file_path, "w+");
	if (!fp)
	{
		perror(__FUNCTION__);
		return;
	}
	fputs(content, fp);
	fclose(fp);
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
void SELF_BENCH0(const char *dragonfly_root)
{
	fprintf(stdout, "\n%s: pumping %d messages to input stage only\n", __FUNCTION__, MAX_BENCH0_MESSAGES);
	fprintf(stdout, "-------------------------------------------------------\n");
	/*
	 * generate lua scripts
	 */

	write_file(CONFIG_TEST_FILE, CONFIG_LUA);
	write_file(FILTER_TEST_FILE, INPUT_LUA);
	write_file(ANALYZER_TEST_FILE, ANALYZER_LUA);

	signal(SIGPIPE, SIG_IGN);
	openlog("dragonfly", LOG_PERROR, LOG_USER);
#ifdef _GNU_SOURCE
	pthread_setname_np(pthread_self(), "bench0-dragonfly");
#endif
	initialize_configuration(dragonfly_root, dragonfly_root, dragonfly_root);
	startup_threads();

	sleep(1);

	DF_HANDLE *pump = dragonfly_io_open("ipc://input.ipc", DF_OUT);
	if (!pump)
	{
		fprintf(stderr, "%s: dragonfly_io_open() failed.\n", __FUNCTION__);
		return;
	}

    char *msg = MSG;
	clock_t last_time = clock();
	for (unsigned long i = 0; i < MAX_BENCH0_MESSAGES; i++)
	{

		if (dragonfly_io_write(pump, msg) < 0)
		{
			fprintf(stderr, "error pumping to \"ipc://input.ipc\"\n");
			abort();
		}

		if ((i > 0) && (i % QUANTUM) == 0)
		{
			clock_t mark_time = clock();
			double elapsed_time = ((double)(mark_time - last_time)) / CLOCKS_PER_SEC; // in seconds
			double ops_per_sec = QUANTUM / elapsed_time;
			fprintf(stdout, "%6.2f /sec\n", ops_per_sec);
			last_time = clock();
		}
	}
	dragonfly_io_close(pump);
	sleep(1);
	shutdown_threads();
	closelog();

	fprintf(stderr, "%s: cleaning up files\n", __FUNCTION__);
	remove(CONFIG_TEST_FILE);
	remove(FILTER_TEST_FILE);
	remove(ANALYZER_TEST_FILE);
	fprintf(stderr, "-------------------------------------------------------\n\n");
	fflush(stderr);
}

/*
 * ---------------------------------------------------------------------------------------
 */

