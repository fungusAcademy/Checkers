#pragma once
#include <chrono>
#include <thread>

#include "../Models/Project_path.h"
#include "Board.h"
#include "Config.h"
#include "Hand.h"
#include "Logic.h"

class Game
{
  public:
    Game() : board(config("WindowSize", "Width"), config("WindowSize", "Hight")), hand(&board), logic(&board, &config)
    {
        ofstream fout(project_path + "log.txt", ios_base::trunc);
        fout.close();
    }

    // to start checkers
    int play()
    {
        // Запуск таймера игры
        auto start = chrono::steady_clock::now();
        // При обновлении доски обновляем логику и перерисовываем доску
        if (is_replay)
        {
            // Создаем объект логики игры
            logic = Logic(&board, &config);
            // Заново загружаем настройки
            config.reload();
            // Перерисовываем игровое поле
            board.redraw();
        }
        else
        {
            // Начинаем рисовать новую игру
            board.start_draw();
        }
        // Сбрасываем флаг повтора
        is_replay = false;
        // Переменная для отслеживания количества ходов
        int turn_num = -1;
        // Флаг выхода из игры
        bool is_quit = false;
        // Максимальное количество ходов берем из settings.json
        const int Max_turns = config("Game", "MaxNumTurns");

        // Игра идёт до победы кого-то или достижения максимального количества ходов
        while (++turn_num < Max_turns)
        {
            // Обнуляем переменную серии ударов
            beat_series = 0;
            // Находим возможные ходы
            logic.find_turns(turn_num % 2);
            // Если нет возможных ходов, выходим из цикла
            if (logic.turns.empty())
                break;
            // Устанавливаем глубину поиска в зависимости от уровня бота из setting.json
            logic.Max_depth = config("Bot", string((turn_num % 2) ? "Black" : "White") + string("BotLevel"));
            // Проверяем, является ли текущий игрок ботом
            if (!config("Bot", string("Is") + string((turn_num % 2) ? "Black" : "White") + string("Bot")))
            {
                // ход игрока
                auto resp = player_turn(turn_num % 2);
                // Обработка различных ответов игрока
                // Выход
                if (resp == Response::QUIT)
                {
                    is_quit = true;
                    break;
                }
                // Повтор игры
                else if (resp == Response::REPLAY)
                {
                    is_replay = true;
                    break;
                }
                // Откатываем состояние доски
                else if (resp == Response::BACK)
                {
                    if (config("Bot", string("Is") + string((1 - turn_num % 2) ? "Black" : "White") + string("Bot")) &&
                        !beat_series && board.history_mtx.size() > 2)
                    {
                        board.rollback();
                        --turn_num;
                    }
                    if (!beat_series)
                        --turn_num;

                    board.rollback();
                    --turn_num;
                    beat_series = 0;
                }
            }
            // Ход бота
            else
                bot_turn(turn_num % 2);
        }
        // Останавливаем таймер
        auto end = chrono::steady_clock::now();
        // Запись времени игры в файл log.txt
        ofstream fout(project_path + "log.txt", ios_base::app);
        fout << "Game time: " << (int)chrono::duration<double, milli>(end - start).count() << " millisec\n";
        fout.close();

        // Если нужно повторить игру, вызывается функция `play()` рекурсивно
        if (is_replay)
            return play();
        // Если игра была завершена досрочно, возвращаем 0
        if (is_quit)
            return 0;
        // Определяем результат игры
        int res = 2;
        if (turn_num == Max_turns)
        {
            res = 0;
        }
        else if (turn_num % 2)
        {
            res = 1;
        }
        // Показываем финальный экран
        board.show_final(res);
        // Ожидаем ответ от пользователя после окончания игры
        auto resp = hand.wait();
        // Если пользователь хочет повторить игру, вызываем функцию `play()` снова
        if (resp == Response::REPLAY)
        {
            is_replay = true;
            return play();
        }
        // Возвращаем результат игры
        return res;
    }

  private:
    // Описывает ход бота
    void bot_turn(const bool color)
    {
        // Запускает таймер
        auto start = chrono::steady_clock::now();

        // Берет задержку перед ходом бота из Settings.json
        auto delay_ms = config("Bot", "BotDelayMS");
        // new thread for equal delay for each turn
        // Создает поток для управления задержкой
        thread th(SDL_Delay, delay_ms);
        // Находит наилучший ход бота
        auto turns = logic.find_best_turns(color);
        // Слияние потоков
        th.join();
        // Является ли ход первым
        bool is_first = true;
        // making moves
        // Выполнение ходов
        for (auto turn : turns)
        {
            // Если ход не первый, то вставляется задержка
            if (!is_first)
            {
                SDL_Delay(delay_ms);
            }
            is_first = false;
            // Увеличивает счетчик ударов
            beat_series += (turn.xb != -1);
            // Выполняет перемещение фигуры
            board.move_piece(turn, beat_series);
        }

        //Завершение работы таймера
        auto end = chrono::steady_clock::now();
        // Запись времени хода бота в log.txt
        ofstream fout(project_path + "log.txt", ios_base::app);
        fout << "Bot turn time: " << (int)chrono::duration<double, milli>(end - start).count() << " millisec\n";
        fout.close();
    }

    Response player_turn(const bool color)
    {
        // return 1 if quit
        // Пара координат
        vector<pair<POS_T, POS_T>> cells;
        // Проходимся по всем возможным ходам и добавляем их в вектор 'cells'
        for (auto turn : logic.turns)
        {
            cells.emplace_back(turn.x, turn.y);
        }
        // Подсвечиваем допустимые ходы на доске
        board.highlight_cells(cells);
        // Переменные для хранения текущей позиции и позиции следующего хода
        move_pos pos = {-1, -1, -1, -1};
        POS_T x = -1, y = -1;
        // trying to make first move
        // Цикл для получения первого хода от игрока
        while (true)
        {
            // Получение координаты ячейки от пользователя
            auto resp = hand.get_cell();
            // Если пользователь выбрал не ячейку, возвращаем соответствующий ответ
            if (get<0>(resp) != Response::CELL)
                return get<0>(resp);
            // Извлечение координат выбранной ячейки
            pair<POS_T, POS_T> cell{get<1>(resp), get<2>(resp)};
            // Проверка, является ли выбранная ячейка допустимым ходом
            bool is_correct = false;
            for (auto turn : logic.turns)
            {
                if (turn.x == cell.first && turn.y == cell.second)
                {
                    is_correct = true;
                    break;
                }
                if (turn == move_pos{x, y, cell.first, cell.second})
                {
                    pos = turn;
                    break;
                }
            }
            // Если найден допустимый ход, выходим из цикла
            if (pos.x != -1)
                break;
            // Если ход недопустим, очищаем подсветку и продолжаем
            if (!is_correct)
            {
                if (x != -1)
                {
                    board.clear_active();
                    board.clear_highlight();
                    board.highlight_cells(cells);
                }
                x = -1;
                y = -1;
                continue;
            }
            // Сохраняем координаты первой ячейки и подсвечиваем доступные ходы из неё
            x = cell.first;
            y = cell.second;
            board.clear_highlight();
            board.set_active(x, y);
            vector<pair<POS_T, POS_T>> cells2;
            for (auto turn : logic.turns)
            {
                if (turn.x == x && turn.y == y)
                {
                    cells2.emplace_back(turn.x2, turn.y2);
                }
            }
            board.highlight_cells(cells2);
        }
        // Очищаем все выделения и выполняем первый ход
        board.clear_highlight();
        board.clear_active();
        board.move_piece(pos, pos.xb != -1);
        // Если взятие фигуры невозможно, возвращаемся
        if (pos.xb == -1)
            return Response::OK;
        // continue beating while can
        // Продолжаем серию взятия фигур, если это возможно
        beat_series = 1;
        while (true)
        {
            // Находим возможные ходы для продолжения серии взятия
            logic.find_turns(pos.x2, pos.y2);
            if (!logic.have_beats)
                break;
            // Подсвечиваем возможные ходы для продолжения серии
            vector<pair<POS_T, POS_T>> cells;
            for (auto turn : logic.turns)
            {
                cells.emplace_back(turn.x2, turn.y2);
            }
            board.highlight_cells(cells);
            board.set_active(pos.x2, pos.y2);
            // trying to make move
            // Цикл для получения следующего хода от игрока
            while (true)
            {
                // Получение следующей ячейки от пользователя
                auto resp = hand.get_cell();
                if (get<0>(resp) != Response::CELL)
                    return get<0>(resp);
                pair<POS_T, POS_T> cell{get<1>(resp), get<2>(resp)};
                // Проверка, является ли следующий ход допустимым
                bool is_correct = false;
                for (auto turn : logic.turns)
                {
                    if (turn.x2 == cell.first && turn.y2 == cell.second)
                    {
                        is_correct = true;
                        pos = turn;
                        break;
                    }
                }
                if (!is_correct)
                    continue;
                // Очистка всех выделений и выполнение следующего хода
                board.clear_highlight();
                board.clear_active();
                beat_series += 1;
                board.move_piece(pos, beat_series);
                break;
            }
        }
        // Выполнение произошло без ошибок
        return Response::OK;
    }

  private:
    Config config;
    Board board;
    Hand hand;
    Logic logic;
    int beat_series;
    bool is_replay = false;
};
