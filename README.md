# CineVault – Movie Collection Manager

A movie collection manager written in **C**. Movies are kept in a **singly linked list**, saved to **`data/movies.dat`** with file handling, and managed through a **web interface** served by a small **C HTTP server** on port 8080.

Features: add, view, search, filter by genre, sort, edit and delete movies; statistics (total, average rating, top rated, genres); automatic poster lookup with a fallback; and a live view of the linked list in memory showing every node's real address and `next` pointer.

---

## 1. Run it on Windows (VS Code + MSYS2)

**One-time setup**

1. Install MSYS2 from https://www.msys2.org.
2. Open the **MSYS2 UCRT64** terminal and run:
   ```
   pacman -S mingw-w64-ucrt-x86_64-gcc
   ```
3. Add `C:\msys64\ucrt64\bin` to your Windows `PATH`, then reopen VS Code.
4. Check it works in a VS Code terminal: `gcc --version`

**Every time**

Open the `CineVault` folder in VS Code, open a terminal (Terminal → New Terminal) and run:

```
build.bat
cinevault.exe
```

Then open **http://localhost:8080** in your browser. Or just double-click `run.bat`: it builds if needed, starts the server and opens the browser.

Stop the server with **Ctrl+C**. It frees every node before exiting.

> Always start `cinevault.exe` from the CineVault folder, so it can find `public/` and `data/`.

**Build command (if you prefer typing it):**
```
gcc -Wall -Wextra -O2 server.c movies.c -o cinevault.exe -lws2_32
```
`-lws2_32` links Winsock, the Windows networking library.

## 2. Run it on Linux or macOS

```
make
./cinevault
```

To use another port: `cinevault 8081` (Windows) or `./cinevault 8081`.

---

## 3. Project structure

```
CineVault/
├── server.c          C HTTP server: sockets, request parsing, routes, JSON
├── movies.c          Linked-list operations, validation, file handling
├── movies.h          Movie struct and function declarations
├── build.bat         Build on Windows
├── run.bat           Build + start + open browser (Windows)
├── Makefile          Build with make (Linux/macOS/MSYS2)
├── data/
│   └── movies.dat    Saved collection (5 sample movies to start)
├── public/
│   ├── index.html    Web interface
│   ├── css/style.css Styling
│   └── js/app.js     Browser logic: API calls, rendering, poster lookup
└── docs/             Architecture, flowcharts, Gantt chart, survey, problem statement, team roles
```

---

## 4. Where each concept is in the code (for the viva)

| Concept | Where | What to point at |
|---|---|---|
| Structure | `movies.h` | `typedef struct Movie { … struct Movie *next; } Movie;` |
| Dynamic memory allocation | `movies.c` → `create_movie()` | `malloc(sizeof(Movie))` |
| Insert into the list | `movies.c` → `insert_movie()` | Walk to the tail, then `cur->next = node` |
| Traversal | `count_movies()`, `average_rating()`, `find_movie()` | `for (cur = head; cur != NULL; cur = cur->next)` |
| Delete a node | `detach_movie()` + `free()` | `prev->next = cur->next`, or `*head = cur->next` for the head |
| Pointer to pointer | `insert_movie(Movie **head, …)` | Needed so the function can change `head` itself |
| Search | `filter_movies()` | Builds a new list of copies that match |
| Sort | `sort_movies()` | **Merge sort on a linked list**: slow/fast pointer split, then merge by relinking |
| Free all memory | `free_movies()` | Saves `next` before `free(cur)` |
| File handling: read | `load_movies()` | `fopen` / `fgets` / split on the pipe character |
| File handling: write | `save_movies()` | Writes `movies.dat.tmp`, then renames it over `movies.dat` (safe if interrupted) |
| Validation | `validate_movie()` | Title, genre, year 1888 to this year + 5, rating 0–10, poster URL |
| HTTP server | `server.c` → `main()` | `socket` → `bind` → `listen` → `select` → `accept` → `recv` → `send` |
| Request parsing | `parse_request_line()`, `content_length()`, `form_get()`, `url_decode()` | |
| JSON output | `buf_json_string()`, `json_movie()` | Escapes quotes and control characters |
| Memory view | `api_list_view()` | Prints each node's address with `PRIxPTR` |

**Why a text file and not `fwrite(struct)`?** The struct holds a pointer (`next`), which is meaningless after a restart. Its size and padding also differ between compilers. A text line per movie can be opened and checked in VS Code, and survives recompiling.

**Why insert at the tail?** It keeps movies in the order they were added, which the "Order added" sort and the memory view show. It is O(n) because the code walks to the end; keeping a tail pointer would make it O(1).

---

## 5. HTTP API

| Method | Path | Does |
|---|---|---|
| GET | `/api/movies?q=&genre=&sort=` | List. `sort` = `added`, `title`, `rating`, `year_new`, `year_old` |
| POST | `/api/movies` | Add (`title`, `genre`, `year`, `rating`, `image`) |
| GET | `/api/movies/{id}` | One movie |
| PUT | `/api/movies/{id}` | Update |
| DELETE | `/api/movies/{id}` | Delete |
| GET | `/api/stats` | Total, average, top rated, genres, year range |
| GET | `/api/list` | Nodes in list order with `addr` and `next` |

Try one from a terminal: `curl http://localhost:8080/api/stats`

## 6. `movies.dat` format

```
# id|title|genre|year|rating|image
1|Inception|Sci-Fi|2010|8.8|
```

Lines starting with `#` are comments. A `|` typed in a title is stored as `/` so it can't break the format.

## 7. Posters

When the poster link is blank, the browser searches Wikipedia for "*title year* film" and uses the article's lead image. If nothing matches, or there is no internet, the movie gets a coloured stand-in poster with its title. The **Find missing posters** button does the lookup for every movie that doesn't have one, including the 5 sample movies.

---

## 8. Test checklist (Phase 2)

- [ ] Add "Pushpa", Action, 2021, 7.6 → appears in the collection and at the end of the strip
- [ ] Add the same title and year again → "already in your collection"
- [ ] Year 1500, rating 11, empty title → each shows an error under its field
- [ ] Edit a movie's rating → the card and the average change
- [ ] Delete a movie → it disappears from the strip; the previous node's `next` skips it
- [ ] Search "dark", filter by genre, try each sort
- [ ] Stop with Ctrl+C, start again → all changes are still there (open `data/movies.dat` to show it)
- [ ] Ctrl+C output shows "Freed N movie nodes"

## 9. Screenshots for the submission

VS Code project tree · server running in the terminal (banner + request log) · the web interface · the Add form filled in · the new movie in the collection and the strip · `data/movies.dat` open in VS Code.

## 10. Troubleshooting

| Problem | Fix |
|---|---|
| `gcc is not recognized` | Add `C:\msys64\ucrt64\bin` to PATH and reopen VS Code |
| `undefined reference to __imp_WSAStartup` | Add `-lws2_32` at the end of the gcc command |
| "Could not start on port 8080" | Another program is using it. Run `cinevault.exe 8081` and open http://localhost:8081 |
| Page says "Can't reach the CineVault server" | The server isn't running. Start `cinevault.exe` again |
| Page shows "Not found" | Start the server from the CineVault folder, not from another folder |
| Windows Firewall prompt | Allowing isn't needed. The server only listens on this computer (127.0.0.1) |
| Posters don't appear | Needs internet. Without it, stand-in posters are used |
