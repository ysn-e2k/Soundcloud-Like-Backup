#include "likes.h"

#define STATE_TEST "test_progress.txt"
#define LOG_TEST "test_log.txt"

static int g_run = 0;
static int g_fail = 0;

#define CHECK(c) do { g_run++; if (!(c)) { g_fail++; printf("  FAIL line %d: %s\n", __LINE__, #c); } } while (0)
#define RUN(f) do { printf("%s\n", #f); f(); } while (0)

static char *build_html(int n)
{
    char *h = (char *)malloc((size_t)n * 220 + 1), *o = h;
    int j;
    if (!h) return NULL;
    for (j = 0; j < n; j++)
        o += sprintf(o, "<div class=\"sound streamContext\"><span class=\"soundTitle__usernameText\">Artist %d</span>"
                        "<a class=\"soundTitle__title sc-link-dark\"><span>Song %d</span></a></div>\n", n - j, n - j);
    *o = 0;
    return h;
}

static void test_parse_order_and_entities(void)
{
    const char *html =
        "<div class=\"sound streamContext\"><span class=\"soundTitle__usernameText\">Daft &amp; Punk</span>"
        "<a class=\"soundTitle__title sc-link-dark\"><span> Rock &#x27;n&#39; Roll &lt;3 </span></a></div>"
        "<div class=\"sound streamContext\"><span class=\"soundTitle__usernameText\">Caf&#233;</span>"
        "<a class=\"soundTitle__title sc-link-dark\"><span>Middle</span></a></div>"
        "<div class=\"sound streamContext\">"
        "<a class=\"soundTitle__title sc-link-dark\"><span>Oldest</span></a></div>";
    Likes l;
    likes_init(&l);
    CHECK(likes_parse(&l, html) == 3);
    CHECK(strcmp(l.titles[0], "Oldest") == 0);
    CHECK(strcmp(l.titles[1], "Middle") == 0);
    CHECK(strcmp(l.titles[2], "Rock 'n' Roll <3") == 0);
    CHECK(l.artists[0] == NULL);
    CHECK(strcmp(l.artists[1], "Caf\xC3\xA9") == 0);
    CHECK(strcmp(l.artists[2], "Daft & Punk") == 0);
    likes_free(&l);
}

static void test_parse_empty_and_null(void)
{
    Likes l;
    likes_init(&l);
    CHECK(likes_parse(&l, "<html></html>") == 0);
    CHECK(likes_parse(&l, NULL) == 0);
    CHECK(likes_load(&l, "does_not_exist.html") == 0);
    likes_free(&l);
}

static void test_line_format(void)
{
    char *html = build_html(3);
    char buf[128];
    Likes l;
    likes_init(&l);
    likes_parse(&l, html);
    CHECK(likes_line(&l, 0, buf, sizeof buf) == 1);
    CHECK(strcmp(buf, "Song 1 - Artist 1") == 0);
    CHECK(likes_line(&l, 5, buf, sizeof buf) == 0);
    CHECK(likes_line(&l, -1, buf, sizeof buf) == 0);
    likes_free(&l);
    free(html);
}

static void test_batch_basic(void)
{
    char *html = build_html(200);
    Likes l;
    likes_init(&l);
    likes_parse(&l, html);
    CHECK(l.count == 200);
    CHECK(likes_batch_rows(&l) == LIKES_BATCH);
    likes_free(&l);
    free(html);
}

static void test_batch_small_list(void)
{
    char *html = build_html(10);
    Likes l;
    likes_init(&l);
    likes_parse(&l, html);
    CHECK(likes_batch_rows(&l) == 10);
    likes_free(&l);
    free(html);
}

static void test_batch_already_liked(void)
{
    char *html = build_html(200);
    Likes l;
    likes_init(&l);
    likes_parse(&l, html);
    likes_set_liked(&l, 0, 1);
    CHECK(likes_batch_rows(&l) == 71);
    likes_set_liked(&l, 70, 1);
    CHECK(likes_batch_rows(&l) == 72);
    likes_set_liked(&l, 71, 1);
    CHECK(likes_batch_rows(&l) == 73);
    likes_set_liked(&l, 0, 0);
    CHECK(likes_batch_rows(&l) == LIKES_BATCH);
    likes_set_liked(&l, 999, 1);
    likes_set_liked(&l, -1, 1);
    CHECK(likes_batch_rows(&l) == LIKES_BATCH);
    likes_free(&l);
    free(html);
}

static void test_done_advances(void)
{
    char *html = build_html(100);
    Likes l;
    likes_init(&l);
    likes_parse(&l, html);
    CHECK(likes_done(&l, "2026-01-01", NULL) == 70);
    CHECK(l.next == 70);
    CHECK(l.day == 2);
    CHECK(likes_batch_rows(&l) == 30);
    CHECK(likes_done(&l, "2026-01-02", NULL) == 30);
    CHECK(l.next == 100);
    CHECK(l.day == 3);
    CHECK(likes_done(&l, "2026-01-03", NULL) == 0);
    likes_free(&l);
    free(html);
}

static void test_done_once_per_day(void)
{
    char *html = build_html(200);
    Likes l;
    likes_init(&l);
    likes_parse(&l, html);
    CHECK(likes_done(&l, "2026-01-01", NULL) == 70);
    CHECK(likes_done(&l, "2026-01-01", NULL) == 0);
    CHECK(l.next == 70);
    CHECK(likes_done(&l, "2026-01-05", NULL) == 70);
    CHECK(l.next == 140);
    likes_free(&l);
    free(html);
}

static void test_skipped_day_keeps_batch(void)
{
    char *html = build_html(200);
    Likes l;
    likes_init(&l);
    likes_parse(&l, html);
    likes_done(&l, "2026-01-01", NULL);
    CHECK(likes_done(&l, "2026-02-15", NULL) == 70);
    CHECK(l.next == 140);
    likes_free(&l);
    free(html);
}

static void test_save_and_load_state(void)
{
    char *html = build_html(100);
    Likes a, b;
    likes_init(&a);
    likes_init(&b);
    likes_parse(&a, html);
    likes_parse(&b, html);
    likes_set_liked(&a, 5, 1);
    likes_set_liked(&a, 80, 1);
    CHECK(likes_done(&a, "2026-03-10", NULL) == 71);
    CHECK(likes_save(&a, STATE_TEST) == 1);
    CHECK(likes_load_state(&b, STATE_TEST) == 1);
    CHECK(b.next == 71);
    CHECK(b.day == 2);
    CHECK(strcmp(b.last, "2026-03-10") == 0);
    CHECK(b.liked[80] == 1);
    CHECK(likes_batch_rows(&b) == 29);
    CHECK(likes_done(&b, "2026-03-10", NULL) == 0);
    likes_free(&a);
    likes_free(&b);
    free(html);
}

static void test_load_state_missing_file(void)
{
    char *html = build_html(10);
    Likes l;
    likes_init(&l);
    likes_parse(&l, html);
    CHECK(likes_load_state(&l, "no_such_state.txt") == 0);
    CHECK(l.next == 0);
    CHECK(l.day == 1);
    likes_free(&l);
    free(html);
}

static void test_load_state_clamped(void)
{
    char *html = build_html(10);
    FILE *f = fopen(STATE_TEST, "w");
    Likes l;
    fprintf(f, "next=500\nday=9\nlast_done=2026-01-01\nalready=3,999\n");
    fclose(f);
    likes_init(&l);
    likes_parse(&l, html);
    CHECK(likes_load_state(&l, STATE_TEST) == 1);
    CHECK(l.next == 10);
    CHECK(l.day == 9);
    CHECK(l.liked[3] == 1);
    likes_free(&l);
    free(html);
}

static void test_log_file(void)
{
    char *html = build_html(100);
    char *log;
    Likes l;
    likes_init(&l);
    likes_parse(&l, html);
    likes_set_liked(&l, 5, 1);
    CHECK(likes_done(&l, "2026-04-01", LOG_TEST) == 71);
    CHECK(likes_done(&l, "2026-04-02", LOG_TEST) == 29);
    log = likes_read_file(LOG_TEST);
    CHECK(log != NULL);
    if (log) {
        CHECK(strstr(log, "2026-04-01  DONE  day 1  songs 1-71 of 100") != NULL);
        CHECK(strstr(log, "(already liked: #6)") != NULL);
        CHECK(strstr(log, "2026-04-02  DONE  day 2  songs 72-100 of 100") != NULL);
        free(log);
    }
    likes_free(&l);
    free(html);
}

static void test_today_format(void)
{
    char d[16];
    likes_today(d);
    CHECK(strlen(d) == 10);
    CHECK(d[4] == '-' && d[7] == '-');
}

int main(void)
{
    remove(STATE_TEST);
    remove(LOG_TEST);
    RUN(test_parse_order_and_entities);
    RUN(test_parse_empty_and_null);
    RUN(test_line_format);
    RUN(test_batch_basic);
    RUN(test_batch_small_list);
    RUN(test_batch_already_liked);
    RUN(test_done_advances);
    RUN(test_done_once_per_day);
    RUN(test_skipped_day_keeps_batch);
    RUN(test_save_and_load_state);
    RUN(test_load_state_missing_file);
    RUN(test_load_state_clamped);
    RUN(test_log_file);
    RUN(test_today_format);
    remove(STATE_TEST);
    remove(LOG_TEST);
    printf("\n%d checks, %d failed\n", g_run, g_fail);
    return g_fail ? 1 : 0;
}
