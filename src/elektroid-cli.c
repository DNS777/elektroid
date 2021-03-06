/*
 *   etls.c
 *   Copyright (C) 2019 David García Goñi <dagargo@gmail.com>
 *
 *   This file is part of Elektroid.
 *
 *   Elektroid is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Elektroid is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Elektroid. If not, see <http://www.gnu.org/licenses/>.
 */

#include "../config.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <signal.h>
#include "connector.h"
#include "sample.h"
#include "utils.h"

static struct connector connector;
static int running;

static gchar *
cli_get_path (gchar * device_path)
{
  gint len = strlen (device_path);
  char *path = device_path;
  gint i = 0;

  while (path[0] != '/' && i < len)
    {
      path++;
      i++;
    }

  return path;
}

static gint
cli_ld ()
{
  gint i;
  struct connector_device device;
  GArray *devices = connector_get_elektron_devices ();

  for (i = 0; i < devices->len; i++)
    {
      device = g_array_index (devices, struct connector_device, i);
      printf ("%d %s\n", device.card, device.name);
    }

  g_array_free (devices, TRUE);

  return EXIT_SUCCESS;
}

static gint
cli_connect (const char *device_path)
{
  gint card = atoi (device_path);
  return connector_init (&connector, card);
}

static int
cli_ls (int argc, char *argv[], int optind)
{
  struct connector_dir_iterator *d_iter;
  gchar *device_path, *path;
  gint res;

  if (optind == argc)
    {
      fprintf (stderr, "Remote path missing\n");
      return EXIT_FAILURE;
    }
  else
    {
      device_path = argv[optind];
    }

  res = cli_connect (device_path);

  if (res < 0)
    {
      return EXIT_FAILURE;
    }

  path = cli_get_path (device_path);

  d_iter = connector_read_dir (&connector, path);
  if (d_iter == NULL)
    {
      return EXIT_FAILURE;
    }

  while (!connector_get_next_dentry (d_iter))
    {
      printf ("%c %.2f %08x %s\n", d_iter->type,
	      d_iter->size / (1024.0 * 1024.0), d_iter->cksum,
	      d_iter->dentry);
    }

  connector_free_dir_iterator (d_iter);

  return EXIT_SUCCESS;
}

static int
cli_mkdir (int argc, char *argv[], int optind)
{
  gchar *device_path, *path;
  gint res;

  if (optind == argc)
    {
      fprintf (stderr, "Remote path missing\n");
      return EXIT_FAILURE;
    }
  else
    {
      device_path = argv[optind];
    }

  res = cli_connect (device_path);

  if (res < 0)
    {
      return EXIT_FAILURE;
    }

  path = cli_get_path (device_path);

  return connector_create_dir (&connector, path);
}

static int
cli_mv (int argc, char *argv[], int optind)
{
  gchar *device_path_src, *device_path_dst, *path_src, *path_dst;
  gint card_src;
  gint card_dst;
  gint res;

  if (optind == argc)
    {
      fprintf (stderr, "Remote path source missing\n");
      return EXIT_FAILURE;
    }
  else
    {
      device_path_src = argv[optind];
      optind++;
    }

  if (optind == argc)
    {
      fprintf (stderr, "Remote path destination missing\n");
      return EXIT_FAILURE;
    }
  else
    {
      device_path_dst = argv[optind];
    }

  card_src = atoi (device_path_src);
  card_dst = atoi (device_path_dst);
  if (card_src != card_dst)
    {
      fprintf (stderr, "Source and destination device must be the same\n");
      return EXIT_FAILURE;
    }

  res = cli_connect (device_path_src);

  if (res < 0)
    {
      return EXIT_FAILURE;
    }

  path_src = cli_get_path (device_path_src);
  path_dst = cli_get_path (device_path_dst);

  return connector_rename (&connector, path_src, path_dst);
}

static int
cli_delete (int argc, char *argv[], int optind, char type)
{
  gchar *device_path, *path;
  gint res;

  if (optind == argc)
    {
      fprintf (stderr, "Remote path missing\n");
      return EXIT_FAILURE;
    }
  else
    {
      device_path = argv[optind];
    }

  res = cli_connect (device_path);

  if (res < 0)
    {
      return EXIT_FAILURE;
    }

  path = cli_get_path (device_path);

  if (type == ELEKTROID_FILE)
    {
      res = connector_delete_file (&connector, path);
    }
  else if (type == ELEKTROID_DIR)
    {
      res = connector_delete_dir (&connector, path);
    }
  else
    {
      res = -1;
    }

  return res;
}

static int
cli_download (int argc, char *argv[], int optind)
{
  gchar *device_path_src, *path_src, *local_path;
  gint res;
  GArray *data;
  ssize_t frames;
  gchar *basec, *bname;

  if (optind == argc)
    {
      fprintf (stderr, "Remote path missing\n");
      return EXIT_FAILURE;
    }
  else
    {
      device_path_src = argv[optind];
    }

  res = cli_connect (device_path_src);

  if (res < 0)
    {
      return EXIT_FAILURE;
    }

  path_src = cli_get_path (device_path_src);

  running = 1;
  data = connector_download (&connector, path_src, &running, NULL);
  if (data == NULL)
    {
      return EXIT_FAILURE;
    }

  basec = strdup (path_src);
  bname = basename (basec);
  local_path = malloc (PATH_MAX);
  snprintf (local_path, PATH_MAX, "./%s.wav", bname);

  frames = sample_save (data, local_path);

  free (basec);
  free (local_path);
  g_array_free (data, TRUE);

  return frames > 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int
cli_upload (int argc, char *argv[], int optind)
{
  gchar *path_src, *device_path_dst, *path_dst;
  gint res;
  ssize_t frames;
  gchar *basec, *bname;
  GArray *sample;

  if (optind == argc)
    {
      fprintf (stderr, "Local path missing\n");
      return EXIT_FAILURE;
    }
  else
    {
      path_src = argv[optind];
      optind++;
    }

  if (optind == argc)
    {
      fprintf (stderr, "Remote path missing\n");
      return EXIT_FAILURE;
    }
  else
    {
      device_path_dst = argv[optind];
    }

  res = cli_connect (device_path_dst);

  if (res < 0)
    {
      return EXIT_FAILURE;
    }

  basec = strdup (path_src);
  bname = basename (basec);
  remove_ext (bname);
  path_dst = cli_get_path (device_path_dst);
  path_dst = chain_path (path_dst, bname);

  sample = g_array_new (FALSE, FALSE, sizeof (short));
  frames = sample_load (sample, NULL, NULL, path_src, NULL, NULL);
  if (frames < 0)
    {
      res = EXIT_FAILURE;
      goto cleanup;
    }

  running = 1;
  frames = connector_upload (&connector, sample, path_dst, &running, NULL);

  res = frames < 0 ? EXIT_FAILURE : EXIT_SUCCESS;

cleanup:
  free (basec);
  free (path_dst);
  g_array_free (sample, TRUE);
  return res;
}

static int
cli_info (int argc, char *argv[], int optind)
{
  gchar *device_path;
  gint res;

  if (optind == argc)
    {
      fprintf (stderr, "Device missing\n");
      return EXIT_FAILURE;
    }
  else
    {
      device_path = argv[optind];
    }

  res = cli_connect (device_path);

  if (res < 0)
    {
      return EXIT_FAILURE;
    }

  printf ("%s\n", connector.device_name);

  return EXIT_SUCCESS;
}

static void
cli_end (int sig)
{
  running = 0;
}

int
main (int argc, char *argv[])
{
  gint c;
  gint res;
  gchar *command;
  gint vflg = 0, errflg = 0;
  struct sigaction action, old_action;

  action.sa_handler = cli_end;
  sigemptyset (&action.sa_mask);
  action.sa_flags = 0;

  sigaction (SIGTERM, NULL, &old_action);
  if (old_action.sa_handler != SIG_IGN)
    {
      sigaction (SIGTERM, &action, NULL);
    }

  sigaction (SIGQUIT, NULL, &old_action);
  if (old_action.sa_handler != SIG_IGN)
    {
      sigaction (SIGQUIT, &action, NULL);
    }

  sigaction (SIGINT, NULL, &old_action);
  if (old_action.sa_handler != SIG_IGN)
    {
      sigaction (SIGINT, &action, NULL);
    }

  sigaction (SIGHUP, NULL, &old_action);
  if (old_action.sa_handler != SIG_IGN)
    {
      sigaction (SIGHUP, &action, NULL);
    }

  while ((c = getopt (argc, argv, "v")) != -1)
    {
      switch (c)
	{
	case 'v':
	  vflg++;
	  break;
	case '?':
	  errflg++;
	}
    }

  if (optind == argc)
    {
      errflg = 1;
    }
  else
    {
      command = argv[optind];
      optind++;
    }

  if (vflg)
    {
      debug_level = vflg;
    }

  if (errflg > 0)
    {
      fprintf (stderr, "%s\n", PACKAGE_STRING);
      char *exec_name = basename (argv[0]);
      fprintf (stderr, "Usage: %s [-v] command [options]\n", exec_name);
      exit (EXIT_FAILURE);
    }

  if (strcmp (command, "ld") == 0)
    {
      res = cli_ld ();
    }
  else if (strcmp (command, "ls") == 0)
    {
      res = cli_ls (argc, argv, optind);
    }
  else if (strcmp (command, "mkdir") == 0)
    {
      res = cli_mkdir (argc, argv, optind);
    }
  else if (strcmp (command, "mv") == 0)
    {
      res = cli_mv (argc, argv, optind);
    }
  else if (strcmp (command, "rm") == 0)
    {
      res = cli_delete (argc, argv, optind, ELEKTROID_FILE);
    }
  else if (strcmp (command, "rmdir") == 0)
    {
      res = cli_delete (argc, argv, optind, ELEKTROID_DIR);
    }
  else if (strcmp (command, "download") == 0)
    {
      res = cli_download (argc, argv, optind);
    }
  else if (strcmp (command, "upload") == 0)
    {
      res = cli_upload (argc, argv, optind);
    }
  else if (strcmp (command, "info") == 0)
    {
      res = cli_info (argc, argv, optind);
    }
  else
    {
      fprintf (stderr, "Command %s not recognized\n", command);
      res = EXIT_FAILURE;
    }

  if (connector_check (&connector))
    {
      connector_destroy (&connector);
    }
  return res;
}
