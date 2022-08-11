/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef __SCRIPT_FU_OPTION_H__
#define __SCRIPT_FU_OPTION_H__

GParamSpec *script_fu_option_get_param_spec (SFScript    *script,
                                              SFArg       *arg,
                                              const gchar *name,
                                              const gchar *nick);

GType     script_fu_option_register_enum     (SFScript  *script,
                                              SFArg     *arg);
GType     script_fu_option_get_gtype         (SFScript  *script,
                                              SFArg     *arg);
GString  *script_fu_option_get_type_name     (SFScript  *script,
                                              SFArg     *arg);

#endif /* __SCRIPT_FU_OPTION_H__ */
