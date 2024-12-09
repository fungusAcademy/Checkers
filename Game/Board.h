#pragma once
#include <iostream>
#include <fstream>
#include <vector>

#include "../Models/Move.h"
#include "../Models/Project_path.h"

#ifdef __APPLE__
    #include <SDL2/SDL.h>
    #include <SDL2/SDL_image.h>
#else
    #include <SDL.h>
    #include <SDL_image.h>
#endif

using namespace std;

class Board
{
public:
    Board() = default;
    Board(const unsigned int W, const unsigned int H) : W(W), H(H)
    {
    }

    // draws start board
    int start_draw()
    {
        // Инициализация библиотеки SDL
        if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
        {
            print_exception("SDL_Init can't init SDL2 lib");
            return 1;
        }
        // Установка размеров окна, если они еще не заданы
        if (W == 0 || H == 0)
        {
            SDL_DisplayMode dm;
            if (SDL_GetDesktopDisplayMode(0, &dm))
            {
                print_exception("SDL_GetDesktopDisplayMode can't get desctop display mode");
                return 1;
            }
            W = min(dm.w, dm.h);
            W -= W / 15;
            H = W;
        }
        // Создание окна
        win = SDL_CreateWindow("Checkers", 0, H / 30, W, H, SDL_WINDOW_RESIZABLE);
        if (win == nullptr)
        {
            print_exception("SDL_CreateWindow can't create window");
            return 1;
        }
        // Создание рендерера
        ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (ren == nullptr)
        {
            print_exception("SDL_CreateRenderer can't create renderer");
            return 1;
        }
        // Загрузка текстур
        board = IMG_LoadTexture(ren, board_path.c_str());
        w_piece = IMG_LoadTexture(ren, piece_white_path.c_str());
        b_piece = IMG_LoadTexture(ren, piece_black_path.c_str());
        w_queen = IMG_LoadTexture(ren, queen_white_path.c_str());
        b_queen = IMG_LoadTexture(ren, queen_black_path.c_str());
        back = IMG_LoadTexture(ren, back_path.c_str());
        replay = IMG_LoadTexture(ren, replay_path.c_str());
        if (!board || !w_piece || !b_piece || !w_queen || !b_queen || !back || !replay)
        {
            print_exception("IMG_LoadTexture can't load main textures from " + textures_path);
            return 1;
        }
        // Получение реальных размеров окна
        SDL_GetRendererOutputSize(ren, &W, &H);
        // Создание начальной матрицы состояния игры
        make_start_mtx();
        // Перерисовка сцены
        rerender();
        // Успешное завершение
        return 0;
    }

    void redraw()
    {
        // Сбрасываем результаты игры
        game_results = -1;
        // Очищаем историю ходов
        history_mtx.clear();
        // Очищаем историю серий ударов
        history_beat_series.clear();
        // Восстанавливаем начальную позицию
        make_start_mtx();
        // Очищаем активные элементы
        clear_active();
        // Очищаем подсветки
        clear_highlight();
    }

    // Перемещает фигуру на новое место, удаляя её со старого места
    void move_piece(move_pos turn, const int beat_series = 0)
    {
        // Если фигура бьёт другую, удаляем побитую фигуру
        if (turn.xb != -1)
        {
            mtx[turn.xb][turn.yb] = 0;
        }
        move_piece(turn.x, turn.y, turn.x2, turn.y2, beat_series);
    }

    // Основная функция перемещения фигуры
    void move_piece(const POS_T i, const POS_T j, const POS_T i2, const POS_T j2, const int beat_series = 0)
    {
        // Если конечное положение занято, выбрасываем исключение
        if (mtx[i2][j2])
        {
            throw runtime_error("final position is not empty, can't move");
        }
        //Если исходное положение пусто, выбрасываем исключение
        if (!mtx[i][j])
        {
            throw runtime_error("begin position is empty, can't move");
        }
        // Если простая фигура дошла до конца поля, превращаем её в королеву
        if ((mtx[i][j] == 1 && i2 == 0) || (mtx[i][j] == 2 && i2 == 7))
            mtx[i][j] += 2;
        // Перемещаем фигуру
        mtx[i2][j2] = mtx[i][j];
        // Удаляем фигуру с исходного положения
        drop_piece(i, j);
        // Добавляем ход в историю
        add_history(beat_series);
    }

    // Удаляет фигуру с доски
    void drop_piece(const POS_T i, const POS_T j)
    {
        // Устанавливаем значение ноль
        mtx[i][j] = 0;
        // Перерисовываем доску
        rerender();
    }

    // Превращает простую фигуру в королеву
    void turn_into_queen(const POS_T i, const POS_T j)
    {
        // Если позиция пустая или уже королева, выбрасываем исключение
        if (mtx[i][j] == 0 || mtx[i][j] > 2)
        {
            throw runtime_error("can't turn into queen in this position");
        }
        // Превращаем в королеву
        mtx[i][j] += 2;
        // Перерисовываем доску
        rerender();
    }
    // Возвращает копию текущей доски
    vector<vector<POS_T>> get_board() const
    {
        return mtx;
    }

    // Подсвечивает указанные клетки
    void highlight_cells(vector<pair<POS_T, POS_T>> cells)
    {
        for (auto pos : cells)
        {
            POS_T x = pos.first, y = pos.second;
            is_highlighted_[x][y] = 1;
        }
        rerender();
    }

    void clear_highlight()
    {
        // Очищаем массив подсветок для каждой строки
        for (POS_T i = 0; i < 8; ++i)
        {
            is_highlighted_[i].assign(8, 0);
        }
        rerender();
    }

    void set_active(const POS_T x, const POS_T y)
    {
        // Устанавливаем активную строку
        active_x = x;
        // Устанавливаем активный столбец
        active_y = y;
        // Перерисовываем доску
        rerender();
    }

    void clear_active()
    {
        // Сбрасываем активную строку
        active_x = -1;
        // Сбрасываем активный столбец
        active_y = -1;
        // Перерисовываем доску
        rerender();
    }

    // Проверяем, подсвечена ли данная клетка
    bool is_highlighted(const POS_T x, const POS_T y)
    {
        return is_highlighted_[x][y];
    }

    void rollback()
    {
        // Берём последнюю серию ударов
        auto beat_series = max(1, *(history_beat_series.rbegin()));
        // Пока серия ударов больше нуля и история не пуста
        while (beat_series-- && history_mtx.size() > 1)
        {
            // Удаляем последний ход из истории
            history_mtx.pop_back();
            // Удаляем последнюю серию ударов
            history_beat_series.pop_back();
        }
        // Восстанавливаем доску из последней записи в истории
        mtx = *(history_mtx.rbegin());
        // Очищаем подсветки
        clear_highlight();
        // Очищаем активные клетки
        clear_active();
    }

    void show_final(const int res)
    {
        // Устанавливаем результат игры
        game_results = res;
        // Перерисовываем доску
        rerender();
    }

    // use if window size changed
    void reset_window_size()
    {
        // Получаем новые размеры окна и перерисовываем доску
        SDL_GetRendererOutputSize(ren, &W, &H);
        rerender();
    }

    // Освобождаем текстуры и завершаем работу с SDL
    void quit()
    {
        SDL_DestroyTexture(board);
        SDL_DestroyTexture(w_piece);
        SDL_DestroyTexture(b_piece);
        SDL_DestroyTexture(w_queen);
        SDL_DestroyTexture(b_queen);
        SDL_DestroyTexture(back);
        SDL_DestroyTexture(replay);
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_Quit();
    }

    // Если окно существует, освобождаем ресурсы
    ~Board()
    {
        if (win)
            quit();
    }

private:
    void add_history(const int beat_series = 0)
    {
        // Добавляем текущее состояние доски в историю
        history_mtx.push_back(mtx);
        // Добавляем информацию о серии ударов
        history_beat_series.push_back(beat_series);
    }
    // function to make start matrix
    // Функция для создания стартовой конфигурации доски
    void make_start_mtx()
    {
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                mtx[i][j] = 0;
                if (i < 3 && (i + j) % 2 == 1)
                    mtx[i][j] = 2;
                if (i > 4 && (i + j) % 2 == 1)
                    mtx[i][j] = 1;
            }
        }
        // Добавляем стартовую доску в историю
        add_history();
    }

    // function that re-draw all the textures
    // Функция для перерисовки всех текстур
    void rerender()
    {
        // draw board
        // Очищаем рендерер
        SDL_RenderClear(ren);
        // Рисуем доску
        SDL_RenderCopy(ren, board, NULL, NULL);

        // draw pieces
        // Рисуем шашки
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                // Пропускаем пустые клетки
                if (!mtx[i][j])
                    continue;
                int wpos = W * (j + 1) / 10 + W / 120;
                int hpos = H * (i + 1) / 10 + H / 120;
                SDL_Rect rect{ wpos, hpos, W / 12, H / 12 };

                SDL_Texture* piece_texture;
                if (mtx[i][j] == 1)
                    piece_texture = w_piece;
                else if (mtx[i][j] == 2)
                    piece_texture = b_piece;
                else if (mtx[i][j] == 3)
                    piece_texture = w_queen;
                else
                    piece_texture = b_queen;

                // Копируем текстуру на рендерер
                SDL_RenderCopy(ren, piece_texture, NULL, &rect);
            }
        }

        // draw hilight
        // Рисуем выделение
        // Устанавливаем зеленый цвет для выделения
        SDL_SetRenderDrawColor(ren, 0, 255, 0, 0);
        const double scale = 2.5;
        // Масштабируем сетку
        SDL_RenderSetScale(ren, scale, scale);
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                // Пропускаем неотмеченные клетки
                if (!is_highlighted_[i][j])
                    continue;
                SDL_Rect cell{ int(W * (j + 1) / 10 / scale), int(H * (i + 1) / 10 / scale), int(W / 10 / scale),
                              int(H / 10 / scale) };
                // Рисуем зеленые рамки вокруг отмеченных клеток
                SDL_RenderDrawRect(ren, &cell);
            }
        }

        // draw active
        // Функция для рисования активной клетки
        if (active_x != -1)
        {
            // Устанавливаем красный цвет для выделения
            SDL_SetRenderDrawColor(ren, 255, 0, 0, 0);
            SDL_Rect active_cell{ int(W * (active_y + 1) / 10 / scale), int(H * (active_x + 1) / 10 / scale),
                                 int(W / 10 / scale), int(H / 10 / scale) };
            // Рисуем красную рамку вокруг активной клетки
            SDL_RenderDrawRect(ren, &active_cell);
        }
        // Восстанавливаем масштаб
        SDL_RenderSetScale(ren, 1, 1);

        // draw arrows
        // Рисование стрелок
        SDL_Rect rect_left{ W / 40, H / 40, W / 15, H / 15 };
        // Рисуем стрелку "Назад"
        SDL_RenderCopy(ren, back, NULL, &rect_left);
        SDL_Rect replay_rect{ W * 109 / 120, H / 40, W / 15, H / 15 };
        // Рисуем кнопку "Повторить"
        SDL_RenderCopy(ren, replay, NULL, &replay_rect);

        // draw result
        // Рисование результатов игры
        if (game_results != -1)
        {
            string result_path = draw_path;
            // Путь к картинке с белым победителем
            if (game_results == 1)
                result_path = white_path;
            // Путь к картинке с черным победителем
            else if (game_results == 2)
                result_path = black_path;
            SDL_Texture* result_texture = IMG_LoadTexture(ren, result_path.c_str());
            if (result_texture == nullptr)
            {
                print_exception("IMG_LoadTexture can't load game result picture from " + result_path);
                return;
            }
            SDL_Rect res_rect{ W / 5, H * 3 / 10, W * 3 / 5, H * 2 / 5 };
            // Копируем текстуру результата на экран
            SDL_RenderCopy(ren, result_texture, NULL, &res_rect);
            // Освобождаем текстуру результата
            SDL_DestroyTexture(result_texture);
        }

        // Обновляем экран
        SDL_RenderPresent(ren);
        // next rows for mac os
        SDL_Delay(10);
        SDL_Event windowEvent;
        SDL_PollEvent(&windowEvent);
    }

    void print_exception(const string& text) {
        ofstream fout(project_path + "log.txt", ios_base::app);
        fout << "Error: " << text << ". "<< SDL_GetError() << endl;
        fout.close();
    }

  public:
    int W = 0;
    int H = 0;
    // history of boards
    // История
    vector<vector<vector<POS_T>>> history_mtx;

  private:
    SDL_Window *win = nullptr;
    SDL_Renderer *ren = nullptr;
    // textures
    // Текстуры
    SDL_Texture *board = nullptr;
    SDL_Texture *w_piece = nullptr;
    SDL_Texture *b_piece = nullptr;
    SDL_Texture *w_queen = nullptr;
    SDL_Texture *b_queen = nullptr;
    SDL_Texture *back = nullptr;
    SDL_Texture *replay = nullptr;
    // texture files names
    // Имена файлов текстур
    const string textures_path = project_path + "Textures/";
    const string board_path = textures_path + "board.png";
    const string piece_white_path = textures_path + "piece_white.png";
    const string piece_black_path = textures_path + "piece_black.png";
    const string queen_white_path = textures_path + "queen_white.png";
    const string queen_black_path = textures_path + "queen_black.png";
    const string white_path = textures_path + "white_wins.png";
    const string black_path = textures_path + "black_wins.png";
    const string draw_path = textures_path + "draw.png";
    const string back_path = textures_path + "back.png";
    const string replay_path = textures_path + "replay.png";
    // coordinates of chosen cell
    // активные координаты
    int active_x = -1, active_y = -1;
    // game result if exist
    // Результат игры
    int game_results = -1;
    // matrix of possible moves
    // Матрица возможных ходов, которые подсвечиваются
    vector<vector<bool>> is_highlighted_ = vector<vector<bool>>(8, vector<bool>(8, 0));
    // matrix of possible moves
    // 1 - white, 2 - black, 3 - white queen, 4 - black queen
    // Состояния доски, где значения 1, 2, 3 и 4 обозначают 
    // белый, черный, белую королеву и черную королеву соответственно
    vector<vector<POS_T>> mtx = vector<vector<POS_T>>(8, vector<POS_T>(8, 0));
    // series of beats for each move
    // Серия ударов
    vector<int> history_beat_series;
};
