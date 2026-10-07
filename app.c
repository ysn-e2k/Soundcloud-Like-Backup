#include "likes.h"

static void show(const Likes *l)
{
    char line[512];
    int i, rows = likes_batch_rows(l);
    printf("\nDay %d - songs %d-%d of %d\n", l->day, l->next + 1, l->next + rows, l->count);
    for (i = l->next; i < l->next + rows; i++) {
        likes_line(l, i, line, sizeof line);
        printf("%d. %s%s\n", i + 1, line, l->liked[i] ? "  [already liked]" : "");
    }
}

int main(void)
{
    Likes l;
    char date[16], cmd[64];
    int n;
    likes_init(&l);
    if (!likes_load(&l, "LIKES.html")) {
        puts("LIKES.html not found");
        return 1;
    }
    likes_load_state(&l, "progress.txt");
    likes_today(date);
    for (;;) {
        show(&l);
        printf("\nnumber = already liked on/off, d = done, q = quit > ");
        if (!fgets(cmd, sizeof cmd, stdin) || cmd[0] == 'q') break;
        if (cmd[0] == 'd') {
            if (likes_done(&l, date, "done_log.txt")) puts("Done, see you tomorrow");
            else puts("Already done today, or no songs left");
            break;
        }
        if (sscanf(cmd, "%d", &n) == 1 && n >= 1 && n <= l.count) {
            likes_set_liked(&l, n - 1, !l.liked[n - 1]);
            likes_save(&l, "progress.txt");
        }
    }
    likes_save(&l, "progress.txt");
    likes_free(&l);
    return 0;
}
