#include "CsvLoader.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace {
std::string Trim(std::string s) {
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}

// Supports quoted fields and escaped quotes (""). Multiline fields are not supported.
std::vector<std::string> ParseCsvRow(const std::string& line) {
    std::vector<std::string> fields;
    std::string field;
    bool quoted = false;

    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (c == '"') {
            if (quoted && i + 1 < line.size() && line[i + 1] == '"') {
                field += '"';
                ++i;
            } else {
                quoted = !quoted;
            }
        } else if (c == ',' && !quoted) {
            fields.push_back(Trim(field));
            field.clear();
        } else {
            field += c;
        }
    }
    if (quoted) throw std::runtime_error("Unclosed quoted field in CSV row: " + line);
    fields.push_back(Trim(field));
    return fields;
}

std::vector<std::vector<std::string>> ReadCsv(const std::string& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("Cannot open CSV file: " + path);

    std::vector<std::vector<std::string>> rows;
    std::string line;
    std::size_t lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (Trim(line).empty() || Trim(line).front() == '#') continue;

        try {
            rows.push_back(ParseCsvRow(line));
        } catch (const std::exception& e) {
            throw std::runtime_error(path + ":" + std::to_string(lineNumber) + ": " + e.what());
        }
    }
    return rows;
}

std::unordered_map<std::string, std::size_t> HeaderMap(
    const std::vector<std::string>& header,
    const std::string& path)
{
    std::unordered_map<std::string, std::size_t> result;
    for (std::size_t i = 0; i < header.size(); ++i) result[header[i]] = i;
    if (result.empty()) throw std::runtime_error("Missing CSV header: " + path);
    return result;
}

std::size_t Column(
    const std::unordered_map<std::string, std::size_t>& header,
    const std::string& name,
    const std::string& path)
{
    auto it = header.find(name);
    if (it == header.end()) {
        throw std::runtime_error("Missing required column '" + name + "' in " + path);
    }
    return it->second;
}

const std::string& Field(
    const std::vector<std::string>& row,
    std::size_t index,
    const std::string& path,
    std::size_t rowNumber)
{
    if (index >= row.size()) {
        throw std::runtime_error(path + ": row " + std::to_string(rowNumber) +
                                 " has fewer fields than the header");
    }
    return row[index];
}

double ParseDouble(const std::string& text, const std::string& where) {
    std::size_t used = 0;
    double value = 0.0;
    try {
        value = std::stod(text, &used);
    } catch (...) {
        throw std::runtime_error("Invalid number '" + text + "' at " + where);
    }
    if (used != text.size()) {
        throw std::runtime_error("Invalid number '" + text + "' at " + where);
    }
    return value;
}
} // namespace

CircuitModel CsvLoader::Load(
    const std::string& componentsPath,
    const std::string& connectionsPath,
    const std::string& measurementsPath)
{
    CircuitModel model;

    // Components
    {
        const auto rows = ReadCsv(componentsPath);
        if (rows.empty()) throw std::runtime_error("No component rows in " + componentsPath);
        const auto header = HeaderMap(rows.front(), componentsPath);
        const auto idCol = Column(header, "id", componentsPath);
        const auto typeCol = Column(header, "type", componentsPath);
        const auto xCol = Column(header, "x", componentsPath);
        const auto yCol = Column(header, "y", componentsPath);

        for (std::size_t r = 1; r < rows.size(); ++r) {
            const auto& row = rows[r];
            const std::string id = Field(row, idCol, componentsPath, r + 1);
            if (id.empty()) throw std::runtime_error("Empty component id in " + componentsPath);
            if (model.componentIndex.count(id)) {
                throw std::runtime_error("Duplicate component id '" + id + "'");
            }

            ComponentType type;
            const std::string typeText = Field(row, typeCol, componentsPath, r + 1);
            if (!ParseComponentType(typeText, type)) {
                throw std::runtime_error("Unknown component type '" + typeText + "' for " + id);
            }

            Component component;
            component.id = id;
            component.type = type;
            component.position.x = static_cast<float>(
                ParseDouble(Field(row, xCol, componentsPath, r + 1), componentsPath));
            component.position.y = static_cast<float>(
                ParseDouble(Field(row, yCol, componentsPath, r + 1), componentsPath));

            model.componentIndex[id] = model.components.size();
            model.components.push_back(std::move(component));
        }
    }

    // Connections
    {
        const auto rows = ReadCsv(connectionsPath);
        if (!rows.empty()) {
            const auto header = HeaderMap(rows.front(), connectionsPath);
            const auto fromCol = Column(header, "from", connectionsPath);
            const auto toCol = Column(header, "to", connectionsPath);

            for (std::size_t r = 1; r < rows.size(); ++r) {
                const auto& row = rows[r];
                Connection connection{
                    Field(row, fromCol, connectionsPath, r + 1),
                    Field(row, toCol, connectionsPath, r + 1)
                };
                if (!model.componentIndex.count(connection.from)) {
                    throw std::runtime_error("Connection references unknown component '" +
                                             connection.from + "'");
                }
                if (!model.componentIndex.count(connection.to)) {
                    throw std::runtime_error("Connection references unknown component '" +
                                             connection.to + "'");
                }
                model.connections.push_back(std::move(connection));
            }
        }
    }

    // Measurements
    {
        const auto rows = ReadCsv(measurementsPath);
        if (!rows.empty()) {
            const auto header = HeaderMap(rows.front(), measurementsPath);
            const auto idCol = Column(header, "component_id", measurementsPath);
            const auto timeCol = Column(header, "timestamp", measurementsPath);
            const auto valueCol = Column(header, "value", measurementsPath);

            for (std::size_t r = 1; r < rows.size(); ++r) {
                const auto& row = rows[r];
                const std::string id = Field(row, idCol, measurementsPath, r + 1);
                auto found = model.componentIndex.find(id);
                if (found == model.componentIndex.end()) {
                    throw std::runtime_error("Measurement references unknown component '" + id + "'");
                }

                const double timestamp = ParseDouble(
                    Field(row, timeCol, measurementsPath, r + 1), measurementsPath);
                const double value = ParseDouble(
                    Field(row, valueCol, measurementsPath, r + 1), measurementsPath);
                if (timestamp < 0.0) {
                    throw std::runtime_error("Negative timestamp for component '" + id + "'");
                }
                model.components[found->second].measurements.push_back({timestamp, value});
            }
        }
    }

    // Stable sort preserves file order for duplicate timestamps; upper_bound then
    // selects the last sample at that timestamp.
    for (auto& component : model.components) {
        std::stable_sort(component.measurements.begin(), component.measurements.end(),
            [](const Measurement& a, const Measurement& b) {
                return a.timestamp < b.timestamp;
            });
    }

    return model;
}
