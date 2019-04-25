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
 * author Randy Caldejon <rc@counterflowai.com>
 *
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <syslog.h>
#include <pthread.h>
#include <assert.h>

#include "test.h"

#define MAX_TEST6_MESSAGES 10000
#define QUANTUM (MAX_TEST6_MESSAGES / 10)
#define INPUT_FILE "input6.txt"

pthread_barrier_t barrier;

static const char *CONFIG_LUA =
	"inputs = {\n"
	"   { tag=\"input\", uri=\"tail://input6.txt<\", script=\"filter.lua\", default_analyzer=\"test6\"}\n"
	"}\n"
	"\n"
	"analyzers = {\n"
	"    { tag=\"test6\", script=\"analyzer.lua\" , default_analyzer=\"\", default_output=\"log6\" },\n"
	"}\n"
	"\n"
	"outputs = {\n"
	"    { tag=\"log6\", uri=\"ipc://output6.ipc\"},\n"
	"}\n"
	"\n";

static const char *INPUT_LUA =
	"function setup()\n"
	"end\n"
	"\n"
	"function loop(msg)\n"
	"   dragonfly.analyze_event (default_analyzer, msg)\n"
	"end\n";

static const char *ANALYZER_LUA =
	"function setup()\n"
	"end\n"
	"function loop (tbl)\n"
	"   dragonfly.output_event (default_output, tbl.msg)\n"
	"end\n\n";
/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
static void write_file(const char *file_path, const char *content)
{
	fprintf(stderr, "generated %s\n", file_path);
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
static void signal_shutdown6(int signum)
{
	dragonfly_mle_break();
	syslog(LOG_INFO, "%s", __FUNCTION__);
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
static void *writer_thread(void *ptr)
{
#ifdef _GNU_SOURCE
	pthread_setname_np(pthread_self(), "writer");
#endif

	char path[PATH_MAX];
	snprintf(path, sizeof(path), "file://%s<", INPUT_FILE);
	DF_HANDLE *pump = dragonfly_io_open(path, DF_OUT);
	if (!pump)
	{
		fprintf(stderr, "%s:%d\n", __FUNCTION__, __LINE__);
		perror(__FUNCTION__);
		exit (EXIT_FAILURE);
	}
	pthread_barrier_wait(&barrier);
	pthread_detach(pthread_self());
	/*
	 * write messages walking the alphabet
	 */
	int mod = 0;
	char buffer[1024];
	unsigned long i = 0;
	for (i = 0; i < MAX_TEST6_MESSAGES; i++)
	{
		char msg[128];
		for (int j = 0; j < (sizeof(msg) - 1); j++)
		{
			msg[j] = 'A' + (mod % 48);
			if (msg[j] == '\\')
				msg[j] = ' ';
			mod++;
		}
		msg[sizeof(msg) - 1] = '\0';
		snprintf(buffer, sizeof(buffer), "{ \"id\": %lu, \"msg\":\"%s\" }", i, msg);
		if (dragonfly_io_write(pump, buffer) < 0)
		{
			fprintf(stderr, "%s:%d\n", __FUNCTION__, __LINE__);
			perror(__FUNCTION__);
			exit (EXIT_FAILURE);
		}
		usleep(10);
	}
	dragonfly_io_close(pump);
	fprintf(stderr, "%s: %lu records written\n", __FILE__, i);
	return (void *)NULL;
}
/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
void SELF_TEST6(const char *dragonfly_root)
{
	fprintf(stderr, "\n\n%s: tailing %d messages from input to output6.ipc\n", __FUNCTION__, MAX_TEST6_MESSAGES);
	fprintf(stderr, "-------------------------------------------------------\n");
	/*
	 * generate lua scripts
	 */

	write_file(CONFIG_TEST_FILE, CONFIG_LUA);
	write_file(FILTER_TEST_FILE, INPUT_LUA);
	write_file(ANALYZER_TEST_FILE, ANALYZER_LUA);

	openlog("dragonfly", LOG_PERROR, LOG_USER);
#ifdef _GNU_SOURCE
	pthread_setname_np(pthread_self(), "dragonfly");
#endif
	signal(SIGINT, signal_shutdown6);
	pthread_barrier_init(&barrier, NULL, 2);
	initialize_configuration(dragonfly_root, dragonfly_root, dragonfly_root);

	DF_HANDLE *input = dragonfly_io_open("ipc://output6.ipc", DF_IN);
	if (!input)
	{
		perror(__FUNCTION__);
		exit (EXIT_FAILURE);
	}

	pthread_t tinfo;
	if (pthread_create(&tinfo, NULL, writer_thread, (void *)NULL) != 0)
	{
		perror(__FUNCTION__);
		exit (EXIT_FAILURE);
	}
	startup_threads();

	/*
	 * write messages walking the alphabet
	 */
	char buffer[4096];
	clock_t last_time = clock();
	pthread_barrier_wait(&barrier);
	for (unsigned long i = 0; i < MAX_TEST6_MESSAGES; i++)
	{
		int len = dragonfly_io_read(input, buffer, (sizeof(buffer) - 1));
		if (len < 0)
		{
			break;
		}
		else if (len == 0)
		{
			fprintf(stderr, "%s: %i file closed \n", __FUNCTION__, __LINE__);
		}
		else if ((i > 0) && (i % QUANTUM) == 0)
		{
			clock_t mark_time = clock();
			double elapsed_time = ((double)(mark_time - last_time)) / CLOCKS_PER_SEC; // in seconds
			double ops_per_sec = QUANTUM / elapsed_time;
			fprintf(stderr, "\t%6.2f/sec (%lu records)\n", ops_per_sec, i);
			last_time = mark_time;
		}
	}
	syslog(LOG_INFO, "shutting down");
	/* allow time to flush output */
	sleep (2);

	shutdown_threads();

	dragonfly_io_close(input);
	pthread_barrier_destroy(&barrier);
	closelog();

	fprintf(stderr, "%s: cleaning up files\n", __FUNCTION__);
	remove(CONFIG_TEST_FILE);
	remove(FILTER_TEST_FILE);
	remove(ANALYZER_TEST_FILE);
	remove(INPUT_FILE);
	fprintf(stderr, "-------------------------------------------------------\n\n");
	fflush(stderr);
}

/*
 * ---------------------------------------------------------------------------------------
 */
