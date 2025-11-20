#ifndef SMM_H
#define SMM_H

/* ====== DEMO LIMITS ====== */
#define MAX_USERS    10
#define MAX_POSTS    30
#define MAX_MESSAGES 20
#define USERNAME_MAX 32
#define PASSWORD_MAX 32
#define CONTENT_MAX  256
#define TIMESTAMP_MAX 32
#define ADMIN_USERNAME_MAX 32
#define ADMIN_PASSWORD_MAX 32

/* ====== ADMIN ====== */
typedef struct Admin {
    char username[ADMIN_USERNAME_MAX];
    char password[ADMIN_PASSWORD_MAX];
    int is_registered;
} Admin;

/* ====== USERS (BST) ====== */
typedef struct User {
    char username[USERNAME_MAX];
    char password[PASSWORD_MAX];
    int followers, following;
} User;

typedef struct UserNode {
    User user;
    struct UserNode *left, *right;
} UserNode;

/* ====== POSTS ====== */
typedef struct Post {
    int id;
    char author[USERNAME_MAX];
    char content[CONTENT_MAX];
    char timestamp[TIMESTAMP_MAX];
} Post;

typedef struct PostArray {
    Post *data;
    int size, cap;
} PostArray;

/* ====== MESSAGE QUEUE ====== */
typedef struct Message {
    char from[USERNAME_MAX];
    char to[USERNAME_MAX];
    char content[CONTENT_MAX];
    char timestamp[TIMESTAMP_MAX];
} Message;

typedef struct MessageQueue {
    Message buf[MAX_MESSAGES];
    int head, tail, count;
} MessageQueue;

/* ====== FOLLOW GRAPH ====== */
typedef struct AdjNode {
    char username[USERNAME_MAX];
    struct AdjNode *next;
} AdjNode;

typedef struct GraphUser {
    char username[USERNAME_MAX];
    AdjNode *following;
    AdjNode *followers;
    struct GraphUser *next;
} GraphUser;

typedef struct Graph {
    GraphUser *head;
    int user_count;
} Graph;

/* ====== APP ====== */
typedef struct App {
    UserNode *users_bst;
    UserNode *current;
    PostArray posts;
    MessageQueue mq;
    Graph graph;
    Admin admin;
    Admin *current_admin;
    int max_users;
    int max_posts;
    int max_messages;
} App;

/* ====== Function Prototypes ====== */
int  get_line(char *buf, int n);
int  get_password(char *buf, int n);
void format_timestamp(char *buf, int n);
int  next_post_id(void);

void posts_init(PostArray *pa, int initial_cap);
void posts_free(PostArray *pa);
int  posts_add(PostArray *pa, const Post *p);
void posts_list_desc(const PostArray *pa);

void mq_init(MessageQueue *q);
int  mq_enqueue(MessageQueue *q, const Message *m);
int  mq_dequeue(MessageQueue *q, Message *out);
void mq_print(const MessageQueue *q);

UserNode* bst_insert(UserNode *root, const char *username, const char *password, int *ok);
UserNode* bst_find(UserNode *root, const char *username);
void      bst_free(UserNode *root);

void       graph_init(Graph *g);
GraphUser* graph_find(Graph *g, const char *username);
int        graph_add_user(Graph *g, const char *username);
int        graph_add_edge(Graph *g, const char *from, const char *to);
int        graph_remove_edge(Graph *g, const char *from, const char *to);
void       graph_show_following(Graph *g, const char *u);
void       graph_show_followers(Graph *g, const char *u);
void       graph_free(Graph *g);

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

void ui_admin_register(App *app);
void ui_admin_login(App *app);
void ui_admin_logout(App *app);
void ui_admin_change_limits(App *app);

void print_menu(void);
void print_admin_menu(void);

#endif
