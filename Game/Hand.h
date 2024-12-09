#pragma once
#include <tuple>

#include "../Models/Move.h"
#include "../Models/Response.h"
#include "Board.h"

// methods for hands
// Класс Hand управляет взаимодействием с пользователем через события мыши и окна
class Hand
{
  public:
    // Конструктор принимает указатель на объект Board
    Hand(Board *board) : board(board)
    {
    }
    // Метод для получения координат ячейки, на которую кликнул пользователь
    tuple<Response, POS_T, POS_T> get_cell() const
    {
        // Объект для хранения событий SDL
        SDL_Event windowEvent;
        // Начальный ответ
        Response resp = Response::OK;
        // Координаты курсора мыши
        int x = -1, y = -1;
        // Преобразованные координаты ячеек
        int xc = -1, yc = -1;
        // Бесконечный цикл для ожидания события
        while (true)
        {
            // Проверяет наличие нового события
            if (SDL_PollEvent(&windowEvent))
            {
                // Обрабатывает различные типы событий
                switch (windowEvent.type)
                {
                // Закрытие окна
                case SDL_QUIT:
                    resp = Response::QUIT;
                    break;
                // Нажатие кнопки мыши
                case SDL_MOUSEBUTTONDOWN:
                    // Получает координаты X и Y
                    x = windowEvent.motion.x;
                    y = windowEvent.motion.y;
                    // Преобразует координаты в индексы ячеек
                    xc = int(y / (board->H / 10) - 1);
                    yc = int(x / (board->W / 10) - 1);
                    // Обрабатывает специальные области экрана
                    if (xc == -1 && yc == -1 && board->history_mtx.size() > 1)
                    {
                        // Кнопка возврата
                        resp = Response::BACK;
                    }
                    else if (xc == -1 && yc == 8)
                    {
                        // Кнопка повтора
                        resp = Response::REPLAY;
                    }
                    else if (xc >= 0 && xc < 8 && yc >= 0 && yc < 8)
                    {
                        // Обычная ячейка
                        resp = Response::CELL;
                    }
                    else
                    {
                        // Неправильная область
                        xc = -1;
                        yc = -1;
                    }
                    break;
                // Изменение размера окна
                case SDL_WINDOWEVENT:
                    if (windowEvent.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                    {
                        // Обновляет размер окна
                        board->reset_window_size();
                        break;
                    }
                }
                // Если получен любой ответ, кроме OK, выход из цикла
                if (resp != Response::OK)
                    break;
            }
        }
        // Возвращает кортеж с ответом и координатами ячейки
        return {resp, xc, yc};
    }

    // Метод для ожидания реакции пользователя после окончания игры
    Response wait() const
    {
        // Объект для хранения событий SDL
        SDL_Event windowEvent;
        // Начальный ответ
        Response resp = Response::OK;
        // Бесконечный цикл для ожидания события
        while (true)
        {
            // Проверяет наличие нового события
            if (SDL_PollEvent(&windowEvent))
            {
                // Обрабатывает различные типы событий
                switch (windowEvent.type)
                {
                // Закрытие окна
                case SDL_QUIT:
                    resp = Response::QUIT;
                    break;
                // Изменение размера окна
                case SDL_WINDOWEVENT_SIZE_CHANGED:
                    // Обновляет размер окна
                    board->reset_window_size();
                    break;
                // Нажатие кнопки мыши
                case SDL_MOUSEBUTTONDOWN: {
                    // Получает координаты X и Y
                    int x = windowEvent.motion.x;
                    int y = windowEvent.motion.y;
                    // Преобразует координаты в индексы ячеек
                    int xc = int(y / (board->H / 10) - 1);
                    int yc = int(x / (board->W / 10) - 1);
                    // Обрабатывает кнопку повтора
                    if (xc == -1 && yc == 8)
                        resp = Response::REPLAY;
                }
                break;
                }
                // Если получен любой ответ, кроме OK, выход из цикла
                if (resp != Response::OK)
                    break;
            }
        }
        // Возвращает ответ
        return resp;
    }

  private:
    Board *board;
};
