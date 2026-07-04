/*
 * heijean: face generation through eigenspace;
 * author: Giuseppe Cocomazzi;
 * contact: sbudella at gmail dot com;
 * date: december 2010;
 */

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include <SDL/SDL.h>
#include <SDL/SDL_image.h>

#include "meschach/matrix.h"
#include "meschach/matrix2.h"

VEC		* get_vec_from_img(SDL_Surface *);
SDL_Surface	* get_img_from_vec(VEC *, int, int);
void		find_eigenvectors(MAT *, MAT *, MAT *);
void		normalize(VEC *);

int
main(int argc, char *argv[])
{
	SDL_Surface *curimg;
	struct dirent **nl;
	int i, w = 0, h = 0, m = 0, n = 0, combnum, r, oldr = 0;
	VEC **gamma, **gamma_idx, *psi, **fi, **eigenimg, *u, *out;
	MAT *a, *atr, *tmp, *xre, *xim;

	if (argc != 3) {
		printf("Usage: %s <img set path> <out img name>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		printf("%s\n", SDL_GetError());
		exit(EXIT_FAILURE);
	}
	atexit(SDL_Quit);

	n = scandir(argv[1], &nl, 0, alphasort);
	if (n == -1) {
		perror("scandir");
		exit(EXIT_FAILURE);
	}

	gamma = calloc(n, sizeof(VEC *));
	gamma_idx = gamma;

	printf("Acquiring image vectors.\n");
	for (i = 0; i < n; i++) {
		if (strcmp(nl[i]->d_name, ".") == 0 || 
		    strcmp(nl[i]->d_name, "..") == 0)
			continue;

		chdir(argv[1]);
		curimg = IMG_Load(nl[i]->d_name);
		if (curimg == NULL) {
			printf("%s\n", SDL_GetError());
			continue;
		}

		*gamma = get_vec_from_img(curimg);
		if (w == 0 && h == 0) {
			w = curimg->w;
			h = curimg->h;
		}
		gamma++;
		m++;

		SDL_FreeSurface(curimg);
		free(nl[i]);
	}
	free(nl);
	chdir("..");

	gamma = gamma_idx;

	/*
	 * calculate the average image (psi); remember that meschach allows
	 * to do in situ additions between two matrices/vectors, that is
	 * the destination operand need not be different from one of the
	 * addendum matrices/vectors.
	 */
	printf("Calculating the average image vector.\n");
	psi = v_get(gamma[0]->dim);		/* be sure every image is of
						 * the same dimension. */
	for (i = 0; i < m; i++)
		v_add(gamma[i], psi, psi);
	sv_mlt((double) 1 / m, psi, psi);


	/*
	 * subtract the mean image from each gamma vector, in situ;
	 * so the gamma vectors become the fi vectors and just to
	 * avoid bringing confusion about symbols names we introduce
	 * a new symbol pointing to the new data.
	 */
	for (i = 0; i < m; i++)
		v_sub(gamma[i], psi, gamma[i]);
	fi = gamma;
	V_FREE(psi);

	/*
	 * If the number of image vectors is smaller than the
	 * dimension of the vectors themselves, we can do PCA
	 * without calculating the covariance matrix, which
	 * often can be very huge in size.
	 */
	if (m > fi[0]->dim) {
		printf("Cannot perform PCA.\n");
		exit(EXIT_FAILURE);
	}

	/*
	 * Get the matrix whose rows are the mean subtracted
	 * vectors fi.
	 */
	a = m_get(m, fi[0]->dim);
	for (i = 0; i < m; i++) {
		a = set_row(a, i, fi[i]);
		V_FREE(fi[i]);
	}
	free(fi);

	tmp = m_get(m, m);
	mmtr_mlt(a, a, tmp);

	/*
	 * find the eigenvectors of the matrix tmp = aa^t.
	 */ 
	printf("Calculating eigenvectors.\n");
	xre = m_get(tmp->m, tmp->n);
	xim = m_get(tmp->m, tmp->n);
	find_eigenvectors(tmp, xre, xim);
	M_FREE(xim);
	M_FREE(tmp);

	printf("Obtaining eigenimages.\n");
	/* i'th eigenimage is 'a^t' multiplied by the i'th column of 'xre' */
	atr = m_get(a->n, a->m);
	m_transp(a, atr);
	M_FREE(a);

	eigenimg = calloc(m, sizeof(VEC *));
	u = v_get(m);
	for (i = 0; i < xre->m; i++) {
		u = get_col(xre, i, u);
		/*
		 * the first dimension of 'a^t' is the final
		 * dimension of the eigenimg, that is w*h (n^2)
		 * of the image from the starting set.
		 */ 
		eigenimg[i] = v_get(atr->m);
		mv_mlt(atr, u, eigenimg[i]);

		/* normalize eigenfaces */
		normalize(eigenimg[i]);
	}
	V_FREE(u);

	printf("Generating image.\n");
	srand(time(NULL));
	do
		combnum = rand() % m;
	while (combnum < 2);

	out = v_get(eigenimg[0]->dim);
	r = oldr = rand() % m;
	v_copy(eigenimg[r], out);
	for (i = 0; i < combnum; i++) {
		do
			r = rand() % m;
		while (r == oldr);
		oldr = r;
		v_add(eigenimg[r], out, out);
	}
	sv_mlt((double) 1 / combnum, out, out);
	
	curimg = get_img_from_vec(out, w, h);
	SDL_SaveBMP(curimg, argv[2]);

	free(curimg->pixels);
	SDL_FreeSurface(curimg);

	for (i = 0; i < m; i++) 
		V_FREE(eigenimg[i]);
	free(eigenimg);
		
	return (0);
}

VEC *
get_vec_from_img(SDL_Surface *img)
{
	unsigned char *pixel;
	int i, j, bpp, dim = img->w * img->h;
	VEC *v;

	v = v_get(dim);

	SDL_LockSurface(img);
	bpp = img->format->BytesPerPixel;

	for (i = 0; i < img->h; i++)
		for (j = 0; j < img->w; j++) {
			pixel = (unsigned char *) img->pixels +
			    (i * img->pitch + j * bpp);
			v->ve[(i * img->w) + j] = *pixel;
		}

	SDL_UnlockSurface(img);

	return (v);

}

SDL_Surface *
get_img_from_vec(VEC *v, int w, int h)
{
	SDL_Surface *img;
	int *pixels;
	int i;

	pixels = malloc(v->dim * sizeof(int));
	for (i = 0; i < v->dim; i++)
		pixels[i] = (int) v->ve[i];

	img = SDL_CreateRGBSurfaceFrom(pixels, w, h, 32, w * sizeof(int),
	    255, 255, 255, 0);
	if (img == NULL) {
		printf("%s\n", SDL_GetError());
		exit(EXIT_FAILURE);
	}

	return (img);
}

/*
 * Find all the eigenvectors of a given matrix.
 * See meschach tutorial.
 */
void
find_eigenvectors(MAT *a, MAT *xre, MAT *xim)
{
	MAT *t, *q;

	/* schur form */
	q = m_get(a->m, a->n);
	t = m_copy(a, MNULL);
	schur(t, q);

	schur_vecs(t, q, xre, xim);

	return;
}

void
normalize(VEC *v)
{
	double max = 0.0, min = 0.0;
	int i;

	for (i = 0; i < v->dim; i++) {
		if (max < v->ve[i])
			max = v->ve[i];
		if (min > v->ve[i])
			min = v->ve[i];
	} 

	for (i = 0; i < v->dim; i++)
		v->ve[i] = (255 * (v->ve[i] - min) / (max - min));
}
