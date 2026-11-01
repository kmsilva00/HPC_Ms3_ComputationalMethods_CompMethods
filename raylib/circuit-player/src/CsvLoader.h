#pragma once

#include "Circuit.h"

#include <string>

class CsvLoader {
public:
    static CircuitModel Load(
        const std::string& componentsPath,
        const std::string& connectionsPath,
        const std::string& measurementsPath);
};
