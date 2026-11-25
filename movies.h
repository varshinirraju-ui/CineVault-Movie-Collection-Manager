/*
 * movies.h - CineVault movie data module
 *
 * Defines the Movie structure (one node of the singly linked list) and every
 * operation the rest of the program uses: creating nodes, inserting,
 * searching, updating, deleting, sorting, statistics and file handling.
 */
#ifndef MOVIES_H
#define MOVIES_H

#include <stddef.h>

#define TITLE_LEN   100
#define GENRE_LEN   50
#define IMAGE_LEN   512
#define MAX_GENRES  64
#define MIN_YEAR    1888          /* year of the first surviving film */

/* One movie record = one node of the singly linked list. */
typedef struct Movie {
    int id;
    char title[TITLE_LEN];
    char genre[GENRE_LEN];
    int year;
    float rating;
    char image[IMAGE_LEN];
    struct Movie *next;           /* pointer to the next node, NULL at the end */
} Movie;

/* Orders the collection view can be sorted in. */
typedef enum {
    SORT_ADDED = 0,               /* list order (order the movies were added) */
    SORT_TITLE,                   /* A to Z */
    SORT_RATING,                  /* highest rating first */
    SORT_YEAR_NEW,                /* newest release first */
    SORT_YEAR_OLD                 /* oldest release first */
} SortKey;

/* Used by the statistics function to count movies per genre. */
typedef struct {
    char name[GENRE_LEN];
    int count;
} GenreCount;

/* ---------- Node and list operations ---------- */
Movie *create_movie(int id, const char *title, const char *genre,
                    int year, float rating, const char *image);
void   insert_movie(Movie **head, Movie *node);          /* append at the tail */
Movie *find_movie(Movie *head, int id);
int    update_movie(Movie *head, int id, const char *title, const char *genre,
                    int year, float rating, const char *image);
Movie *detach_movie(Movie **head, int id, Movie **prev_out);
void   relink_movie(Movie **head, Movie *node, Movie *prev);
int    delete_movie(Movie **head, int id);               /* 1 if a node was freed */
int    free_movies(Movie **head);                        /* returns nodes freed */

/* ---------- Queries ---------- */
int          count_movies(const Movie *head);
float        average_rating(const Movie *head);
const Movie *top_rated(const Movie *head);
int          next_movie_id(const Movie *head);
int          is_duplicate(const Movie *head, const char *title, int year, int ignore_id);
int          genre_counts(const Movie *head, GenreCount *out, int max);
Movie       *filter_movies(const Movie *head, const char *query, const char *genre);
Movie       *sort_movies(Movie *head, SortKey key);      /* merge sort, returns new head */
SortKey      sort_key_from_string(const char *s);

/* ---------- Validation helpers ---------- */
int  current_year(void);
void clean_text(char *dst, const char *src, size_t dst_size);
int  validate_movie(const char *title, const char *genre, int year, float rating,
                    const char *image, char *err, size_t err_size,
                    const char **field);

/* ---------- File handling ---------- */
int load_movies(Movie **head, const char *path);         /* -1 if the file is missing */
int save_movies(const Movie *head, const char *path);    /* 1 on success, 0 on error */

#endif /* MOVIES_H */
