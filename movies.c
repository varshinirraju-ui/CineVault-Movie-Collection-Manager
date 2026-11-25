/*
 * movies.c - CineVault movie operations
 *
 * Everything in this file works on a singly linked list of Movie nodes:
 *
 *   head -> [Movie 1] -> [Movie 2] -> [Movie 3] -> NULL
 *
 * Nodes are created with malloc(), linked through their `next` pointer and
 * released with free(). The list is saved to and loaded from data/movies.dat.
 */
#define __USE_MINGW_ANSI_STDIO 1   /* C99 printf/scanf behaviour on MinGW */

#include "movies.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ------------------------------------------------------------------ */
/* Small string helpers                                                */
/* ------------------------------------------------------------------ */

/* Case-insensitive string compare (ASCII), portable across compilers. */
static int str_icmp(const char *a, const char *b)
{
    while (*a && *b) {
        int ca = tolower((unsigned char)*a);
        int cb = tolower((unsigned char)*b);
        if (ca != cb) return ca - cb;
        a++;
        b++;
    }
    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

/* Case-insensitive "does haystack contain needle?" */
static int str_icontains(const char *haystack, const char *needle)
{
    size_t n = strlen(needle);
    if (n == 0) return 1;
    for (; *haystack; haystack++) {
        size_t i = 0;
        while (i < n && haystack[i] &&
               tolower((unsigned char)haystack[i]) == tolower((unsigned char)needle[i])) {
            i++;
        }
        if (i == n) return 1;
    }
    return 0;
}

/*
 * Copy src into dst (size dst_size) while making it safe to store:
 *  - leading/trailing spaces are trimmed
 *  - '|' (our field separator in movies.dat) becomes '/'
 *  - tabs, newlines and other control characters become spaces
 *  - if the text is too long it is cut without splitting a UTF-8 character
 */
void clean_text(char *dst, const char *src, size_t dst_size)
{
    size_t len = 0;
    if (dst_size == 0) return;
    if (src == NULL) src = "";

    while (*src && isspace((unsigned char)*src)) src++;

    for (; *src && len + 1 < dst_size; src++) {
        unsigned char c = (unsigned char)*src;
        if (c == '|') c = '/';
        else if (c < 32 || c == 127) c = ' ';
        dst[len++] = (char)c;
    }
    /* If we cut inside a multi-byte UTF-8 character, drop its first bytes too. */
    if (*src && ((unsigned char)*src & 0xC0) == 0x80) {
        while (len > 0 && ((unsigned char)dst[len - 1] & 0xC0) == 0x80) len--;
        if (len > 0) len--;          /* the character's lead byte */
    }
    while (len > 0 && isspace((unsigned char)dst[len - 1])) len--;
    dst[len] = '\0';
}

int current_year(void)
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    return t ? t->tm_year + 1900 : 2026;
}

/* ------------------------------------------------------------------ */
/* Node and list operations                                            */
/* ------------------------------------------------------------------ */

/* Allocate a new node on the heap and fill it in. Returns NULL if malloc fails. */
Movie *create_movie(int id, const char *title, const char *genre,
                    int year, float rating, const char *image)
{
    Movie *node = (Movie *)malloc(sizeof(Movie));
    if (node == NULL) return NULL;

    node->id = id;
    clean_text(node->title, title, sizeof(node->title));
    clean_text(node->genre, genre, sizeof(node->genre));
    node->year = year;
    node->rating = rating;
    clean_text(node->image, image, sizeof(node->image));
    node->next = NULL;
    return node;
}

/* Insert at the tail so the list keeps the order in which movies were added. */
void insert_movie(Movie **head, Movie *node)
{
    Movie *cur;
    if (head == NULL || node == NULL) return;

    node->next = NULL;
    if (*head == NULL) {          /* empty list: the new node becomes the head */
        *head = node;
        return;
    }
    cur = *head;
    while (cur->next != NULL) {   /* walk to the last node */
        cur = cur->next;
    }
    cur->next = node;             /* link the last node to the new node */
}

Movie *find_movie(Movie *head, int id)
{
    for (Movie *cur = head; cur != NULL; cur = cur->next) {
        if (cur->id == id) return cur;
    }
    return NULL;
}

int update_movie(Movie *head, int id, const char *title, const char *genre,
                 int year, float rating, const char *image)
{
    Movie *node = find_movie(head, id);
    if (node == NULL) return 0;

    clean_text(node->title, title, sizeof(node->title));
    clean_text(node->genre, genre, sizeof(node->genre));
    node->year = year;
    node->rating = rating;
    clean_text(node->image, image, sizeof(node->image));
    return 1;
}

/*
 * Unlink the node with this id from the list without freeing it.
 * *prev_out receives the node that was before it (NULL if it was the head),
 * so the caller can put it back with relink_movie() if saving fails.
 */
Movie *detach_movie(Movie **head, int id, Movie **prev_out)
{
    Movie *prev = NULL;
    Movie *cur;
    if (head == NULL) return NULL;

    cur = *head;
    while (cur != NULL && cur->id != id) {
        prev = cur;
        cur = cur->next;
    }
    if (cur == NULL) return NULL;           /* not found */

    if (prev == NULL) *head = cur->next;    /* removing the head */
    else prev->next = cur->next;            /* bypass the node */

    cur->next = NULL;
    if (prev_out) *prev_out = prev;
    return cur;
}

/* Put a detached node back where it was (right after prev, or at the head). */
void relink_movie(Movie **head, Movie *node, Movie *prev)
{
    if (head == NULL || node == NULL) return;
    if (prev == NULL) {
        node->next = *head;
        *head = node;
    } else {
        node->next = prev->next;
        prev->next = node;
    }
}

int delete_movie(Movie **head, int id)
{
    Movie *node = detach_movie(head, id, NULL);
    if (node == NULL) return 0;
    free(node);
    return 1;
}

int free_movies(Movie **head)
{
    int freed = 0;
    Movie *cur;
    if (head == NULL) return 0;

    cur = *head;
    while (cur != NULL) {
        Movie *next = cur->next;  /* remember the next node before freeing */
        free(cur);
        cur = next;
        freed++;
    }
    *head = NULL;
    return freed;
}

/* ------------------------------------------------------------------ */
/* Queries                                                             */
/* ------------------------------------------------------------------ */

int count_movies(const Movie *head)
{
    int count = 0;
    for (const Movie *cur = head; cur != NULL; cur = cur->next) count++;
    return count;
}

float average_rating(const Movie *head)
{
    double sum = 0.0;
    int count = 0;
    for (const Movie *cur = head; cur != NULL; cur = cur->next) {
        sum += cur->rating;
        count++;
    }
    return count ? (float)(sum / count) : 0.0f;
}

const Movie *top_rated(const Movie *head)
{
    const Movie *best = NULL;
    for (const Movie *cur = head; cur != NULL; cur = cur->next) {
        if (best == NULL || cur->rating > best->rating) best = cur;
    }
    return best;
}

int next_movie_id(const Movie *head)
{
    int max_id = 0;
    for (const Movie *cur = head; cur != NULL; cur = cur->next) {
        if (cur->id > max_id) max_id = cur->id;
    }
    return max_id + 1;
}

/* Same title (ignoring case) and same year counts as the same movie. */
int is_duplicate(const Movie *head, const char *title, int year, int ignore_id)
{
    char clean[TITLE_LEN];
    clean_text(clean, title, sizeof(clean));
    for (const Movie *cur = head; cur != NULL; cur = cur->next) {
        if (cur->id != ignore_id && cur->year == year && str_icmp(cur->title, clean) == 0) {
            return 1;
        }
    }
    return 0;
}

/* Count movies per genre. Returns the number of different genres found. */
int genre_counts(const Movie *head, GenreCount *out, int max)
{
    int used = 0;
    for (const Movie *cur = head; cur != NULL; cur = cur->next) {
        int i;
        for (i = 0; i < used; i++) {
            if (str_icmp(out[i].name, cur->genre) == 0) {
                out[i].count++;
                break;
            }
        }
        if (i == used && used < max) {
            clean_text(out[used].name, cur->genre, sizeof(out[used].name));
            out[used].count = 1;
            used++;
        }
    }
    /* Most common genre first (simple insertion sort, the array is tiny). */
    for (int i = 1; i < used; i++) {
        GenreCount key = out[i];
        int j = i - 1;
        while (j >= 0 && (out[j].count < key.count ||
                          (out[j].count == key.count && str_icmp(out[j].name, key.name) > 0))) {
            out[j + 1] = out[j];
            j--;
        }
        out[j + 1] = key;
    }
    return used;
}

/*
 * Build a NEW linked list containing copies of the movies whose title contains
 * `query` and whose genre equals `genre` (empty strings mean "any").
 * The original list is not changed. The caller must free the result.
 */
Movie *filter_movies(const Movie *head, const char *query, const char *genre)
{
    Movie *result = NULL;
    Movie *tail = NULL;

    for (const Movie *cur = head; cur != NULL; cur = cur->next) {
        Movie *copy;
        if (query && *query && !str_icontains(cur->title, query)) continue;
        if (genre && *genre && str_icmp(cur->genre, genre) != 0) continue;

        copy = create_movie(cur->id, cur->title, cur->genre, cur->year, cur->rating, cur->image);
        if (copy == NULL) {           /* out of memory: give back what we built */
            free_movies(&result);
            return NULL;
        }
        if (tail == NULL) result = copy;   /* keep a tail pointer: O(1) append */
        else tail->next = copy;
        tail = copy;
    }
    return result;
}

/* ---------- Merge sort on a singly linked list ---------- */

/* Returns non-zero if a should come before b (ties keep the id order). */
static int comes_before(const Movie *a, const Movie *b, SortKey key)
{
    int c = 0;
    switch (key) {
    case SORT_TITLE:    c = str_icmp(a->title, b->title); break;
    case SORT_RATING:   c = (a->rating > b->rating) ? -1 : (a->rating < b->rating) ? 1 : 0; break;
    case SORT_YEAR_NEW: c = b->year - a->year; break;
    case SORT_YEAR_OLD: c = a->year - b->year; break;
    case SORT_ADDED:
    default:            c = 0; break;
    }
    if (c == 0) c = a->id - b->id;
    return c <= 0;
}

/* Split the list in two halves using the slow/fast pointer technique. */
static Movie *split_half(Movie *head)
{
    Movie *slow = head;
    Movie *fast = head->next;
    Movie *second;
    while (fast != NULL && fast->next != NULL) {
        slow = slow->next;
        fast = fast->next->next;
    }
    second = slow->next;
    slow->next = NULL;          /* cut the list after the middle node */
    return second;
}

/* Merge two already-sorted lists by relinking their nodes. */
static Movie *merge_lists(Movie *a, Movie *b, SortKey key)
{
    Movie dummy;
    Movie *tail = &dummy;
    dummy.next = NULL;

    while (a != NULL && b != NULL) {
        if (comes_before(a, b, key)) { tail->next = a; a = a->next; }
        else                         { tail->next = b; b = b->next; }
        tail = tail->next;
    }
    tail->next = (a != NULL) ? a : b;
    return dummy.next;
}

Movie *sort_movies(Movie *head, SortKey key)
{
    Movie *second;
    if (key == SORT_ADDED || head == NULL || head->next == NULL) return head;

    second = split_half(head);
    head = sort_movies(head, key);
    second = sort_movies(second, key);
    return merge_lists(head, second, key);
}

SortKey sort_key_from_string(const char *s)
{
    if (s == NULL) return SORT_ADDED;
    if (strcmp(s, "title") == 0)    return SORT_TITLE;
    if (strcmp(s, "rating") == 0)   return SORT_RATING;
    if (strcmp(s, "year_new") == 0) return SORT_YEAR_NEW;
    if (strcmp(s, "year_old") == 0) return SORT_YEAR_OLD;
    return SORT_ADDED;
}

/* ------------------------------------------------------------------ */
/* Validation                                                          */
/* ------------------------------------------------------------------ */

/*
 * Check the values before a node is created or updated.
 * Returns 1 if valid. Otherwise writes a message into err, points *field at
 * the name of the form field that is wrong and returns 0.
 */
int validate_movie(const char *title, const char *genre, int year, float rating,
                   const char *image, char *err, size_t err_size, const char **field)
{
    char clean[IMAGE_LEN * 2];
    int max_year = current_year() + 5;

    clean_text(clean, title, sizeof(clean));
    if (clean[0] == '\0') {
        snprintf(err, err_size, "Enter a movie title.");
        *field = "title";
        return 0;
    }
    if (strlen(clean) >= TITLE_LEN) {
        snprintf(err, err_size, "Keep the title under %d characters.", TITLE_LEN);
        *field = "title";
        return 0;
    }

    clean_text(clean, genre, sizeof(clean));
    if (clean[0] == '\0') {
        snprintf(err, err_size, "Enter a genre, for example Action or Drama.");
        *field = "genre";
        return 0;
    }
    if (strlen(clean) >= GENRE_LEN) {
        snprintf(err, err_size, "Keep the genre under %d characters.", GENRE_LEN);
        *field = "genre";
        return 0;
    }

    if (year < MIN_YEAR || year > max_year) {
        snprintf(err, err_size, "Release year must be between %d and %d.", MIN_YEAR, max_year);
        *field = "year";
        return 0;
    }

    if (!(rating >= 0.0f && rating <= 10.0f)) {   /* also rejects NaN */
        snprintf(err, err_size, "Rating must be between 0 and 10.");
        *field = "rating";
        return 0;
    }

    clean_text(clean, image, sizeof(clean));
    if (clean[0] != '\0') {
        if (strncmp(clean, "http://", 7) != 0 && strncmp(clean, "https://", 8) != 0) {
            snprintf(err, err_size, "Poster link must start with http:// or https://.");
            *field = "image";
            return 0;
        }
        if (strlen(clean) >= IMAGE_LEN) {
            snprintf(err, err_size, "Poster link is too long (limit %d characters).", IMAGE_LEN - 1);
            *field = "image";
            return 0;
        }
        if (strpbrk(clean, " \"'<>\\") != NULL) {
            snprintf(err, err_size, "Poster link contains characters a URL can't have.");
            *field = "image";
            return 0;
        }
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* File handling: data/movies.dat                                       */
/* ------------------------------------------------------------------ */
/*
 * movies.dat is a plain text file, one movie per line:
 *
 *   id|title|genre|year|rating|image
 *
 * Lines starting with '#' are comments. A text format keeps the file readable
 * (you can open it in VS Code) and independent of struct padding and pointer
 * sizes, which a raw fwrite() of the struct would not be.
 */

/* Split a line on '|' in place. Returns how many fields were found. */
static int split_fields(char *line, char *fields[], int max)
{
    int count = 0;
    char *start = line;
    for (char *p = line; ; p++) {
        if (*p == '|' || *p == '\0') {
            int at_end = (*p == '\0');
            if (count < max) fields[count] = start;
            count++;
            *p = '\0';
            if (at_end) break;
            start = p + 1;
        }
    }
    return count;
}

int load_movies(Movie **head, const char *path)
{
    char line[1024];
    int loaded = 0;
    int line_no = 0;
    FILE *fp = fopen(path, "r");

    if (fp == NULL) return -1;          /* no file yet: start with an empty list */

    while (fgets(line, sizeof(line), fp) != NULL) {
        char *fields[6];
        char *end;
        size_t len = strlen(line);
        long id, year;
        double rating;
        Movie *node;

        line_no++;
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) line[--len] = '\0';
        if (len == 0 || line[0] == '#') continue;

        if (split_fields(line, fields, 6) != 6) {
            fprintf(stderr, "  movies.dat line %d skipped: expected 6 fields\n", line_no);
            continue;
        }
        id = strtol(fields[0], &end, 10);
        if (*end != '\0' || id <= 0) {
            fprintf(stderr, "  movies.dat line %d skipped: bad id\n", line_no);
            continue;
        }
        year = strtol(fields[3], &end, 10);
        if (*end != '\0') {
            fprintf(stderr, "  movies.dat line %d skipped: bad year\n", line_no);
            continue;
        }
        rating = strtod(fields[4], &end);
        if (*end != '\0') {
            fprintf(stderr, "  movies.dat line %d skipped: bad rating\n", line_no);
            continue;
        }
        if (find_movie(*head, (int)id) != NULL) {
            fprintf(stderr, "  movies.dat line %d skipped: duplicate id %ld\n", line_no, id);
            continue;
        }

        node = create_movie((int)id, fields[1], fields[2], (int)year, (float)rating, fields[5]);
        if (node == NULL) {
            fprintf(stderr, "  Out of memory while loading movies.dat\n");
            break;
        }
        insert_movie(head, node);
        loaded++;
    }
    fclose(fp);
    return loaded;
}

/*
 * Write the whole list to a temporary file first, then replace movies.dat.
 * If the program stops halfway through writing, the old file is still intact.
 */
int save_movies(const Movie *head, const char *path)
{
    char tmp_path[512];
    FILE *fp;

    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
    fp = fopen(tmp_path, "w");
    if (fp == NULL) return 0;

    fprintf(fp, "# CineVault movie records - one movie per line\n");
    fprintf(fp, "# id|title|genre|year|rating|image\n");
    for (const Movie *cur = head; cur != NULL; cur = cur->next) {
        fprintf(fp, "%d|%s|%s|%d|%.1f|%s\n",
                cur->id, cur->title, cur->genre, cur->year, cur->rating, cur->image);
    }
    if (ferror(fp)) {
        fclose(fp);
        remove(tmp_path);
        return 0;
    }
    if (fclose(fp) != 0) {
        remove(tmp_path);
        return 0;
    }

    remove(path);                      /* rename() on Windows fails if the target exists */
    if (rename(tmp_path, path) != 0) return 0;
    return 1;
}
