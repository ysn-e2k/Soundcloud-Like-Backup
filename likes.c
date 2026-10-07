#include "likes.h"

void likes_init(Likes *l)
{
    memset(l, 0, sizeof *l);
    l->day = 1;
}

void likes_free(Likes *l)
{
    int i;
    for (i = 0; i < l->count; i++) {
        free(l->titles[i]);
        free(l->artists[i]);
    }
    free(l->titles);
    free(l->artists);
    free(l->liked);
    likes_init(l);
}

char *likes_read_file(const char *name)
{
    FILE *f = fopen(name, "rb");
    long n;
    char *buf;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = (char *)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    n = (long)fread(buf, 1, (size_t)n, f);
    buf[n] = 0;
    fclose(f);
    return buf;
}

static char *likes_utf8(char *o, unsigned cp)
{
    if (cp < 0x80) {
        *o++ = (char)cp;
    } else if (cp < 0x800) {
        *o++ = (char)(0xC0 | (cp >> 6));
        *o++ = (char)(0x80 | (cp & 63));
    } else if (cp < 0x10000) {
        *o++ = (char)(0xE0 | (cp >> 12));
        *o++ = (char)(0x80 | ((cp >> 6) & 63));
        *o++ = (char)(0x80 | (cp & 63));
    } else {
        *o++ = (char)(0xF0 | (cp >> 18));
        *o++ = (char)(0x80 | ((cp >> 12) & 63));
        *o++ = (char)(0x80 | ((cp >> 6) & 63));
        *o++ = (char)(0x80 | (cp & 63));
    }
    return o;
}

static int likes_is_space(char c)
{
    return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

static char *likes_decode(const char *s, size_t n)
{
    static const char *names[] = {"amp", "lt", "gt", "quot", "apos", "nbsp"};
    static const char chars[] = "&<>\"' ";
    char *out = (char *)malloc(n + 8), *o = out;
    size_t i = 0;
    if (!out) return NULL;
    while (i < n) {
        if (s[i] == '&') {
            size_t j = i + 1, len;
            while (j < n && j - i < 12 && s[j] != ';') j++;
            if (j < n && s[j] == ';') {
                len = j - i - 1;
                if (len > 1 && s[i + 1] == '#') {
                    unsigned cp = (s[i + 2] == 'x' || s[i + 2] == 'X')
                        ? (unsigned)strtoul(s + i + 3, NULL, 16)
                        : (unsigned)strtoul(s + i + 2, NULL, 10);
                    if (cp) {
                        o = likes_utf8(o, cp);
                        i = j + 1;
                        continue;
                    }
                } else {
                    int k;
                    for (k = 0; k < 6; k++) {
                        if (strlen(names[k]) == len && strncmp(s + i + 1, names[k], len) == 0) {
                            *o++ = chars[k];
                            i = j + 1;
                            break;
                        }
                    }
                    if (k < 6) continue;
                }
            }
        }
        *o++ = s[i++];
    }
    *o = 0;
    while (o > out && likes_is_space(o[-1])) *--o = 0;
    o = out;
    while (likes_is_space(*o)) o++;
    if (o != out) memmove(out, o, strlen(o) + 1);
    return out;
}

static const char *likes_find(const char *from, const char *to, const char *needle)
{
    size_t n = strlen(needle);
    const char *p;
    if (to < from || (size_t)(to - from) < n) return NULL;
    for (p = from; p + n <= to; p++)
        if (memcmp(p, needle, n) == 0) return p;
    return NULL;
}

static const char *likes_rfind(const char *from, const char *to, const char *needle)
{
    size_t n = strlen(needle), k;
    if (to < from || (size_t)(to - from) < n) return NULL;
    for (k = (size_t)(to - from) - n + 1; k > 0; k--)
        if (memcmp(from + k - 1, needle, n) == 0) return from + k - 1;
    return NULL;
}

static void likes_add(Likes *l, char *title, char *artist)
{
    if (!title) {
        free(artist);
        return;
    }
    if (l->count == l->cap) {
        l->cap = l->cap ? l->cap * 2 : 256;
        l->titles = (char **)realloc(l->titles, (size_t)l->cap * sizeof(char *));
        l->artists = (char **)realloc(l->artists, (size_t)l->cap * sizeof(char *));
    }
    l->titles[l->count] = title;
    l->artists[l->count] = artist;
    l->count++;
}

int likes_parse(Likes *l, const char *html)
{
    const char *marker = "soundTitle__title sc-link-dark";
    const char *user = "soundTitle__usernameText\">";
    const char *p = html, *prev = html;
    int i;
    if (!html) return 0;
    while ((p = strstr(p, marker)) != NULL) {
        const char *mark = p, *sp, *gt, *end, *bs, *a, *e;
        char *artist = NULL;
        p += strlen(marker);
        sp = strstr(p, "<span");
        if (!sp) break;
        gt = strchr(sp, '>');
        if (!gt) break;
        end = strstr(gt + 1, "</span>");
        if (!end) break;
        bs = likes_rfind(prev, mark, "sound streamContext\"");
        if (!bs) bs = prev;
        a = likes_find(bs, mark, user);
        if (a) {
            a += strlen(user);
            e = likes_find(a, mark, "</span>");
            if (e) artist = likes_decode(a, (size_t)(e - a));
        }
        likes_add(l, likes_decode(gt + 1, (size_t)(end - gt - 1)), artist);
        prev = p = end;
    }
    for (i = 0; i < l->count / 2; i++) {
        int j = l->count - 1 - i;
        char *t = l->titles[i];
        l->titles[i] = l->titles[j];
        l->titles[j] = t;
        t = l->artists[i];
        l->artists[i] = l->artists[j];
        l->artists[j] = t;
    }
    free(l->liked);
    l->liked = (char *)calloc((size_t)l->count + 1, 1);
    return l->count;
}

int likes_load(Likes *l, const char *html_file)
{
    char *html = likes_read_file(html_file);
    int n;
    if (!html) return 0;
    n = likes_parse(l, html);
    free(html);
    return n;
}

void likes_set_liked(Likes *l, int idx, int value)
{
    if (idx >= 0 && idx < l->count) l->liked[idx] = value ? 1 : 0;
}

int likes_batch_rows(const Likes *l)
{
    int rows = LIKES_BATCH;
    for (;;) {
        int end = l->next + rows, n = 0, i, want;
        if (end > l->count) end = l->count;
        for (i = l->next; i < end; i++) n += l->liked[i];
        want = LIKES_BATCH + n;
        if (l->next + want > l->count) want = l->count - l->next;
        if (want <= rows) break;
        rows = want;
    }
    if (l->next + rows > l->count) rows = l->count - l->next;
    return rows < 0 ? 0 : rows;
}

int likes_line(const Likes *l, int idx, char *out, size_t size)
{
    if (idx < 0 || idx >= l->count || size == 0) return 0;
    if (l->artists[idx] && *l->artists[idx])
        snprintf(out, size, "%s - %s", l->titles[idx], l->artists[idx]);
    else
        snprintf(out, size, "%s", l->titles[idx]);
    return 1;
}

void likes_today(char *out)
{
    time_t t = time(NULL);
    strftime(out, 16, "%Y-%m-%d", localtime(&t));
}

int likes_save(const Likes *l, const char *file)
{
    int i, first = 1;
    FILE *f = fopen(file, "w");
    if (!f) return 0;
    fprintf(f, "next=%d\nday=%d\nlast_done=%s\nalready=", l->next, l->day, l->last);
    for (i = l->next; i < l->count; i++) {
        if (!l->liked[i]) continue;
        fprintf(f, first ? "%d" : ",%d", i);
        first = 0;
    }
    fprintf(f, "\n");
    fclose(f);
    return 1;
}

int likes_load_state(Likes *l, const char *file)
{
    static char line[65536];
    FILE *f = fopen(file, "r");
    if (!f) return 0;
    while (fgets(line, sizeof line, f)) {
        int v;
        if (sscanf(line, "next=%d", &v) == 1) {
            l->next = v;
        } else if (sscanf(line, "day=%d", &v) == 1) {
            l->day = v;
        } else if (strncmp(line, "last_done=", 10) == 0) {
            sscanf(line + 10, "%15s", l->last);
        } else if (strncmp(line, "already=", 8) == 0) {
            char *p = line + 8;
            while (*p) {
                char *e;
                long idx = strtol(p, &e, 10);
                if (e == p) {
                    p++;
                    continue;
                }
                likes_set_liked(l, (int)idx, 1);
                p = e;
            }
        }
    }
    fclose(f);
    if (l->next < 0) l->next = 0;
    if (l->next > l->count) l->next = l->count;
    return 1;
}

int likes_log(const Likes *l, const char *file, int from, int to, const char *date)
{
    int i, n = 0;
    FILE *f = fopen(file, "a");
    if (!f) return 0;
    fprintf(f, "%s  DONE  day %d  songs %d-%d of %d", date, l->day, from + 1, to, l->count);
    for (i = from; i < to; i++) {
        if (!l->liked[i]) continue;
        fprintf(f, n ? ", #%d" : "  (already liked: #%d", i + 1);
        n++;
    }
    if (n) fputc(')', f);
    fputc('\n', f);
    fclose(f);
    return 1;
}

int likes_done(Likes *l, const char *date, const char *log_file)
{
    int from = l->next, rows;
    if (l->next >= l->count || strcmp(l->last, date) == 0) return 0;
    rows = likes_batch_rows(l);
    if (log_file) likes_log(l, log_file, from, from + rows, date);
    l->next += rows;
    l->day++;
    snprintf(l->last, sizeof l->last, "%s", date);
    return rows;
}
