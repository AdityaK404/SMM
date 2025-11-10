/* smm.h — Minimal Social Media Manager (MVP)
   DS used:
   - Linear: Dynamic Array (posts), Circular Queue (messages)
   - Non-linear: BST (users), Graph w/ adjacency lists (follow system)
   Build: gcc smm.c -o smm
*/
#ifndef SMM_H
#define SMM_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

/* ====== DEMO LIMITS (Preset S) ====== */
#define MAX_USERS    10
#define MAX_POSTS    30
#define MAX_MESSAGES 20
#define USERNAME_MAX 32
#define PASSWORD_MAX 32
#define CONTENT_MAX  256

/* ====== USERS (BST) ====== */
typedef struct User {
    char username[USERNAME_MAX];
    char password[PASSWORD_MAX]; /* plaintext for demo */
    int followers, following;
} User;

typedef struct UserNode {
    User user;
    struct UserNode *left, *right;
} UserNode;

/* ====== POSTS (Dynamic Array) ====== */
typedef struct Post {
    int id;
    char author[USERNAME_MAX];
    char content[CONTENT_MAX];
    long timestamp;
} Post;

typedef struct PostArray {
    Post *data;
    int size, cap;
} PostArray;

/* ====== MESSAGES (Circular Queue) ====== */
typedef struct Message {
    char from[USERNAME_MAX];
    char to[USERNAME_MAX];
    char content[CONTENT_MAX];
    long timestamp;
} Message;

typedef struct MessageQueue {
    Message buf[MAX_MESSAGES];
    int head, tail, count; /* FIFO */
} MessageQueue;

/* ====== FOLLOW GRAPH (Adjacency Lists) ====== */
typedef struct AdjNode {
    char username[USERNAME_MAX];
    struct AdjNode *next;
} AdjNode;

typedef struct GraphUser {
    char username[USERNAME_MAX];
    AdjNode *following; /* users I follow */
    AdjNode *followers; /* users who follow me */
    struct GraphUser *next;
} GraphUser;

typedef struct Graph {
    GraphUser *head;
    int user_count;
} Graph;

/* ====== APP ====== */
typedef struct App {
    UserNode *users_bst;
    UserNode *current;     /* login session: NULL if none */
    PostArray posts;
    MessageQueue mq;
    Graph graph;
} App;

/* ====== Helpers ====== */
int  get_line(char *buf, int n);
long now_seconds(void);
int  next_post_id(void);

/* ====== Posts ====== */
void posts_init(PostArray *pa, int initial_cap);
void posts_free(PostArray *pa);
int  posts_add(PostArray *pa, const Post *p);
void posts_list_desc(const PostArray *pa);

/* ====== Queue ====== */
void mq_init(MessageQueue *q);
int  mq_enqueue(MessageQueue *q, const Message *m);
int  mq_dequeue(MessageQueue *q, Message *out);
void mq_print(const MessageQueue *q);

/* ====== BST (Users) ====== */
UserNode* bst_insert(UserNode *root, const char *username, const char *password, int *ok);
UserNode* bst_find(UserNode *root, const char *username);
void      bst_free(UserNode *root);

/* ====== Graph (Follow system) ====== */
void       graph_init(Graph *g);
GraphUser* graph_find(Graph *g, const char *username);
int        graph_add_user(Graph *g, const char *username);
int        graph_add_edge(Graph *g, const char *from, const char *to);
int        graph_remove_edge(Graph *g, const char *from, const char *to);
void       graph_show_following(Graph *g, const char *u);
void       graph_show_followers(Graph *g, const char *u);
void       graph_free(Graph *g);

/* ====== App UI ====== */
void app_init(App *app);
void app_free(App *app);

void ui_register(App *app);
void ui_login(App *app);
void ui_logout(App *app);
void ui_create_post(App *app);
void ui_view_posts(App *app);
void ui_follow(App *app);
void ui_unfollow(App *app);
void ui_show_following(App *app);
void ui_show_followers(App *app);
void ui_send_message(App *app);
void ui_process_message(App *app);
void ui_show_messages(App *app);
void print_menu(void);

#endif /* SMM_H */
