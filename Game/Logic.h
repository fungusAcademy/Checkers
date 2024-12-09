#pragma once
#include <random>
#include <vector>

#include "../Models/Move.h"
#include "Board.h"
#include "Config.h"

const int INF = 1e9;

class Logic
{
  public:
    Logic(Board *board, Config *config) : board(board), config(config)
    {
        rand_eng = std::default_random_engine (
            !((*config)("Bot", "NoRandom")) ? unsigned(time(0)) : 0);
        scoring_mode = (*config)("Bot", "BotScoringType");
        optimization = (*config)("Bot", "Optimization");
    }

    vector<move_pos> find_best_turns(const bool color)
    {
        // Сбрасываем данные
        next_move.clear();
        next_best_state.clear();
        // Ищем первый лучший ход
        find_first_best_turn(board->get_board(), color, -1, -1, 0);

        vector<move_pos> result;
        int cur_state = 0;

        // Хотя бы один ход будет, поэтому do ... while
        do
        {
            // Записываем следующий успешный ход и состояние
            result.push_back(next_move[cur_state]);
            cur_state = next_best_state[cur_state];
        } 
        while (cur_state != -1 && next_move[cur_state].x != -1);

        return result;
    }

private:
    // Возвращает новое состояние доски после выполнения хода
    vector<vector<POS_T>> make_turn(vector<vector<POS_T>> mtx, move_pos turn) const
    {
        // Удаляет фигуру
        if (turn.xb != -1)
            mtx[turn.xb][turn.yb] = 0;
        // Превращает пешку в ферзя
        if ((mtx[turn.x][turn.y] == 1 && turn.x2 == 0) || (mtx[turn.x][turn.y] == 2 && turn.x2 == 7))
            mtx[turn.x][turn.y] += 2;
        // Перемещает фигуру
        mtx[turn.x2][turn.y2] = mtx[turn.x][turn.y];
        mtx[turn.x][turn.y] = 0;
        // Возвращает новое состояние доски
        return mtx;
    }

    // возвращает оценку текущего положения на доске в виде double
    double calc_score(const vector<vector<POS_T>> &mtx, const bool first_bot_color) const
    {
        // color - who is max player
        // Переменные для подсчета пешек
        double w = 0, wq = 0, b = 0, bq = 0;
        // Два вложенных цикла проходят по всем клеткам доски.
        // Если на текущей клетке находится белая пешка, увеличивается счетчик белых пешек.
        // Аналогично для черных пешек и ферзей.
        // Если режим оценки "NumberAndPotential", дополнительно учитывается положение пешек на доске.
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                w += (mtx[i][j] == 1);
                wq += (mtx[i][j] == 3);
                b += (mtx[i][j] == 2);
                bq += (mtx[i][j] == 4);
                if (scoring_mode == "NumberAndPotential")
                {
                    w += 0.05 * (mtx[i][j] == 1) * (7 - i);
                    b += 0.05 * (mtx[i][j] == 2) * (i);
                }
            }
        }
        // Если бот играет черными, происходит обмен значений переменных,
        // чтобы максимизирующий игрок всегда был белым.
        if (!first_bot_color)
        {
            swap(b, w);
            swap(bq, wq);
        }
        // Победа черного игрока
        if (w + wq == 0)
            return INF;
        // победа белого игрока
        if (b + bq == 0)
            return 0;
        // Коэффициент важности королевы
        int q_coef = 4;

        if (scoring_mode == "NumberAndPotential")
        {
            q_coef = 5;
        }
        //вычисляется итоговая оценка как отношение сил черного и белого игрока
        return (b + bq * q_coef) / (w + wq * q_coef);
    }

    double find_first_best_turn(vector<vector<POS_T>> mtx, const bool color, const POS_T x, const POS_T y, size_t state,
                                double alpha = -1)
    {
        // Обнуляем следующий ход и состояние
        next_best_state.push_back(-1);
        next_move.emplace_back(-1, -1, -1, -1);
        // Считаем возможные ходы, если можно ходить
        if (state != 0)
        {
            find_turns(x, y, mtx);
        }
        
        // Делаем копии
        auto cur_turns = turns;
        bool cur_have_beats = have_beats;

        // Если нельзя побить и ход закончен, то ходит следующий игрок
        if (!cur_have_beats && state != 0)
        {
            return find_best_turns_rec(mtx, 1 - color, 0, alpha);
        }

        // Лучший счет
        double best_score = -1;
        // Перебираем возможные ходы
        for (auto turn : cur_turns)
        {
            // Текущее состояние
            size_t next_state = next_move.size();
            double score;
            // Если можно побить, то бьем и перемещаемся, игрок не меняется
            if (cur_have_beats)
            {
                score = find_first_best_turn(make_turn(mtx, turn), color, turn.x2, turn.y2, next_state, best_score);
            }
            // Если бить некого, то ходит следующий игрок
            else
            {
                score = find_best_turns_rec(make_turn(mtx, turn), 1 - color, 0, best_score);
            }

            // Выбираем первый оптимальный ход
            if (score > best_score)
            {
                best_score = score;
                next_best_state[state] = (cur_have_beats ? next_state : -1);
                next_move[state] = turn;
            }
        }

        return best_score;
    }

    double find_best_turns_rec(vector<vector<POS_T>> mtx, const bool color, const size_t depth, double alpha = -1, double beta = INF + 1, const POS_T x = -1, const POS_T y = -1)
    {
        // Если достигли максимальной глубины поиска, возвращаем оценку позиции
        if (depth == Max_depth) 
        {
            return calc_score(mtx, (depth % 2 == color));
        }

        // Если указаны конкретные координаты, ищем ходы из этой позиции
        if (x != -1 && y != -1)
        {
            find_turns(x, y, mtx);
        }
        // Иначе ищем ходы для данного цвета
        else                    
        {
            find_turns(color, mtx);
        }

        // Сохраняем текущие ходы и признак наличия ударов
        auto curTurns = turns;
        auto cur_have_beats = have_beats;

        // Если нет ударов и это не первый ход, меняем очередь хода
        if (!cur_have_beats && x != -1)
        {
            return find_best_turns_rec(mtx, 1 - color, depth + 1, alpha, beta, -1, -1);
        }

        // Если нет доступных ходов, то игрок проиграл
        if (curTurns.empty()) 
        {
            return (depth % 2 ? 0 : INF);
        }

        // Инициализация минимального и максимального значений оценок
        double min_score = INF + 1;
        double max_score = -1;

        // Перебор всех возможных ходов
        for (auto turn : curTurns) 
        {
            double score = 0.0;

            // Продолжаем искать лучшие ходы для удара
            if (cur_have_beats) 
            {
                score = find_best_turns_rec(make_turn(mtx, turn), color, depth, alpha, beta, turn.x2, turn.y2);
            }
            // Если это не удар, то меняем очередь хода
            else 
            {
                score = find_best_turns_rec(make_turn(mtx, turn), 1 - color, depth + 1, alpha, beta);
            }

            // Обновляем минимум и максимум
            min_score = min(min_score, score);
            max_score = max(max_score, score);

            return (depth % 2 ? max_score : min_score);
        }

        // Возвращаем итоговый результат
        return (depth % 2 ? max_score : min_score);
    }

public:
    void find_turns(const bool color)
    {
        find_turns(color, board->get_board());
    }

    void find_turns(const POS_T x, const POS_T y)
    {
        find_turns(x, y, board->get_board());
    }

private:
    // находит все возможные ходы для фигур определенного цвета
    // на шахматной доске, представленной матрицей mtx.
    void find_turns(const bool color, const vector<vector<POS_T>> &mtx)
    {
        vector<move_pos> res_turns;
        bool have_beats_before = false;
        // Цикл проходят по всем клеткам доски.Если текущая клетка содержит фигуру противоположного цвета,
        // вызывается внутренняя функция find_turns, которая ищет доступные ходы для этой фигуры.
        // Если найден удар, то предыдущие ходы очищаются, так как сначала нужно рассмотреть все удары.
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                if (mtx[i][j] && mtx[i][j] % 2 != color)
                {
                    find_turns(i, j, mtx);
                    if (have_beats && !have_beats_before)
                    {
                        have_beats_before = true;
                        res_turns.clear();
                    }
                    if ((have_beats_before && have_beats) || !have_beats_before)
                    {
                        res_turns.insert(res_turns.end(), turns.begin(), turns.end());
                    }
                }
            }
        }
        turns = res_turns;
        // Ходы перемешиваются случайным образов
        shuffle(turns.begin(), turns.end(), rand_eng);
        have_beats = have_beats_before;
    }

    // находит все возможные ходы для конкретной фигуры,
    // стоящей на позиции (x, y) на шахматной доске mtx.
    void find_turns(const POS_T x, const POS_T y, const vector<vector<POS_T>> &mtx)
    {
        turns.clear();
        have_beats = false;
        POS_T type = mtx[x][y];
        // check beats
        // В зависимости от типа фигуры (пешки или ферзи),
        // выполняются разные алгоритмы поиска возможных ходов.
        switch (type)
        {
        case 1:
        case 2:
            // check pieces
            // Для каждой возможной клетки удара проверяется,
            // находится ли она внутри доски, есть ли там фигура противника
            // и нет ли между ними других фигур.
            // Если условия выполнены, добавляется новый ход.
            for (POS_T i = x - 2; i <= x + 2; i += 4)
            {
                for (POS_T j = y - 2; j <= y + 2; j += 4)
                {
                    if (i < 0 || i > 7 || j < 0 || j > 7)
                        continue;
                    POS_T xb = (x + i) / 2, yb = (y + j) / 2;
                    if (mtx[i][j] || !mtx[xb][yb] || mtx[xb][yb] % 2 == type % 2)
                        continue;
                    turns.emplace_back(x, y, i, j, xb, yb);
                }
            }
            break;
        default:
            // check queens
            // Ферзь может двигаться по диагонали, поэтому цикл проходит по восьми направлениям.
            // Если встречается фигура противника, она добавляется к возможным ударам.
            for (POS_T i = -1; i <= 1; i += 2)
            {
                for (POS_T j = -1; j <= 1; j += 2)
                {
                    POS_T xb = -1, yb = -1;
                    for (POS_T i2 = x + i, j2 = y + j; i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1; i2 += i, j2 += j)
                    {
                        if (mtx[i2][j2])
                        {
                            if (mtx[i2][j2] % 2 == type % 2 || (mtx[i2][j2] % 2 != type % 2 && xb != -1))
                            {
                                break;
                            }
                            xb = i2;
                            yb = j2;
                        }
                        if (xb != -1 && xb != i2)
                        {
                            turns.emplace_back(x, y, i2, j2, xb, yb);
                        }
                    }
                }
            }
            break;
        }
        // check other turns
        // Если были найдены удары, устанавливается флаг и выполнение функции прекращается.
        if (!turns.empty())
        {
            have_beats = true;
            return;
        }
        switch (type)
        {
        case 1:
        case 2:
            // check pieces
            // Поиск обычных ходов для пешек
            // Пешка может ходить вперед на одну клетку, если эта клетка свободна
            {
                POS_T i = ((type % 2) ? x - 1 : x + 1);
                for (POS_T j = y - 1; j <= y + 1; j += 2)
                {
                    if (i < 0 || i > 7 || j < 0 || j > 7 || mtx[i][j])
                        continue;
                    turns.emplace_back(x, y, i, j);
                }
                break;
            }
        default:
            // check queens
            // Поиск обычных ходов для ферзей
            // Ферзь может двигаться по любой прямой линии, пока не встретит другую фигуру.
            for (POS_T i = -1; i <= 1; i += 2)
            {
                for (POS_T j = -1; j <= 1; j += 2)
                {
                    for (POS_T i2 = x + i, j2 = y + j; i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1; i2 += i, j2 += j)
                    {
                        if (mtx[i2][j2])
                            break;
                        turns.emplace_back(x, y, i2, j2);
                    }
                }
            }
            break;
        }
    }

  public:
    // хранит список возможных ходов для текущего состояния доски
    vector<move_pos> turns;
    // указывает, есть ли среди возможных ходов удары
    bool have_beats;
    // Глубина поиска алгоритма
    int Max_depth;

  private:
    // генератор случайных чисел
    default_random_engine rand_eng;
    //определяет метод оценки позиции
    // NumberAndPotential или NumberOnly
    string scoring_mode;
    //Параметр оптимизации O0, O1 или O2
    string optimization;
    //Содержит следующий лучший ход, выбранный алгоритмом
    vector<move_pos> next_move;
    // содержит информацию о лучшем состоянии после следующего хода
    vector<int> next_best_state;
    // Текущее состояние доски
    Board *board;
    // Указатель на настройки (settings.json)
    Config *config;
};
