/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef __GSD_SCREENSAVER_MANAGER_H
#define __GSD_SCREENSAVER_MANAGER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GSD_TYPE_SCREENSAVER_MANAGER         (gsd_screensaver_manager_get_type ())
#define GSD_SCREENSAVER_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GSD_TYPE_SCREENSAVER_MANAGER, GsdScreensaverManager))
#define GSD_SCREENSAVER_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GSD_TYPE_SCREENSAVER_MANAGER, GsdScreensaverManagerClass))
#define GSD_IS_SCREENSAVER_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GSD_TYPE_SCREENSAVER_MANAGER))
#define GSD_IS_SCREENSAVER_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GSD_TYPE_SCREENSAVER_MANAGER))
#define GSD_SCREENSAVER_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GSD_TYPE_SCREENSAVER_MANAGER, GsdScreensaverManagerClass))

typedef struct GsdScreensaverManagerPrivate GsdScreensaverManagerPrivate;

typedef struct
{
        GObject                     parent;
        GsdScreensaverManagerPrivate *priv;
} GsdScreensaverManager;

typedef struct
{
        GObjectClass   parent_class;
} GsdScreensaverManagerClass;

GType                   gsd_screensaver_manager_get_type            (void);

GsdScreensaverManager *       gsd_screensaver_manager_new                 (void);
gboolean                gsd_screensaver_manager_start               (GsdScreensaverManager *manager,
                                                               GError         **error);
void                    gsd_screensaver_manager_stop                (GsdScreensaverManager *manager);

G_END_DECLS

#endif /* __GSD_SCREENSAVER_MANAGER_H */
