#define _POSIX_C_SOURCE 199309L  // For nanosleep and timespec

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <fcntl.h>  // for fcntl and its flags
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <errno.h>
#include "encrypt.h" // include the encryption header

#ifdef _WIN32
    #include <conio.h> // for kbhit() and getch()
    #include <windows.h>
    #define CLEAR_SCREEN "cls"
    void sleep_ms(int ms) {
        Sleep(ms);
    }
#else
    #include <unistd.h>
    #include <termios.h>
    #define CLEAR_SCREEN "clear"

    void sleep_ms(int ms) {
        struct timespec ts;
        ts.tv_sec = ms / 1000;
        ts.tv_nsec = (ms % 1000) * 1000000;
        nanosleep(&ts, NULL);
    }

    int _kbhit() {
        static struct termios old_T, new_T; 
        int ch;
        int oldf;
        tcgetattr(STDIN_FILENO, &old_T); 
        new_T = old_T;
        new_T.c_lflag &= ~(ICANON | ECHO); 
        tcsetattr(STDIN_FILENO, TCSANOW, &new_T); 
        oldf = fcntl(STDIN_FILENO, F_GETFL, 0); 
        fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK); 

        ch = getchar();

        tcsetattr(STDIN_FILENO, TCSANOW, &old_T); 
        fcntl(STDIN_FILENO, F_SETFL, oldf); 
        if (ch != EOF) {
            ungetc(ch, stdin); 
            return 1; 
        }

        return 0; 
    }

    int _getch(void) {
        int ch;
        struct termios old_T, new_T;
        tcgetattr(STDIN_FILENO, &old_T);
        new_T = old_T;
        new_T.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &new_T);

        ch = getchar();
        tcsetattr(STDIN_FILENO, TCSANOW, &old_T);
        return ch;
    }
#endif

#define WIDTH 30
#define HEIGHT 20
#define INIT_SNAKE_LENGTH 3

typedef struct {
    int x;
    int y;
} Position;

typedef enum {
    UP,
    DOWN,
    LEFT,
    RIGHT
} Direction;

typedef struct {
    Position pos;
    bool exists;
} Food;

typedef struct {
    Position body[WIDTH * HEIGHT];
    int length;
    Direction dir;
} Snake;

// input recorders
typedef struct {
    char key;
    double timestampe;
}InputRecord;

typedef struct {
    InputRecord inputs[10000];
    int input_count;
    double start_time;
} InputRecorder;

typedef struct {
    Snake snake;
    Food food;
    bool game_over;
    int score;
    InputRecorder input_recorder;
} Game;

typedef struct {
    pthread_t render_thread;
    pthread_mutex_t game_mutex;
    bool running_thread;
    Game* game_ptr;
} ThreadControl;

void Init_Game(Game *game);
void Generate_Food(Game *game);
void Process_Input(Game *game);
void Update_Game(Game *game);
void Render_Game(Game *game);
bool Is_Collision(Position pos, Game *game);
void Save_Input_Record(Game *game);
void* Render_Thread_Func(void* arg);
void Init_Threads(Game* game, ThreadControl* control);
void Cleanup_Threads(ThreadControl* control);

int main(){

    srand(time(NULL));

    Game game; // create  game object
    ThreadControl control; // create thread control object

    // initgame and render threads
    Init_Game(&game); // initialize game
    Init_Threads(&game, &control); // initialize threads


    //loop until game over
    while (!game.game_over) {
        pthread_mutex_lock(&control.game_mutex);
        Process_Input(&game);
        Update_Game(&game);
        // Render_Game(&game);
        pthread_mutex_unlock(&control.game_mutex);
        sleep_ms(100); // we can adjust the speed of the game by changing this value
    }
    Cleanup_Threads(&control); // cleanup threads befrore exiting
    printf("\nGame Over!!! Score is %d\n", game.score);
    Save_Input_Record(&game); 
    return 0;
}

void Init_Game(Game *game){
    game->snake.length = INIT_SNAKE_LENGTH;
    game->snake.dir = RIGHT;

    // place snake @ center of the screen
    int Middle_X = WIDTH / 2;
    int Middle_Y = HEIGHT / 2;

    for(int i = 0; i < game->snake.length; ++i){
        game->snake.body[i].x = Middle_X - i;
        game->snake.body[i].y = Middle_Y;
    }

    // check game state
    game->game_over = false;
    game->score = 0;

    // gen init fod
    Generate_Food(game);

    // init recorder
    game->input_recorder.input_count = 0;
    game->input_recorder.start_time = time(NULL);
}

void Generate_Food(Game *game){
    bool valid_position = false;
    // use a Position object to store the new food position
    Position new_food_pos;

    do{
        valid_position = true;
        new_food_pos.x = rand() % WIDTH;
        new_food_pos.y = rand() % HEIGHT;

        for (int i = 0; i < game->snake.length; ++i) {
            if(game->snake.body[i].x == new_food_pos.x &&
              game->snake.body[i].y == new_food_pos.y) {
                valid_position = false;
                break;
            }
        }
    } while (!valid_position);
    game->food.pos = new_food_pos;
}

void Process_Input(Game *game){

    if(_kbhit()){
         /* we ue W(up) As(left) S(Down) D(Right) so it's easier */
        char ch = _getch();

        // record input
        if (game->input_recorder.input_count < 10000) {
            game->input_recorder.inputs[game->input_recorder.input_count].key = ch;
            game->input_recorder.inputs[game->input_recorder.input_count].timestampe = 
                time(NULL) - game->input_recorder.start_time;
            ++game->input_recorder.input_count;
        }
        switch(ch){
            case 'W':
            case 'w':
                if(game->snake.dir != DOWN) game->snake.dir = UP;
                break;
            case 'S':
            case 's':
                if(game->snake.dir != UP) game->snake.dir = DOWN;
                break;
            case 'A':
            case 'a':
                if(game->snake.dir != RIGHT) game->snake.dir = LEFT;
                break;
            case 'D':
            case 'd':
                if(game->snake.dir != LEFT) game->snake.dir = RIGHT;
                break;
            case 'Q':
            case 'q':
                game->game_over = true; // quit the game
                break;
        } // end switch
    } // end if
}

void Save_Input_Record(Game *game) {
    const char *file_path = "./input_record.txt"; // ensure file is created in the current directory
    FILE *file = fopen(file_path, "w");
    if (!file) {
        perror("Error: Cannot open file for writing");
        return;
    }

    fprintf(file, "Snake Game Input Record\n");
    fprintf(file, "Time(s)\tKey\n");

    for (int i = 0; i < game->input_recorder.input_count; ++i) {
        fprintf(file, "%.1f\t%c\n", game->input_recorder.inputs[i].timestampe, 
            game->input_recorder.inputs[i].key);
    }

    fclose(file);
    printf("Input record saved to %s\n", file_path);

        
    // call the encryption function
    const char *encrypted_file_path = "./input_record.enc";
    const char *password = "SnakeGameSecretPassword";
    
    if (encrypt_file(file_path, encrypted_file_path, password)) {
        // remove the plaintext file if encryption was successful
        remove(file_path);
        printf("Input record encrypted and saved to %s\n", encrypted_file_path);
    } else {
        printf("Failed to encrypt input record\n");
    }
}

void Update_Game(Game *game){
    Position new_head = game->snake.body[0]; // get the current head position

    // update head pos. based on dir.
    switch(game->snake.dir){
        case UP:
            --new_head.y; // we use
            break;
        case DOWN:
            ++new_head.y;
            break;
        case LEFT:
           --new_head.x;
            break;
        case RIGHT:
            ++new_head.x;
            break;
    } // end switch

    // check wall collision
    if (new_head.x < 0 || new_head.x >= WIDTH ||
        new_head.y < 0 || new_head.y >= HEIGHT) {
        game->game_over = true;
        return;
    }

    // check self collision ?
    if (Is_Collision(new_head, game)) {
        game->game_over = true;
        return;
    }

    // check food collision
    bool ate_food = (new_head.x == game->food.pos.x &&
                     new_head.y == game->food.pos.y);


    // we start at the end of the snake
    // and move each segment to the position of the segment in front of it
    for(int i = game->snake.length - 1; i > 0; --i) {
        game->snake.body[i] = game->snake.body[i - 1];
    }

    //update head pos
    game->snake.body[0] = new_head;

    // glutony
    if (ate_food) {
        ++game->snake.length;
        game->score += 10; 
        Generate_Food(game); // generate new food
        if (game->input_recorder.input_count < 10000) {
            // we add a food key to record when food is eaten 
            game->input_recorder.inputs[game->input_recorder.input_count].key = 'F'; 
            game->input_recorder.inputs[game->input_recorder.input_count].timestampe = 
                time(NULL) - game->input_recorder.start_time;
            ++game->input_recorder.input_count;
        }
    }
}

bool Is_Collision(Position pos, Game *game) {
    for (int i = 0; i < game->snake.length; ++i) {
        if (game->snake.body[i].x == pos.x && game->snake.body[i].y == pos.y) {
            return true; // collision detected
        }
    }
    return false; // no collision
}

void Render_Game(Game *game) {
    system(CLEAR_SCREEN); // clear the screen

    char buffer[HEIGHT][WIDTH + 1];

    // init buffer with empty spaces
    for(int y = 0 ; y < HEIGHT; ++y) {
        for(int x = 0; x < WIDTH; ++x) {
            buffer[y][x] = '.';
        }
        buffer[y][WIDTH] = '\0'; // null-terminate the string
    }

    // place snake in buffer
    for(int i = 0; i < game->snake.length; ++i) {
        int x = game->snake.body[i].x;
        int y = game->snake.body[i].y;

        if( x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
            // head and body
            buffer[y][x] = (i == 0) ? 'P' : 'o'; // if it's the head, use 'P', else use 'o'
        }
    }
    // place food
    buffer[game->food.pos.y][game->food.pos.x] = '*'; // food

    // create the border
    printf("+");
    for (int x = 0; x < WIDTH; ++x) {
        printf("-");
    }
    printf("+\n");

    for(int y = 0; y < HEIGHT; ++y) {
        printf("|");
        printf("%s", buffer[y]); // print the row
        printf("|\n");
    }

    printf("+");
    for(int x = 0 ; x < WIDTH; ++x) {
        printf("-");
    }
    printf("+\n");
    printf("Score: %d | Controls: W/A/S/D | Q to quit\n", game->score);
}

void* Render_Thread_Func(void* arg) {
    ThreadControl* control = (ThreadControl*)arg;
    Game* game = control->game_ptr;

    while (control->running_thread) {
        pthread_mutex_lock(&control->game_mutex);
        Render_Game(game);
        pthread_mutex_unlock(&control->game_mutex);
        sleep_ms(100); // adjust the render speed
    }
    return NULL;
}

void Init_Threads(Game* game, ThreadControl* control) {
    control->game_ptr = game;
    control->running_thread = true;

    // init mutex
    pthread_mutex_init(&control->game_mutex, NULL);

    // create render thread
    if (pthread_create(&control->render_thread, NULL, Render_Thread_Func, control) != 0) {
        perror("Error creating render thread");
        exit(EXIT_FAILURE);
    }
}

void Cleanup_Threads(ThreadControl* control) {
    control->running_thread = false;
    pthread_join(control->render_thread, NULL);
    pthread_mutex_destroy(&control->game_mutex);
   // printf("Render thread cleaned up\n");
}