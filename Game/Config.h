#pragma once
#include <fstream>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "../Models/Project_path.h"

class Config
{
  public:
    Config()
    {
        reload();
    }
    /*
    Метод reload() отвечает за чтение файла settings.json и сохранение его содержимого в объект config.
    Этот метод выполняется всякий раз, когда происходит изменение файла settings.json,
    чтобы гарантировать актуальность данных в объекте config.
    */
    void reload()
    {
        std::ifstream fin(project_path + "settings.json");
        fin >> config;
        fin.close();
    }

    /*
    Оператор () используется для доступа к паре (ключ, значение) из setting.json,
    например, (Height, 0) или (IsWhiteBot, false)
    */
    auto operator()(const string &setting_dir, const string &setting_name) const
    {
        return config[setting_dir][setting_name];
    }

  private:
    json config;
};
