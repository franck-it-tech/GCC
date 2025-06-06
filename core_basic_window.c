
#include "raylib.h"
#include <stdlib.h>
#include <time.h>
#include <stdio.h>

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define CELL_SIZE 20
#define MAX_SNAKE_LENGTH 100
#define MAX_OBSTACLES 100

// --- Structures du Jeu ---
typedef struct {
    int x, y;
} Vector2Int;

typedef enum { UP, DOWN, LEFT, RIGHT } Direction;

// Structure pour un serpent individuel
typedef struct {
    Vector2Int body[MAX_SNAKE_LENGTH]; // Le corps du serpent
    int length;                       // La longueur du serpent
    Direction dir;                    // La direction du serpent
    Color color;                      // La couleur du serpent pour l'affichage
    bool isDead;                      // Indique si ce serpent est mort
} Snake;

// Structure de l'état global du jeu
typedef struct {
    Snake player1;
    Snake player2; // Utilisé seulement en mode 2 joueurs

    Vector2Int fruit;
    int score1;
    int score2; // Utilisé seulement en mode 2 joueurs
    bool gameOver;
    bool gamePaused;
    int speed;
    int highScore;
    Vector2Int obstacles[MAX_OBSTACLES];
    int obstacleCount;
    float timeElapsed;
    int winningPlayer; // 0=aucun, 1=joueur1, 2=joueur2, 3=égalité

    // Sons
    Sound gameOverSound;
    Music backgroundMusic;
    bool gameOverSoundPlayed;

    int gameMode; // 1 pour 1 joueur, 2 pour 2 joueurs
    // Ajout pour garder les paramètres de la partie en cours
    int lastSelectedSpeed;
    int lastSelectedGameMode;

} GameState;

// --- Fonctions Utilitaires ---

// Vérifie si deux positions sont identiques
bool IsCollision(Vector2Int a, Vector2Int b) {
    return a.x == b.x && a.y == b.y;
}

// Vérifie si une position est sur un obstacle
bool IsOnObstacle(GameState *game, Vector2Int pos) {
    for (int i = 0; i < game->obstacleCount; i++) {
        if (IsCollision(pos, game->obstacles[i])) return true;
    }
    return false;
}

// Vérifie si une position est sur le corps d'un des serpents
bool IsOnAnySnake(GameState *game, Vector2Int pos) {
    // Vérifie toujours le joueur 1
    for (int i = 0; i < game->player1.length; i++) {
        if (IsCollision(pos, game->player1.body[i])) return true;
    }
    // Si en mode 2 joueurs, vérifie aussi le joueur 2
    if (game->gameMode == 2) {
        for (int i = 0; i < game->player2.length; i++) {
            if (IsCollision(pos, game->player2.body[i])) return true;
        }
    }
    return false;
}

// Génère une nouvelle position pour le fruit, en évitant les serpents et les obstacles
void GenerateNewFruitPosition(GameState *game) {
    Vector2Int newFruitPos;
    bool validPos;
    do {
        newFruitPos.x = rand() % (SCREEN_WIDTH / CELL_SIZE);
        newFruitPos.y = rand() % (SCREEN_HEIGHT / CELL_SIZE);
        validPos = true;

        if (IsCollision(newFruitPos, game->player1.body[0]) || // Avoid spawning fruit on player1 head
            (game->gameMode == 2 && IsCollision(newFruitPos, game->player2.body[0])) || // Avoid spawning fruit on player2 head in 2-player mode
            IsOnAnySnake(game, newFruitPos) || IsOnObstacle(game, newFruitPos)) {
            validPos = false;
        }
    } while (!validPos);
    game->fruit = newFruitPos;
}

// Ajoute un obstacle aléatoirement sur la carte, en évitant le fruit, les serpents et les obstacles existants
void AddObstacle(GameState *game) {
    if (game->obstacleCount >= MAX_OBSTACLES) return;
    Vector2Int o;
    do {
        o.x = rand() % (SCREEN_WIDTH / CELL_SIZE);
        o.y = rand() % (SCREEN_HEIGHT / CELL_SIZE);
    } while (IsCollision(o, game->fruit) || IsOnObstacle(game, o) || IsOnAnySnake(game, o));
    game->obstacles[game->obstacleCount++] = o;
}

// --- Fonctions de Jeu ---

// Initialise l'état du jeu pour une nouvelle partie
void InitGame(GameState *game, int speed, int mode) {
    game->gameMode = mode; // Définit le mode de jeu (1 ou 2 joueurs)
    game->lastSelectedSpeed = speed; // Sauvegarde la vitesse choisie
    game->lastSelectedGameMode = mode; // Sauvegarde le mode choisi

    // Initialisation du Joueur 1
    game->player1.length = 2;
    game->player1.dir = RIGHT;
    game->player1.color = GREEN;
    game->player1.isDead = false;
    for (int i = 0; i < game->player1.length; i++) {
        game->player1.body[i] = (Vector2Int){10 - i, 10};
    }
    game->score1 = 0;

    // Initialisation du Joueur 2 (seulement si mode 2 joueurs)
    if (game->gameMode == 2) {
        game->player2.length = 2;
        game->player2.dir = LEFT;
        game->player2.color = BLUE;
        game->player2.isDead = false;
        for (int i = 0; i < game->player2.length; i++) {
            game->player2.body[i] = (Vector2Int){(SCREEN_WIDTH / CELL_SIZE) - 10 + i, (SCREEN_HEIGHT / CELL_SIZE) - 10};
        }
        game->score2 = 0;
    } else {
        // En mode 1 joueur, assurez-vous que le J2 est considéré comme "mort" pour éviter des bugs de collision
        game->player2.isDead = true;
        game->player2.length = 0; // Pas de corps pour le J2
        game->score2 = 0; // Réinitialiser le score J2
    }


    game->gameOver = false;
    game->gamePaused = false;
    game->speed = speed;
    game->obstacleCount = 0;
    game->timeElapsed = 0.0f;
    game->winningPlayer = 0;
    game->gameOverSoundPlayed = false;

    srand(time(NULL));
    GenerateNewFruitPosition(game);
}

// Met à jour la logique d'un serpent (mouvement, collisions)
// Prend en paramètre le serpent actuel, l'état du jeu, et l'autre serpent (peut être null en mode 1 joueur)
void UpdateSnakeLogic(Snake *snake, GameState *game, Snake *otherSnake, int *score) {
    if (snake->isDead) return;

    // Déplacement du corps
    for (int i = snake->length - 1; i > 0; i--) {
        snake->body[i] = snake->body[i - 1];
    }
    // Déplacement de la tête
    switch (snake->dir) {
        case UP: snake->body[0].y--; break;
        case DOWN: snake->body[0].y++; break;
        case LEFT: snake->body[0].x--; break;
        case RIGHT: snake->body[0].x++; break;
    }

    // Vérification des collisions pour ce serpent
    // Collision avec les bords de l'écran ou un obstacle
    if (snake->body[0].x < 0 || snake->body[0].x >= SCREEN_WIDTH / CELL_SIZE ||
        snake->body[0].y < 0 || snake->body[0].y >= SCREEN_HEIGHT / CELL_SIZE ||
        IsOnObstacle(game, snake->body[0])) {
        snake->isDead = true;
    }
    // Auto-collision
    for (int i = 1; i < snake->length; i++) {
        if (IsCollision(snake->body[0], snake->body[i])) {
            snake->isDead = true;
            break;
        }
    }
    // Collision avec l'autre serpent (seulement en mode 2 joueurs et si l'autre serpent n'est pas "mort")
    if (game->gameMode == 2 && !otherSnake->isDead) {
        for (int i = 0; i < otherSnake->length; i++) {
            if (IsCollision(snake->body[0], otherSnake->body[i])) {
                snake->isDead = true;
                break;
            }
        }
    }

    // Gestion du fruit si le serpent n'est pas encore mort
    if (!snake->isDead && IsCollision(snake->body[0], game->fruit)) {
        if (snake->length < MAX_SNAKE_LENGTH) snake->length++;
        (*score)++;
        
        GenerateNewFruitPosition(game);

        if (game->speed < 20) game->speed++;
        if ((*score) % 3 == 0) AddObstacle(game);
    }
}

// Dessine un serpent
void DrawSnake(Snake *snake) {
    if (snake->isDead && snake->length == 0) return; // Ne dessine pas les serpents "morts" du mode 1 joueur

    for (int i = 0; i < snake->length; i++) {
        DrawCircle(snake->body[i].x * CELL_SIZE + CELL_SIZE / 2,
                   snake->body[i].y * CELL_SIZE + CELL_SIZE / 2,
                   CELL_SIZE / 2 - 1, snake->color);
    }

    // Yeux sur la tête
    if (snake->length > 0) { // S'assure que la tête existe pour dessiner les yeux
        Vector2Int head = snake->body[0];
        int cx = head.x * CELL_SIZE + CELL_SIZE / 2;
        int cy = head.y * CELL_SIZE + CELL_SIZE / 2;
        int offset = 4;
        Color eyeColor = BLACK;

        if (snake->dir == UP || snake->dir == DOWN) {
            DrawCircle(cx - offset, cy - (snake->dir == UP ? offset : -offset), 2, eyeColor);
            DrawCircle(cx + offset, cy - (snake->dir == UP ? offset : -offset), 2, eyeColor);
        } else {
            DrawCircle(cx + (snake->dir == RIGHT ? offset : -offset), cy - offset, 2, eyeColor);
            DrawCircle(cx + (snake->dir == RIGHT ? offset : -offset), cy + offset, 2, eyeColor);
        }
    }
}


// --- Fonction Principale ---
int main(void) {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Snake Game - Raylib");
    InitAudioDevice();

    Texture2D background = LoadTexture("serpent.jpg");

    GameState game = {0};
    game.backgroundMusic = LoadMusicStream("fondson.mp3");
    game.gameOverSound = LoadSound("crash.mp3"); // Chargement du son de fin de partie

    PlayMusicStream(game.backgroundMusic);
    SetMusicVolume(game.backgroundMusic, 0.3f);
    SetSoundVolume(game.gameOverSound, 0.8f);


    // États du menu principal
    // 0: Choix du mode de jeu (1 ou 2 joueurs)
    // 1: Choix de la difficulté (après avoir choisi le mode)
    // 2: Jeu en cours
    int menuState = 0;
    int selectedGameMode = 1; // 1 par défaut pour 1 joueur
    int selectedSpeed = 5;    // 5 par défaut pour la difficulté "Facile"

    char *gameModes[] = { "1 Joueur", "2 Joueurs" };
    char *difficulty[] = { "Facile", "Normal", "Hardcore" };
    
    FILE *f = fopen("highscore.txt", "r");
    if (f) { fscanf(f, "%d", &game.highScore); fclose(f); }
    else { game.highScore = 0; }

    SetTargetFPS(60);

    float timer = 0;

    while (!WindowShouldClose()) {
        UpdateMusicStream(game.backgroundMusic);

        BeginDrawing();
        ClearBackground(BLACK);
        DrawTexture(background, 0, 0, WHITE);

        if (menuState == 0) { // Menu de sélection du mode de jeu
            DrawText("Snake Game", SCREEN_WIDTH / 2 - MeasureText("Snake Game", 40)/2, 100, 40, GREEN);
            DrawText("Choisir le mode de jeu :", SCREEN_WIDTH / 2 - MeasureText("Choisir le mode de jeu :", 20)/2, 200, 20, WHITE);

            for (int i = 0; i < 2; i++) {
                DrawText(gameModes[i], SCREEN_WIDTH / 2 - MeasureText(gameModes[i], 20)/2, 240 + i * 30, 20,
                             (selectedGameMode == (i + 1)) ? YELLOW : GRAY);
            }

            if (IsKeyPressed(KEY_UP) && selectedGameMode > 1) selectedGameMode--;
            if (IsKeyPressed(KEY_DOWN) && selectedGameMode < 2) selectedGameMode++;
            if (IsKeyPressed(KEY_ENTER)) {
                menuState = 1; // Passe à l'écran de sélection de difficulté
            }

        } else if (menuState == 1) { // Menu de sélection de la difficulté
            DrawText("Snake Game", SCREEN_WIDTH / 2 - MeasureText("Snake Game", 40)/2, 100, 40, GREEN);
            DrawText("Choisissez la difficulté :", SCREEN_WIDTH / 2 - MeasureText("Choisissez la difficulté :", 20)/2, 200, 20, WHITE);

            for (int i = 0; i < 3; i++) {
                DrawText(difficulty[i], SCREEN_WIDTH / 2 - MeasureText(difficulty[i], 20)/2, 240 + i * 30, 20,
                             (selectedSpeed == (i + 5)) ? YELLOW : GRAY);
            }

            if (IsKeyPressed(KEY_UP) && selectedSpeed > 5) selectedSpeed--;
            if (IsKeyPressed(KEY_DOWN) && selectedSpeed < 7) selectedSpeed++;
            if (IsKeyPressed(KEY_ENTER)) {
                InitGame(&game, selectedSpeed, selectedGameMode); // Initialise le jeu avec le mode et la vitesse choisis
                menuState = 2; // Passe à l'état de jeu
            }
        }
        else if (menuState == 2) { // État du jeu en cours
            if (!game.gameOver && !game.gamePaused) {
                // Contrôles Joueur 1 (Flèches)
                if (IsKeyPressed(KEY_UP) && game.player1.dir != DOWN) game.player1.dir = UP;
                if (IsKeyPressed(KEY_DOWN) && game.player1.dir != UP) game.player1.dir = DOWN;
                if (IsKeyPressed(KEY_LEFT) && game.player1.dir != RIGHT) game.player1.dir = LEFT;
                if (IsKeyPressed(KEY_RIGHT) && game.player1.dir != LEFT) game.player1.dir = RIGHT;

                // Contrôles Joueur 2 (WASD) - Seulement en mode 2 joueurs
                if (game.gameMode == 2) {
                    if (IsKeyPressed(KEY_W) && game.player2.dir != DOWN) game.player2.dir = UP;
                    if (IsKeyPressed(KEY_S) && game.player2.dir != UP) game.player2.dir = DOWN;
                    if (IsKeyPressed(KEY_A) && game.player2.dir != RIGHT) game.player2.dir = LEFT;
                    if (IsKeyPressed(KEY_D) && game.player2.dir != LEFT) game.player2.dir = RIGHT;
                }
            }

            // Gestion de la pause
            if (IsKeyPressed(KEY_SPACE)) game.gamePaused = !game.gamePaused;

            // Gestion du redémarrage et retour au menu depuis la pause ou le game over
            if (game.gamePaused || game.gameOver) {
                if (IsKeyPressed(KEY_ENTER)) {
                    InitGame(&game, game.lastSelectedSpeed, game.lastSelectedGameMode); // Redémarrer avec les mêmes paramètres
                }
               
            }


            float delta = GetFrameTime();
            game.timeElapsed += delta;
            timer += delta;

            // Mise à jour de la logique du jeu
            if (timer >= 1.0f / game.speed && !game.gameOver && !game.gamePaused) {
                timer = 0;

                // Mise à jour pour le Joueur 1
                UpdateSnakeLogic(&game.player1, &game, &game.player2, &game.score1);

                // Mise à jour pour le Joueur 2 (seulement en mode 2 joueurs)
                if (game.gameMode == 2) {
                    UpdateSnakeLogic(&game.player2, &game, &game.player1, &game.score2);
                }

                // Déterminer l'état de fin de jeu
                // Le jeu est terminé si le joueur 1 est mort (toujours)
                // OU si le joueur 2 est mort (seulement en mode 2 joueurs)
                if (game.player1.isDead || (game.gameMode == 2 && game.player2.isDead)) {
                    game.gameOver = true;
                    if (!game.gameOverSoundPlayed) {
                        PlaySound(game.gameOverSound);
                        game.gameOverSoundPlayed = true;
                    }

                    if (game.gameMode == 1) { // Mode 1 joueur : le joueur 1 perd
                        game.winningPlayer = 0; // Pas de gagnant dans ce cas, juste "Game Over"
                    } else { // Mode 2 joueurs
                        if (game.player1.isDead && game.player2.isDead) {
                            game.winningPlayer = 3; // Égalité
                        } else if (game.player1.isDead) {
                            game.winningPlayer = 2; // Joueur 2 gagne
                        } else if (game.player2.isDead) {
                            game.winningPlayer = 1; // Joueur 1 gagne
                        }
                    }
                }
            }

            // Affichage des serpents
            DrawSnake(&game.player1);
            if (game.gameMode == 2) {
                DrawSnake(&game.player2);
            }

            // Affichage du fruit
            DrawCircle(game.fruit.x * CELL_SIZE + CELL_SIZE / 2,
                       game.fruit.y * CELL_SIZE + CELL_SIZE / 2,
                       CELL_SIZE / 2 - 1, RED);

            // Affichage des obstacles
            for (int i = 0; i < game.obstacleCount; i++) {
                DrawRectangle(game.obstacles[i].x * CELL_SIZE, game.obstacles[i].y * CELL_SIZE,
                              CELL_SIZE, CELL_SIZE, DARKGRAY);
            }

            // Affichage des informations de jeu
            if (game.gameMode == 1) {
                DrawText(TextFormat("Score: %d", game.score1), 10, 10, 20, game.player1.color);
            } else { // Mode 2 joueurs
                DrawText(TextFormat("Score J1: %d", game.score1), 10, 10, 20, game.player1.color);
                DrawText(TextFormat("Score J2: %d", game.score2), SCREEN_WIDTH - MeasureText(TextFormat("Score J2: %d", game.score2), 20) - 10, 10, 20, game.player2.color);
            }
            DrawText(TextFormat("High Score: %d", game.highScore), 10, 40, 20, YELLOW);
            DrawText(TextFormat("Temps: %.1f sec", game.timeElapsed), SCREEN_WIDTH / 2 - MeasureText(TextFormat("Temps: %.1f sec", game.timeElapsed), 20)/2, 10, 20, ORANGE);

            if (game.gamePaused && !game.gameOver) {
                DrawText("PAUSE", SCREEN_WIDTH / 2 - MeasureText("PAUSE", 40)/2, SCREEN_HEIGHT / 2 - 50, 40, ORANGE);
                DrawText("Appuyez sur ENTREE pour recommencer", SCREEN_WIDTH / 2 - MeasureText("Appuyez sur ENTREE pour recommencer", 20)/2, SCREEN_HEIGHT / 2 + 10, 20, WHITE);
               
            }


            if (game.gameOver) {
                
                DrawText("GAME OVER", SCREEN_WIDTH / 2 - MeasureText("GAME OVER", 40)/2, SCREEN_HEIGHT / 2 - 80, 40, RED);
                 //StopMusicStream(game.backgroundMusic);              
                if (game.gameMode == 1) {
                    DrawText("Vous avez perdu !", SCREEN_WIDTH / 2 - MeasureText("Vous avez perdu !", 25)/2, SCREEN_HEIGHT / 2 - 30, 25, WHITE);
                } else { // Mode 2 joueurs
                    if (game.winningPlayer == 1) {
                        DrawText("Le Joueur 1 a gagné !", SCREEN_WIDTH / 2 - MeasureText("Le Joueur 1 a gagné !", 25)/2, SCREEN_HEIGHT / 2 - 30, 25, game.player1.color);
                    } else if (game.winningPlayer == 2) {
                        DrawText("Le Joueur 2 a gagné !", SCREEN_WIDTH / 2 - MeasureText("Le Joueur 2 a gagné !", 25)/2, SCREEN_HEIGHT / 2 - 30, 25, game.player2.color);
                    } else if (game.winningPlayer == 3) {
                        DrawText("ÉGALITÉ !", SCREEN_WIDTH / 2 - MeasureText("ÉGALITÉ !", 25)/2, SCREEN_HEIGHT / 2 - 30, 25, WHITE);
                    }
                }
                DrawText("Appuyez sur ENTREE pour recommencer", SCREEN_WIDTH / 2 - MeasureText("Appuyez sur ENTREE pour recommencer", 20)/2, SCREEN_HEIGHT / 2 + 10, 20, WHITE);
                


                int currentMaxScore = game.score1; // En mode 1 joueur, c'est le score du J1
                if (game.gameMode == 2) { // En mode 2 joueurs, c'est le meilleur des deux
                    currentMaxScore = (game.score1 > game.score2) ? game.score1 : game.score2;
                }
                if (currentMaxScore > game.highScore) {
                    game.highScore = currentMaxScore;
                    FILE *f = fopen("highscore.txt", "w");
                    if (f) { fprintf(f, "%d", game.highScore); fclose(f); }
                }
            }
        }

        EndDrawing();
    }

    // Libération des ressources
    UnloadTexture(background);
    UnloadMusicStream(game.backgroundMusic);
    UnloadSound(game.gameOverSound);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
