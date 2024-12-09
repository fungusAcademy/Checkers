#pragma once

/*
Respone определяет набор значений,
каждое из которых представляет собой ответ на событие в игре
OK - успешное завершение программы
BACK - нажатие на кнопку "back"
REPLAY - нажатие на кнопку "replay"
QUIT - Нажатие на кнопку "выход"
CELL - Нажатие на ячейку
*/
enum class Response
{
    OK,
    BACK,
    REPLAY,
    QUIT,
    CELL
};
