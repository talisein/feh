/* imlib.c

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
#include "options.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

Display *disp = NULL;
Visual *vis = NULL;
Screen *scr = NULL;
Colormap cm;
int depth;
Atom wmDeleteWindow;
XContext xid_context = 0;
Window root = 0;

/* Xinerama support */
#ifdef HAVE_LIBXINERAMA
XineramaScreenInfo *xinerama_screens = NULL;
int xinerama_screen;
int num_xinerama_screens;
#endif				/* HAVE_LIBXINERAMA */

#ifdef HAVE_LIBXINERAMA
void init_xinerama(void)
{
	if (opt.xinerama && XineramaIsActive(disp)) {
		int major, minor;
		if (getenv("XINERAMA_SCREEN"))
			xinerama_screen = atoi(getenv("XINERAMA_SCREEN"));
		else
			xinerama_screen = 0;
		XineramaQueryVersion(disp, &major, &minor);
		xinerama_screens = XineramaQueryScreens(disp, &num_xinerama_screens);
	}
}
#endif				/* HAVE_LIBXINERAMA */

void init_imlib_fonts(void)
{
	/* Set up the font stuff */
	imlib_add_path_to_font_path(".");
	imlib_add_path_to_font_path(PREFIX "/share/feh/fonts");

	return;
}

void init_x_and_imlib(void)
{
	disp = XOpenDisplay(NULL);
	if (!disp)
		eprintf("Can't open X display. It *is* running, yeah?");
	vis = DefaultVisual(disp, DefaultScreen(disp));
	depth = DefaultDepth(disp, DefaultScreen(disp));
	cm = DefaultColormap(disp, DefaultScreen(disp));
	root = RootWindow(disp, DefaultScreen(disp));
	scr = ScreenOfDisplay(disp, DefaultScreen(disp));
	xid_context = XUniqueContext();

#ifdef HAVE_LIBXINERAMA
	init_xinerama();
#endif				/* HAVE_LIBXINERAMA */

	imlib_context_set_display(disp);
	imlib_context_set_visual(vis);
	imlib_context_set_colormap(cm);
	imlib_context_set_color_modifier(NULL);
	imlib_context_set_progress_function(NULL);
	imlib_context_set_operation(IMLIB_OP_COPY);
	wmDeleteWindow = XInternAtom(disp, "WM_DELETE_WINDOW", False);

	/* Initialise random numbers */
	srand(getpid() * time(NULL) % ((unsigned int) -1));

	return;
}

int feh_load_image_char(Imlib_Image * im, char *filename)
{
	feh_file *file;
	int i;

	file = feh_file_new(filename);
	i = feh_load_image(im, file);
	feh_file_free(file);
	return(i);
}

int feh_load_image(Imlib_Image * im, feh_file * file)
{
	Imlib_Load_Error err;

	D(("filename is %s, image is %p\n", file->filename, im));

	if (!file || !file->filename)
		return(0);

	/* Handle URLs */
	if ((!strncmp(file->filename, "http://", 7)) || (!strncmp(file->filename, "https://", 8))
			|| (!strncmp(file->filename, "ftp://", 6))) {
		char *tmpname = NULL;
		char *tempcpy;

		tmpname = feh_http_load_image(file->filename);
		if (tmpname == NULL)
			return(0);
		*im = imlib_load_image_with_error_return(tmpname, &err);
		if (im) {
			/* load the info now, in case it's needed after we delete the
			   temporary image file */
			tempcpy = file->filename;
			file->filename = tmpname;
			feh_file_info_load(file, *im);
			file->filename = tempcpy;
		}
		if ((opt.slideshow) && (opt.reload == 0)) {
			/* Http, no reload, slideshow. Let's keep this image on hand... */
			free(file->filename);
			file->filename = estrdup(tmpname);
		} else {
			/* Don't cache the image if we're doing reload + http (webcams etc) */
			if (!opt.keep_http)
				unlink(tmpname);
		}
		if (!opt.keep_http)
			add_file_to_rm_filelist(tmpname);
		free(tmpname);
	} else {
		*im = imlib_load_image_with_error_return(file->filename, &err);
	}

	if ((err) || (!im)) {
		if (opt.verbose && !opt.quiet) {
			fprintf(stdout, "\n");
			reset_output = 1;
		}
		/* Check error code */
		switch (err) {
		case IMLIB_LOAD_ERROR_FILE_DOES_NOT_EXIST:
			if (!opt.quiet)
				weprintf("%s - File does not exist", file->filename);
			break;
		case IMLIB_LOAD_ERROR_FILE_IS_DIRECTORY:
			if (!opt.quiet)
				weprintf("%s - Directory specified for image filename", file->filename);
			break;
		case IMLIB_LOAD_ERROR_PERMISSION_DENIED_TO_READ:
			if (!opt.quiet)
				weprintf("%s - No read access to directory", file->filename);
			break;
		case IMLIB_LOAD_ERROR_UNKNOWN:
		case IMLIB_LOAD_ERROR_NO_LOADER_FOR_FILE_FORMAT:
			if (!opt.quiet)
				weprintf("%s - No Imlib2 loader for that file format", file->filename);
			break;
		case IMLIB_LOAD_ERROR_PATH_TOO_LONG:
			if (!opt.quiet)
				weprintf("%s - Path specified is too long", file->filename);
			break;
		case IMLIB_LOAD_ERROR_PATH_COMPONENT_NON_EXISTANT:
			if (!opt.quiet)
				weprintf("%s - Path component does not exist", file->filename);
			break;
		case IMLIB_LOAD_ERROR_PATH_COMPONENT_NOT_DIRECTORY:
			if (!opt.quiet)
				weprintf("%s - Path component is not a directory", file->filename);
			break;
		case IMLIB_LOAD_ERROR_PATH_POINTS_OUTSIDE_ADDRESS_SPACE:
			if (!opt.quiet)
				weprintf("%s - Path points outside address space", file->filename);
			break;
		case IMLIB_LOAD_ERROR_TOO_MANY_SYMBOLIC_LINKS:
			if (!opt.quiet)
				weprintf("%s - Too many levels of symbolic links", file->filename);
			break;
		case IMLIB_LOAD_ERROR_OUT_OF_MEMORY:
			if (!opt.quiet)
				weprintf("While loading %s - Out of memory", file->filename);
			break;
		case IMLIB_LOAD_ERROR_OUT_OF_FILE_DESCRIPTORS:
			eprintf("While loading %s - Out of file descriptors", file->filename);
			break;
		case IMLIB_LOAD_ERROR_PERMISSION_DENIED_TO_WRITE:
			if (!opt.quiet)
				weprintf("%s - Cannot write to directory", file->filename);
			break;
		case IMLIB_LOAD_ERROR_OUT_OF_DISK_SPACE:
			if (!opt.quiet)
				weprintf("%s - Cannot write - out of disk space", file->filename);
			break;
		default:
			if (!opt.quiet)
				weprintf("While loading %s - Unknown error (%d). Attempting to continue",
						file->filename, err);
			break;
		}
		D(("Load *failed*\n"));
		return(0);
	}

	D(("Loaded ok\n"));
	return(1);
}

char *feh_http_load_image(char *url)
{
	char *tmpname;
	char *basename;
	char *path = NULL;

	if (opt.keep_http) {
		if (opt.output_dir)
			path = opt.output_dir;
		else
			path = "";
	} else
		path = "/tmp/";

	basename = strrchr(url, '/') + 1;
	tmpname = feh_unique_filename(path, basename);

	if (opt.builtin_http) {
		/* state for HTTP header parser */
#define SAW_NONE    1
#define SAW_ONE_CM  2
#define SAW_ONE_CJ  3
#define SAW_TWO_CM  4
#define IN_BODY     5

#define OUR_BUF_SIZE 1024
#define EOL "\015\012"

		int sockno = 0;
		int size;
		int body = SAW_NONE;
		struct addrinfo hints;
		struct addrinfo *result, *rp;
		char *hostname;
		char *get_string;
		char *host_string;
		char *query_string;
		char *get_url;
		static char buf[OUR_BUF_SIZE];
		char ua_string[] = "User-Agent: feh image viewer";
		char accept_string[] = "Accept: image/*";
		FILE *fp;

		D(("using builtin http collection\n"));
		fp = fopen(tmpname, "w");
		if (!fp) {
			weprintf("couldn't write to file %s:", tmpname);
			free(tmpname);
			return(NULL);
		}

		hostname = feh_strip_hostname(url);
		if (!hostname) {
			weprintf("couldn't work out hostname from %s:", url);
			fclose(fp);
			unlink(tmpname);
			free(tmpname);
			return(NULL);
		}

		D(("trying hostname %s\n", hostname));

		memset(&hints, 0, sizeof(struct addrinfo));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = AI_NUMERICSERV;
		hints.ai_protocol = 0;
		if (getaddrinfo(hostname, "80", &hints, &result) != 0) {
			weprintf("error resolving host %s:", hostname);
			fclose(fp);
			unlink(tmpname);
			free(hostname);
			free(tmpname);
			return(NULL);
		}
		for (rp = result; rp != NULL; rp = rp->ai_next) {
			sockno = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
			if (sockno == -1) {
				continue;
			}
			if (connect(sockno, rp->ai_addr, rp->ai_addrlen) != -1) {
				break;
			}
			close(sockno);
		}
		if (rp == NULL) {
			weprintf("error connecting socket:");
			freeaddrinfo(result);
			fclose(fp);
			unlink(tmpname);
			free(tmpname);
			free(hostname);
			return(NULL);
		}
		freeaddrinfo(result);

		get_url = strchr(url, '/') + 2;
		get_url = strchr(get_url, '/');

		get_string = estrjoin(" ", "GET", get_url, "HTTP/1.0", NULL);
		host_string = estrjoin(" ", "Host:", hostname, NULL);
		query_string = estrjoin(EOL, get_string, host_string, accept_string, ua_string, "", "", NULL);
		/* At this point query_string looks something like
		 **
		 **    GET /dir/foo.jpg?123456 HTTP/1.0^M^J
		 **    Host: www.example.com^M^J
		 **    Accept: image/ *^M^J
		 **    User-Agent: feh image viewer^M^J
		 **    ^M^J
		 **
		 ** Host: is required by HTTP/1.1 and very important for some sites,
		 ** even with HTTP/1.0
		 **
		 ** -- BEG
		 */
		if ((send(sockno, query_string, strlen(query_string), 0)) == -1) {
			free(get_string);
			free(host_string);
			free(query_string);
			free(tmpname);
			free(hostname);
			weprintf("error sending over socket:");
			return(NULL);
		}
		free(get_string);
		free(host_string);
		free(query_string);
		free(hostname);

		while ((size = read(sockno, &buf, OUR_BUF_SIZE))) {
			if (body == IN_BODY) {
				fwrite(buf, 1, size, fp);
			} else {
				int i;

				for (i = 0; i < size; i++) {
					/* We are looking for ^M^J^M^J, but will accept
					 ** ^J^J from broken servers. Stray ^Ms will be
					 ** ignored.
					 **
					 ** TODO:
					 ** Checking the headers for a
					 **    Content-Type: image/ *
					 ** header would help detect problems with results.
					 ** Maybe look at the response code too? But there is
					 ** no fundamental reason why a 4xx or 5xx response
					 ** could not return an image, it is just the 3xx
					 ** series we need to worry about.
					 **
					 ** Also, grabbing the size from the Content-Length
					 ** header and killing the connection after that
					 ** many bytes where read would speed up closing the
					 ** socket.
					 ** -- BEG
					 */

					switch (body) {

					case IN_BODY:
						fwrite(buf + i, 1, size - i, fp);
						i = size;
						break;

					case SAW_ONE_CM:
						if (buf[i] == '\012') {
							body = SAW_ONE_CJ;
						} else {
							body = SAW_NONE;
						}
						break;

					case SAW_ONE_CJ:
						if (buf[i] == '\015') {
							body = SAW_TWO_CM;
						} else {
							if (buf[i] == '\012') {
								body = IN_BODY;
							} else {
								body = SAW_NONE;
							}
						}
						break;

					case SAW_TWO_CM:
						if (buf[i] == '\012') {
							body = IN_BODY;
						} else {
							body = SAW_NONE;
						}
						break;

					case SAW_NONE:
						if (buf[i] == '\015') {
							body = SAW_ONE_CM;
						} else {
							if (buf[i] == '\012') {
								body = SAW_ONE_CJ;
							}
						}
						break;

					}	/* switch */
				}	/* for i */
			}
		}		/* while read */
		close(sockno);
		fclose(fp);
	} else {
		int pid;
		int status;

		if ((pid = fork()) < 0) {
			weprintf("open url: fork failed:");
			free(tmpname);
			return(NULL);
		} else if (pid == 0) {
			char *quiet = NULL;

			if (!opt.verbose)
				quiet = estrdup("-q");

			execlp("wget", "wget", "--no-clobber", "--cache=off",
					"-O", tmpname, url, quiet, NULL);
			eprintf("url: Is 'wget' installed? Failed to exec wget:");
		} else {
			waitpid(pid, &status, 0);

			if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
				weprintf("url: wget failed to load URL %s\n", url);
				unlink(tmpname);
				free(tmpname);
				return(NULL);
			}
		}
	}

	return(tmpname);
}

char *feh_strip_hostname(char *url)
{
	char *ret;
	char *start;
	char *finish;
	int len;

	start = strchr(url, '/');
	if (!start)
		return(NULL);

	start += 2;

	finish = strchr(start, '/');
	if (!finish)
		return(NULL);

	len = finish - start;

	ret = emalloc(len + 1);
	strncpy(ret, start, len);
	ret[len] = '\0';
	return(ret);
}

void feh_draw_zoom(winwidget w)
{
	static Imlib_Font fn = NULL;
	int tw = 0, th = 0;
	Imlib_Image im = NULL;
	char buf[100];
	static DATA8 atab[256];

	if (!w->im)
		return;

	if (opt.font)
		fn = gib_imlib_load_font(opt.font);

	if (!fn) {
		fn = gib_imlib_load_font(DEFAULT_FONT);
	}

	if (!fn) {
		weprintf("Couldn't load font for zoom printing");
		return;
	}

	memset(atab, 0, sizeof(atab));

	snprintf(buf, sizeof(buf), "%.0f%%, %dx%d", w->zoom * 100,
			(int) (w->im_w * w->zoom), (int) (w->im_h * w->zoom));

	/* Work out how high the font is */
	gib_imlib_get_text_size(fn, buf, NULL, &tw, &th, IMLIB_TEXT_TO_RIGHT);

	tw += 3;
	th += 3;
	im = imlib_create_image(tw, th);
	if (!im)
		eprintf("Couldn't create image. Out of memory?");

	gib_imlib_image_set_has_alpha(im, 1);
	gib_imlib_apply_color_modifier_to_rectangle(im, 0, 0, tw, th, NULL, NULL, NULL, atab);
	gib_imlib_image_fill_rectangle(im, 0, 0, tw, th, 0, 0, 0, 0);

	gib_imlib_text_draw(im, fn, NULL, 2, 2, buf, IMLIB_TEXT_TO_RIGHT, 0, 0, 0, 255);
	gib_imlib_text_draw(im, fn, NULL, 1, 1, buf, IMLIB_TEXT_TO_RIGHT, 255, 255, 255, 255);
	gib_imlib_render_image_on_drawable(w->bg_pmap, im, 0, w->h - th, 1, 1, 0);
	gib_imlib_free_image_and_decache(im);
	return;
}

void feh_draw_filename(winwidget w)
{
	static Imlib_Font fn = NULL;
	int tw = 0, th = 0;
	Imlib_Image im = NULL;
	static DATA8 atab[256];
	char *s = NULL;
	int len = 0;

	if ((!w->file) || (!FEH_FILE(w->file->data))
			|| (!FEH_FILE(w->file->data)->filename))
		return;

	if (opt.font)
		fn = gib_imlib_load_font(opt.font);

	if (!fn) {
		if (w->full_screen)
			fn = gib_imlib_load_font(DEFAULT_FONT_BIG);
		else
			fn = gib_imlib_load_font(DEFAULT_FONT);
	}

	if (!fn) {
		weprintf("Couldn't load font for filename printing");
		return;
	}

	memset(atab, 0, sizeof(atab));

	/* Work out how high the font is */
	gib_imlib_get_text_size(fn, FEH_FILE(w->file->data)->filename, NULL, &tw, &th, IMLIB_TEXT_TO_RIGHT);

	/* tw is no longer correct, if the filename is shorter than
	 * the string "%d of %d" used below in fullscreen mode */
	tw += 3;
	th += 3;
	im = imlib_create_image(tw, 2 * th);
	if (!im)
		eprintf("Couldn't create image. Out of memory?");

	gib_imlib_image_set_has_alpha(im, 1);
	gib_imlib_apply_color_modifier_to_rectangle(im, 0, 0, tw, 2 * th, NULL, NULL, NULL, atab);
	gib_imlib_image_fill_rectangle(im, 0, 0, tw, 2 * th, 0, 0, 0, 0);

	gib_imlib_text_draw(im, fn, NULL, 2, 2, FEH_FILE(w->file->data)->filename,
			IMLIB_TEXT_TO_RIGHT, 0, 0, 0, 255);
	gib_imlib_text_draw(im, fn, NULL, 1, 1, FEH_FILE(w->file->data)->filename,
			IMLIB_TEXT_TO_RIGHT, 255, 255, 255, 255);
	/* Print the position in the filelist, if we have >=2 files */
	if (gib_list_length(filelist) > 1) {
		/* sic! */
		len = snprintf(NULL, 0, "%d of %d", gib_list_length(filelist), gib_list_length(filelist)) + 1;
		s = emalloc(len);
		snprintf(s, len, "%d of %d", gib_list_num(filelist, current_file) + 1, gib_list_length(filelist));
		/* This should somehow be right-aligned */
		gib_imlib_text_draw(im, fn, NULL, 2, th + 1, s, IMLIB_TEXT_TO_RIGHT, 0, 0, 0, 255);
		gib_imlib_text_draw(im, fn, NULL, 1, th, s, IMLIB_TEXT_TO_RIGHT, 255, 255, 255, 255);
		free(s);
	}

	gib_imlib_render_image_on_drawable(w->bg_pmap, im, 0, 0, 1, 1, 0);

	gib_imlib_free_image_and_decache(im);
	return;
}

void feh_draw_info(winwidget w)
{
	static Imlib_Font fn = NULL;
	int tw = 0, th = 0;
	Imlib_Image im = NULL;
	static DATA8 atab[256];
	int no_lines = 0;
	char *info_cmd;
	char info_buf[256];
	FILE *info_pipe;

	if ((!w->file) || (!FEH_FILE(w->file->data))
			|| (!FEH_FILE(w->file->data)->filename))
		return;

	if (opt.font)
		fn = gib_imlib_load_font(opt.font);

	if (!fn) {
		if (w->full_screen)
			fn = gib_imlib_load_font(DEFAULT_FONT_BIG);
		else
			fn = gib_imlib_load_font(DEFAULT_FONT);
	}

	if (!fn) {
		weprintf("Couldn't load font for filename printing");
		return;
	}

	info_cmd = feh_printf(opt.info_cmd, FEH_FILE(w->file->data));

	memset(atab, 0, sizeof(atab));

	gib_imlib_get_text_size(fn, "w", NULL, &tw, &th, IMLIB_TEXT_TO_RIGHT);

	info_pipe = popen(info_cmd, "r");

	im = imlib_create_image(290 * tw, 20 * th);
	if (!im)
		eprintf("Couldn't create image. Out of memory?");

	gib_imlib_image_set_has_alpha(im, 1);
	gib_imlib_apply_color_modifier_to_rectangle(im, 0, 0, 290 * tw, 20 * th, NULL, NULL, NULL, atab);
	gib_imlib_image_fill_rectangle(im, 0, 0, 290 * tw, 20 * th, 0, 0, 0, 0);

	if (!info_pipe) {
		gib_imlib_text_draw(im, fn, NULL, 2, 2,
				"Error runnig info command", IMLIB_TEXT_TO_RIGHT,
				255, 0, 0, 255);
		gib_imlib_get_text_size(fn, "Error running info command", NULL, &tw, &th,
				IMLIB_TEXT_TO_RIGHT);
		no_lines = 1;
	}
	else {
		while ((no_lines < 20) && fgets(info_buf, 256, info_pipe)) {
			info_buf[strlen(info_buf)-1] = '\0';
			gib_imlib_text_draw(im, fn, NULL, 2, (no_lines*th)+2, info_buf,
					IMLIB_TEXT_TO_RIGHT, 0, 0, 0, 255);
			gib_imlib_text_draw(im, fn, NULL, 1, (no_lines*th)+1, info_buf,
					IMLIB_TEXT_TO_RIGHT, 255, 255, 255, 255);
			no_lines++;
		}
		pclose(info_pipe);
	}


	gib_imlib_render_image_on_drawable(w->bg_pmap, im, 2,
			w->h - (th * no_lines) - 2, 1, 1, 0);

	gib_imlib_free_image_and_decache(im);
	return;
}

char *build_caption_filename(feh_file * file)
{
	char *caption_filename;
	char *s, *dir, *caption_dir;
	struct stat cdir_stat;
	s = strrchr(file->filename, '/');
	if (s) {
		dir = estrdup(file->filename);
		s = strrchr(dir, '/');
		*s = '\0';
	} else {
		dir = estrdup(".");
	}

	caption_dir = estrjoin("/", dir, opt.caption_path, NULL);

	D(("dir %s, cp %s, cdir %s\n", dir, opt.caption_path, caption_dir))

	if (stat(caption_dir, &cdir_stat) == -1) {
		if (mkdir(caption_dir, 0755) == -1)
			eprintf("Failed to create caption directory %s:", caption_dir);
	} else if (!S_ISDIR(cdir_stat.st_mode))
		eprintf("Caption directory (%s) exists, but is not a directory.",
			caption_dir);

	free(caption_dir);

	caption_filename = estrjoin("", dir, "/", opt.caption_path, "/", file->name, ".txt", NULL);
	free(dir);
	return caption_filename;
}

void feh_draw_caption(winwidget w)
{
	static Imlib_Font fn = NULL;
	int tw = 0, th = 0, ww, hh;
	int x, y;
	Imlib_Image im = NULL;
	static DATA8 atab[256];
	char *p;
	gib_list *lines, *l;
	static gib_style *caption_style = NULL;
	feh_file *file;

	if (!w->file) {
		return;
	}
	file = FEH_FILE(w->file->data);
	if (!file->filename) {
		return;
	}

	if (!file->caption) {
		char *caption_filename;
		caption_filename = build_caption_filename(file);
		/* read caption from file */
		file->caption = ereadfile(caption_filename);
		free(caption_filename);
	}

	if (file->caption == NULL) {
		/* caption file is not there, we want to cache that, otherwise we'll stat
		 * the damn file every time we render the image. Reloading an image will
		 * always cause the caption to be reread though so we're safe to do so.
		 * (Before this bit was added, when zooming a captionless image with
		 * captions enabled, the captions file would be stat()d like 30 times a
		 * second) - don't forget this function is called from
		 * winwidget_render_image().
		 */
		file->caption = estrdup("");
	}

	if (*(file->caption) == '\0' && !w->caption_entry)
		return;

	caption_style = gib_style_new("caption");
	caption_style->bits = gib_list_add_front(caption_style->bits,
		gib_style_bit_new(0, 0, 0, 0, 0, 0));
	caption_style->bits = gib_list_add_front(caption_style->bits,
		gib_style_bit_new(1, 1, 0, 0, 0, 255));

	if (opt.font)
		fn = gib_imlib_load_font(opt.font);

	if (!fn) {
		if (w->full_screen)
			fn = gib_imlib_load_font(DEFAULT_FONT_BIG);
		else
			fn = gib_imlib_load_font(DEFAULT_FONT);
	}

	if (!fn) {
		weprintf("Couldn't load font for caption printing");
		return;
	}

	memset(atab, 0, sizeof(atab));

	if (*(file->caption) == '\0') {
		p = estrdup("Caption entry mode - Hit ESC to cancel");
		lines = feh_wrap_string(p, w->w, fn, NULL);
		free(p);
	} else
		lines = feh_wrap_string(file->caption, w->w, fn, NULL);

	if (!lines)
		return;

	/* Work out how high/wide the caption is */
	l = lines;
	while (l) {
		p = (char *) l->data;
		gib_imlib_get_text_size(fn, p, caption_style, &ww, &hh, IMLIB_TEXT_TO_RIGHT);
		if (ww > tw)
			tw = ww;
		th += hh;
		if (l->next)
			th += 1;	/* line spacing */
		l = l->next;
	}

	/* we don't want the caption overlay larger than our window */
	if (th > w->h)
		th = w->h;
	if (tw > w->w)
		tw = w->w;

	im = imlib_create_image(tw, th);
	if (!im)
		eprintf("Couldn't create image. Out of memory?");

	gib_imlib_image_set_has_alpha(im, 1);
	gib_imlib_apply_color_modifier_to_rectangle(im, 0, 0, tw, th, NULL, NULL, NULL, atab);
	gib_imlib_image_fill_rectangle(im, 0, 0, tw, th, 0, 0, 0, 0);

	l = lines;
	x = 0;
	y = 0;
	while (l) {
		p = (char *) l->data;
		gib_imlib_get_text_size(fn, p, caption_style, &ww, &hh, IMLIB_TEXT_TO_RIGHT);
		x = (tw - ww) / 2;
		if (w->caption_entry && (*(file->caption) == '\0'))
			gib_imlib_text_draw(im, fn, caption_style, x, y, p,
				IMLIB_TEXT_TO_RIGHT, 255, 255, 127, 255);
		else if (w->caption_entry)
			gib_imlib_text_draw(im, fn, caption_style, x, y, p,
				IMLIB_TEXT_TO_RIGHT, 255, 255, 0, 255);
		else
			gib_imlib_text_draw(im, fn, caption_style, x, y, p,
				IMLIB_TEXT_TO_RIGHT, 255, 255, 255, 255);

		y += hh + 1;	/* line spacing */
		l = l->next;
	}

	gib_imlib_render_image_on_drawable(w->bg_pmap, im, (w->w - tw) / 2, w->h - th, 1, 1, 0);
	gib_imlib_free_image_and_decache(im);
	gib_list_free_and_data(lines);
	return;
}

unsigned char reset_output = 0;

void feh_display_status(char stat)
{
	static int i = 0;
	static int init_len = 0;
	int j = 0;

	D(("filelist %p, filelist->next %p\n", filelist, filelist->next));

	if (!init_len)
		init_len = gib_list_length(filelist);

	if (i) {
		if (reset_output) {
			/* There's just been an error message. Unfortunate ;) */
			for (j = 0; j < (((i % 50) + ((i % 50) / 10)) + 7); j++)
				fprintf(stdout, " ");
		}

		if (!(i % 50)) {
			int len = gib_list_length(filelist);

			fprintf(stdout, " %5d/%d (%d)\n[%3d%%] ",
					i, init_len, len, ((int) ((float) i / init_len * 100)));

		} else if ((!(i % 10)) && (!reset_output))
			fprintf(stdout, " ");

		reset_output = 0;
	} else
		fprintf(stdout, "[  0%%] ");

	fprintf(stdout, "%c", stat);
	fflush(stdout);
	i++;
	return;
}

void feh_edit_inplace_orient(winwidget w, int orientation)
{
	int ret;
	Imlib_Image old;
	if (!w->file || !w->file->data || !FEH_FILE(w->file->data)->filename)
		return;

	if (!strcmp(gib_imlib_image_format(w->im), "jpeg")) {
		feh_edit_inplace_lossless_rotate(w, orientation);
		feh_reload_image(w, 1, 1);
		return;
	}

	ret = feh_load_image(&old, FEH_FILE(w->file->data));
	if (ret) {
		gib_imlib_image_orientate(old, orientation);
		gib_imlib_save_image(old, FEH_FILE(w->file->data)->filename);
		gib_imlib_free_image(old);
		feh_reload_image(w, 1, 1);
	} else {
		weprintf("failed to load image from disk to edit it in place\n");
	}

	return;
}

gib_list *feh_wrap_string(char *text, int wrap_width, Imlib_Font fn, gib_style * style)
{
	gib_list *ll, *lines = NULL, *list = NULL, *words;
	gib_list *l = NULL;
	char delim[2] = { '\n', '\0' };
	int w, line_width;
	int tw, th;
	char *p, *pp;
	char *line = NULL;
	char *temp;
	int space_width = 0, m_width = 0, t_width = 0, new_width = 0;

	lines = gib_string_split(text, delim);

	if (wrap_width) {
		gib_imlib_get_text_size(fn, "M M", style, &t_width, NULL, IMLIB_TEXT_TO_RIGHT);
		gib_imlib_get_text_size(fn, "M", style, &m_width, NULL, IMLIB_TEXT_TO_RIGHT);
		space_width = t_width - (2 * m_width);
		w = wrap_width;
		l = lines;
		while (l) {
			line_width = 0;
			p = (char *) l->data;
			/* quick check to see if whole line fits okay */
			gib_imlib_get_text_size(fn, p, style, &tw, &th, IMLIB_TEXT_TO_RIGHT);
			if (tw <= w) {
				list = gib_list_add_end(list, estrdup(p));
			} else if (strlen(p) == 0) {
				list = gib_list_add_end(list, estrdup(""));
			} else if (!strcmp(p, " ")) {
				list = gib_list_add_end(list, estrdup(" "));
			} else {
				words = gib_string_split(p, " ");
				if (words) {
					ll = words;
					while (ll) {
						pp = (char *) ll->data;
						if (strcmp(pp, " ")) {
							gib_imlib_get_text_size
							    (fn, pp, style, &tw, &th, IMLIB_TEXT_TO_RIGHT);
							if (line_width == 0)
								new_width = tw;
							else
								new_width = line_width + space_width + tw;
							if (new_width <= w) {
								/* add word to line */
								if (line) {
									int len;

									len = strlen(line)
									    + strlen(pp)
									    + 2;
									temp = emalloc(len);
									snprintf(temp, len, "%s %s", line, pp);
									free(line);
									line = temp;
								} else {
									line = estrdup(pp);
								}
								line_width = new_width;
							} else if (line_width == 0) {
								/* can't fit single word in :/
								   increase width limit to width of word
								   and jam the bastard in anyhow */
								w = tw;
								line = estrdup(pp);
								line_width = new_width;
							} else {
								/* finish this line, start next and add word there */
								if (line) {
									list = gib_list_add_end(list, estrdup(line));
									free(line);
									line = NULL;
								}
								line = estrdup(pp);
								line_width = tw;
							}
						}
						ll = ll->next;
					}
					if (line) {
						/* finish last line */
						list = gib_list_add_end(list, estrdup(line));
						free(line);
						line = NULL;
						line_width = 0;
					}
					gib_list_free_and_data(words);
				}
			}
			l = l->next;
		}
		gib_list_free_and_data(lines);
		lines = list;
	}
	return lines;
}

void feh_edit_inplace_lossless_rotate(winwidget w, int orientation)
{
	char *filename = FEH_FILE(w->file->data)->filename;
	char rotate_str[4];
	int len = strlen(filename) + 1;
	char *file_str = emalloc(len);
	int rotatearg = 90 * orientation;
	int pid, status;

	snprintf(rotate_str, 4, "%d", rotatearg);
	snprintf(file_str, len, "%s", filename);

	if ((pid = fork()) < 0) {
		weprintf("lossless rotate: fork failed:");
		return;
	} else if (pid == 0) {

		execlp("jpegtran", "jpegtran", "-copy", "all", "-rotate",
				rotate_str, "-outfile", file_str, file_str, NULL);

		eprintf("lossless rotate: Is 'jpegtran' installed? Failed to exec:");
	} else {
		waitpid(pid, &status, 0);

		if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
			weprintf("lossless rotate: Got exitcode %d from jpegtran."
					" Commandline was:\n"
					"jpegtran -copy all -rotate %d -outfile %s %s\n",
					status >> 8, rotate_str, file_str, file_str);
			return;
		}
	}
}

void feh_draw_actions(winwidget w)
{
	static Imlib_Font fn = NULL;
	int tw = 0, th = 0;
	int th_offset = 0;
	int max_tw = 0;
	int line_th = 0;
	Imlib_Image im = NULL;
	static DATA8 atab[256];
	int i = 0;
	int num_actions = 0;
	int cur_action = 0;
	char index[2];
	char *line;

	/* Count number of defined actions. This method sucks a bit since it needs
	 * to be changed if the number of actions changes, but at least it doesn't
	 * miss actions 2 to 9 if action1 isn't defined
	 */
	for (i = 0; i < 10; i++) {
		if (opt.actions[i])
			num_actions++;
	}

	if (num_actions == 0)
		return;

	if ((!w->file) || (!FEH_FILE(w->file->data))
			|| (!FEH_FILE(w->file->data)->filename))
		return;

	if (opt.font)
		fn = gib_imlib_load_font(opt.font);

	if (!fn) {
		if (w->full_screen)
			fn = gib_imlib_load_font(DEFAULT_FONT_BIG);
		else
			fn = gib_imlib_load_font(DEFAULT_FONT);
	}

	if (!fn) {
		weprintf("Couldn't load font for actions printing");
		return;
	}

	memset(atab, 0, sizeof(atab));

	gib_imlib_get_text_size(fn, "defined actions:", NULL, &tw, &th, IMLIB_TEXT_TO_RIGHT);
/* Check for the widest line */
	max_tw = tw;

	for (i = 0; i < 10; i++) {
		if (opt.actions[i]) {
			line = emalloc(strlen(opt.actions[i]) + 5);
			strcpy(line, "0: ");
			line = strcat(line, opt.actions[i]);
			gib_imlib_get_text_size(fn, line, NULL, &tw, &th, IMLIB_TEXT_TO_RIGHT);
			free(line);
			if (tw > max_tw)
				max_tw = tw;
		}
	}

	tw = max_tw;
	tw += 3;
	th += 3;
	line_th = th;
	th = (th * num_actions) + line_th;

	/* This depends on feh_draw_filename internals...
	 * should be fixed some time
	 */
	if (opt.draw_filename)
		th_offset = line_th * 2;

	im = imlib_create_image(tw, th);
	if (!im)
		eprintf("Couldn't create image. Out of memory?");

	gib_imlib_image_set_has_alpha(im, 1);
	gib_imlib_apply_color_modifier_to_rectangle(im, 0, 0, tw, th, NULL, NULL, NULL, atab);
	gib_imlib_image_fill_rectangle(im, 0, 0, tw, th, 0, 0, 0, 0);

	gib_imlib_text_draw(im, fn, NULL, 2, 2, "defined actions:", IMLIB_TEXT_TO_RIGHT, 0, 0, 0, 255);
	gib_imlib_text_draw(im, fn, NULL, 1, 1, "defined actions:", IMLIB_TEXT_TO_RIGHT, 255, 255, 255, 255);

	for (i = 0; i < 10; i++) {
		if (opt.actions[i]) {
			cur_action++;
			line = emalloc(strlen(opt.actions[i]) + 5);
			sprintf(index, "%d", i);
			strcpy(line, index);
			strcat(line, ": ");
			strcat(line, opt.actions[i]);

			gib_imlib_text_draw(im, fn, NULL, 2,
					(cur_action * line_th) + 2, line,
					IMLIB_TEXT_TO_RIGHT, 0, 0, 0, 255);
			gib_imlib_text_draw(im, fn, NULL, 1,
					(cur_action * line_th) + 1, line,
					IMLIB_TEXT_TO_RIGHT, 255, 255, 255, 255);
			free(line);
		}
	}

	gib_imlib_render_image_on_drawable(w->bg_pmap, im, 0, 0 + th_offset, 1, 1, 0);

	gib_imlib_free_image_and_decache(im);
	return;
}
