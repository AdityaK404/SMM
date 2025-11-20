/* smm.c — Minimal Social Media Manager (MVP) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "smm.h"
#ifdef _WIN32
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

/* ====== Small utilities ====== */
int get_line(char *buf, int n) {
    if (!fgets(buf, n, stdin)) return 0;
    char *nl = strchr(buf, '\n');
    if (nl) *nl = '\0';
    else { int c; while ((c = getchar()) != '\n' && c != EOF); }
    return 1;
}

/* Read password with masked asterisks while typing. Cross-platform.
 * Returns 1 on success, 0 on EOF/error.
 */
int get_password(char *buf, int n) {
#ifdef _WIN32
    int idx = 0;
    int ch;
    while ((ch = _getch()) != '\r' && ch != '\n' && ch != EOF) {
        if (ch == 3) return 0; /* Ctrl-C */
        if (ch == 8 || ch == 127) { /* backspace */
            if (idx > 0) { idx--; buf[idx] = '\0'; printf("\b \b"); }
        } else if (idx < n-1) {
            buf[idx++] = (char)ch;
            putchar('*');
        }
    }
    buf[idx] = '\0'; putchar('\n'); return 1;
#else
    struct termios oldt, newt;
    if (tcgetattr(STDIN_FILENO, &oldt) != 0) return 0;
    newt = oldt;
    newt.c_lflag &= ~(ECHO | ICANON);
    if (tcsetattr(STDIN_FILENO, TCSANOW, &newt) != 0) return 0;
    int idx = 0;
    int ch;
    while (1) {
        ch = getchar();
        if (ch == EOF) { tcsetattr(STDIN_FILENO, TCSANOW, &oldt); return 0; }
        if (ch == '\n' || ch == '\r') break;
        if (ch == 3) { tcsetattr(STDIN_FILENO, TCSANOW, &oldt); return 0; } /* Ctrl-C */
        if (ch == 8 || ch == 127) { /* backspace */
            if (idx > 0) { idx--; buf[idx] = '\0'; printf("\b \b"); }
        } else if (idx < n-1) {
            buf[idx++] = (char)ch;
            putchar('*');
            fflush(stdout);
        }
    }
    buf[idx] = '\0';
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    putchar('\n');
    return 1;
#endif
}

/* Format current time as dd/mm/yyyy hh:mm:ss am/pm */
void format_timestamp(char *buf, int n) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    int hour = t->tm_hour;
    const char *ampm = (hour >= 12) ? "pm" : "am";
    if (hour == 0) hour = 12;
    else if (hour > 12) hour -= 12;
    snprintf(buf, n, "%02d/%02d/%04d %02d:%02d:%02d %s",
             t->tm_mday, t->tm_mon + 1, t->tm_year + 1900,
             hour, t->tm_min, t->tm_sec, ampm);
}

int next_post_id(void) { static int id = 1; return id++; }

static int valid_name(const char *s) {
    if (!s || !*s || strlen(s) >= USERNAME_MAX) return 0;
    for (const unsigned char *p=(const unsigned char*)s; *p; ++p)
        if (isspace(*p) || !isprint(*p)) return 0;
    return 1;
}

/* ====== Posts (Dynamic Array) ====== */
void posts_init(PostArray *pa, int initial_cap) {
    pa->size = 0; pa->cap = initial_cap;
    pa->data = (Post*)malloc(sizeof(Post)*pa->cap);
}
void posts_free(PostArray *pa) {
    free(pa->data); pa->data = NULL; pa->size = pa->cap = 0;
}
int posts_add(PostArray *pa, const Post *p) {
    if (pa->size >= MAX_POSTS) return 0; /* hard stop for demo */
    if (pa->size == pa->cap) {
        int nc = pa->cap*2; if (nc > MAX_POSTS) nc = MAX_POSTS;
        Post *tmp = (Post*)realloc(pa->data, sizeof(Post)*nc);
        if (!tmp) return 0;
        pa->data = tmp; pa->cap = nc;
    }
    pa->data[pa->size++] = *p; return 1;
}
void posts_list_desc(const PostArray *pa) {
    if (pa->size == 0) { puts("No posts yet."); return; }
    puts("Posts (newest first):");
    for (int i = pa->size-1; i >= 0; --i) {
        const Post *p = &pa->data[i];
        printf(" #%d by %s at %s: %s\n", p->id, p->author, p->timestamp, p->content);
    }
}

/* ====== Queue (Circular, fixed-cap) ====== */
void mq_init(MessageQueue *q){ q->head=q->tail=q->count=0; }
int mq_enqueue(MessageQueue *q, const Message *m){
    if (q->count == MAX_MESSAGES) return 0;
    q->buf[q->tail] = *m;
    q->tail = (q->tail + 1) % MAX_MESSAGES;
    q->count++;
    return 1;
}
int mq_dequeue(MessageQueue *q, Message *out){
    if (q->count == 0) return 0;
    if (out) *out = q->buf[q->head];
    q->head = (q->head + 1) % MAX_MESSAGES;
    q->count--;
    return 1;
}
void mq_print(const MessageQueue *q){
    if (q->count == 0) { puts("Message queue is empty."); return; }
    puts("Messages in queue (front..back):");
    for (int i=0, idx=q->head; i<q->count; ++i, idx=(idx+1)%MAX_MESSAGES){
        const Message *m = &q->buf[idx];
        printf(" from:%s -> to:%s at %s | %.80s\n", m->from, m->to, m->timestamp, m->content);
    }
}

/* ====== BST (Users) ====== */
static UserNode* make_user(const char *u, const char *p){
    UserNode *n=(UserNode*)calloc(1,sizeof(UserNode));
    if (!n) return NULL;
    strncpy(n->user.username,u,USERNAME_MAX-1);
    strncpy(n->user.password,p,PASSWORD_MAX-1);
    return n;
}
UserNode* bst_insert(UserNode *root, const char *username, const char *password, int *ok){
    if (!root){ *ok=1; return make_user(username,password); }
    int c = strcmp(username, root->user.username);
    if (c==0){ *ok=0; return root; }
    if (c<0) root->left = bst_insert(root->left, username, password, ok);
    else     root->right= bst_insert(root->right,username, password, ok);
    return root;
}
UserNode* bst_find(UserNode *root, const char *username){
    if (!root) return NULL;
    int c = strcmp(username, root->user.username);
    if (c==0) return root;
    if (c<0)  return bst_find(root->left, username);
    return bst_find(root->right, username);
}
void bst_free(UserNode *root){
    if (!root) return;
    bst_free(root->left); bst_free(root->right); free(root);
}

/* ====== Graph (Adjacency) ====== */
void graph_init(Graph *g){ g->head=NULL; g->user_count=0; }

GraphUser* graph_find(Graph *g, const char *username){
    for (GraphUser *cu=g->head; cu; cu=cu->next)
        if (strcmp(cu->username, username)==0) return cu;
    return NULL;
}
static AdjNode* adj_prepend(AdjNode *head, const char *u){
    AdjNode *n=(AdjNode*)malloc(sizeof(AdjNode));
    if (!n) return head;
    strncpy(n->username,u,USERNAME_MAX-1); n->username[USERNAME_MAX-1]='\0';
    n->next=head; return n;
}
static int adj_has(AdjNode *head, const char *u){
    for (; head; head=head->next) if (strcmp(head->username,u)==0) return 1;
    return 0;
}
static AdjNode* adj_remove(AdjNode *head, const char *u, int *removed){
    AdjNode *cur=head,*prev=NULL; *removed=0;
    while(cur){
        if(strcmp(cur->username,u)==0){
            *removed=1;
            if(prev) prev->next=cur->next; else head=cur->next;
            free(cur); break;
        }
        prev=cur; cur=cur->next;
    }
    return head;
}
int graph_add_user(Graph *g, const char *username){
    if (graph_find(g, username)) return 1;
    if (g->user_count >= MAX_USERS) return 0;
    GraphUser *nu=(GraphUser*)calloc(1,sizeof(GraphUser));
    if (!nu) return 0;
    strncpy(nu->username, username, USERNAME_MAX-1);
    nu->next = g->head; g->head = nu; g->user_count++;
    return 1;
}
int graph_add_edge(Graph *g, const char *from, const char *to){
    GraphUser *A=graph_find(g,from), *B=graph_find(g,to);
    if (!A||!B || strcmp(from,to)==0) return 0;
    if (!adj_has(A->following,to)) A->following = adj_prepend(A->following,to);
    if (!adj_has(B->followers,from)) B->followers = adj_prepend(B->followers,from);
    return 1;
}
int graph_remove_edge(Graph *g, const char *from, const char *to){
    GraphUser *A=graph_find(g,from), *B=graph_find(g,to);
    if (!A||!B) return 0;
    int r1=0,r2=0;
    A->following = adj_remove(A->following,to,&r1);
    B->followers = adj_remove(B->followers,from,&r2);
    return r1&&r2;
}
void graph_show_following(Graph *g, const char *u){
    GraphUser *gu = graph_find(g,u);
    if (!gu){ printf("User '%s' not found.\n", u); return; }
    printf("%s follows:\n", u);
    int c=0; for (AdjNode *a=gu->following; a; a=a->next){ printf(" - %s\n", a->username); c++; }
    if (!c) puts(" (none)");
}
void graph_show_followers(Graph *g, const char *u){
    GraphUser *gu = graph_find(g,u);
    if (!gu){ printf("User '%s' not found.\n", u); return; }
    printf("%s is followed by:\n", u);
    int c=0; for (AdjNode *a=gu->followers; a; a=a->next){ printf(" - %s\n", a->username); c++; }
    if (!c) puts(" (none)");
}
void graph_free(Graph *g){
    GraphUser *cu=g->head;
    while(cu){
        GraphUser *nx=cu->next;
        AdjNode *a=cu->following; while(a){ AdjNode *an=a->next; free(a); a=an; }
        a=cu->followers; while(a){ AdjNode *an=a->next; free(a); a=an; }
        free(cu); cu=nx;
    }
    g->head=NULL; g->user_count=0;
}

/* ====== App ====== */
void app_init(App *app){
    app->users_bst=NULL; app->current=NULL;
    posts_init(&app->posts, 8);
    mq_init(&app->mq);
    graph_init(&app->graph);
    app->admin.is_registered = 0;
    app->current_admin = NULL;
    app->max_users = MAX_USERS;
    app->max_posts = MAX_POSTS;
    app->max_messages = MAX_MESSAGES;
}
void app_free(App *app){
    bst_free(app->users_bst);
    posts_free(&app->posts);
    graph_free(&app->graph);
}

/* ====== UI Actions ====== */
static int session_required(App *app){
    if (!app->current){ puts("Please login first."); return 0; }
    return 1;
}

void ui_register(App *app){
    if (app->graph.user_count >= app->max_users){ puts("User limit reached."); return; }
    char u[USERNAME_MAX], p[PASSWORD_MAX];
    printf("New username: "); if (!get_line(u,sizeof u)) return;
    if (!valid_name(u)){ puts("Invalid username."); return; }
    if (bst_find(app->users_bst,u)){ puts("Username already exists."); return; }
    printf("Set password: "); if (!get_password(p,sizeof p)) return;
    int ok=0; app->users_bst = bst_insert(app->users_bst,u,p,&ok);
    if (!ok){ puts("Insert failed."); return; }
    if (!graph_add_user(&app->graph,u)){ puts("Graph add failed."); }
    puts("User created.");
}

void ui_login(App *app){
    char u[USERNAME_MAX], p[PASSWORD_MAX];
    printf("Username: "); if (!get_line(u,sizeof u)) return;
    printf("Password: "); if (!get_password(p,sizeof p)) return;
    UserNode *n = bst_find(app->users_bst,u);
    if (!n || strcmp(n->user.password,p)!=0){ puts("Invalid credentials."); return; }
    app->current = n;
    printf("Logged in as %s\n", n->user.username);
}

void ui_logout(App *app){
    if (!app->current){ puts("Not logged in."); return; }
    printf("Goodbye, %s\n", app->current->user.username);
    app->current=NULL;
}

void ui_create_post(App *app){
    if (!session_required(app)) return;
    if (app->posts.size >= app->max_posts){ puts("Post limit reached."); return; }
    char text[CONTENT_MAX];
    printf("Content: "); if (!get_line(text,sizeof text)) return;
    if (!*text){ puts("Empty content."); return; }
    Post p; p.id = next_post_id();
    strncpy(p.author, app->current->user.username, USERNAME_MAX-1); p.author[USERNAME_MAX-1]='\0';
    strncpy(p.content, text, CONTENT_MAX-1); p.content[CONTENT_MAX-1]='\0';
    format_timestamp(p.timestamp, TIMESTAMP_MAX);
    if (posts_add(&app->posts,&p)) puts("Posted.");
    else puts("Failed to post.");
}

void ui_view_posts(App *app){ (void)app; posts_list_desc(&((App*)app)->posts); }

void ui_follow(App *app){
    if (!session_required(app)) return;
    char target[USERNAME_MAX];
    printf("Follow username: "); if (!get_line(target,sizeof target)) return;
    if (!bst_find(app->users_bst,target)){ puts("User not found."); return; }
    if (graph_add_edge(&app->graph, app->current->user.username, target)) {
        app->current->user.following++;
        UserNode *t=bst_find(app->users_bst,target); if (t) t->user.followers++;
        printf("Now following %s\n", target);
    } else puts("Follow failed (maybe already following).");
}

void ui_unfollow(App *app){
    if (!session_required(app)) return;
    char target[USERNAME_MAX];
    printf("Unfollow username: "); if (!get_line(target,sizeof target)) return;
    if (graph_remove_edge(&app->graph, app->current->user.username, target)) {
        if (app->current->user.following>0) app->current->user.following--;
        UserNode *t=bst_find(app->users_bst,target); if (t && t->user.followers>0) t->user.followers--;
        printf("Unfollowed %s\n", target);
    } else puts("Unfollow failed (maybe not following).");
}

void ui_show_following(App *app){
    if (!session_required(app)) return;
    graph_show_following(&app->graph, app->current->user.username);
}
void ui_show_followers(App *app){
    if (!session_required(app)) return;
    graph_show_followers(&app->graph, app->current->user.username);
}

void ui_send_message(App *app){
    if (!session_required(app)) return;
    char to[USERNAME_MAX], text[CONTENT_MAX];
    printf("Send to: "); if (!get_line(to,sizeof to)) return;
    if (!bst_find(app->users_bst,to)){ puts("Recipient not found."); return; }
    printf("Message: "); if (!get_line(text,sizeof text)) return;
    Message m;
    strncpy(m.from, app->current->user.username, USERNAME_MAX-1); m.from[USERNAME_MAX-1]='\0';
    strncpy(m.to, to, USERNAME_MAX-1); m.to[USERNAME_MAX-1]='\0';
    strncpy(m.content, text, CONTENT_MAX-1); m.content[CONTENT_MAX-1]='\0';
    format_timestamp(m.timestamp, TIMESTAMP_MAX);
    if (mq_enqueue(&app->mq,&m)) puts("Message queued.");
    else puts("Queue full.");
}

void ui_process_message(App *app){
    Message m;
    if (mq_dequeue(&app->mq,&m))
        printf("Delivered: %s -> %s | %s\n", m.from, m.to, m.content);
    else puts("No messages to deliver.");
}

void ui_show_messages(App *app){
    if (!session_required(app)) return;
    mq_print(&app->mq);
}

/* ====== Admin Functions ====== */
void ui_admin_register(App *app){
    if (app->admin.is_registered){ puts("Admin already registered."); return; }
    char u[ADMIN_USERNAME_MAX], p[ADMIN_PASSWORD_MAX];
    printf("Admin username: "); if (!get_line(u,sizeof u)) return;
    if (!*u){ puts("Invalid username."); return; }
    printf("Admin password: "); if (!get_password(p,sizeof p)) return;
    if (!*p){ puts("Invalid password."); return; }
    strncpy(app->admin.username, u, ADMIN_USERNAME_MAX-1); app->admin.username[ADMIN_USERNAME_MAX-1]='\0';
    strncpy(app->admin.password, p, ADMIN_PASSWORD_MAX-1); app->admin.password[ADMIN_PASSWORD_MAX-1]='\0';
    app->admin.is_registered = 1;
    puts("Admin registered successfully.");
}

void ui_admin_login(App *app){
    if (!app->admin.is_registered){ puts("No admin registered. Please register first."); return; }
    if (app->current_admin){ puts("Admin already logged in."); return; }
    char u[ADMIN_USERNAME_MAX], p[ADMIN_PASSWORD_MAX];
    printf("Admin username: "); if (!get_line(u,sizeof u)) return;
    printf("Admin password: "); if (!get_password(p,sizeof p)) return;
    if (strcmp(app->admin.username,u)!=0 || strcmp(app->admin.password,p)!=0){
        puts("Invalid admin credentials."); return;
    }
    app->current_admin = &app->admin;
    printf("Admin logged in as %s\n", app->admin.username);
}

void ui_admin_logout(App *app){
    if (!app->current_admin){ puts("Admin not logged in."); return; }
    printf("Admin %s logged out.\n", app->admin.username);
    app->current_admin = NULL;
}

void ui_admin_change_limits(App *app){
    if (!app->current_admin){ puts("Admin access required."); return; }
    char buf[32];
    int new_val;
    
    printf("\nCurrent limits:\n");
    printf(" MAX_USERS: %d\n", app->max_users);
    printf(" MAX_POSTS: %d\n", app->max_posts);
    printf(" MAX_MESSAGES: %d\n\n", app->max_messages);
    
    printf("Enter new MAX_USERS (0 to skip): ");
    if (get_line(buf,sizeof buf) && *buf){
        new_val = (int)strtol(buf, NULL, 10);
        if (new_val > 0){ app->max_users = new_val; printf("MAX_USERS set to %d\n", new_val); }
    }
    
    printf("Enter new MAX_POSTS (0 to skip): ");
    if (get_line(buf,sizeof buf) && *buf){
        new_val = (int)strtol(buf, NULL, 10);
        if (new_val > 0){ app->max_posts = new_val; printf("MAX_POSTS set to %d\n", new_val); }
    }
    
    printf("Enter new MAX_MESSAGES (0 to skip): ");
    if (get_line(buf,sizeof buf) && *buf){
        new_val = (int)strtol(buf, NULL, 10);
        if (new_val > 0){ app->max_messages = new_val; printf("MAX_MESSAGES set to %d\n", new_val); }
    }
    
    puts("Limits updated.");
}

/* ====== Menu ====== */
void print_menu(void){
    puts("\n--- SMM MVP ---");
    puts("1. Register user");
    puts("2. Login");
    puts("3. Logout");
    puts("4. Create post");
    puts("5. View posts");
    puts("6. Follow user");
    puts("7. Unfollow user");
    puts("8. Show following");
    puts("9. Show followers");
    puts("10. Send message (enqueue)");
    puts("11. Process message (dequeue)");
    puts("12. Show messages (queue)");
    puts("13. Admin register");
    puts("14. Admin login");
    puts("15. Admin logout");
    puts("16. Admin change limits");
    puts("0. Exit");
    printf("Choice: ");
}
