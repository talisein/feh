/* events.c

Copyright (C) 1999-2003 Tom Gilbert.
Copyright (C) 2010-2011 Daniel Friesel.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to
deal in the Software without restriction, including without limitation the
rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies of the Software and its documentation and acknowledgment shall be
given in the documentation and software packages that this Software was
used.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#include "feh.h"
#include "filelist.h"
#include "winwidget.h"
#include "timers.h"
#include "options.h"
#include "events.h"
#include "thumbnail.h"

feh_event_handler *ev_handler[LASTEvent];

static void feh_event_handle_ButtonPress(XEvent * ev);
static void feh_event_handle_ButtonRelease(XEvent * ev);
static void feh_event_handle_ConfigureNotify(XEvent * ev);
static void feh_event_handle_LeaveNotify(XEvent * ev);
static void feh_event_handle_MotionNotify(XEvent * ev);
static void feh_event_handle_ClientMessage(XEvent * ev);

void feh_event_init(void)
{
	int i;

	for (i = 0; i < LASTEvent; i++)
		ev_handler[i] = NULL;

	ev_handler[KeyPress] = feh_event_handle_keypress;
	ev_handler[ButtonPress] = feh_event_handle_ButtonPress;
	ev_handler[ButtonRelease] = feh_event_handle_ButtonRelease;
	ev_handler[ConfigureNotify] = feh_event_handle_ConfigureNotify;
	ev_handler[LeaveNotify] = feh_event_handle_LeaveNotify;
	ev_handler[MotionNotify] = feh_event_handle_MotionNotify;
	ev_handler[ClientMessage] = feh_event_handle_ClientMessage;

	return;
}

static void feh_event_handle_ButtonPress(XEvent * ev)
{
	winwidget winwid = NULL;

	/* get the heck out if it's a mouse-click on the
	   cover, we'll hide the menus on release */
	if (ev->xbutton.window == menu_cover) {
		return;
	}

	winwid = winwidget_get_from_window(ev->xbutton.window);
	if (winwid && winwid->caption_entry) {
		return;
	}

	if (!opt.no_menus && EV_IS_MENU_BUTTON(ev)) {
		D(("Menu Button Press event\n"));
		if (winwid != NULL) {
			winwidget_show_menu(winwid);
		}
	} else if ((ev->xbutton.button == opt.rotate_button)
		   && ((opt.no_rotate_ctrl_mask)
		       || (ev->xbutton.state & ControlMask))) {
		if (winwid != NULL) {
			opt.mode = MODE_ROTATE;
			winwid->mode = MODE_ROTATE;
			D(("rotate starting at %d, %d\n", ev->xbutton.x, ev->xbutton.y));
		}
	} else if ((ev->xbutton.button == opt.blur_button)
		   && ((opt.no_blur_ctrl_mask)
		       || (ev->xbutton.state & ControlMask))) {
		if (winwid != NULL) {
			opt.mode = MODE_BLUR;
			winwid->mode = MODE_BLUR;
			D(("blur starting at %d, %d\n", ev->xbutton.x, ev->xbutton.y));
		}
	} else if (ev->xbutton.button == opt.pan_button) {
		D(("Pan Button Press event\n"));
		if (winwid != NULL) {
			D(("Next button, but could be pan mode\n"));
			opt.mode = MODE_NEXT;
			winwid->mode = MODE_NEXT;
			D(("click offset is %d,%d\n", ev->xbutton.x, ev->xbutton.y));
			winwid->click_offset_x = ev->xbutton.x - winwid->im_x;
			winwid->click_offset_y = ev->xbutton.y - winwid->im_y;
		}
	} else if (ev->xbutton.button == opt.zoom_button) {
		D(("Zoom Button Press event\n"));
		if (winwid != NULL) {
			D(("Zoom mode baby!\n"));
			opt.mode = MODE_ZOOM;
			winwid->mode = MODE_ZOOM;
			D(("click offset is %d,%d\n", ev->xbutton.x, ev->xbutton.y));
			winwid->click_offset_x = ev->xbutton.x;
			winwid->click_offset_y = ev->xbutton.y;
			winwid->old_zoom = winwid->zoom;

			/* required to adjust the image position in zoom mode */
			winwid->im_click_offset_x = (winwid->click_offset_x
					- winwid->im_x) / winwid->old_zoom;
			winwid->im_click_offset_y = (winwid->click_offset_y
					- winwid->im_y) / winwid->old_zoom;
		}
	} else if (ev->xbutton.button == opt.reload_button) {
		D(("Reload Button Press event\n"));
		if (winwid != NULL)
			feh_reload_image(winwid, 0, 0);
	} else if (ev->xbutton.button == opt.prev_button) {
		D(("Prev Button Press event\n"));
		if ((winwid != NULL)
				&& (winwid->type == WIN_TYPE_SLIDESHOW))
			slideshow_change_image(winwid, SLIDE_PREV);
	} else if (ev->xbutton.button == opt.next_button) {
		D(("Next Button Press event\n"));
		if ((winwid != NULL)
				&& (winwid->type == WIN_TYPE_SLIDESHOW))
			slideshow_change_image(winwid, SLIDE_NEXT);
	} else {
		D(("Received other ButtonPress event\n"));
	}
	return;
}

static void feh_event_handle_ButtonRelease(XEvent * ev)
{
	winwidget winwid = NULL;

	if (menu_root) {
		/* if menus are open, close them, and execute action if needed */

		if (ev->xbutton.window == menu_cover) {
			feh_menu_hide(menu_root, True);
		} else if (menu_root) {
			feh_menu *m;

			if ((m = feh_menu_get_from_window(ev->xbutton.window))) {
				feh_menu_item *i = NULL;

				i = feh_menu_find_selected(m);
				feh_menu_item_activate(m, i);
			}
		}
		return;
	}

	winwid = winwidget_get_from_window(ev->xbutton.window);
	if (winwid && winwid->caption_entry) {
		return;
	}

	if ((ev->xbutton.button == opt.menu_button)
			&& (((!opt.menu_ctrl_mask)
					&& ((!(ev->xbutton.state & ControlMask))
						|| ((ev->xbutton.state & ControlMask)
							&& (opt.menu_ctrl_mask)))))
			&& (opt.no_menus))
		winwidget_destroy_all();
	else if (ev->xbutton.button == opt.pan_button) {
		if (opt.mode == MODE_PAN) {
			if (winwid != NULL) {
				D(("Disabling pan mode\n"));
				opt.mode = MODE_NORMAL;
				winwid->mode = MODE_NORMAL;
				winwidget_sanitise_offsets(winwid);
				winwidget_render_image(winwid, 0, 1);
			}
		} else if (opt.mode == MODE_NEXT) {
			opt.mode = MODE_NORMAL;
			if (winwid != NULL)
				winwid->mode = MODE_NORMAL;
			if ((winwid != NULL)
					&& (winwid->type == WIN_TYPE_SLIDESHOW)) {
				slideshow_change_image(winwid, SLIDE_NEXT);
			} else if ((winwid != NULL)
					&& (winwid->type == WIN_TYPE_THUMBNAIL)) {
				feh_file *thumbfile;
				winwidget thumbwin = NULL;
				int x, y;
				char *s;

				x = ev->xbutton.x;
				y = ev->xbutton.y;
				x -= winwid->im_x;
				y -= winwid->im_y;
				x /= winwid->zoom;
				y /= winwid->zoom;
				thumbfile = feh_thumbnail_get_file_from_coords(x, y);
				if (thumbfile) {
					if (!opt.thumb_title)
						s = thumbfile->name;
					else
						s = feh_printf(opt.thumb_title, thumbfile);
					thumbwin = winwidget_get_first_window_of_type(WIN_TYPE_THUMBNAIL_VIEWER);
					if (!thumbwin) {
						thumbwin = winwidget_create_from_file(
								gib_list_add_front(NULL, thumbfile),
								s, WIN_TYPE_THUMBNAIL_VIEWER);
						if (thumbwin)
							winwidget_show(thumbwin);
					} else if (FEH_FILE(thumbwin->file->data) != thumbfile) {
						free(thumbwin->file);
						thumbwin->file = gib_list_add_front(NULL, thumbfile);
						winwidget_rename(thumbwin, s);
						feh_reload_image(thumbwin, 1, 0);
					}
				}
			}
		} else {
			if (winwid != NULL) {
				opt.mode = MODE_NORMAL;
				winwid->mode = MODE_NORMAL;
			}
		}
	} else if ((ev->xbutton.button == opt.rotate_button)
			|| (ev->xbutton.button == opt.zoom_button)) {
		D(("Mode-based Button Release event\n"));
		if (winwid != NULL) {
			D(("Disabling mode\n"));
			opt.mode = MODE_NORMAL;
			winwid->mode = MODE_NORMAL;

			if ((ev->xbutton.button == opt.zoom_button)
					&& (ev->xbutton.x == winwid->click_offset_x)
					&& (ev->xbutton.y == winwid->click_offset_y)) {
				winwid->zoom = 1.0;
				winwidget_center_image(winwid);
			} else
				winwidget_sanitise_offsets(winwid);

			winwidget_render_image(winwid, 0, 1);
		}
	} else if ((ev->xbutton.button == opt.blur_button)
			&& ((opt.no_blur_ctrl_mask)
				|| (ev->xbutton.state & ControlMask))) {
		D(("Blur Button Release event\n"));
		if (winwid != NULL) {
			D(("Disabling Blur mode\n"));
			opt.mode = MODE_NORMAL;
			winwid->mode = MODE_NORMAL;
		}
	}
	return;
}

static void feh_event_handle_ConfigureNotify(XEvent * ev)
{
	while (XCheckTypedWindowEvent(disp, ev->xconfigure.window, ConfigureNotify, ev));
	if (!menu_root) {
		winwidget w = winwidget_get_from_window(ev->xconfigure.window);

		if (w) {
			D(("configure size %dx%d\n", ev->xconfigure.width, ev->xconfigure.height));
			if ((w->w != ev->xconfigure.width)
					|| (w->h != ev->xconfigure.height)) {
				D(("assigning size and rerendering\n"));
				w->w = ev->xconfigure.width;
				w->h = ev->xconfigure.height;
				w->had_resize = 1;
				if (opt.geom_flags & WidthValue || opt.geom_flags & HeightValue) {
					opt.geom_w = w->w;
					opt.geom_h = w->h;
				}
				winwidget_render_image(w, 0, 1);
			}
		}
	}

	return;
}

static void feh_event_handle_LeaveNotify(XEvent * ev)
{
	if ((menu_root) && (ev->xcrossing.window == menu_root->win)) {
		feh_menu_item *ii;

		D(("It is for a menu\n"));
		for (ii = menu_root->items; ii; ii = ii->next) {
			if (MENU_ITEM_IS_SELECTED(ii)) {
				D(("Unselecting menu\n"));
				MENU_ITEM_SET_NORMAL(ii);
				menu_root->updates =
					imlib_update_append_rect(menu_root->updates, ii->x, ii->y, ii->w, ii->h);
				menu_root->needs_redraw = 1;
			}
		}
		feh_raise_all_menus();
	}

	return;
}

static void feh_event_handle_MotionNotify(XEvent * ev)
{
	winwidget winwid = NULL;
	int dx, dy;
	int scr_width, scr_height;

	scr_width = scr->width;
	scr_height = scr->height;
#ifdef HAVE_LIBXINERAMA
	if (opt.xinerama && xinerama_screens) {
		scr_width = xinerama_screens[xinerama_screen].width;
		scr_height = xinerama_screens[xinerama_screen].height;
	}
#endif				/* HAVE_LIBXINERAMA */

	if (menu_root) {
		feh_menu *m;
		feh_menu_item *selected_item, *mouseover_item;

		D(("motion notify with menus open\n"));
		while (XCheckTypedWindowEvent(disp, ev->xmotion.window, MotionNotify, ev));

		if (ev->xmotion.window == menu_cover) {
			return;
		} else if ((m = feh_menu_get_from_window(ev->xmotion.window))) {
			selected_item = feh_menu_find_selected(m);
			mouseover_item = feh_menu_find_at_xy(m, ev->xmotion.x, ev->xmotion.y);

			if (selected_item != mouseover_item) {
				D(("selecting a menu item\n"));
				if (selected_item)
					feh_menu_deselect_selected(m);
				if ((mouseover_item)
						&& ((mouseover_item->action)
							|| (mouseover_item->submenu)
							|| (mouseover_item->func_gen_sub)))
					feh_menu_select(m, mouseover_item);
			}
			/* check if we are close to the right and/or the bottom edge of the
			 * screen. If so, and if the menu we are currently over is partially
			 * hidden, slide the menu to the left and/or up until it is
			 * fully visible */

			/* FIXME: get this working nicely with xinerama screen edges --
			 * at the moment it does really funky stuff with
			 * scr_{width,height} instead of scr->{width,height} -- pabs*/
			if (mouseover_item
					&& ((scr->width - (ev->xmotion.x + m->x)) <
						m->w || (scr->height - (ev->xmotion.y + m->y)) < m->w)) {
				dx = scr_width - (m->x + m->w);
				dy = scr_height - (m->y + m->h);
				dx = dx < 0 ? dx : 0;
				dy = dy < 0 ? dy : 0;
				if (dx || dy)
					feh_menu_slide_all_menus_relative(dx, dy);
			}
			/* if a submenu is open we want to see that also */
			if (mouseover_item && m->next && ((scr->width - (ev->xmotion.x + m->next->x))
						< m->next->w
						|| (scr->height -
							(ev->xmotion.y + m->next->y)) < m->next->w)) {
				dx = scr->width - (m->next->x + m->next->w);
				dy = scr->height - (m->next->y + m->next->h);
				dx = dx < 0 ? dx : 0;
				dy = dy < 0 ? dy : 0;
				if (dx || dy)
					feh_menu_slide_all_menus_relative(dx, dy);
			}
		}
	} else if (opt.mode == MODE_ZOOM) {
		while (XCheckTypedWindowEvent(disp, ev->xmotion.window, MotionNotify, ev));

		winwid = winwidget_get_from_window(ev->xmotion.window);
		if (winwid) {
			if (ev->xmotion.x > winwid->click_offset_x)
				winwid->zoom = winwid->old_zoom + (
						((double) ev->xmotion.x - (double) winwid->click_offset_x)
						/ 128.0);
						/*/ ((double) (winwid->click_offset_x + 1)));*/
			else
				winwid->zoom = winwid->old_zoom - (
						((double) winwid->click_offset_x - (double) ev->xmotion.x)
						/ 128.0);
						/*/ ((double) (winwid->click_offset_x + 1)));*/

			if (winwid->zoom < 0.01)
				winwid->zoom = 0.01;

			/* center around click_offset */
			winwid->im_x = winwid->click_offset_x
					- (winwid->im_click_offset_x * winwid->zoom);
			winwid->im_y = winwid->click_offset_y
					- (winwid->im_click_offset_y * winwid->zoom);

			winwidget_render_image(winwid, 0, 0);
		}
	} else if ((opt.mode == MODE_PAN) || (opt.mode == MODE_NEXT)) {
		int orig_x, orig_y;

		while (XCheckTypedWindowEvent(disp, ev->xmotion.window, MotionNotify, ev));
		winwid = winwidget_get_from_window(ev->xmotion.window);
		if (winwid) {
			if (opt.mode == MODE_NEXT) {
				opt.mode = MODE_PAN;
				winwid->mode = MODE_PAN;
			}
			D(("Panning\n"));
			orig_x = winwid->im_x;
			orig_y = winwid->im_y;

			winwid->im_x = ev->xmotion.x - winwid->click_offset_x;
			winwid->im_y = ev->xmotion.y - winwid->click_offset_y;

			winwidget_sanitise_offsets(winwid);

			D(("im_x %d, im_w %d, off %d, mx %d\n", winwid->im_x,
				winwid->im_w, winwid->click_offset_x, ev->xmotion.x));

			/* XWarpPointer generates a MotionNotify event which we will
			 * parse. Since that event would undo the effect of the pointer
			 * warp, we need to change the click_offset to compensate this.
			 */
			if ((winwid->w - ev->xmotion.x <= 1)
					&& (winwid->click_offset_x >= winwid->w - 4))
			{
				XWarpPointer(disp, None, winwid->win, 0, 0, 0, 0, 3,
					ev->xmotion.y);
				winwid->click_offset_x -= winwid->w - 4;
			}
			else if ((ev->xmotion.x <= 1) && (winwid->click_offset_x
					<= (winwid->im_w * winwid->zoom) - winwid->w - 3))
			{
				XWarpPointer(disp, None, winwid->win, 0, 0, 0, 0,
					winwid->w - 4, ev->xmotion.y);
				winwid->click_offset_x += winwid->w - 4;
			}
			else if ((winwid->h - ev->xmotion.y <= 1)
					&& (winwid->click_offset_y >= winwid->h - 4))
			{
				XWarpPointer(disp, None, winwid->win, 0, 0, 0, 0,
					ev->xmotion.x, 3);
				winwid->click_offset_y -= winwid->h - 4;
			}
			else if ((ev->xmotion.y <= 1) && (winwid->click_offset_y
					<= (winwid->im_h * winwid->zoom) - winwid->h - 3))
			{
				XWarpPointer(disp, None, winwid->win, 0, 0, 0, 0,
					ev->xmotion.x, winwid->h - 4);
				winwid->click_offset_y += winwid->h - 4;
			}

			if ((winwid->im_x != orig_x)
					|| (winwid->im_y != orig_y))
				winwidget_render_image(winwid, 0, 0);
		}
	} else if (opt.mode == MODE_ROTATE) {
		while (XCheckTypedWindowEvent(disp, ev->xmotion.window, MotionNotify, ev));
		winwid = winwidget_get_from_window(ev->xmotion.window);
		if (winwid) {
			D(("Rotating\n"));
			if (!winwid->has_rotated) {
				Imlib_Image temp;

				temp = gib_imlib_create_rotated_image(winwid->im, 0.0);
				winwid->im_w = gib_imlib_image_get_width(temp);
				winwid->im_h = gib_imlib_image_get_height(temp);
				gib_imlib_free_image_and_decache(temp);
				if (!winwid->full_screen && !opt.geom_flags)
					winwidget_resize(winwid, winwid->im_w, winwid->im_h);
				winwid->has_rotated = 1;
			}
			winwid->im_angle = (ev->xmotion.x - winwid->w / 2) / ((double) winwid->w / 2) * 3.1415926535;
			D(("angle: %f\n", winwid->im_angle));
			winwidget_render_image(winwid, 0, 0);
		}
	} else if (opt.mode == MODE_BLUR) {
		while (XCheckTypedWindowEvent(disp, ev->xmotion.window, MotionNotify, ev));
		winwid = winwidget_get_from_window(ev->xmotion.window);
		if (winwid) {
			Imlib_Image temp, ptr;
			signed int blur_radius;

			D(("Blurring\n"));

			temp = gib_imlib_clone_image(winwid->im);
			blur_radius = (((double) ev->xmotion.x / winwid->w) * 20) - 10;
			D(("angle: %d\n", blur_radius));
			if (blur_radius > 0)
				gib_imlib_image_sharpen(temp, blur_radius);
			else
				gib_imlib_image_blur(temp, 0 - blur_radius);
			ptr = winwid->im;
			winwid->im = temp;
			winwidget_render_image(winwid, 0, 0);
			gib_imlib_free_image_and_decache(winwid->im);
			winwid->im = ptr;
		}
	} else {
		while (XCheckTypedWindowEvent(disp, ev->xmotion.window, MotionNotify, ev));
		winwid = winwidget_get_from_window(ev->xmotion.window);
		if (winwid != NULL) {
			if (winwid->type == WIN_TYPE_ABOUT) {
				Imlib_Image orig_im;
				int x, y;

				x = ev->xmotion.x - winwid->im_x;
				y = ev->xmotion.y - winwid->im_y;
				orig_im = winwid->im;
				winwid->im = gib_imlib_clone_image(orig_im);
				imlib_context_set_image(winwid->im);
				imlib_apply_filter("bump_map_point(x=[],y=[],map="
						PREFIX "/share/feh/images/about.png);", &x, &y);
				winwidget_render_image(winwid, 0, 1);
				gib_imlib_free_image_and_decache(winwid->im);
				winwid->im = orig_im;
			} else if (winwid->type == WIN_TYPE_THUMBNAIL) {
				static feh_thumbnail *last_thumb = NULL;
				feh_thumbnail *thumbnail;
				int x, y;

				x = (ev->xbutton.x - winwid->im_x) / winwid->zoom;
				y = (ev->xbutton.y - winwid->im_y) / winwid->zoom;
				thumbnail = feh_thumbnail_get_thumbnail_from_coords(x, y);
				if (thumbnail != last_thumb) {
					if (thumbnail) {
						Imlib_Image origwin;

						origwin = winwid->im;
						winwid->im = gib_imlib_clone_image(origwin);
						gib_imlib_image_fill_rectangle(winwid->im,
								thumbnail->x, thumbnail->y, thumbnail->w,
								thumbnail->h, 50, 50, 255, 100);
						gib_imlib_image_draw_rectangle(winwid->im,
								thumbnail->x, thumbnail->y, thumbnail->w,
								thumbnail->h, 255, 255, 255, 255);
						gib_imlib_image_draw_rectangle(winwid->im,
								thumbnail->x + 1, thumbnail->y + 1,
								thumbnail->w - 2, thumbnail->h - 2,
								0, 0, 0, 255);
						gib_imlib_image_draw_rectangle(winwid->im,
								thumbnail->x + 2, thumbnail->y + 2,
								thumbnail->w - 4, thumbnail->h - 4,
								255, 255, 255, 255);
						winwidget_render_image(winwid, 0, 1);
						gib_imlib_free_image_and_decache(winwid->im);
						winwid->im = origwin;
					} else
						winwidget_render_image(winwid, 0, 1);
				}
				last_thumb = thumbnail;
			}
		}
	}
	return;
}

static void feh_event_handle_ClientMessage(XEvent * ev)
{
	winwidget winwid = NULL;

	if (ev->xclient.format == 32 && ev->xclient.data.l[0] == (signed) wmDeleteWindow) {
		winwid = winwidget_get_from_window(ev->xclient.window);
		if (winwid)
			winwidget_destroy(winwid);
	}

	return;
}
