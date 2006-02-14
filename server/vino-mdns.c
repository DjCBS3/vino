/*
 * Copyright (C) 2005 Ethium, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Authors:
 *      Sebastien Estienne <sebastien.estienne@gmail.com>
 *
 */

#include <config.h>

#include "vino-mdns.h" 

#ifdef VINO_HAVE_AVAHI

#include <string.h>
#include <avahi-client/publish.h>
#include <avahi-common/alternative.h>
#include <avahi-common/error.h>
#include <avahi-glib/glib-watch.h>
#include <avahi-glib/glib-malloc.h>

#include "vino-util.h"

static void vino_mdns_add_services (AvahiClient *client);

static GHashTable      *mdns_services = NULL;
static AvahiGLibPoll   *mdns_glib_poll = NULL;
static AvahiClient     *mdns_client = NULL;
static AvahiEntryGroup *mdns_entry_group = NULL;
char                   *mdns_service_name = NULL; 

static const char *
vino_mdns_get_service_name (void)
{
  if (mdns_service_name == NULL)
    {
      const char *show_username;

      /* 
       * Translators: translate "vino-mdns:showusername" to
       * "1" if "%s's remote desktop" doesn't make sense in
       * your language.
       */
      show_username = _("vino-mdns:showusername");
      if (strcmp (show_username, "1") == 0)
        {
          mdns_service_name = g_strdup (g_get_user_name ());
        }
      else
        {
          /* 
           * Translators: this string is used ONLY if you 
           * translated "vino-mdns:showusername" to anything
           * other than "1"
           */
          mdns_service_name = g_strdup_printf (_("%s's remote desktop"),
                                               g_get_user_name ());  
        }
    }

  return mdns_service_name;
}

static void
vino_mdns_make_alternative_service_name (void)
{
  char *alternative_name;

  vino_mdns_get_service_name ();

  g_assert (mdns_service_name != NULL);

  alternative_name = avahi_alternative_service_name (mdns_service_name);
  g_free (mdns_service_name);
  mdns_service_name = alternative_name;
}

static void 
vino_mdns_entry_group_state_changed (AvahiEntryGroup      *entry_group,
                                     AvahiEntryGroupState  state,
                                     AvahiClient          *client) 
{
  switch (state)
    {
    case AVAHI_ENTRY_GROUP_ESTABLISHED:
      dprintf (MDNS, "Avahi: Service '%s' successfully established.\n",
               vino_mdns_get_service_name ());
      break;

    case AVAHI_ENTRY_GROUP_COLLISION:
      vino_mdns_make_alternative_service_name ();

      dprintf (MDNS, "Avahi: Service name collision, renaming service to '%s'\n",
               vino_mdns_get_service_name ());

      vino_mdns_add_services (client);
      break;

    case AVAHI_ENTRY_GROUP_FAILURE:
    case AVAHI_ENTRY_GROUP_UNCOMMITED:
    case AVAHI_ENTRY_GROUP_REGISTERING:
      break;

    default:
      g_assert_not_reached ();
      break;
    }
}

static void
vino_mdns_add_service_foreach (char            *type,
                               gpointer         value,
                               AvahiEntryGroup *entry_group)
{
  int port;
  int ret;

  port = GPOINTER_TO_INT (value);

  ret = avahi_entry_group_add_service (mdns_entry_group, 
                                       AVAHI_IF_UNSPEC, 
                                       AVAHI_PROTO_UNSPEC, 
                                       0,
                                       vino_mdns_get_service_name (), 
                                       type, 
                                       NULL, 
                                       NULL, 
                                       port, 
                                       NULL);
  if (ret < 0)
    {
      dprintf (MDNS, "Avahi: Failed to add %s on port %d : %s\n",
               type, port, avahi_strerror (ret));
    }
}

static void
vino_mdns_add_services (AvahiClient *client) 
{
  int ret = 0;

  if (mdns_entry_group == NULL)
    {
      mdns_entry_group =
        avahi_entry_group_new (client,
                               (AvahiEntryGroupCallback) vino_mdns_entry_group_state_changed,
                               client);
      if (mdns_entry_group == NULL)
        {
	  dprintf (MDNS, "Avahi: avahi_entry_group_new () failed: %s\n",
                   avahi_strerror (avahi_client_errno (client)));
	  return;
	}
    }

  dprintf (MDNS, "Avahi: Adding service '%s'\n",
           vino_mdns_get_service_name ());

  g_hash_table_foreach (mdns_services,
                        (GHFunc) vino_mdns_add_service_foreach,
                        mdns_entry_group);

  if ((ret = avahi_entry_group_commit (mdns_entry_group)) < 0) 
    {
      dprintf (MDNS, "Avahi: Failed to commit entry_group: %s\n",
               avahi_strerror (ret));
    }
}

static void
vino_mdns_client_state_changed (AvahiClient      *client,
                                AvahiClientState  state,
                                void             *userdata) 
{
  switch (state)
    {
    case AVAHI_CLIENT_S_RUNNING:
      vino_mdns_add_services (client);
      break;

    case AVAHI_CLIENT_S_COLLISION:
      if (mdns_entry_group != NULL)
	avahi_entry_group_reset (mdns_entry_group);
      break;

    case AVAHI_CLIENT_FAILURE:
      /* 
       * FIXME: handle disconnection better
       */
      dprintf (MDNS, "Connection with Avahi daemon terminated.\n");
      break;

    case AVAHI_CLIENT_CONNECTING:
    case AVAHI_CLIENT_S_REGISTERING:
      break;

    default:
      g_assert_not_reached ();
      break;
    }
}

void 
vino_mdns_add_service (const char *type,
                       int         port)
{
  if (mdns_services == NULL)
    {
      mdns_services = g_hash_table_new_full (g_str_hash,
                                             g_str_equal,
                                             g_free,
                                             NULL);
    }

  g_hash_table_replace (mdns_services,
                        g_strdup (type),
                        GINT_TO_POINTER (port));
}

void
vino_mdns_start ()
{
  dprintf (MDNS, "Starting MDNS support.\n");

  if (mdns_client != NULL)
    return; /* already started */

  if (mdns_services == NULL)
    return; /* no services */

  avahi_set_allocator (avahi_glib_allocator ());

  mdns_glib_poll = avahi_glib_poll_new (NULL, G_PRIORITY_DEFAULT);
  
  mdns_client = avahi_client_new (avahi_glib_poll_get (mdns_glib_poll),
                                  AVAHI_CLIENT_NO_FAIL,
                                  vino_mdns_client_state_changed,
                                  NULL,
                                  NULL);
  if (mdns_client == NULL)
    {
      dprintf (MDNS, "Failed to create AvahiClient.\n");
      vino_mdns_stop ();
      return;
    }
}

void
vino_mdns_stop (void)
{
  if (mdns_service_name != NULL)
    g_free (mdns_service_name);
  mdns_service_name = NULL;

  if (mdns_entry_group != NULL)
    avahi_entry_group_free (mdns_entry_group);
  mdns_entry_group = NULL;

  if (mdns_client != NULL)
    avahi_client_free (mdns_client);
  mdns_client = NULL;

  if (mdns_glib_poll != NULL)
    avahi_glib_poll_free (mdns_glib_poll);
  mdns_glib_poll = NULL;

  if (mdns_services != NULL)
    g_hash_table_destroy (mdns_services);
  mdns_services = NULL;
}

#else /* !defined (VINO_HAVE_AVAHI) */

void 
vino_mdns_add_service (const char *type,
                       int         port)
{
}

void
vino_mdns_start (void)
{
}

void
vino_mdns_stop (void)
{
}

#endif /* VINO_HAVE_AVAHI */