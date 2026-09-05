//
// Created by charlie on 9/2/26.
//
#include <cmath>
#include "sim/Primitive_Modules.h"

namespace Sim::Primitive_Modules {
    Table_Module::Table_Module(const char *name, const int input_cnt, std::vector<std::vector<Core::WireState>*> *outputs,
                             const int delay) {
        this->type_id = -1;
        if (input_cnt <= 0) {
            std::cerr << "Invalid table module made." << std::endl << "Requires at least one input." <<
                    std::endl;
        }

        if (outputs->size() != static_cast<long>(pow(2, input_cnt))) {
            std::cerr << "Invalid table module made." << std::endl << "Incorrect number of outsput states" <<
                    std::endl << "has:"<<outputs->size()<<" expected:"<<pow(2, input_cnt);
        }

        this->input_cnt = input_cnt;
        this->output_cnt = static_cast<int>(outputs->at(0)->size());
        this->table = outputs;
        this->delay = delay;
        this->name = name;
    }

    void Table_Module::update(Core::Module *module, Core::EDA_Environment *env) {

        // Determine out put index
        int input_index = 0;
        for (int i = 0; i < this->input_cnt; i++) {
            if (const Core::WireState wire_state = env->get_wire(module->inputs[i])->state;
                Core::isWireStateImg(wire_state)) {
                // Propagate imaginary behavior.
                for (const Core::WireId target_wire: module->outputs) {
                    env->setWire(target_wire, wire_state, module->module_id);
                }
                return;
                } else if (wire_state == Core::WireState::HIGH) {
                    input_index = input_index | (1 << i);
                }
        }

        // Set outputs
        const std::vector<Core::WireState> *output = this->table->at(input_index);
        for (int i = 0; i < this->output_cnt; i++) {
            env->setWire(module->outputs[i], output->at(i), module->module_id);
        }
    }

    int Table_Module::inputUpdateDelay() {
        return this->delay;
    }

    Core::ModuleTypeDef* inverterTypeDef() {
        using namespace Core;
        auto table = new std::vector{new std::vector{WireState::HIGH}, new std::vector{WireState::LOW}};
        return new Table_Module("Inverter",
            1,
            table,
            1);
    }

    Core::ModuleTypeDef* andTypeDef() {
        using namespace Core;
        auto table = new std::vector{new std::vector{WireState::LOW}, new std::vector{WireState::LOW},new std::vector{WireState::LOW},new std::vector{WireState::HIGH}};
        return new Table_Module("And",
            2,
            table,
            1);
    }

    Log::Log (const char *prefix) {
        this->type_id = -1;
        this->input_cnt = 1;
        this->output_cnt = 0;
        this->name = "Logger";
        this->prefix = prefix;
    }

    void Log::update(Core::Module *module, Core::EDA_Environment *env) {
        std::cout << prefix << env->get_wire(module->inputs[0])->state << " @ " << env->getCurrentTime() <<
                std::endl;
    }

    int Log::inputUpdateDelay() {
        return 1;
    }

    Core::ModuleId Log::attach_logger(Core::EDA_Environment *env, const Core::WireId wire_id,
                                        const char *prefix) {
        const Core::ModuleId id = env->newModule(env->addModuleTypeDef(new Log(prefix)));
        env->bind_module_input(id, 0, wire_id);
        return id;
    }
}