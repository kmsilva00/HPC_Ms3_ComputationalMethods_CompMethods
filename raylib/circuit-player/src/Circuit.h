#pragma once

#include "raylib.h"

#include <string>
#include <unordered_map>
#include <vector>

enum class ComponentType {
    Junction,
    Diode,
    Resistor,
    Inductor,
    Capacitor,
    PowerIn,
    Gate,
    LoadCell
};

struct ComponentDefinition {
    const char* displayName;
    const char* iconPath;
    const char* unit;
};

inline bool ParseComponentType(const std::string& text, ComponentType& out) {
    if (text == "junction")  { out = ComponentType::Junction; return true; }
    if (text == "diode")     { out = ComponentType::Diode; return true; }
    if (text == "resistor")  { out = ComponentType::Resistor; return true; }
    if (text == "inductor")  { out = ComponentType::Inductor; return true; }
    if (text == "capacitor") { out = ComponentType::Capacitor; return true; }
    if (text == "power_in")  { out = ComponentType::PowerIn; return true; }
    if (text == "gate")      { out = ComponentType::Gate; return true; }
    if (text == "load_cell") { out = ComponentType::LoadCell; return true; }
    return false;
}

inline const ComponentDefinition& DefinitionFor(ComponentType type) {
    static const ComponentDefinition junction {"", "", ""};
    static const ComponentDefinition diode    {"Diode",      "icons/diode.png",      "V"};
    static const ComponentDefinition resistor {"Resistor",   "icons/resistor.png",   "ohm"};
    static const ComponentDefinition inductor {"Inductor",   "icons/inductor.png",   "V"};
    static const ComponentDefinition capacitor{"Capacitor",  "icons/capacitor.png",  "V"};
    static const ComponentDefinition powerIn  {"Power Input","icons/cell.png",      "V"};
    static const ComponentDefinition gate     {"Switch",     "icons/switch_off.png", ""};
    static const ComponentDefinition loadCell {"Load Cell",  "icons/cell.png",      "N"};

    switch (type) {
        case ComponentType::Junction: return junction;
        case ComponentType::Diode:    return diode;
        case ComponentType::Resistor: return resistor;
        case ComponentType::Inductor: return inductor;
        case ComponentType::Capacitor:return capacitor;
        case ComponentType::PowerIn:  return powerIn;
        case ComponentType::Gate:     return gate;
        case ComponentType::LoadCell: return loadCell;
    }
    return resistor;
}

struct Measurement {
    double timestamp = 0.0;
    double value = 0.0;
};

struct Component {
    std::string id;
    ComponentType type = ComponentType::Resistor;
    Vector2 position{0.0f, 0.0f}; // top-left of icon
    std::vector<Measurement> measurements;
};

struct Connection {
    std::string from;
    std::string to;
};

struct CircuitModel {
    std::vector<Component> components;
    std::vector<Connection> connections;
    std::unordered_map<std::string, std::size_t> componentIndex;

    double Duration() const {
        double result = 0.0;
        for (const auto& component : components) {
            if (!component.measurements.empty()) {
                result = (component.measurements.back().timestamp > result)
                    ? component.measurements.back().timestamp : result;
            }
        }
        return result;
    }
};
