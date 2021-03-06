/*
 *   connector.h
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

#include <glib.h>
#include <alsa/asoundlib.h>

struct connector
{
  snd_rawmidi_t *inputp;
  snd_rawmidi_t *outputp;
  gchar *device_name;
  gushort seq;
  GMutex mutex;
};

struct connector_dir_iterator
{
  gchar *dentry;
  gchar type;
  guint size;
  guint32 cksum;
  GByteArray *msg;
  guint pos;
};

struct connector_device
{
  gchar *name;
  guint card;
};

enum connector_sysex_transfer_status
{
  WAITING,
  SENDING,
  RECEIVING,
  FINISHED
};

struct connector_sysex_transfer
{
  gboolean active;
  enum connector_sysex_transfer_status status;
  GByteArray *data;
  gboolean timeout;
  gboolean batch;
};

gint connector_init (struct connector *, gint);

void connector_destroy (struct connector *);

gboolean connector_check (struct connector *);

struct connector_dir_iterator *connector_read_dir (struct connector *,
						   const gchar *);

void connector_free_dir_iterator (struct connector_dir_iterator *);

guint connector_get_next_dentry (struct connector_dir_iterator *);

gint connector_rename (struct connector *, const gchar *, const gchar *);

gint connector_delete_file (struct connector *, const gchar *);

gint connector_delete_dir (struct connector *, const gchar *);

GArray *connector_download (struct connector *, const gchar *, gint *,
			    void (*)(gdouble));

ssize_t
connector_upload (struct connector *, GArray *, gchar *, gint *,
		  void (*)(gdouble));

void connector_get_sample_info_from_msg (GByteArray *, gint *, guint *);

gint connector_create_dir (struct connector *, const gchar *);

GArray *connector_get_elektron_devices ();

ssize_t connector_tx_sysex (struct connector *,
			    struct connector_sysex_transfer *);

GByteArray *connector_rx_sysex (struct connector *,
				struct connector_sysex_transfer *);

void free_msg (gpointer);
