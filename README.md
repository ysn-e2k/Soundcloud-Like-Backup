# LikesHelper

Tiny C library to go through an archived SoundCloud likes page (`LIKES.html`) in daily batches, oldest like first.

- Parses `LIKES.html` and returns titles and artists, oldest first
- Daily batch of 70 songs
- Songs you already liked can be flagged: each one adds a row so you still get 70 new songs
- One `DONE` per day, a skipped day never skips a batch
- Progress saved in `progress.txt`, history in `done_log.txt`
- No dependencies, plain C99, no GUI

## Files

| File | Role |
|------|------|
| `likes.h` | Types and function prototypes |
| `likes.c` | Implementation |
| `main.c` | Test suite (61 checks) |

## Build and test

```
gcc -Wall -Wextra -std=c99 main.c likes.c -o tests
./tests
```
```
gcc app.c likes.c -o app
```
MSVC:

```
cl main.c likes.c
tests.exe
```

## Usage

```c
#include "likes.h"

int main(void)
{
    Likes l;
    char date[16], line[512];
    int i, rows;

    likes_init(&l);
    if (!likes_load(&l, "LIKES.html")) return 1;
    likes_load_state(&l, "progress.txt");

    rows = likes_batch_rows(&l);
    for (i = l.next; i < l.next + rows; i++) {
        likes_line(&l, i, line, sizeof line);
        puts(line);
    }

    likes_today(date);
    likes_done(&l, date, "done_log.txt");
    likes_save(&l, "progress.txt");
    likes_free(&l);
    return 0;
}
```

## API

| Function | Description |
|----------|-------------|
| `likes_init(l)` / `likes_free(l)` | Init and free |
| `likes_load(l, file)` | Parse an HTML file, returns the number of songs |
| `likes_parse(l, html)` | Parse an HTML string |
| `likes_batch_rows(l)` | Size of the current batch (70 + already liked songs) |
| `likes_line(l, i, buf, size)` | Writes `title - artist` |
| `likes_set_liked(l, i, v)` | Flags a song as already liked |
| `likes_done(l, date, log)` | Validates the batch, returns its size, `0` if already done that day |
| `likes_save(l, file)` / `likes_load_state(l, file)` | Save and restore progress |
| `likes_today(buf)` | Today as `YYYY-MM-DD` |

## Files created

`progress.txt`

```
next=70
day=2
last_done=2026-01-01
already=75,80
```

`done_log.txt`

```
2026-01-01  DONE  day 1  songs 1-71 of 100  (already liked: #6)
```

## Expected HTML

The parser looks for SoundCloud's `soundTitle__title sc-link-dark` and `soundTitle__usernameText` blocks. The page is listed newest first, the library reverses it.

## License

MIT
