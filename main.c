#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "smm.h"

int main(void){
    App app;
    app_init(&app);

    char buf[32];
    int choice;

    puts("Welcome to SMM (MVP) — session-based, in-memory.");

    while (1) {
        print_menu();
        if (!get_line(buf, sizeof buf)) break;
        choice = (int)strtol(buf, NULL, 10);

        switch(choice){
            case 1: ui_register(&app); break;
            case 2: ui_login(&app); break;
            case 3: ui_logout(&app); break;
            case 4: ui_create_post(&app); break;
            case 5: ui_view_posts(&app); break;
            case 6: ui_follow(&app); break;
            case 7: ui_unfollow(&app); break;
            case 8: ui_show_following(&app); break;
            case 9: ui_show_followers(&app); break;
            case 10: ui_send_message(&app); break;
            case 11: ui_process_message(&app); break;
            case 12: ui_show_messages(&app); break;
            case 13: ui_admin_register(&app); break;
            case 14: ui_admin_login(&app); break;
            case 15: ui_admin_logout(&app); break;
            case 16: ui_admin_change_limits(&app); break;
            case 0: 
                app_free(&app); 
                puts("Bye!");
                return 0;
            default: puts("Invalid choice.");
        }
    }

    app_free(&app);
    return 0;
}
