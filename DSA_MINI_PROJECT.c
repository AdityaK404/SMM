#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#define USERNAME_MAX 32
#define PASSWORD_MAX 64
#define CONTENT_MAX 512
#define INITIAL_POST_CAP 16

/* Centralized error codes for helper functions (0 == OK) */
typedef enum {
    ERR_OK = 0,                 // If the whole code runs succesfully
    ERR_NULL_ARG = 1,           // it triggers when null pointer is passed
    ERR_ALLOC = 2,              // memory allocation  fails
    ERR_NOT_FOUND = 3,          // if the item is not found
    ERR_INVALID = 4,            //for invalid input
    ERR_TOO_LONG = 5    
} ErrorCode;

/* Data types */
typedef struct User {
    char username[USERNAME_MAX];
    char password[PASSWORD_MAX]; /* NOTE: plaintext in memory (toy app) */
    int followers;
    int following;
} User;

typedef struct UserNode {
    User user;
    struct UserNode *left;
    struct UserNode *right;
} UserNode;

typedef struct Post {
    int id;
    char author[USERNAME_MAX];
    char content[CONTENT_MAX];
    long timestamp;
} Post;

typedef struct PostArray {
    Post *data;
    int size;
    int capacity;
} PostArray;

/* Scheduled posts list */
typedef struct ScheduledPost {
    int id;
    char author[USERNAME_MAX];
    char content[CONTENT_MAX];
    long scheduled_time;
} ScheduledPost;

typedef struct SNode {
    ScheduledPost post;
    struct SNode *next;
} SNode;

typedef struct SList {
    SNode *head;
} SList;

/* Message queue (FIFO) */
typedef struct Message {
    char from[USERNAME_MAX];
    char to[USERNAME_MAX];
    char content[CONTENT_MAX];
    long timestamp;
} Message;

typedef struct MQNode {
    Message msg;
    struct MQNode *next;
} MQNode;

typedef struct MessageQueue {
    MQNode *front;
    MQNode *rear;
    int size;
} MessageQueue;

/* Application container */
typedef struct App {
    UserNode *users;
    PostArray published;
    SList scheduled;
    MessageQueue mq;
} App;

/* Validate username: non-empty, printable, no spaces; returns 0 if ok */
static int validate_username_strict(const char *s) {
    if (!s) return ERR_NULL_ARG;
    size_t len = strlen(s);
    if (len == 0) return ERR_INVALID;
    if (len >= USERNAME_MAX) return ERR_TOO_LONG;
    for (size_t i = 0; i < len; ++i) {
        unsigned char c = (unsigned char)s[i];
        if (!isprint(c) || isspace(c)) return ERR_INVALID;
    }
    return ERR_OK;
}

/* Validate password: allow printable chars but not empty */
static int validate_password(const char *s) {
    if (!s) return ERR_NULL_ARG;
    size_t len = strlen(s);
    if (len == 0) return ERR_INVALID;
    if (len >= PASSWORD_MAX) return ERR_TOO_LONG;
    for (size_t i = 0; i < len; ++i) {
        unsigned char c = (unsigned char)s[i];
        if (!isprint(c)) return ERR_INVALID;
    }
    return ERR_OK;
}

/* Validate content length */
static int validate_content(const char *s) {
    if (!s) return ERR_NULL_ARG;
    size_t len = strlen(s);
    if (len == 0) return ERR_INVALID;
    if (len >= CONTENT_MAX) return ERR_TOO_LONG;
    return ERR_OK;
}

/* Safe trimmed fgets wrapper */
static int get_line(char *buffer, int size) {
    if (!buffer || size <= 0) return 0;
    if (!fgets(buffer, size, stdin)) {
        return 0; /* EOF or error */
    }
    char *nl = strchr(buffer, '\n');
    if (nl) *nl = '\0';
    else {
        /* flush remainder */
        int c;
        while ((c = getchar()) != '\n' && c != EOF);
    }
    return 1;
}

/* Return current epoch seconds as long */
static long now_seconds(void) {
    return (long)time(NULL);
}

/* Next post id generator (not thread-safe) */
static int next_post_id(void) {
    static int pid = 1;
    return pid++;
}

/* -----------------------
 * PostArray (dynamic array)
 * - init, free, push with validation
 * Complexity notes: init O(1), push amortized O(1) (O(n) when resizing)
 * ----------------------- */

/* Initialize PostArray */
static int init_post_array(PostArray *pa) {
    if (!pa) return ERR_NULL_ARG;
    pa->capacity = INITIAL_POST_CAP;
    pa->size = 0;
    pa->data = malloc(sizeof(Post) * pa->capacity);
    if (!pa->data) { pa->capacity = 0; return ERR_ALLOC; }
    return ERR_OK;
}

/* Free PostArray */
static void free_post_array(PostArray *pa) {
    if (!pa) return;
    free(pa->data);
    pa->data = NULL;
    pa->size = pa->capacity = 0;
}

/* Append a Post */
static int push_post_array(PostArray *pa, const Post *p) {
    if (!pa || !p) return ERR_NULL_ARG;
    if (!pa->data) {
        int r = init_post_array(pa);
        if (r != ERR_OK) return r;
    }
    if (pa->size >= pa->capacity) {
        int newcap = pa->capacity * 2;
        Post *tmp = realloc(pa->data, sizeof(Post) * newcap);
        if (!tmp) return ERR_ALLOC;
        pa->data = tmp;
        pa->capacity = newcap;
    }
    pa->data[pa->size++] = *p;
    return ERR_OK;
}

/* -----------------------
 * BST for users
 * Complexity: insert/find O(h) worst O(n) if unbalanced
 * ----------------------- */

static UserNode* create_user_node(const char *username, const char *password) {
    if (!username || !password) return NULL;
    UserNode *n = (UserNode*)malloc(sizeof(UserNode));
    if (!n) return NULL;
    memset(n, 0, sizeof(UserNode));
    strncpy(n->user.username, username, USERNAME_MAX-1);
    n->user.username[USERNAME_MAX-1] = '\0';
    strncpy(n->user.password, password, PASSWORD_MAX-1);
    n->user.password[PASSWORD_MAX-1] = '\0';
    n->user.followers = 0;
    n->user.following = 0;
    n->left = n->right = NULL;
    return n;
}

/* Insert into BST. inserted set to 1 if inserted, 0 if duplicate.
   Returns new root (or NULL on alloc failure). */
static UserNode* bst_insert(UserNode *root, const char *username, const char *password, int *inserted) {
    if (!username || !password || !inserted) return root;
    if (!root) {
        UserNode *n = create_user_node(username, password);
        if (!n) { *inserted = 0; return NULL; }
        *inserted = 1;
        return n;
    }
    int cmp = strcmp(username, root->user.username);
    if (cmp == 0) { *inserted = 0; return root; }
    if (cmp < 0) root->left = bst_insert(root->left, username, password, inserted);
    else root->right = bst_insert(root->right, username, password, inserted);
    return root;
}

/* Find in BST */
static UserNode* bst_find(UserNode *root, const char *username) {
    if (!root || !username) return NULL;
    int cmp = strcmp(username, root->user.username);
    if (cmp == 0) return root;
    if (cmp < 0) return bst_find(root->left, username);
    return bst_find(root->right, username);
}

/* Inorder print */
static void bst_inorder_print(UserNode *root) {
    if (!root) return;
    bst_inorder_print(root->left);
    printf(" - %s (followers:%d, following:%d)\n",
           root->user.username, root->user.followers, root->user.following);
    bst_inorder_print(root->right);
}

/* Free BST */
static void bst_free(UserNode *root) {
    if (!root) return;
    bst_free(root->left);
    bst_free(root->right);
    free(root);
}

/* -----------------------
 * Sorted singly-linked SList (scheduled posts)
 * Complexity: insert O(n), pop_due O(k)
 * ----------------------- */

static void init_slist(SList *sl) {
    if (!sl) return;
    sl->head = NULL;
}

/* Insert ScheduledPost into sorted SList by scheduled_time
   Returns ERR_OK or error code */
static int schedule_post_add(SList *sl, const ScheduledPost *sp) {
    if (!sl || !sp) return ERR_NULL_ARG;
    SNode *n = (SNode*)malloc(sizeof(SNode));
    if (!n) return ERR_ALLOC;
    n->post = *sp;
    n->next = NULL;
    if (!sl->head || sp->scheduled_time < sl->head->post.scheduled_time) {
        n->next = sl->head;
        sl->head = n;
        return ERR_OK;
    }
    SNode *cur = sl->head;
    while (cur->next && cur->next->post.scheduled_time <= sp->scheduled_time) cur = cur->next;
    n->next = cur->next;
    cur->next = n;
    return ERR_OK;
}

/* Print SList */
static void slist_print(SList *sl) {
    if (!sl) { fprintf(stderr, "slist_print: NULL\n"); return; }
    if (!sl->head) { printf("No scheduled posts.\n"); return; }
    SNode *cur = sl->head;
    printf("Scheduled posts (by time):\n");
    while (cur) {
        printf(" id:%d author:%s at %ld -> %.60s\n",
               cur->post.id, cur->post.author, cur->post.scheduled_time, cur->post.content);
        cur = cur->next;
    }
}

/* Pop up to buf_len posts with scheduled_time <= now into out_buf.
   Returns number popped */
static int slist_pop_due(SList *sl, long now, ScheduledPost *out_buf, int buf_len) {
    if (!sl || !out_buf || buf_len <= 0) return 0;
    int taken = 0;
    while (sl->head && sl->head->post.scheduled_time <= now && taken < buf_len) {
        SNode *n = sl->head;
        sl->head = n->next;
        out_buf[taken++] = n->post;
        free(n);
    }
    return taken;
}

/* Free SList */
static void slist_free(SList *sl) {
    if (!sl) return;
    SNode *cur = sl->head;
    while (cur) {
        SNode *tmp = cur;
        cur = cur->next;
        free(tmp);
    }
    sl->head = NULL;
}

/* -----------------------
 * MessageQueue (FIFO)
 * Complexity: enqueue/dequeue O(1)
 * ----------------------- */

static void init_mq(MessageQueue *q) {
    if (!q) return;
    q->front = q->rear = NULL;
    q->size = 0;
}

/* Enqueue, returns ERR_OK or error */
static int mq_enqueue(MessageQueue *q, const Message *m) {
    if (!q || !m) return ERR_NULL_ARG;
    MQNode *n = (MQNode*)malloc(sizeof(MQNode));
    if (!n) return ERR_ALLOC;
    n->msg = *m;
    n->next = NULL;
    if (!q->rear) q->front = q->rear = n;
    else { q->rear->next = n; q->rear = n; }
    q->size++;
    return ERR_OK;
}

/* Dequeue: returns 1 on success and fills out if not NULL, 0 if empty, -1 on error */
static int mq_dequeue(MessageQueue *q, Message *out) {
    if (!q) return -1;
    if (!q->front) return 0;
    MQNode *n = q->front;
    if (out) *out = n->msg;
    q->front = n->next;
    if (!q->front) q->rear = NULL;
    free(n);
    q->size--;
    return 1;
}

static void mq_print(MessageQueue *q) {
    if (!q) { fprintf(stderr, "mq_print: NULL\n"); return; }
    if (!q->front) { printf("No messages in queue.\n"); return; }
    MQNode *cur = q->front;
    printf("Message Queue (FIFO):\n");
    while (cur) {
        printf(" From:%s To:%s at %ld -> %.60s\n",
               cur->msg.from, cur->msg.to, cur->msg.timestamp, cur->msg.content);
        cur = cur->next;
    }
}

static void mq_free(MessageQueue *q) {
    if (!q) return;
    Message tmp;
    while (mq_dequeue(q, &tmp) == 1) { /* discard */ }
}

/* -----------------------
 * App-level functions & UI
 * ----------------------- */

static int init_app(App *app) {
    if (!app) return ERR_NULL_ARG;
    app->users = NULL;
    int r = init_post_array(&app->published);
    if (r != ERR_OK) return r;
    init_slist(&app->scheduled);
    init_mq(&app->mq);
    return ERR_OK;
}

static void free_app(App *app) {
    if (!app) return;
    bst_free(app->users);
    free_post_array(&app->published);
    slist_free(&app->scheduled);
    mq_free(&app->mq);
}

/* verify_password: plaintext check, returns 1 if matches, 0 otherwise */
static int verify_password(UserNode *u) {
    if (!u) return 0;
    char input[PASSWORD_MAX];
    printf("Enter password for %s: ", u->user.username);
    if (!get_line(input, sizeof(input))) return 0;
    if (strcmp(input, u->user.password) == 0) return 1;
    printf("Incorrect password.\n");
    return 0;
}

/* create_user: UI to create user; enforces validation */
static void create_user(App *app) {
    if (!app) { fprintf(stderr, "create_user: NULL app\n"); return; }
    char username[USERNAME_MAX];
    char password[PASSWORD_MAX];
    printf("Enter username (no spaces, printable, max %d chars): ", USERNAME_MAX - 1);
    if (!get_line(username, sizeof(username))) return;
    if (validate_username_strict(username) != ERR_OK) { printf("Invalid username.\n"); return; }
    printf("Set password (printable, max %d chars): ", PASSWORD_MAX - 1);
    if (!get_line(password, sizeof(password))) return;
    if (validate_password(password) != ERR_OK) { printf("Invalid password.\n"); return; }
    int inserted = 0;
    UserNode *newroot = bst_insert(app->users, username, password, &inserted);
    if (!newroot && inserted == 0) {
        printf("Memory failure while creating user.\n");
        return;
    }
    app->users = newroot;
    if (inserted) printf("User '%s' created.\n", username);
    else printf("User '%s' already exists.\n", username);
}

/* list users */
static void list_users(const App *app) {
    if (!app) { fprintf(stderr, "list_users: NULL app\n"); return; }
    if (!app->users) { printf("No users.\n"); return; }
    printf("Users (in-order):\n");
    bst_inorder_print(app->users);
}

/* follow or unfollow */
static void follow_unfollow(App *app, int follow) {
    if (!app) { fprintf(stderr, "follow_unfollow: NULL app\n"); return; }
    char a[USERNAME_MAX], b[USERNAME_MAX];
    printf("Enter your username: ");
    if (!get_line(a, sizeof(a))) return;
    printf("Enter target username: ");
    if (!get_line(b, sizeof(b))) return;
    UserNode *A = bst_find(app->users, a);
    UserNode *B = bst_find(app->users, b);
    if (!A || !B) { printf("One or both users not found.\n"); return; }
    if (!verify_password(A)) return;
    if (follow) {
        A->user.following++;
        B->user.followers++;
        printf("%s now follows %s\n", a, b);
    } else {
        if (A->user.following > 0) A->user.following--;
        if (B->user.followers > 0) B->user.followers--;
        printf("%s unfollowed %s\n", a, b);
    }
}

/* publish_post helper (internal) */
static int publish_post(App *app, const char *author, const char *content) {
    if (!app || !author || !content) return ERR_NULL_ARG;
    if (validate_content(content) != ERR_OK) return ERR_INVALID;
    Post p;
    p.id = next_post_id();
    strncpy(p.author, author, USERNAME_MAX - 1);
    p.author[USERNAME_MAX - 1] = '\0';
    strncpy(p.content, content, CONTENT_MAX - 1);
    p.content[CONTENT_MAX - 1] = '\0';
    p.timestamp = now_seconds();
    int r = push_post_array(&app->published, &p);
    if (r == ERR_OK) {
        printf("Published post id:%d by %s at %ld\n", p.id, p.author, p.timestamp);
        return ERR_OK;
    }
    return r;
}

/* create_post_ui */
static void create_post_ui(App *app) {
    if (!app) { fprintf(stderr, "create_post_ui: NULL app\n"); return; }
    char author[USERNAME_MAX];
    char content[CONTENT_MAX];
    printf("Author username: ");
    if (!get_line(author, sizeof(author))) return;
    UserNode *u = bst_find(app->users, author);
    if (!u) { printf("User not found.\n"); return; }
    if (!verify_password(u)) return;
    printf("Enter content (max %d chars): ", CONTENT_MAX - 1);
    if (!get_line(content, sizeof(content))) return;
    if (validate_content(content) != ERR_OK) { printf("Invalid/empty content.\n"); return; }
    int r = publish_post(app, author, content);
    if (r != ERR_OK) printf("Failed to publish post (error %d).\n", r);
}

/* schedule_post_ui */
static void schedule_post_ui(App *app) {
    if (!app) { fprintf(stderr, "schedule_post_ui: NULL app\n"); return; }
    char author[USERNAME_MAX];
    char content[CONTENT_MAX];
    char timebuf[32];
    long when;
    printf("Author username: ");
    if (!get_line(author, sizeof(author))) return;
    UserNode *u = bst_find(app->users, author);
    if (!u) { printf("User not found.\n"); return; }
    if (!verify_password(u)) return;
    printf("Enter scheduled time as epoch seconds (e.g., %ld for now): ", now_seconds());
    if (!get_line(timebuf, sizeof(timebuf))) return;
    char *endp;
    when = strtol(timebuf, &endp, 10);
    if (endp == timebuf || *endp != '\0') { printf("Invalid time format.\n"); return; }
    printf("Enter content (max %d chars): ", CONTENT_MAX - 1);
    if (!get_line(content, sizeof(content))) return;
    if (validate_content(content) != ERR_OK) { printf("Invalid/empty content.\n"); return; }
    ScheduledPost sp;
    sp.id = next_post_id();
    strncpy(sp.author, author, USERNAME_MAX - 1);
    sp.author[USERNAME_MAX - 1] = '\0';
    strncpy(sp.content, content, CONTENT_MAX - 1);
    sp.content[CONTENT_MAX - 1] = '\0';
    sp.scheduled_time = when;
    int r = schedule_post_add(&app->scheduled, &sp);
    if (r == ERR_OK) {
        printf("Scheduled post id:%d by %s at %ld\n", sp.id, sp.author, sp.scheduled_time);
    } else {
        printf("Failed to schedule post (error %d).\n", r);
    }
}

/* process_scheduled: publish due posts */
static void process_scheduled(App *app, long now) {
    if (!app) { fprintf(stderr, "process_scheduled: NULL app\n"); return; }
    /* buffer size 256 to batch-publish up to that many */
    ScheduledPost buf[256];
    int taken = slist_pop_due(&app->scheduled, now, buf, (int)(sizeof(buf)/sizeof(buf[0])));
    if (taken <= 0) { printf("No scheduled posts due at %ld.\n", now); return; }
    for (int i = 0; i < taken; ++i) {
        int r = publish_post(app, buf[i].author, buf[i].content);
        if (r != ERR_OK) printf("Failed to publish scheduled post id:%d (error %d)\n", buf[i].id, r);
    }
}

/* list_published */
static void list_published(const App *app) {
    if (!app) { fprintf(stderr, "list_published: NULL app\n"); return; }
    if (!app->published.data || app->published.size == 0) { printf("No published posts.\n"); return; }
    printf("Published posts (most recent first):\n");
    for (int i = app->published.size - 1; i >= 0; --i) {
        const Post *p = &app->published.data[i];
        printf(" id:%d author:%s at %ld -> %.120s\n", p->id, p->author, p->timestamp, p->content);
    }
}

/* send_message_ui */
static void send_message_ui(App *app) {
    if (!app) { fprintf(stderr, "send_message_ui: NULL app\n"); return; }
    char from[USERNAME_MAX], to[USERNAME_MAX], content[CONTENT_MAX];
    printf("From username: ");
    if (!get_line(from, sizeof(from))) return;
    printf("To username: ");
    if (!get_line(to, sizeof(to))) return;
    UserNode *A = bst_find(app->users, from);
    UserNode *B = bst_find(app->users, to);
    if (!A || !B) { printf("One or both users not found.\n"); return; }
    if (!verify_password(A)) return;
    printf("Message content (max %d chars): ", CONTENT_MAX - 1);
    if (!get_line(content, sizeof(content))) return;
    if (validate_content(content) != ERR_OK) { printf("Invalid/empty message.\n"); return; }
    Message m;
    strncpy(m.from, from, USERNAME_MAX - 1);
    m.from[USERNAME_MAX - 1] = '\0';
    strncpy(m.to, to, USERNAME_MAX - 1);
    m.to[USERNAME_MAX - 1] = '\0';
    strncpy(m.content, content, CONTENT_MAX - 1);
    m.content[CONTENT_MAX - 1] = '\0';
    m.timestamp = now_seconds();
    int r = mq_enqueue(&app->mq, &m);
    if (r == ERR_OK) printf("Message queued.\n");
    else printf("Failed to queue message (error %d).\n", r);
}

/* process_one_message */
static void process_one_message(App *app) {
    if (!app) { fprintf(stderr, "process_one_message: NULL app\n"); return; }
    Message m;
    int r = mq_dequeue(&app->mq, &m);
    if (r == 0) { printf("No messages to process.\n"); return; }
    if (r < 0) { printf("Message dequeue error.\n"); return; }
    printf("Delivering message: From:%s To:%s at %ld -> %s\n",
           m.from, m.to, m.timestamp, m.content);
}

/* analytics_post_count (naive O(n^2)) */
static void analytics_post_count(const App *app) {
    if (!app) { fprintf(stderr, "analytics_post_count: NULL app\n"); return; }
    if (!app->published.data || app->published.size == 0) { printf("No posts for analytics.\n"); return; }
    printf("Post counts per author:\n");
    for (int i = 0; i < app->published.size; ++i) {
        int already_seen = 0;
        for (int j = 0; j < i; ++j) {
            if (strcmp(app->published.data[i].author, app->published.data[j].author) == 0) {
                already_seen = 1; break;
            }
        }
        if (already_seen) continue;
        int count = 0;
        for (int k = 0; k < app->published.size; ++k) {
            if (strcmp(app->published.data[k].author, app->published.data[i].author) == 0) count++;
        }
        printf(" %s -> %d posts\n", app->published.data[i].author, count);
    }
}

/* print menu */
static void print_menu(void) {
    puts("\n--- SMM Terminal Menu ---");
    puts("1. Create user");
    puts("2. List users");
    puts("3. Follow user");
    puts("4. Unfollow user");
    puts("5. Create post (publish now)");
    puts("6. Schedule post");
    puts("7. Process scheduled posts (publish due now)");
    puts("8. List published posts");
    puts("9. Send message (enqueue)");
    puts("10. Process one message (dequeue)");
    puts("11. Show message queue");
    puts("12. Show scheduled posts");
    puts("13. Analytics: post counts");
    puts("0. Exit");
    printf("-------------------------\n");
    printf("Enter choice: ");
}



int main(void) {
    App app;
    if (init_app(&app) != ERR_OK) {
        fprintf(stderr, "Failed to initialize application (memory error)\n");
        return 1;
    }

    char buf[64];
    int choice = -1;
    printf("Welcome to Terminal SMM (validated)\n");
    while (1) {
        print_menu();
        if (!get_line(buf, sizeof(buf))) break;
        char *endp;
        choice = (int)strtol(buf, &endp, 10);
        if (endp == buf || *endp != '\0') {
            puts("Invalid input; please enter a number.");
            continue;
        }
        switch (choice) {
            case 1: create_user(&app); break;
            case 2: list_users(&app); break;
            case 3: follow_unfollow(&app, 1); break;
            case 4: follow_unfollow(&app, 0); break;
            case 5: create_post_ui(&app); break;
            case 6: schedule_post_ui(&app); break;
            case 7: {
                long t = now_seconds();
                printf("Processing scheduled posts due at or before %ld\n", t);
                process_scheduled(&app, t);
                break;
            }
            case 8: list_published(&app); break;
            case 9: send_message_ui(&app); break;
            case 10: process_one_message(&app); break;
            case 11: mq_print(&app.mq); break;
            case 12: slist_print(&app.scheduled); break;
            case 13: analytics_post_count(&app); break;
            case 0:
                puts("Exiting... freeing resources.");
                free_app(&app);
                return 0;
            default:
                puts("Unknown choice.");
        }
    }

    free_app(&app);
    return 0;
}