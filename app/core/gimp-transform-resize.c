/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995-2001 Spencer Kimball, Peter Mattis, and others
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

#include "config.h"

#include <string.h>

#include <gio/gio.h>
#include <gegl.h>

#include "libgimpmath/gimpmath.h"

#include "core-types.h"

#include "gimp-transform-resize.h"
#include "gimp-transform-utils.h"
#include "gimp-utils.h"


#define EPSILON 0.00000001


typedef struct
{
  GimpVector2 a, b, c, d;
  gdouble     area;
  gdouble     aspect;
  GimpVector2 target_centre;
  GimpVector2 current_centre;
} Rectangle;

typedef struct
{
  gdouble       area [5];
  GimpVector2   coord [5];
} t_rectangle_areas;

gdouble     side_min_x [5];
gdouble     side_max_x [5];
gdouble     side_min_y [5];
gdouble     side_max_y [5];
gdouble     side_gradient [5];      /* (not valid for vertical sides) */
gdouble     side_offset [5];        /* (not valid for vertical sides) */
gboolean    side_vertical [5];
gboolean    side_horizontal [5];

static void      gimp_transform_resize_adjust    (const GimpVector2 *points,
                                                  gint               n_points,
                                                  gint              *x1,
                                                  gint              *y1,
                                                  gint              *x2,
                                                  gint              *y2);
static void      gimp_transform_resize_crop      (const GimpVector2 *points,
                                                  gint               n_points,
                                                  gdouble            aspect,
                                                  gint              *x1,
                                                  gint              *y1,
                                                  gint              *x2,
                                                  gint              *y2);

/*
 * This function wants to be passed the inverse transformation matrix!!
 */
gboolean
gimp_transform_resize_boundary (const GimpMatrix3   *inv,
                                GimpTransformResize  resize,
                                gdouble              u1,
                                gdouble              v1,
                                gdouble              u2,
                                gdouble              v2,
                                gint                *x1,
                                gint                *y1,
                                gint                *x2,
                                gint                *y2)
{
  GimpVector2 bounds[4];
  GimpVector2 points[5];
  gint        n_points;
  gboolean    valid;
  gint        i;

  g_return_val_if_fail (inv != NULL, FALSE);

  /*  initialize with the original boundary  */
  *x1 = floor (u1);
  *y1 = floor (v1);
  *x2 = ceil  (u2);
  *y2 = ceil  (v2);

  /* if clipping then just return the original rectangle */
  if (resize == GIMP_TRANSFORM_RESIZE_CLIP)
    return TRUE;

  bounds[0] = (GimpVector2) { u1, v1 };
  bounds[1] = (GimpVector2) { u2, v1 };
  bounds[2] = (GimpVector2) { u2, v2 };
  bounds[3] = (GimpVector2) { u1, v2 };

  gimp_transform_polygon (inv, bounds, 4, TRUE,
                          points, &n_points);

  valid = (n_points >= 2);

  /*  check if the transformation matrix is valid at all  */
  for (i = 0; i < n_points && valid; i++)
    valid = (isfinite (points[i].x) && isfinite (points[i].y));

  if (! valid)
    {
      /* since there is no sensible way to deal with this, just do the same as
       * with GIMP_TRANSFORM_RESIZE_CLIP: return
       */
      return FALSE;
    }

  switch (resize)
    {
    case GIMP_TRANSFORM_RESIZE_ADJUST:
      /* return smallest rectangle (with sides parallel to x- and y-axis)
       * that surrounds the new points */
      gimp_transform_resize_adjust (points, n_points,
                                    x1, y1, x2, y2);
      break;

    case GIMP_TRANSFORM_RESIZE_CROP:
      gimp_transform_resize_crop (points, n_points,
                                  0.0,
                                  x1, y1, x2, y2);
      break;

    case GIMP_TRANSFORM_RESIZE_CROP_WITH_ASPECT:
      gimp_transform_resize_crop (points, n_points,
                                  (u2 - u1) / (v2 - v1),
                                  x1, y1, x2, y2);
      break;

    case GIMP_TRANSFORM_RESIZE_CLIP:
      /* Remove warning about not handling all enum values. We handle
       * this case in the beginning of the function
       */
      break;
    }

  /* ensure that resulting rectangle has at least area 1 */
  if (*x1 == *x2)
    (*x2)++;

  if (*y1 == *y2)
    (*y2)++;

  return TRUE;
}

/* this calculates the smallest rectangle (with sides parallel to x- and
 * y-axis) that contains the points d1 to d4
 */
static void
gimp_transform_resize_adjust (const GimpVector2 *points,
                              gint               n_points,
                              gint              *x1,
                              gint              *y1,
                              gint              *x2,
                              gint              *y2)
{
  GimpVector2 top_left;
  GimpVector2 bottom_right;
  gint        i;

  top_left = bottom_right = points[0];

  for (i = 1; i < n_points; i++)
    {
      top_left.x     = MIN (top_left.x,     points[i].x);
      top_left.y     = MIN (top_left.y,     points[i].y);

      bottom_right.x = MAX (bottom_right.x, points[i].x);
      bottom_right.y = MAX (bottom_right.y, points[i].y);
    }

  *x1 = (gint) floor (top_left.x + EPSILON);
  *y1 = (gint) floor (top_left.y + EPSILON);

  *x2 = (gint) ceil (bottom_right.x - EPSILON);
  *y2 = (gint) ceil (bottom_right.y - EPSILON);
}

/* Check for a new maximum rectangle area. The area is constrained by an aspect
   ratio if r->aspect != 0.0 */

static gdouble
check_for_new_max_area (Rectangle    *r,
                        GimpVector2   lower_left_corner,
                        gdouble       height,
                        gdouble       width)
{
  gdouble       area;       /* the area of the rectangle (taking into account
                               the aspect ratio if appropriate */
  gdouble       new_height = height;
  gdouble       new_width = width;
  GimpVector2   new_centre;

  if (r->aspect != 0)
    {
      /* looking for the largest rectangle of the specified aspect ratio */
      if (r->aspect > 1.0)
        {
          /* looking for a landscape orientation */
          if (height >= width / r->aspect)
            new_height = width / r->aspect;
          else
            new_width = height * r->aspect;
        }
      else
        {
          /* looking for a square or portrait orientation */
          if (width >= height * r->aspect)
            new_width = height * r->aspect;
          else
            new_height = width / r->aspect;
        }

      if (new_height < height)
        lower_left_corner.y -= (height - new_height) / 2.0;

      if (new_width < width)
        lower_left_corner.x += (width - new_width) / 2.0;

    }

  area = new_width * new_height;
  new_centre.x = lower_left_corner.x + new_width / 2.0;
  new_centre.y = lower_left_corner.y - new_height / 2.0;

  if ((area > r->area)                  ||
      ((area == r->area) &&
        ((fabs (new_centre.x - r->target_centre.x) <=
             fabs (r->current_centre.x - r->target_centre.x)) ||
         (fabs (new_centre.y - r->target_centre.y) <=
             fabs (r->current_centre.y - r->target_centre.y)))))
    {
      /*  found a new maximum area or an area the same size but closer
       *  to the centre of the shape
       *  corners are:      a    b
       *                    d    c
       */
      r->area = area;
      r->d = lower_left_corner;
      r->c.x = r->d.x + new_width;
      r->c.y = r->d.y;
      r->a.x = r->d.x;
      r->a.y = r->d.y - new_height;
      r->b.x = r->c.x;
      r->b.y = r->a.y;
      r->current_centre = new_centre;
    }
  return area;
}

/* Compare an array of 5 areas to determine which is the largest. The largest
 * and the adjacent areas are transferred to elementa 0, 2 and 4 of the array
 * and the x-offsets for array elements 1 and 3 set to be mid-way between
 * elements 0 and 2, and 2 and 4 respectively.
 *
 * If the separation between adjacent x coordinates is <= 0.5 the function
 * performs no processing and returns a result of FALSE to indicate that the
 * search for the largest rectangle has completed. Otherwise the
 * processing noted above is performed and the function returns TRUE
 */

static gboolean
compare_areas (t_rectangle_areas *rectangle_areas)
{
  gdouble   max_area = 0.0;
  gint      max_area_index = 0;
  gint      centre_index;
  gint      width_of_max = 0;
  gint      i;
  gboolean  result;

  for (i = 0; i < 5; i++)
    {
      if (rectangle_areas->area[i] == max_area)
        width_of_max++;

      if (rectangle_areas->area[i] > max_area)
        {
          width_of_max = 1;
          max_area_index = i;
          max_area = rectangle_areas->area[i];
        }
    }

 if (((fabs(rectangle_areas->coord[1].x -
            rectangle_areas->coord[0].x) <= 0.5) &&
       (fabs(rectangle_areas->coord[1].y -
             rectangle_areas->coord[0].y) <= 0.5)) ||
       (max_area == 0.0))
    {
      /*  reached the minimum spacing between the readings or no valid results
       * - stop the search
       */
      result = FALSE;
    }
  else
    {
      result = TRUE;

      centre_index = max_area_index + (width_of_max / 2);

      if (centre_index == 4)
        centre_index = 3;

      if (centre_index == 0)
        centre_index++;

      /* now set the three peak areas into elements 0, 2 and 4 of the array */

      rectangle_areas->area[0] = rectangle_areas->area[centre_index - 1];
      rectangle_areas->area[4] = rectangle_areas->area[centre_index + 1];
      rectangle_areas->area[2] = rectangle_areas->area[centre_index];

      rectangle_areas->coord[0] = rectangle_areas->coord[centre_index - 1];
      rectangle_areas->coord[4] = rectangle_areas->coord[centre_index + 1];
      rectangle_areas->coord[2] = rectangle_areas->coord[centre_index];

      rectangle_areas->coord[1].x = rectangle_areas->coord[0].x +
         ((rectangle_areas->coord[2].x - rectangle_areas->coord[0].x) / 2.0);
      rectangle_areas->coord[1].y = rectangle_areas->coord[0].y +
         ((rectangle_areas->coord[2].y - rectangle_areas->coord[0].y) / 2.0);
      rectangle_areas->coord[3].x = rectangle_areas->coord[2].x +
         ((rectangle_areas->coord[4].x - rectangle_areas->coord[2].x) / 2.0);
      rectangle_areas->coord[3].y = rectangle_areas->coord[2].y +
         ((rectangle_areas->coord[4].y - rectangle_areas->coord[2].y) / 2.0);
      rectangle_areas->area[1] = 0.0;
      rectangle_areas->area[3] = 0.0;
    }

  return result;
}

/* Initialise the t_rectangle_areas structure used to find the rectangle of
 * maximum area
 */

static void
init_rectangle_areas (t_rectangle_areas    *rectangle_areas,
                      GimpVector2           point1,
                      GimpVector2           point2)
{
  gint          i;
  gdouble       x_reading_spacing;
  gdouble       y_reading_spacing;

  /* (doesn't matter if x_reading_spacing is negative) */
  x_reading_spacing = (point2.x - point1.x) / 4.0;
  y_reading_spacing = (point2.y - point1.y) / 4.0;

  rectangle_areas->coord[0] = point1;
  rectangle_areas->coord[4] = point2;

  for (i = 1; i < 4; i++)
    {
      rectangle_areas->coord[i].x = point1.x + (x_reading_spacing * i);
      rectangle_areas->coord[i].y = point1.y + (y_reading_spacing * i);
    }

  for (i = 0; i < 5; i++)
    rectangle_areas->area[i] = 0.0;
}

/* Find the intersection of a horizontal line specified point on one side of the
 * polygon with another side.
 *
 * On entry:
 *  The following arrays must have been set-up:
 *          side_min_x [5]
 *          side_max_x [5]
 *          side_min_y [5]
 *          side_max_y [5]
 *          side_gradient [5]
 *          side_offset [5]
 *          side_vertical [5]
 *          side_horizontal [5]
 *
 * The function will return FALSE if the line passes through a vertex which is
 * at the top or bottom of the polygon.
 */

static gboolean
find_horizontal_intersection (GimpVector2           point,
                              const  GimpVector2   *points,
                              gint                  point_side,
                              gint                 *intersect_side,
                              gint                  num_sides,
                              GimpVector2          *intersection)
{
  gint      i;
  gboolean  intersection_found = FALSE;

  i = 0;
  while ((i < num_sides) && !intersection_found)
    {
      if ((i != point_side)                           &&
          (point.x != points[i].x)                    &&  /* not at the shared
                                                             vertex with another
                                                             side */
          (point.y != points[i].y)                    &&
          (point.x != points[(i + 1) % num_sides].x)  &&
          (point.y != points[(i + 1) % num_sides].y))
        if ((point.y >= side_min_y[i]) &&
            (point.y <= side_max_y[i]))
          {
            intersection_found = TRUE;
            *intersect_side = i;
            intersection->y = point.y;
            if (side_vertical[i])
              intersection->x = side_min_x[i];
            else
              intersection->x = (point.y - side_offset[i])/side_gradient[i];
          }
      i++;
    }
  return intersection_found;
}

/* Find the intersection of a vertical line specified point on one side of the
 * polygon with another side.
 *
 * On entry:
 *  The following arrays must have been set-up:
 *          side_min_x [5]
 *          side_max_x [5]
 *          side_min_y [5]
 *          side_max_y [5]
 *          side_gradient [5]
 *          side_offset [5]
 *          side_vertical [5]
 *          side_horizontal [5]
 *
 * The function will return FALSE if the line passes through a vertex which is
 * on the leftmost or rightmost point of the polygon.
 */

static gboolean
find_vertical_intersection (GimpVector2           point,
                            const  GimpVector2   *points,
                            gint                  point_side,
                            gint                 *intersect_side,
                            gint                  num_sides,
                            GimpVector2          *intersection)
{
  gint      i;
  gboolean  intersection_found = FALSE;

  i = 0;
  while ((i < num_sides) && !intersection_found)
    {
      if ((i != point_side)                             &&
          (point.x != points[i].x)                      &&  /* not at the shared
                                                               vertex with
                                                               another side */
          (point.y != points[i].y)                      &&
          (point.x != points[(i + 1) % num_sides].x)    &&
          (point.y != points[(i + 1) % num_sides].y))
        if ((point.x >= side_min_x[i]) &&
            (point.x <= side_max_x[i]))
          {
            intersection_found = TRUE;
            *intersect_side = i;
            intersection->x = point.x;
            intersection->y = (point.x * side_gradient[i]) + side_offset[i];
          }
      i++;
    }
  return intersection_found;
}

/* Find the intersection of a horizontal line in the specified direction from
 * the specified point with a side of the polygon.
 *
 * On entry:
 *      right =  TRUE for right, FALSE for left
 */

static gboolean
horizontal_intersection_from_internal_point (GimpVector2    point,
                                             gint           num_sides,
                                             gboolean       right,
                                             GimpVector2   *i1)
{
  gint      i;
  gboolean  intersection_found = FALSE;

  i = 0;
  while ((i< num_sides) && !intersection_found)
  {
    if ((point.y >= side_min_y[i]) && (point.y <= side_max_y[i]))
      {
        if (side_vertical[i])
          i1->x = side_min_x[i];
        else
          i1->x = (point.y - side_offset[i])/side_gradient[i];

        if (((i1->x > point.x) && right) ||
            ((i1->x < point.x) && !right))
          {
            i1->y = point.y;
            intersection_found = TRUE;
          }
      }
    i++;
  }

  return intersection_found;
}

/* Find the intersection of a vertical line in the specified direction from the
 * specified point with a side of the polygon
 *
 * On entry:
 *      upwards = TRUE for up, FALSE for down
 */

static gboolean
vertical_intersection_from_internal_point (GimpVector2    point,
                                           gint           num_sides,
                                           gboolean       upwards,
                                           GimpVector2   *i1)
{
  gint      i;
  gboolean  intersection_found = FALSE;

  i = 0;
  while ((i< num_sides) && !intersection_found)
  {
    if ((point.x >= side_min_x[i]) &&
        (point.x <= side_max_x[i]) &&
        (!side_vertical[i]))
      {
        i1->y = (point.x * side_gradient[i]) + side_offset[i];

        /* (upwards is to lower y values) */
        if (((i1->y > point.y) && !upwards) ||
            ((i1->y < point.y) && upwards))
          {
            i1->x = point.x;
            intersection_found = TRUE;
          }
      }
    i++;
  }

  return intersection_found;
}

/* Take a reading for the function two_orthogonals_area()
 *
 *  On entry:
 *      index = index into sub_rectangular_areas
 *      corner = a point on a sloping side
 *      horizontal_intersection = the point at which a horizontal line
 *                                from corner intersects another side
 *      vertical_intersection = the point at which a horizontal line from
 *                              corner intersects another side
 *      up_or_right - TRUE => up or right, FALSE => down or left
 */

static void
two_orthogonals_reading (Rectangle           *r,
                         t_rectangle_areas   *sub_rectangle_areas,
                         gint                 index,
                         gint                 num_sides,
                         GimpVector2          corner,
                         GimpVector2          horizontal_intersection,
                         GimpVector2          vertical_intersection,
                         gboolean             from_horizontal_line,
                         gboolean             up_or_right)

{
  gboolean      result;
  gdouble       height;
  gdouble       width;
  GimpVector2   lower_left_corner;
  GimpVector2   intersection;
  GimpVector2   point;

  point = sub_rectangle_areas->coord[index];

  if (from_horizontal_line)
    result = vertical_intersection_from_internal_point (point,
                                                        num_sides,
                                                        up_or_right,
                                                        &intersection);
  else
    result = horizontal_intersection_from_internal_point (point,
                                                          num_sides,
                                                          up_or_right,
                                                          &intersection);

  if (result)
    {
      lower_left_corner = corner;

      if (from_horizontal_line)
        {
          if (up_or_right)
            {
              height = point.y - MAX (intersection.y, vertical_intersection.y);
            }
          else
            {
              height = MIN (intersection.y, vertical_intersection.y) - point.y;
              lower_left_corner.y = point.y + height;
            }

          width = fabs (corner.x - point.x);
          lower_left_corner.x = MIN (point.x, corner.x);
        }
      else
        {
          height = fabs (corner.y - point.y);
          if (up_or_right)
            {
              width = MIN (intersection.x, horizontal_intersection.x) - point.x;
            }
          else
            {
              width = point.x - MAX (intersection.x, horizontal_intersection.x);
              lower_left_corner.x = point.x - width;
            }
          lower_left_corner.y = MAX (point.y, corner.y);
        }

      sub_rectangle_areas->area[index] =
                check_for_new_max_area (r,
                                        lower_left_corner,
                                        height,
                                        width);
    }
}

/* Find the maximum area rectangle in a polygon from a point on a sloping side
 *
 * On entry:
 *      index = index into rectangular_areas
 *      point = the point on a sloping side
 *      horizontal_intersect = where the horizontal line from point intersects
 *                             the polygon side
 *      vertical_intersect = where the horizontal line from point intersects the
 *                           polygon side
 */

static void
two_orthogonals_area (Rectangle           *r,
                      t_rectangle_areas   *rectangle_areas,
                      gint                 index,
                      gint                 num_sides,
                      GimpVector2          point,
                      GimpVector2          horizontal_intersect,
                      GimpVector2          vertical_intersect)

{
  t_rectangle_areas   sub_rectangle_areas;
  GimpVector2         end_point;
  gboolean            process_horizontal;
  gboolean            right_or_up;
  gint                i;
  gdouble             max_area;

  if (fabs (point.x - horizontal_intersect.x) >
      fabs (point.y - vertical_intersect.y))
    /* go for the greatest resolution. Note that processing both
     * horizontal and vertical lines may result in a slightly
     * larger area being determined but at a cost of increasing
     * the processing time and in practice the difference is not
     * noticable. If users are really concerned with total
     * accuracy then, presumably, they would be using the Adjust
     * mode and then manually cropping.
     */
    {
      process_horizontal = TRUE;
      end_point = horizontal_intersect;
      right_or_up = (point.y > vertical_intersect.y);
    }
  else
    {
      process_horizontal = FALSE;
      end_point = vertical_intersect;
      right_or_up = (point.x < horizontal_intersect.x);
    }

  init_rectangle_areas (&sub_rectangle_areas,
                         point,
                         end_point);



  for (i = 0; i < 5; i++)
    /* take the initial readings */
    {
      two_orthogonals_reading (r,
                               &sub_rectangle_areas,
                               i,
                               num_sides,
                               point,
                               horizontal_intersect,
                               vertical_intersect,
                               process_horizontal,
                               right_or_up);
    }

  while (compare_areas (&sub_rectangle_areas))
    {
      /* only need readings 1 and 3 on subsequent passes */
      two_orthogonals_reading (r,
                               &sub_rectangle_areas,
                               1,
                               num_sides,
                               point,
                               horizontal_intersect,
                               vertical_intersect,
                               process_horizontal,
                               right_or_up);
      two_orthogonals_reading (r,
                              &sub_rectangle_areas,
                               3,
                               num_sides,
                               point,
                               horizontal_intersect,
                               vertical_intersect,
                               process_horizontal,
                               right_or_up);
    }

  max_area = 0.0;
  for (i = 0; i < 5; i++)
    if (sub_rectangle_areas.area[i] > max_area)
      max_area = sub_rectangle_areas.area[i];

  rectangle_areas->area[index] = max_area;
}

/* Check for the maximum area rectangle in an orthogonal triangle - taking into
 * acount the aspect ratio if appropriate.
 *
 * On entry:
 *      corner =  the coordinates of the right-angle corner
 *      p1 and p2 are the coordinates of the ends of the hypotenuse
 *      index = the index into rectangle_areas
 */

static void
check_orthogonal_triangle (Rectangle             *r,
                           t_rectangle_areas     *rectangle_areas,
                           gint                   index,
                           GimpVector2            corner,
                           GimpVector2            p1,
                           GimpVector2            p2)
{
  gdouble   height;
  gdouble   width;
  gboolean  corner_at_top;
  gboolean  corner_at_right;

  if (p1.y == corner.y)
    {
      /* corner to p1 is horizontal */
      width = fabs (p1.x - corner.x);
      if (p1.x < corner.x)
        corner_at_right = TRUE;
      else
        corner_at_right = FALSE;

      height = fabs (p2.y - corner.y);
      if (p2.y < corner.y)
        corner_at_top = FALSE;
      else
        corner_at_top = TRUE;
    }
  else
    {
      /* corner to p2 is horizontal */
      width = fabs (p2.x - corner.x);
      if (p2.x < corner.x)
        corner_at_right = TRUE;
      else
        corner_at_right = FALSE;

      height = fabs (p1.y - corner.y);
      if (p1.y < corner.y)
        corner_at_top = FALSE;
      else
        corner_at_top = TRUE;
    }

  if (r->aspect == 0.0)
    {
      /* just want the maximum area rectangle */
      height /= 2.0;
      width /= 2.0;
    }
  else
    {
      /* need the largest rectangle of the specified aspect ratio */
      if (height != 0.0 && width != 0)
        {
          height = width / (width/height + r->aspect);
          width = r->aspect * height;
        }
    }

  if (corner_at_right)
    corner.x -= width;

  if (corner_at_top)
    corner.y += height;

  rectangle_areas->area[index] = check_for_new_max_area (r,
                                                         corner,
                                                         height,
                                                         width);
}

/* Check for the maximum area rectangle of the specified aspect ratio */

static void
check_aspect_ratio_rectangle (Rectangle             *r,
                              t_rectangle_areas     *rectangle_areas,
                              gint                   index,
                              GimpVector2            corner,
                              GimpVector2            horizontal_intersect,
                              GimpVector2            vertical_intersect,
                              gint                   num_sides)

{
  gdouble       diagonal_gradient;
  gdouble       diagonal_offset;
  GimpVector2   diagonal_intersect;
  GimpVector2   lower_left_corner;
  gdouble       height;
  gdouble       width;
  gdouble       width_limit;
  gboolean      intersect_right;
  gboolean      intersect_upwards;
  gboolean      intersection_found;
  gboolean      possible_intersection;
  gint          i;

  diagonal_gradient = 1.0 / r->aspect;      /* +ve gradient used if corner is
                                               top right or bottom left */

  if (vertical_intersect.y > corner.y)
    {                                       /* corner is at the top of the
                                               rectangle (lower y value) */
      intersect_upwards = FALSE;

      if (horizontal_intersect.x > corner.x)
        {
                                            /* corner is at top left */
          intersect_right = TRUE;
        }
      else
        {
          intersect_right = FALSE;          /* corner is in top rigt */
          diagonal_gradient *= -1.0;
        }
    }
  else
    {                                       /* corner is at the bottom of the
                                               rectangle */
      intersect_upwards = TRUE;

      if (horizontal_intersect.x > corner.x)
        {                                   /* corner is at bottom right */
          diagonal_gradient *= -1.0;
          intersect_right = TRUE;           /* corner is at bottom left */
        }
      else
        {
          intersect_right = FALSE;
        }
    }

  diagonal_offset = corner.y - (corner.x * diagonal_gradient);

  intersection_found = FALSE;
  i = 0;
  while ((i < num_sides) && !intersection_found)
    {
      possible_intersection = FALSE;
      if (diagonal_gradient != side_gradient[i])
        {
          /* the two lines must intersect somewhere (but this may not be
             between the endpoints of side i) */
          if (side_vertical[i])
            {
              diagonal_intersect.x = side_min_x[i];
              diagonal_intersect.y = diagonal_intersect.x *
                     diagonal_gradient + diagonal_offset;
              if ((diagonal_intersect.y >= side_min_y[i]) &&
                  (diagonal_intersect.y <= side_max_y[i]))
                possible_intersection = TRUE;
            }
          else
            {
              diagonal_intersect.x = (side_offset[i] - diagonal_offset) /
                                        (diagonal_gradient - side_gradient[i]);
              diagonal_intersect.y = (diagonal_intersect.x *
                     diagonal_gradient) + diagonal_offset;

              if ((diagonal_intersect.x >= side_min_x[i]) &&
                  (diagonal_intersect.x <= side_max_x[i]))
                possible_intersection = TRUE;
            }

          if (possible_intersection)
            {
              if ((((diagonal_intersect.x > corner.x) && intersect_right) ||
                   ((diagonal_intersect.x < corner.x) && !intersect_right)) &&
                  (((diagonal_intersect.y < corner.y) && intersect_upwards) ||
                   ((diagonal_intersect.y > corner.y) && !intersect_upwards)))
                intersection_found = TRUE;
            }
        }
      i++;
    }

  if (intersection_found)
    {                                   /* should always get here */
      if (intersect_upwards)
        height = corner.y - MAX (diagonal_intersect.y, vertical_intersect.y);
      else
        height = MIN (diagonal_intersect.y, vertical_intersect.y) - corner.y;

      width = height * r->aspect;

      width_limit = fabs (corner.x - horizontal_intersect.x);
      if (width > width_limit)
        {
          width = width_limit;
          height = width / r->aspect;
        }

      lower_left_corner = corner;
      if (!intersect_upwards)
        lower_left_corner.y += height;
      if (!intersect_right)
        lower_left_corner.x -= width;

      rectangle_areas->area[index] = check_for_new_max_area (r,
                                                             lower_left_corner,
                                                             height,
                                                             width);
    }
}

/* Find the rectangle area associated with a point on a sloping
 * (non-ortho-aligned) side of the polygon.
 *
 * Note that this function relies on the fact that a horizontal or vertical line
 * from a point on one side of the polygon will intersect one, and only one,
 * other side (unless the point is at a vertex at the top, bottom, left or right
 * of the polygon)
 *
 *  On entry:
 *      side = 0 for the line defined by points[0]..points[1] etc.
 */

static void
sloping_side_reading (Rectangle            *r,
                      t_rectangle_areas    *rectangle_areas,
                      gint                  index,
                      const  GimpVector2   *points,
                      gint                  num_sides,
                      gint                  side)
{
  GimpVector2   start_corner;
  GimpVector2   horizontal_intersect;
  GimpVector2   vertical_intersect;
  gint          horizontal_intersect_side;
  gint          vertical_intersect_side;


  start_corner = rectangle_areas->coord[index];

  if (find_horizontal_intersection (start_corner,
                                    points,
                                    side,
                                    &horizontal_intersect_side,
                                    num_sides,
                                    &horizontal_intersect)          &&
      find_vertical_intersection (start_corner,
                                  points,
                                  side,
                                  &vertical_intersect_side,
                                  num_sides,
                                  &vertical_intersect))
    {
      if (r->aspect == 0.0)
        {
          if (horizontal_intersect_side == vertical_intersect_side)
              /* have an orthogonal triangle formed by start_point and
                 the two intersection points */
              check_orthogonal_triangle (r,
                                         rectangle_areas,
                                         index,
                                         start_corner,
                                         horizontal_intersect,
                                         vertical_intersect);
          else
            two_orthogonals_area (r,
                                  rectangle_areas,
                                  index,
                                  num_sides,
                                  start_corner,
                                  horizontal_intersect,
                                  vertical_intersect);
        }
      else
        {
          if (horizontal_intersect_side == vertical_intersect_side)
            /* have an orthogonal triangle formed by start_point and
               the two intersection points */
            check_orthogonal_triangle (r,
                                       rectangle_areas,
                                       index,
                                       start_corner,
                                       horizontal_intersect,
                                       vertical_intersect);
          else
            check_aspect_ratio_rectangle (r,
                                          rectangle_areas,
                                          index,
                                          start_corner,
                                          horizontal_intersect,
                                          vertical_intersect,
                                          num_sides);
        }
    }
}

/* Process a sloping side in a polygon
 *
 * On entry:
 *    side = 0 for the line defined by points[0]..points[1] etc.
 */

static void
process_sloping_side (Rectangle            *r,
                      const  GimpVector2   *points,
                      gint                  num_sides,
                      gint                  side)
{
  gint                 i;
  t_rectangle_areas    rectangle_areas;


  init_rectangle_areas (&rectangle_areas,
                        points[side],
                        points[(side + 1) % num_sides]);

  for (i = 0; i < 5; i++)                   /* take the initial readings */
    sloping_side_reading (r,
                          &rectangle_areas,
                          i,
                          points,
                          num_sides,
                          side);
  while (compare_areas (&rectangle_areas))
    {
      /* only need readings 1 and 3 on subsequent passes */
      sloping_side_reading (r,
                            &rectangle_areas,
                            1,
                            points,
                            num_sides,
                            side);

      sloping_side_reading (r,
                            &rectangle_areas,
                            3,
                            points,
                            num_sides,
                            side);
    }
}

/* Record the area of the rectangle or the area of the largest rectangle of the
 * specified aspect ratio
 */

static void
handle_rectangle (Rectangle           *r,
                  const  GimpVector2  *points)
{
  gdouble       height;
  gdouble       width;
  GimpVector2   lower_left_corner;

  lower_left_corner.x =
                 MIN4 (points[0].x, points[1].x, points[2].x, points[3].x);
  lower_left_corner.y =
                 MAX4 (points[0].y, points[1].y, points[2].y, points[3].y);
  height = lower_left_corner.y -
                 MIN4 (points[0].y, points[1].y, points[2].y, points[3].y);
  width = MAX4 (points[0].x, points[1].x, points[2].x, points[3].x) -
                lower_left_corner.x;

  check_for_new_max_area(r,
                         lower_left_corner,
                         height,
                         width);
}


static void
gimp_transform_resize_crop (const GimpVector2 *orig_points,
                            gint               n_points,
                            gdouble            aspect,
                            gint              *x1,
                            gint              *y1,
                            gint              *x2,
                            gint              *y2)
{
  GimpVector2 points[5];
  Rectangle   r;
  GimpVector2 t,a;
  gint        i, j;
  gint        min;
  gdouble     min_x;
  gdouble     max_x;
  gdouble     min_y;
  gdouble     max_y;

  gint        num_horizontal_sides;
  gint        num_vertical_sides;

  memcpy (points, orig_points, sizeof (GimpVector2) * n_points);

  /* find lowest, rightmost corner of surrounding rectangle */
  a.x = 0;
  a.y = 0;
  for (i = 0; i < n_points; i++)
    {
      if (points[i].x < a.x)
        a.x = points[i].x;

      if (points[i].y < a.y)
        a.y = points[i].y;
    }

  /* and translate all the points to the first quadrant */
  for (i = 0; i < n_points; i++)
    {
      points[i].x += (-a.x) * 2;
      points[i].y += (-a.y) * 2;
    }

  /* find the convex hull using Jarvis's March as the points are passed
   * in different orders due to gimp_matrix3_transform_point()
   */
  min = 0;
  for (i = 0; i < n_points; i++)
    {
      if (points[i].y < points[min].y)
        min = i;
    }

  t = points[0];
  points[0] = points[min];
  points[min] = t;

  for (i = 1; i < n_points - 1; i++)
    {
      gdouble min_theta;
      gdouble min_mag;
      int next;

      next = n_points - 1;
      min_theta = 2.0 * G_PI;
      min_mag = DBL_MAX;

      for (j = i; j < n_points; j++)
        {
          gdouble theta;
          gdouble sy;
          gdouble sx;
          gdouble mag;

          sy = points[j].y - points[i - 1].y;
          sx = points[j].x - points[i - 1].x;

          if ((sx == 0.0) && (sy == 0.0))
            {
              next = j;
              break;
            }

          theta = atan2 (-sy, -sx);
          mag = (sx * sx) + (sy * sy);

          if ((theta < min_theta) ||
              ((theta == min_theta) && (mag < min_mag)))
            {
              min_theta = theta;
              min_mag = mag;
              next = j;
            }
        }

      t = points[i];
      points[i] = points[next];
      points[next] = t;
    }

  /* reverse the order of points */
  for (i = 0; i < n_points / 2; i++)
    {
      t                        = points[i];
      points[i]                = points[n_points - i - 1];
      points[n_points - i - 1] = t;
    }

  r.a.x = r.a.y = r.b.x = r.b.y = r.c.x = r.c.y = r.d.x = r.d.y = r.area = 0;
  r.aspect = aspect;

  min_x = points[0].x;
  max_x = points[0].x;
  min_y = points[0].y;
  max_y = points[0].y;

  num_horizontal_sides = 0;
  num_vertical_sides = 0;

  for (i = 0; i < n_points; i++)
    {
      /* set up arrays to speed later processing */
      if (points[i].x < min_x)
        min_x = points[i].x;
      if (points[i].x > max_x)
        max_x = points[i].x;
      if (points[i].y < min_y)
        min_y = points[i].y;
      if (points[i].y > max_y)
        max_y = points[i].y;

      j = (i + 1) % n_points;
      side_vertical[i] = FALSE;
      side_horizontal[i] = FALSE;

      if (points[i].x < points[j].x)
        {
          side_min_x[i] = points[i].x;
          side_max_x[i] = points[j].x;
        }
      else
        {
          side_min_x[i] = points[j].x;
          side_max_x[i] = points[i].x;
       }

       if (points[i].y < points[j].y)
        {
          side_min_y[i] = points[i].y;
          side_max_y[i] = points[j].y;
        }
      else
        {
          side_min_y[i] = points[j].y;
          side_max_y[i] = points[i].y;
        }

      if (side_min_y[i] == side_max_y[i])
        {
          num_horizontal_sides++;
          side_horizontal[i] = TRUE;
        }

      if (side_min_x[i] == side_max_x[i])
        {
          num_vertical_sides++;
          side_vertical[i] = TRUE;
        }

      if (!side_vertical[i])
        {
          /* calculate the equation for the side */
          side_gradient[i] = (points[j].y - points[i].y) /
                             (points[j].x - points[i].x);
          side_offset[i] = points[i].y - (points[i].x * side_gradient[i]);
        }
    }

  /* r.target_centre is used to try to centre the clipped area and stop it
   * dancing around as the shape is altered
   */
  r.target_centre.x = min_x + (max_x - min_x) / 2.0;
  r.target_centre.y = min_y + (max_y - min_y) / 2.0;

  if (((num_horizontal_sides + num_vertical_sides) == 4) && (n_points == 4))
    handle_rectangle(&r,
                      points);
  else
    for (i = 0; i < n_points; i++)
      if (!side_horizontal[i] && !side_vertical[i])
        process_sloping_side(&r,
                              points,
                              n_points,
                              i);

  if (r.area == 0)
    {
      /* saveguard if something went wrong, adjust and give warning */
      gimp_transform_resize_adjust (orig_points, n_points,
                                    x1, y1, x2, y2);
      g_printerr ("no rectangle found by algorithm, no cropping done\n");
    }
  else
    {
      /* round and translate the calculated points back */
      *x1 = ceil (r.a.x);
      *y1 = ceil (r.a.y);
      *x2 = floor  (r.c.x + 0.5);
      *y2 = floor  (r.c.y + 0.5);

      *x1 = *x1 - ((-a.x) * 2);
      *y1 = *y1 - ((-a.y) * 2);
      *x2 = *x2 - ((-a.x) * 2);
      *y2 = *y2 - ((-a.y) * 2);

    }
}
