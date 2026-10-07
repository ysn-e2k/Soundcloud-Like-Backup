#ifndef LIKES_H
#define LIKES_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define LIKES_BATCH 70

typedef struct {
    char **titles;
    char **artists;
    char *liked;
    int count;
    int cap;
    int next;
    int day;
    char last[16];
} Likes;

void likes_init(Likes *l);
void likes_free(Likes *l);
char *likes_read_file(const char *name);
int likes_parse(Likes *l, const char *html);
int likes_load(Likes *l, const char *html_file);
void likes_set_liked(Likes *l, int idx, int value);
int likes_batch_rows(const Likes *l);
int likes_line(const Likes *l, int idx, char *out, size_t size);
void likes_today(char *out);
int likes_save(const Likes *l, const char *file);
int likes_load_state(Likes *l, const char *file);
int likes_log(const Likes *l, const char *file, int from, int to, const char *date);
int likes_done(Likes *l, const char *date, const char *log_file);

#endif
